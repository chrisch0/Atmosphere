#include "stdafx.h"
#include "App.h"
#include "MultiViewport.h"
#include "imgui/imgui_impl_win32.h"
#include "CompiledShaders/imgui_vert.h"
#include "CompiledShaders/imgui_pixel.h"

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
	if (m_d3dDevice != nullptr)
		WaitForLastSubmittedFrame();
	ShutdownWindow();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
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

	CreateAppRootSignature();
	CreateAppPipelineState();
	CreateFontTexture();

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

	// Create fence
	ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf())));

	m_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_fenceEvent == NULL)
		return false;

	m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_dsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_cbvSrvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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

	CreateCommandObjects();
	CreateSrvRtvAndDsvDescriptorHeaps();
	CreateSwapChain();

	return true;
}

void App::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 1;
	ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.GetAddressOf())));

	InitFrameContext();

	ThrowIfFailed(m_d3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_frameContexts[0]->GetCmdAllocator(),
		nullptr,
		IID_PPV_ARGS(m_commandList.GetAddressOf())));

	m_commandList->Close();
}

void App::CreateSrvRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = c_swapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 1;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < c_swapChainBufferCount; ++i)
	{
		m_swapChainRTVDespcriptorHandle[i] = rtvHandle;
		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.GetAddressOf())));
}

void App::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC1 sd;
	{
		ZeroMemory(&sd, sizeof(DXGI_SWAP_CHAIN_DESC1));
		sd.BufferCount = c_swapChainBufferCount;
		sd.Width = m_clientWidth;
		sd.Height = m_clientHeight;
		sd.Format = m_backBufferFormat;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SampleDesc.Count = m_4xMSAAState ? 4 : 1;
		sd.SampleDesc.Quality = m_4xMSAAState ? (m_4xMSAAQuality - 1) : 0;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		sd.Scaling = DXGI_SCALING_STRETCH;
		sd.Stereo = FALSE;
	}
	IDXGISwapChain1* swapChain1 = NULL;
	ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hMainWnd, &sd, NULL, NULL, &swapChain1));
	ThrowIfFailed(swapChain1->QueryInterface(IID_PPV_ARGS(m_swapChain.GetAddressOf())));
	swapChain1->Release();
	m_swapChain->SetMaximumFrameLatency(c_swapChainBufferCount);
	m_swapChainWaitableOject = m_swapChain->GetFrameLatencyWaitableObject();

	// Create Render Target
	{
		for (int i = 0; i < c_swapChainBufferCount; ++i)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(m_swapChainBuffer[i].GetAddressOf())));
			m_d3dDevice->CreateRenderTargetView(m_swapChainBuffer[i].Get(), nullptr, m_swapChainRTVDespcriptorHandle[i]);
		}
	}

}

void App::InitFrameContext()
{
	for (int i = 0; i < m_numFrameContexts; ++i)
	{
		m_frameContexts.push_back(std::make_unique<FrameContext>(m_d3dDevice.Get()));
	}
	m_currFrameContext = m_frameContexts[m_currFrameContextIndex].get();
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
	// Create the root signature
	{
		D3D12_DESCRIPTOR_RANGE descRange = {};
		descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descRange.NumDescriptors = 1;
		descRange.BaseShaderRegister = 0;
		descRange.RegisterSpace = 0;
		descRange.OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER param[2] = {};

		param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		param[0].Constants.ShaderRegister = 0;
		param[0].Constants.RegisterSpace = 0;
		param[0].Constants.Num32BitValues = 16;
		param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param[1].DescriptorTable.NumDescriptorRanges = 1;
		param[1].DescriptorTable.pDescriptorRanges = &descRange;
		param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = _countof(param);
		desc.pParameters = param;
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = &staticSampler;
		desc.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		ID3DBlob* blob = NULL;
		ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL));

		ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(m_appRootSignature.ReleaseAndGetAddressOf())));
		blob->Release();
	}
}

