#include "stdafx.h"
#include "App.h"
#include "MultiViewport.h"
#include "D3D12/CommandListManager.h"
#include "D3D12/CommandContext.h"
#include "D3D12/TextureManager.h"
#include "D3D12/PostProcess.h"
#include "Mesh/Mesh.h"

#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_internal.h"
#include "CompiledShaders/imgui_vert.h"
#include "CompiledShaders/imgui_pixel.h"
#include "CompiledShaders/TileTexture3D_PS.h"
#include "CompiledShaders/PreviewTexture3D_PS.h"
#include "CompiledShaders/DrawQuad_VS.h"
#include "CompiledShaders/PresentSDR_PS.h"

using namespace Microsoft::WRL;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return App::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

App* App::m_app = nullptr;

App::App()
{
	assert(m_app == nullptr);
	m_app = this;
}

App::~App()
{
	// Wait for the GPU is done processing the command queue before release the resource
	g_CommandManager.IdleGPU();

	ShutdownWindow();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	PostProcess::Shutdown();

	CommandContext::DestroyAllContexts();
	g_CommandManager.Shutdown();
	DescriptorAllocator::DestroyAll();
}

App* App::GetApp()
{
	return m_app;
}

HINSTANCE App::AppInst() const
{
	return m_hAppInst;
}

HWND App::MainWnd() const
{
	return m_hMainWnd;
}

ID3D12Device* App::GetDevice() const
{
	return m_d3dDevice.Get();
}

float App::AspectRatio() const
{
	return static_cast<float>(m_clientWidth) / m_clientHeight;
}

bool App::Get4xMSAAState() const
{
	return m_4xMSAAState;
}

void App::Set4xMSAAState(bool v)
{
	if (m_4xMSAAState != v)
	{
		m_4xMSAAState = v;

		CreateSwapChain();
		OnResize();
	}
}

bool App::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D())
		return false;

	InitImgui();

	Global::InitializeGlobalStates();
	CreateAppRootSignature();
	CreateAppPipelineState();
	CreateFontTexture();

	m_fullScreenQuad.reset(Mesh::CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f));
	PostProcess::Initialize(m_sceneColorBuffer.get());

	return true;
}

bool App::InitMainWindow()
{
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"Atmosphere App", NULL };

	if (!::RegisterClassEx(&wc))
	{
		MessageBoxW(0, L"Register Class Failed.", 0, 0);
		return false;
	}

	RECT R = { 0, 0, m_clientWidth, m_clientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	m_hMainWnd = ::CreateWindow(wc.lpszClassName, m_mainWndCaption.c_str(), WS_OVERLAPPEDWINDOW, 100, 100, width, height, 0, 0, wc.hInstance, 0);
	if (!m_hMainWnd)
	{
		MessageBoxW(0, L"Create Window Failed.", 0, 0);
		return false;
	}
	m_hAppInst = wc.hInstance;
	ShowWindow(m_hMainWnd, SW_SHOWDEFAULT);
	UpdateWindow(m_hMainWnd);

	return true;
}

bool App::InitDirect3D()
{
	// Enable debug layer
#if defined(_DEBUG) || defined(DEBUG)
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())));
		debugController->EnableDebugLayer();
	}
#endif

	// Create device
	{
		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_dxgiFactory.GetAddressOf())));

		HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(m_d3dDevice.GetAddressOf()));

		if (FAILED(hardwareResult))
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter> pWarpAdapter;
			ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(pWarpAdapter.GetAddressOf())));

			ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(m_d3dDevice.GetAddressOf())));
		}
	}

	g_Device = m_d3dDevice.Get();
	g_CommandManager.Create(g_Device);

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = m_backBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));

	m_4xMSAAQuality = msQualityLevels.NumQualityLevels;
	assert(m_4xMSAAQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
	LogAdapters();
#endif

	CreateSwapChain();

	return true;
}