void App::CreateAppPipelineState()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	memset(&psoDesc, 0, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.NodeMask = 1;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.pRootSignature = m_appRootSignature.Get();
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_backBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// Create the vertex shader
	{
		psoDesc.VS = { g_pimgui_vert, sizeof(g_pimgui_vert) };

		// Create the input layout
		static D3D12_INPUT_ELEMENT_DESC local_layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		psoDesc.InputLayout = { local_layout, 3 };
	}

	// Create the pixel shader
	{
		psoDesc.PS = { g_pimgui_pixel, sizeof(g_pimgui_pixel) };
	}

	// Create the blending setup
	{
		D3D12_BLEND_DESC& desc = psoDesc.BlendState;
		desc.AlphaToCoverageEnable = false;
		desc.RenderTarget[0].BlendEnable = true;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	// Create the rasterizer state
	{
		D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.DepthClipEnable = true;
		desc.MultisampleEnable = FALSE;
		desc.AntialiasedLineEnable = FALSE;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}

	// Create depth-stencil State
	{
		D3D12_DEPTH_STENCIL_DESC& desc = psoDesc.DepthStencilState;
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.StencilEnable = false;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.BackFace = desc.FrontFace;
	}

	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_appPipelineState.GetAddressOf())));
}

void App::CreateFontTexture()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Upload texture to graphics system
	{
		D3D12_HEAP_PROPERTIES props;
		memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
		props.Type = D3D12_HEAP_TYPE_DEFAULT;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = 0;
		desc.Width = width;
		desc.Height = height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* pTexture = NULL;
		m_d3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture));

		UINT uploadPitch = (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
		UINT uploadSize = height * uploadPitch;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = uploadSize;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		ID3D12Resource* uploadBuffer = NULL;
		HRESULT hr = m_d3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));
		IM_ASSERT(SUCCEEDED(hr));

		void* mapped = NULL;
		D3D12_RANGE range = { 0, uploadSize };
		hr = uploadBuffer->Map(0, &range, &mapped);
		IM_ASSERT(SUCCEEDED(hr));
		for (int y = 0; y < height; y++)
			memcpy((void*)((uintptr_t)mapped + y * uploadPitch), pixels + y * width * 4, width * 4);
		uploadBuffer->Unmap(0, &range);

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = uploadBuffer;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srcLocation.PlacedFootprint.Footprint.Width = width;
		srcLocation.PlacedFootprint.Footprint.Height = height;
		srcLocation.PlacedFootprint.Footprint.Depth = 1;
		srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = pTexture;
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = 0;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pTexture;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		ID3D12Fence* fence = NULL;
		hr = m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		IM_ASSERT(SUCCEEDED(hr));

		HANDLE event = CreateEvent(0, 0, 0, 0);
		IM_ASSERT(event != NULL);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;

		ID3D12CommandQueue* cmdQueue = NULL;
		hr = m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12CommandAllocator* cmdAlloc = NULL;
		hr = m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12GraphicsCommandList* cmdList = NULL;
		hr = m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
		IM_ASSERT(SUCCEEDED(hr));

		cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
		cmdList->ResourceBarrier(1, &barrier);

		hr = cmdList->Close();
		IM_ASSERT(SUCCEEDED(hr));

		cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmdList);
		hr = cmdQueue->Signal(fence, 1);
		IM_ASSERT(SUCCEEDED(hr));

		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);

		cmdList->Release();
		cmdAlloc->Release();
		cmdQueue->Release();
		CloseHandle(event);
		fence->Release();
		uploadBuffer->Release();

		// Create texture view
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		m_fontSrvCpuDescHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
		m_fontSrvGpuDescHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
		m_d3dDevice->CreateShaderResourceView(pTexture, &srvDesc, m_fontSrvCpuDescHandle);
		m_fontResource.Reset();
		m_fontResource = pTexture;
	}

	// Store our identifier
	static_assert(sizeof(ImTextureID) >= sizeof(m_fontSrvGpuDescHandle.ptr), "Can't pack descriptor handle into TexID, 32-bit not supported yet.");
	io.Fonts->TexID = (ImTextureID)m_fontSrvGpuDescHandle.ptr;
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

			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			CalculateFrameStats();
			Update(m_timer);
			Draw(m_timer);

			DrawUI();
			DrawImGuiDemo();
			RenderImGui();
		}
	}

	//ImGui_ImplWin32_Shutdown();
	//ImGui::DestroyContext();

	return (int)msg.wParam;
}