void App::CreateSwapChain()
{
	auto cmdQueue = g_CommandManager.GetCommandQueue();

	DXGI_SWAP_CHAIN_DESC1 sd;
	{
		ZeroMemory(&sd, sizeof(DXGI_SWAP_CHAIN_DESC1));
		sd.BufferCount = c_swapChainBufferCount;
		sd.Width = m_clientWidth;
		sd.Height = m_clientHeight;
		sd.Format = m_backBufferFormat;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SampleDesc.Count = m_4xMSAAState ? 4 : 1;
		sd.SampleDesc.Quality = m_4xMSAAState ? (m_4xMSAAQuality - 1) : 0;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		sd.Scaling = DXGI_SCALING_STRETCH;
		sd.Stereo = FALSE;
	}
	IDXGISwapChain1* swapChain1 = NULL;
	ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(cmdQueue, m_hMainWnd, &sd, NULL, NULL, &swapChain1));
	ThrowIfFailed(swapChain1->QueryInterface(IID_PPV_ARGS(m_swapChain.GetAddressOf())));
	swapChain1->Release();

	// Create Render Target
	{
		for (int i = 0; i < c_swapChainBufferCount; ++i)
		{
			ComPtr<ID3D12Resource> displayPlaneBuffer;
			ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(displayPlaneBuffer.GetAddressOf())));
			m_displayBuffer[i].CreateFromSwapChain(L"Primary SwapChain Buffer", displayPlaneBuffer.Detach());
		}
		m_sceneColorBuffer = std::make_shared<ColorBuffer>();
		m_sceneColorBuffer->Create(L"Scene Color Buffer", m_clientWidth, m_clientHeight, 1, m_sceneBufferFormat);
	}

	m_screenViewport.TopLeftX = 0.0;
	m_screenViewport.TopLeftY = 0.0;
	m_screenViewport.Width = static_cast<float>(m_clientWidth);
	m_screenViewport.Height = static_cast<float>(m_clientHeight);
	m_screenViewport.MinDepth = 0.0f;
	m_screenViewport.MaxDepth = 1.0f;

	m_scissorRect = { 0, 0, m_clientWidth, m_clientHeight };

}

void App::InitImgui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplWin32_Init(m_hMainWnd);

	io.BackendRendererName = "imgui_impl_dx12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional) // FIXME-VIEWPORT: Actually unfinished..

	// Create a dummy ImGuiViewportDataDx12 holder for the main viewport,
	// Since this is created and managed by the application, we will only use the ->Resources[] fields.
	ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	main_viewport->RendererUserData = IM_NEW(ImGuiViewportDataDx12)();

	// Setup backend capabilities flags
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;    // We can create multi-viewports on the Renderer side (optional)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
		platform_io.Renderer_CreateWindow = :: CreateSubWindow;
		platform_io.Renderer_DestroyWindow = ::DestroySubWindow;
		platform_io.Renderer_SetWindowSize = ::SetSubWindowSize;
		platform_io.Renderer_RenderWindow = ::RenderSubWindow;
		platform_io.Renderer_SwapBuffers = ::SwapSubWindowBuffers;
	}

	// Load font
	io.Fonts->AddFontFromFileTTF("FiraCode-Regular.ttf", 16.0f);
	if (io.Fonts == nullptr)
		io.Fonts->AddFontDefault();

}

void App::CreateAppRootSignature()
{
	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.MipLODBias = 0.f;
	staticSampler.MaxAnisotropy = 0;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	staticSampler.MinLOD = 0.f;
	staticSampler.MaxLOD = 0.f;
	staticSampler.ShaderRegister = 0;
	staticSampler.RegisterSpace = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	m_renderUIRS.Reset(3, 1);
	m_renderUIRS[0].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_renderUIRS[1].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_renderUIRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	m_renderUIRS.InitStaticSampler(0, staticSampler);
	m_renderUIRS.Finalize(L"RenderUIRS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_presentRS.Reset(1, 0);
	m_presentRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	m_presentRS.Finalize(L"PresentRS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

void App::CreateAppPipelineState()
{
	static D3D12_INPUT_ELEMENT_DESC local_layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_INPUT_ELEMENT_DESC present_layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	CD3DX12_RASTERIZER_DESC rasterDesc(D3D12_DEFAULT);
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;

	D3D12_DEPTH_STENCIL_DESC depthDesc;
	ZeroMemory(&depthDesc, sizeof(depthDesc));
	depthDesc.DepthEnable = false;
	depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthDesc.StencilEnable = false;
	depthDesc.FrontFace.StencilFailOp = depthDesc.FrontFace.StencilDepthFailOp = depthDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthDesc.BackFace = depthDesc.FrontFace;

	m_renderUIPSO.SetRootSignature(m_renderUIRS);
	m_renderUIPSO.SetSampleMask(UINT_MAX);
	m_renderUIPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_renderUIPSO.SetRenderTargetFormat(m_backBufferFormat, DXGI_FORMAT_UNKNOWN);
	m_renderUIPSO.SetVertexShader(g_pimgui_vert, sizeof(g_pimgui_vert));
	m_renderUIPSO.SetPixelShader(g_pimgui_pixel, sizeof(g_pimgui_pixel));
	m_renderUIPSO.SetInputLayout(_countof(local_layout), local_layout);
	m_renderUIPSO.SetBlendState(blendDesc);
	m_renderUIPSO.SetRasterizerState(rasterDesc);
	m_renderUIPSO.SetDepthStencilState(depthDesc);
	m_renderUIPSO.Finalize();

	m_tiledVolumeTexturePSO = m_renderUIPSO;
	m_tiledVolumeTexturePSO.SetPixelShader(g_pTileTexture3D_PS, sizeof(g_pTileTexture3D_PS));
	m_tiledVolumeTexturePSO.Finalize();

	m_previewVolumeTexturePSO = m_tiledVolumeTexturePSO;
	m_previewVolumeTexturePSO.SetPixelShader(g_pPreviewTexture3D_PS, sizeof(g_pPreviewTexture3D_PS));
	m_previewVolumeTexturePSO.Finalize();

	m_presentLDRPSO = m_renderUIPSO;
	m_presentLDRPSO.SetRootSignature(m_presentRS);
	m_presentLDRPSO.SetVertexShader(g_pDrawQuad_VS, sizeof(g_pDrawQuad_VS));
	m_presentLDRPSO.SetPixelShader(g_pPresentSDR_PS, sizeof(g_pPresentSDR_PS));
	m_presentLDRPSO.SetBlendState(Global::BlendDisable);
	m_presentLDRPSO.SetDepthStencilState(Global::DepthStateDisabled);
	m_presentLDRPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_presentLDRPSO.SetInputLayout(_countof(present_layout), present_layout);
	m_presentLDRPSO.Finalize();
}

void App::CreateFontTexture()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	m_fontColorBuffer = TextureManager::CreateTexture2D(L"FontTexture2D", width, height, DXGI_FORMAT_R8G8B8A8_UNORM, pixels);
	// IMPORTANT: the pcmd->TextureId usage is different with the ImGui demo, so use the D3D12_CPU_DESCRIPTOR_HANDLE to set the io.Fonts->TexID
	io.Fonts->TexID = (ImTextureID)m_fontColorBuffer->GetSRV().ptr;
}

int App::Run()
{
	MSG msg = { 0 };

	m_timer.Reset();

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			m_timer.Tick();
			CalculateFrameStats();

			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			if (m_renderUI)
			{
				UpdateUI();
				UpdateAppUI();
			}

			Update(m_timer);
			Draw(m_timer);
			PostProcess::Render();

			Display();

			SwapBackBuffer();
		}
	}

	return (int)msg.wParam;
}