void App::OnResize()
{
	WaitForLastSubmittedFrame();

	//ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

	for (int i = 0; i < c_swapChainBufferCount; ++i)
	{
		m_swapChainBuffer[i].Reset();
	}
	m_depthStencilBuffer.Reset();

	DXGI_SWAP_CHAIN_DESC1 sd;
	m_swapChain->GetDesc1(&sd);
	sd.Width = m_clientWidth;
	sd.Height = m_clientHeight;

	m_swapChain->Release();
	CloseHandle(m_swapChainWaitableOject);

	IDXGISwapChain1* swapChain1 = NULL;
	m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hMainWnd, &sd, NULL, NULL, &swapChain1);
	swapChain1->QueryInterface(IID_PPV_ARGS(m_swapChain.GetAddressOf()));
	swapChain1->Release();
	
	m_swapChain->SetMaximumFrameLatency(c_swapChainBufferCount);

	m_swapChainWaitableOject = m_swapChain->GetFrameLatencyWaitableObject();
	assert(m_swapChainWaitableOject != NULL);

	m_currBackBuffer = 0;

	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < c_swapChainBufferCount; ++i)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffer[i])));
		m_d3dDevice->CreateRenderTargetView(m_swapChainBuffer[i].Get(), nullptr, m_swapChainRTVDespcriptorHandle[i]);
		//rtvHeapHandle.Offset(1, m_rtvDescriptorSize);
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

void App::DrawUI()
{

}

void App::DrawImGuiDemo()
{
	m_clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (m_showDemoWindow)
		ImGui::ShowDemoWindow(&m_showDemoWindow);

	// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
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
}

void App::RenderImGui()
{
	// Rendering
	WaitForNextFrameResource();
	UINT backBufferIdx = m_swapChain->GetCurrentBackBufferIndex();
	m_currFrameContext->GetCmdAllocator()->Reset();

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_swapChainBuffer[backBufferIdx].Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	m_commandList->Reset(m_currFrameContext->GetCmdAllocator(), NULL);
	m_commandList->ResourceBarrier(1, &barrier);
	m_commandList->ClearRenderTargetView(m_swapChainRTVDespcriptorHandle[backBufferIdx], (float*)&m_clearColor, 0, NULL);
	m_commandList->OMSetRenderTargets(1, &m_swapChainRTVDespcriptorHandle[backBufferIdx], FALSE, NULL);
	m_commandList->SetDescriptorHeaps(1, m_srvHeap.GetAddressOf());
	ImGui::Render();
	RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	m_commandList->ResourceBarrier(1, &barrier);
	m_commandList->Close();

	ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ImGuiIO& io = ImGui::GetIO();
	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(NULL, (void*)m_commandList.Get());
	}

	m_swapChain->Present(1, 0); // Present with vsync
	//g_pSwapChain->Present(0, 0); // Present without vsync

	UINT64 fenceValue = m_currentFence + 1;
	m_commandQueue->Signal(m_fence.Get(), fenceValue);
	m_currentFence = fenceValue;
	m_currFrameContext->SetFenceValue(fenceValue);
}