void App::OnResize()
{
	g_CommandManager.IdleGPU();

	for (int i = 0; i < c_swapChainBufferCount; ++i)
	{
		m_displayBuffer[i].Destroy();
	}
	m_depthStencilBuffer.Reset();

	ThrowIfFailed(m_swapChain->ResizeBuffers(c_swapChainBufferCount, m_clientWidth, m_clientHeight, m_backBufferFormat, 0));

	m_currBackBuffer = 0;

	for (int i = 0; i < c_swapChainBufferCount; ++i)
	{
		ComPtr<ID3D12Resource> displayPlaneBuffer;
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&displayPlaneBuffer)));
		m_displayBuffer[i].CreateFromSwapChain(L"Primary SwapChain Buffer", displayPlaneBuffer.Detach());
	}

	m_screenViewport.TopLeftX = 0.0;
	m_screenViewport.TopLeftY = 0.0;
	m_screenViewport.Width = static_cast<float>(m_clientWidth);
	m_screenViewport.Height = static_cast<float>(m_clientHeight);
	m_screenViewport.MinDepth = 0.0f;
	m_screenViewport.MaxDepth = 1.0f;

	m_scissorRect = { 0, 0, m_clientWidth, m_clientHeight };

}

void App::Update(const Timer& t)
{

}

void App::Draw(const Timer& t)
{
	
}

void App::UpdateUI()
{

}

void App::UpdateAppUI()
{
	m_clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (m_showDemoWindow)
		ImGui::ShowDemoWindow(&m_showDemoWindow);

	// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
	if (m_showHelloWindow)
	{
		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &m_showDemoWindow);      // Edit bools storing our window open/close state
		ImGui::Checkbox("Another Window", &m_showAnotherDemoWindow);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		ImGui::ColorEdit3("clear color", (float*)&m_clearColor); // Edit 3 floats representing a color

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	// 3. Show another simple window.
	if (m_showAnotherDemoWindow)
	{
		ImGui::Begin("Another Window", &m_showAnotherDemoWindow);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			m_showAnotherDemoWindow = false;
		ImGui::End();
	}

	static bool show_hdr_setting = false;
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("HDR"))
		{
			show_hdr_setting = true;
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// HDR setting
	if (show_hdr_setting)
	{
		ImGui::Begin("HDR Setting", &show_hdr_setting);
		ImGui::Checkbox("Enable HDR", &PostProcess::EnableHDR);
		if (PostProcess::EnableHDR)
		{
			ImGui::Checkbox("Enable Adapt Exposure", &PostProcess::EnableAdaptation);
			static float exposure_min = 0.00390625f;
			static float exposure_max = 256.0f;
			if (PostProcess::EnableAdaptation)
			{
				ImGui::DragFloat("Target Luminance", &PostProcess::ExposureCB.targetLuminance, 0.01f, 0.01f, 0.99f);
				ImGui::DragFloat("Adaptation Rate", &PostProcess::ExposureCB.adaptationRate, 0.01f, 0.01f, 1.0f);
				//ImGui::DragFloat("Min Exposure", &PostProcess::ExposureCB.minExposure, 0.25f, 0.00390625f, 1.0f);
				ImGui::DragScalar("Min Exposure", ImGuiDataType_Float, &PostProcess::ExposureCB.minExposure, 0.25f, &exposure_min, &exposure_max, "%f", ImGuiSliderFlags_Logarithmic);
				//ImGui::DragFloat("Max Exposure", &PostProcess::ExposureCB.maxExposure, 0.25f, 1.0f, 256.0f);
				ImGui::DragScalar("Min Exposure", ImGuiDataType_Float, &PostProcess::ExposureCB.maxExposure, 0.25f, &exposure_min, &exposure_max, "%f", ImGuiSliderFlags_Logarithmic);
				ImGui::Checkbox("Draw Histogram", &PostProcess::DrawHistogram);
				if (PostProcess::DrawHistogram)
				{
					ImGui::Image((ImTextureID)(PostProcess::HistogramColorBuffer.GetSRV().ptr), ImVec2((float)PostProcess::HistogramColorBuffer.GetWidth(), (float)PostProcess::HistogramColorBuffer.GetHeight()));
				}
			}
			else
			{
				ImGui::DragScalar("Exposure", ImGuiDataType_Float, &PostProcess::Exposure, 0.25f, &exposure_min, &exposure_max, "%f", ImGuiSliderFlags_Logarithmic);
			}
			
		}
		ImGui::End();
	}
}

void App::Display()
{
	if (ImGui::IsKeyPressedMap(ImGuiKey_U))
		m_renderUI = !m_renderUI;

	GraphicsContext& context = GraphicsContext::Begin(L"Display");
	// Present LDR 
	{
		context.TransitionResource(*m_sceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		context.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		context.ClearColor(m_displayBuffer[m_currBackBuffer]);
		context.SetRootSignature(m_presentRS);
		context.SetPipelineState(m_presentLDRPSO);
		context.SetRenderTarget(m_displayBuffer[m_currBackBuffer].GetRTV());
		context.SetDynamicDescriptor(0, 0, m_sceneColorBuffer->GetSRV());
		context.SetPrimitiveTopology(m_fullScreenQuad->Topology());
		context.SetViewportAndScissor(m_screenViewport, m_scissorRect);
		context.SetVertexBuffer(0, m_fullScreenQuad->VertexBufferView());
		context.SetIndexBuffer(m_fullScreenQuad->IndexBufferView());
		context.DrawIndexed(m_fullScreenQuad->IndexCount(), 0, 0);
	}

	//m_displayBuffer[m_currBackBuffer].SetClearColor(m_clearColor);
	//context.ClearColor(m_displayBuffer[m_currBackBuffer]);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] =
	{
		m_displayBuffer[m_currBackBuffer].GetRTV()
	};
	context.SetRenderTargets(_countof(rtvs), rtvs); // or context.SetRenderTarget(m_displayBuffer[m_currBackBuffer].GetRTV());

	ImGui::Render();
	auto drawData = ImGui::GetDrawData();

	RenderUI(context, drawData);

	context.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_PRESENT);
	context.Finish();

	ImGuiIO& io = ImGui::GetIO();

	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(NULL, NULL);
	}
}

void App::SwapBackBuffer()
{
	//m_swapChain->Present(1, 0); // Present with vsync
	m_swapChain->Present(0, 0); // Present without vsync
	m_currBackBuffer = (m_currBackBuffer + 1) % c_swapChainBufferCount;
	g_CommandManager.IdleGPU();
}