void App::RenderDrawData(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx)
{
	// Avoid rendering when minimized
	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
		return;

	ImGuiViewportDataDx12* render_data = (ImGuiViewportDataDx12*)draw_data->OwnerViewport->RendererUserData;
	render_data->FrameIndex++;
	ImGui_RenderBuffers* fr = &render_data->FrameRenderBuffers[render_data->FrameIndex % c_swapChainBufferCount];

	// Create and grow vertex/index buffers if needed
	if (fr->VertexBuffer == NULL || fr->VertexBufferSize < draw_data->TotalVtxCount)
	{
		SafeRelease(fr->VertexBuffer);
		fr->VertexBufferSize = draw_data->TotalVtxCount + 5000;
		D3D12_HEAP_PROPERTIES props;
		memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(D3D12_RESOURCE_DESC));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = fr->VertexBufferSize * sizeof(ImDrawVert);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (m_d3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&fr->VertexBuffer)) < 0)
			return;
	}
	if (fr->IndexBuffer == NULL || fr->IndexBufferSize < draw_data->TotalIdxCount)
	{
		SafeRelease(fr->IndexBuffer);
		fr->IndexBufferSize = draw_data->TotalIdxCount + 10000;
		D3D12_HEAP_PROPERTIES props;
		memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(D3D12_RESOURCE_DESC));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = fr->IndexBufferSize * sizeof(ImDrawIdx);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (m_d3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&fr->IndexBuffer)) < 0)
			return;
	}

	// Upload vertex/index data into a single contiguous GPU buffer
	void* vtx_resource, *idx_resource;
	D3D12_RANGE range;
	memset(&range, 0, sizeof(D3D12_RANGE));
	if (fr->VertexBuffer->Map(0, &range, &vtx_resource) != S_OK)
		return;
	if (fr->IndexBuffer->Map(0, &range, &idx_resource) != S_OK)
		return;
	ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
	ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	fr->VertexBuffer->Unmap(0, &range);
	fr->IndexBuffer->Unmap(0, &range);

	// Setup desired DX state
	SetupRenderState(draw_data, ctx, fr);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_vtx_offset = 0;
	int global_idx_offset = 0;
	ImVec2 clip_off = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					SetupRenderState(draw_data, ctx, fr);
				else
					pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Apply Scissor, Bind texture, Draw
				const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
				if (r.right > r.left && r.bottom > r.top)
				{
					ctx->SetGraphicsRootDescriptorTable(1, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId);
					ctx->RSSetScissorRects(1, &r);
					ctx->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
				}
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}

struct VERTEX_CONSTANT_BUFFER
{
	float   mvp[4][4];
};