void App::RenderUI(GraphicsContext& context, ImDrawData* drawData)
{
	// Avoid rendering when minimized
	if (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
		return;

	ImGuiViewportDataDx12* render_data = (ImGuiViewportDataDx12*)drawData->OwnerViewport->RendererUserData;

	// Set dynamic vertex buffer and dynamic index buffer
	size_t vertexStride = sizeof(ImDrawVert);
	int vtxCount = drawData->TotalVtxCount == 0 ? 100 : drawData->TotalVtxCount;
	int idxCount = drawData->TotalIdxCount == 0 ? 100 : drawData->TotalIdxCount;
	DynMem vertexBuffer = context.AllocateMemory(vtxCount * vertexStride);
	DynMem indexBuffer = context.AllocateMemory(idxCount * sizeof(ImDrawIdx));

	ImDrawVert* vtx_dst = (ImDrawVert*)vertexBuffer.dataPtr;
	ImDrawIdx* idx_dst = (ImDrawIdx*)indexBuffer.dataPtr;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = drawData->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	context.SetDynamicVB(0, vtxCount, sizeof(ImDrawVert), vertexBuffer.dataPtr);
	context.SetDynamicIB(idxCount, (uint16_t*)indexBuffer.dataPtr);

	// Setup viewport
	D3D12_VIEWPORT vp;
	memset(&vp, 0, sizeof(D3D12_VIEWPORT));
	vp.Width = drawData->DisplaySize.x;
	vp.Height = drawData->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	context.SetViewport(vp);

	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).

	float L = drawData->DisplayPos.x;
	float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
	float T = drawData->DisplayPos.y;
	float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
	float mvp[16] =
	{
		2.0f / (R - L),   0.0f,           0.0f,       0.0f ,
		0.0f,         2.0f / (T - B),     0.0f,       0.0f ,
		0.0f,         0.0f,           0.5f,       0.0f ,
		(R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f ,
	};

	context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.SetRootSignature(m_renderUIRS);
	context.SetConstantArray(0, 16, mvp);
	context.SetBlendFactor({ 0.0f, 0.0f, 0.0f, 0.0f });

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_vtx_offset = 0;
	int global_idx_offset = 0;
	ImVec2 clip_off = drawData->DisplayPos;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = drawData->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
				{
					//SetupRenderState(draw_data, ctx, fr);
					//context.SetPipelineState(m_displayPSO);
					//context.SetRootSignature(m_displayRootSignature);
					//context.SetConstantArray(0, 16, mvp);
				}
				else
				{
					pcmd->UserCallback(context, cmd_list, pcmd);
				}
			}
			else
			{
				// Apply Scissor, Bind texture, Draw
				const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
				if (r.right > r.left && r.bottom > r.top)
				{
					if (pcmd->TextureDisplayState == 0)
					{
						context.SetPipelineState(m_renderUIPSO);
					}
					else if ((pcmd->TextureDisplayState & 1))
					{
						context.SetPipelineState(m_tiledVolumeTexturePSO);
						int dim = (pcmd->TextureDisplayState >> 16);
						context.SetConstantArray(1, 1, &dim);
					}
					else if ((pcmd->TextureDisplayState & 0x10))
					{
						context.SetPipelineState(m_previewVolumeTexturePSO);
						int dim = (pcmd->TextureDisplayState >> 16);
						context.SetConstantArray(1, 1, &dim);
					}
					//ctx->SetGraphicsRootDescriptorTable(1, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId);
					// IMPORTANT: the pcmd->TextureId usage is different with the ImGui demo, use D3D12_CPU_DESCRIPOR_HANDLE to set the ImGui texID (ImGui::Image, font.TexID, ..., etc)
					context.SetDynamicDescriptor(2, 0, *(D3D12_CPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId);
					context.SetScissor(r);
					context.DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
				}
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}

void App::ShutdownWindow()
{
	// Manually delete main viewport render resources in-case we haven't initialized for viewports
	ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	if (ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)main_viewport->RendererUserData)
	{
		for (UINT i = 0; i < c_swapChainBufferCount; i++)
		{
			m_displayBuffer[i].Destroy();
		}
		IM_DELETE(data);
		main_viewport->RendererUserData = NULL;
	}

	// Clean up windows and device objects
	ImGui::DestroyPlatformWindows();
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->TexID = NULL; // We copied g_pFontTextureView to io.Fonts->TexID so let's clear that as well.
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (m_d3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			m_clientHeight = HIWORD(lParam);
			m_clientWidth = LOWORD(lParam);
			OnResize();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

void App::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.f;

	frameCnt++;

	if ((m_timer.TotalTime() - timeElapsed) >= 1.f)
	{
		float fps = (float)frameCnt;
		float mspf = 1000.f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = m_mainWndCaption +
			L"	 fps: " + fpsStr +
			L"	mspf: " + mspfStr;

		SetWindowTextW(m_hMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.f;
	}
}

void App::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (m_dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugStringW(text.c_str());

		adapterList.push_back(adapter);
		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		if (adapterList[i])
			adapterList[i]->Release();
	}
}

void App::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugStringW(text.c_str());

		LogOutputDisplayModes(output, m_backBufferFormat);
		if (output)
			output->Release();
		++i;
	}
}

void App::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		OutputDebugStringW(text.c_str());
	}
}

void App::CreateAppSubWindow(ImGuiViewport* viewport)
{
	ImGuiViewportDataDx12* data = IM_NEW(ImGuiViewportDataDx12)();
	viewport->RendererUserData = data;

	// PlatformHandleRaw should always be a HWND, whereas PlatformHandle might be a higher-level handle (e.g. GLFWWindow*, SDL_Window*).
	// Some backends will leave PlatformHandleRaw NULL, in which case we assume PlatformHandle will contain the HWND.
	HWND hwnd = viewport->PlatformHandleRaw ? (HWND)viewport->PlatformHandleRaw : (HWND)viewport->PlatformHandle;
	IM_ASSERT(hwnd != 0);

	auto cmdQueue = g_CommandManager.GetCommandQueue();

	// Create swap chain
	DXGI_SWAP_CHAIN_DESC1 sd1;
	ZeroMemory(&sd1, sizeof(sd1));
	sd1.BufferCount = c_swapChainBufferCount;
	sd1.Width = (UINT)viewport->Size.x;
	sd1.Height = (UINT)viewport->Size.y;
	sd1.Format = m_backBufferFormat;
	sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd1.SampleDesc.Count = 1;
	sd1.SampleDesc.Quality = 0;
	sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	sd1.Scaling = DXGI_SCALING_STRETCH;
	sd1.Stereo = FALSE;

	IDXGISwapChain1* swap_chain = NULL;
	ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(cmdQueue, hwnd, &sd1, NULL, NULL, &swap_chain));

	// Or swapChain.As(&mSwapChain)
	IM_ASSERT(data->SwapChain == NULL);
	swap_chain->QueryInterface(IID_PPV_ARGS(&data->SwapChain));
	swap_chain->Release();

	// Create render targets
	if (data->SwapChain)
	{

		for (uint32_t i = 0; i < c_swapChainBufferCount; ++i)
		{
			ComPtr<ID3D12Resource> displayPlane;
			data->SwapChain->GetBuffer(i, IID_PPV_ARGS(&displayPlane));
			data->RenderTargetBuffer[i].CreateFromSwapChain(L"Sub-window SwapChain Buffer", displayPlane.Detach());
		}
	}
}

void App::DestroyAppSubWindow(ImGuiViewport* viewport)
{
	g_CommandManager.IdleGPU();
	// The main viewport (owned by the application) will always have RendererUserData == NULL since we didn't create the data for it.
	if (ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData)
	{
		if (data->SwapChain)
			data->SwapChain->Release();
		data->SwapChain = NULL;

		for (UINT i = 0; i < c_swapChainBufferCount; i++)
		{
			data->RenderTargetBuffer[i].Destroy();
		}
		IM_DELETE(data);
	}
	viewport->RendererUserData = NULL;
}

void App::SetAppSubWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	g_CommandManager.IdleGPU();
	for (uint32_t i = 0; i < c_swapChainBufferCount; ++i)
	{
		data->RenderTargetBuffer[i].Destroy();
	}

	if (data->SwapChain)
	{
		data->SwapChain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0);
		for (uint32_t i = 0; i < c_swapChainBufferCount; ++i)
		{
			ComPtr<ID3D12Resource> displayPlane;
			data->SwapChain->GetBuffer(i, IID_PPV_ARGS(&displayPlane));
			data->RenderTargetBuffer[i].CreateFromSwapChain(L"Sub-window SwapChain Buffer", displayPlane.Detach());
		}
	}
}

void App::RenderAppSubWindow(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	//ImGui_FrameContext* frame_context = &data->FrameCtx[data->FrameIndex % c_swapChainBufferCount];
	UINT back_buffer_idx = data->SwapChain->GetCurrentBackBufferIndex();
	auto& current_render_target = data->RenderTargetBuffer[back_buffer_idx];

	const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

	GraphicsContext& context = GraphicsContext::Begin(L"Sub-window Display");

	context.TransitionResource(current_render_target, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
	{
		current_render_target.SetClearColor(clear_color);
		context.ClearColor(current_render_target);
	}
	context.SetRenderTarget(current_render_target.GetRTV());

	auto drawData = viewport->DrawData;
	RenderUI(context, drawData);

	context.TransitionResource(current_render_target, D3D12_RESOURCE_STATE_PRESENT);
	context.Finish();
}

void App::SwapAppSubWindowBuffers(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	data->SwapChain->Present(0, 0);
	g_CommandManager.IdleGPU();
}