void App::SetupRenderState(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx, ImGui_RenderBuffers* fr)
{
	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
	VERTEX_CONSTANT_BUFFER vertex_constant_buffer;

	//DirectX::XMFLOAT4X4 vertex_constant_buffer;
	{
		
		float L = draw_data->DisplayPos.x;
		float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T = draw_data->DisplayPos.y;
		float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		float mvp[4][4] =
		{
			{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
			{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
			{ 0.0f,         0.0f,           0.5f,       0.0f },
			{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
		};
		memcpy(&vertex_constant_buffer.mvp, mvp, sizeof(mvp));
	}

	// Setup viewport
	D3D12_VIEWPORT vp;
	memset(&vp, 0, sizeof(D3D12_VIEWPORT));
	vp.Width = draw_data->DisplaySize.x;
	vp.Height = draw_data->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	ctx->RSSetViewports(1, &vp);

	// Bind shader and vertex buffers
	unsigned int stride = sizeof(ImDrawVert);
	unsigned int offset = 0;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	memset(&vbv, 0, sizeof(D3D12_VERTEX_BUFFER_VIEW));
	vbv.BufferLocation = fr->VertexBuffer->GetGPUVirtualAddress() + offset;
	vbv.SizeInBytes = fr->VertexBufferSize * stride;
	vbv.StrideInBytes = stride;
	ctx->IASetVertexBuffers(0, 1, &vbv);
	D3D12_INDEX_BUFFER_VIEW ibv;
	memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
	ibv.BufferLocation = fr->IndexBuffer->GetGPUVirtualAddress();
	ibv.SizeInBytes = fr->IndexBufferSize * sizeof(ImDrawIdx);
	ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	ctx->IASetIndexBuffer(&ibv);
	ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->SetPipelineState(m_appPipelineState.Get());
	ctx->SetGraphicsRootSignature(m_appRootSignature.Get());
	ctx->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);

	// Setup blend factor
	const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendFactor(blend_factor);
}

void App::WaitForLastSubmittedFrame()
{
	FrameContext* frameContext = m_frameContexts[m_currFrameContextIndex].get();
	
	UINT64 fenceValue = frameContext->GetFenceValue();
	//if (frameContext->GetFenceValue() == 0)
		//return;

	//frameContext->SetFenceValue(0);

	if (m_fence->GetCompletedValue() < fenceValue)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}

void App::WaitForNextFrameResource()
{
	m_frameIndex++;
	m_currFrameContextIndex = (m_currFrameContextIndex + 1) % m_numFrameContexts;

	HANDLE waitableObjects[] = { m_swapChainWaitableOject, NULL };
	DWORD numWaitableObjects = 1;

	m_currFrameContext = m_frameContexts[m_currFrameContextIndex].get();
	UINT64 fenceValue = m_currFrameContext->GetFenceValue();
	//if (fenceValue != 0)
	{
		//m_currFrameContext->SetFenceValue(0);
		m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
		waitableObjects[1] = m_fenceEvent;
		numWaitableObjects = 2;
	}

	WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);
}

void App::ShutdownWindow()
{
	// Manually delete main viewport render resources in-case we haven't initialized for viewports
	ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	if (ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)main_viewport->RendererUserData)
	{
		// We could just call ImGui_ImplDX12_DestroyWindow(main_viewport) as a convenience but that would be misleading since we only use data->Resources[]
		for (UINT i = 0; i < c_swapChainBufferCount; i++)
		{
			SafeRelease(data->FrameRenderBuffers[i].IndexBuffer);
			SafeRelease(data->FrameRenderBuffers[i].VertexBuffer);
			data->FrameRenderBuffers[i].IndexBufferSize = data->FrameRenderBuffers[i].VertexBufferSize = 0;
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

	data->FrameIndex = UINT_MAX;

	// Create command queue.
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&data->CommandQueue)));

	for (int i = 0; i < c_swapChainBufferCount; ++i)
	{
		ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&data->FrameCtx[i].CommandAllocator)));
	}

	// Create command list
	ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, data->FrameCtx[0].CommandAllocator, NULL, IID_PPV_ARGS(&data->CommandList)));
	data->CommandList->Close();

	// Create Fence
	ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&data->Fence)));
	data->FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(data->FenceEvent != NULL);

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
	ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(data->CommandQueue, hwnd, &sd1, NULL, NULL, &swap_chain));

	// Or swapChain.As(&mSwapChain)
	IM_ASSERT(data->SwapChain == NULL);
	swap_chain->QueryInterface(IID_PPV_ARGS(&data->SwapChain));
	swap_chain->Release();

	// Create render targets
	if (data->SwapChain)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = c_swapChainBufferCount;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 1;

		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&data->RtvDescHeap)));

		SIZE_T rtv_descriptor_size = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = data->RtvDescHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT i = 0; i < c_swapChainBufferCount; i++)
		{
			data->FrameCtx[i].RenderTargetCpuDescriptors = rtv_handle;
			rtv_handle.ptr += rtv_descriptor_size;
		}

		ID3D12Resource* back_buffer;
		for (UINT i = 0; i < c_swapChainBufferCount; i++)
		{
			IM_ASSERT(data->FrameCtx[i].RenderTarget == NULL);
			data->SwapChain->GetBuffer(i, IID_PPV_ARGS(&back_buffer));
			m_d3dDevice->CreateRenderTargetView(back_buffer, NULL, data->FrameCtx[i].RenderTargetCpuDescriptors);
			data->FrameCtx[i].RenderTarget = back_buffer;
		}
	}

	for (int i = 0; i < c_swapChainBufferCount; ++i)
	{
		SafeRelease(data->FrameRenderBuffers[i].IndexBuffer);
		SafeRelease(data->FrameRenderBuffers[i].VertexBuffer);
		data->FrameRenderBuffers[i].IndexBufferSize = data->FrameRenderBuffers[i].VertexBufferSize = 0;
	}
}

void App::DestroyAppSubWindow(ImGuiViewport* viewport)
{
	// The main viewport (owned by the application) will always have RendererUserData == NULL since we didn't create the data for it.
	if (ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData)
	{
		FlushSubWindowCommandQueue(data);

		SafeRelease(data->CommandQueue);
		SafeRelease(data->CommandList);
		SafeRelease(data->SwapChain);
		SafeRelease(data->RtvDescHeap);
		SafeRelease(data->Fence);
		::CloseHandle(data->FenceEvent);
		data->FenceEvent = NULL;

		for (UINT i = 0; i < c_swapChainBufferCount; i++)
		{
			SafeRelease(data->FrameCtx[i].RenderTarget);
			SafeRelease(data->FrameCtx[i].CommandAllocator);
			SafeRelease(data->FrameRenderBuffers[i].IndexBuffer);
			SafeRelease(data->FrameRenderBuffers[i].VertexBuffer);
			data->FrameRenderBuffers[i].IndexBufferSize = data->FrameRenderBuffers[i].VertexBufferSize = 0;
		}
		IM_DELETE(data);
	}
	viewport->RendererUserData = NULL;
}

void App::FlushSubWindowCommandQueue(ImGuiViewportDataDx12* data)
{
	if (data && data->CommandQueue && data->Fence && data->FenceEvent)
	{
		ThrowIfFailed(data->CommandQueue->Signal(data->Fence, ++data->FenceSignaledValue));
		if (data->Fence->GetCompletedValue() < data->FenceSignaledValue)
		{
			::WaitForSingleObject(data->FenceEvent, 0); // Reset any forgotten waits
			ThrowIfFailed(data->Fence->SetEventOnCompletion(data->FenceSignaledValue, data->FenceEvent));
			::WaitForSingleObject(data->FenceEvent, INFINITE);
		}
	}
}

void App::SetAppSubWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	FlushSubWindowCommandQueue(data);

	for (int i = 0; i < c_swapChainBufferCount; ++i)
	{
		SafeRelease(data->FrameCtx[i].RenderTarget);
	}

	if (data->SwapChain)
	{
		ID3D12Resource* back_buffer = NULL;
		data->SwapChain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0);
		for (UINT i = 0; i < c_swapChainBufferCount; i++)
		{
			data->SwapChain->GetBuffer(i, IID_PPV_ARGS(&back_buffer));
			m_d3dDevice->CreateRenderTargetView(back_buffer, NULL, data->FrameCtx[i].RenderTargetCpuDescriptors);
			data->FrameCtx[i].RenderTarget = back_buffer;
		}
	}
}

void App::RenderAppSubWindow(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	ImGui_FrameContext* frame_context = &data->FrameCtx[data->FrameIndex % c_swapChainBufferCount];
	UINT back_buffer_idx = data->SwapChain->GetCurrentBackBufferIndex();

	const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = data->FrameCtx[back_buffer_idx].RenderTarget;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	// Draw
	ID3D12GraphicsCommandList* cmd_list = data->CommandList;

	frame_context->CommandAllocator->Reset();
	cmd_list->Reset(frame_context->CommandAllocator, NULL);
	cmd_list->ResourceBarrier(1, &barrier);
	cmd_list->OMSetRenderTargets(1, &data->FrameCtx[back_buffer_idx].RenderTargetCpuDescriptors, FALSE, NULL);
	if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
		cmd_list->ClearRenderTargetView(data->FrameCtx[back_buffer_idx].RenderTargetCpuDescriptors, (float*)&clear_color, 0, NULL);
	cmd_list->SetDescriptorHeaps(1, m_srvHeap.GetAddressOf());

	RenderDrawData(viewport->DrawData, cmd_list);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	cmd_list->ResourceBarrier(1, &barrier);
	cmd_list->Close();

	data->CommandQueue->Wait(data->Fence, data->FenceSignaledValue);
	data->CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmd_list);
	data->CommandQueue->Signal(data->Fence, ++data->FenceSignaledValue);
}

void App::SwapAppSubWindowBuffers(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	data->SwapChain->Present(0, 0);
	while (data->Fence->GetCompletedValue() < data->FenceSignaledValue)
		::SwitchToThread();
}