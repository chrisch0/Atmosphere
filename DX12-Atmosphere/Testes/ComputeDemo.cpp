#include "stdafx.h"
#include "ComputeDemo.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"
#include "D3D12/CommandContext.h"
#include "D3D12/PostProcess.h"
#include "Mesh/Mesh.h"
#include "Math/Common.h"

#include "CompiledShaders/NoiseDemo_CS.h"
#include "CompiledShaders/Noise3DDemo_CS.h"
#include "CompiledShaders/DrawQuad_VS.h"
#include "CompiledShaders/ComputeDemo_PS.h"
#include "CompiledShaders/Compute3DDemo_PS.h"

ComputeDemo::ComputeDemo()
{

}

ComputeDemo::~ComputeDemo()
{
	m_noiseGenerator.Destroy();
}

bool ComputeDemo::Initialize()
{
	m_clientHeight = 1024;
	m_clientWidth = 1024;
	if (!App::Initialize())
		return false;

	CreateRootSignature();
	CreatePipelineState();
	CreateResources();

	m_showAnotherDemoWindow = false;
	m_showDemoWindow = false;
	m_showHelloWindow = false;

	m_seed = 1346;
	m_frequency = 0.015f;

	m_isNoiseSettingDirty = true;
	m_isFirst = true;
	m_generate3D = false;

	m_noiseGenerator.Initialize();

	PostProcess::EnableHDR = false;
	return true;
}

void ComputeDemo::CreateRootSignature()
{
	m_computeRS.Reset(2, 0);
	m_computeRS[0].InitAsConstants(2, 0, 0);
	m_computeRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 2);
	m_computeRS.Finalize(L"GenNoiseRS");

	m_graphicsRS.Reset(1, 1);
	m_graphicsRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_graphicsRS.InitStaticSampler(0, Global::SamplerPointClampDesc);
	m_graphicsRS.Finalize(L"GraphicsRS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

void ComputeDemo::CreatePipelineState()
{
	m_computePSO.SetRootSignature(m_computeRS);
	m_computePSO.SetComputeShader(g_pNoiseDemo_CS, sizeof(g_pNoiseDemo_CS));
	m_computePSO.Finalize();

	m_computeNoise3DPSO.SetRootSignature(m_computeRS);
	m_computeNoise3DPSO.SetComputeShader(g_pNoise3DDemo_CS, sizeof(g_pNoise3DDemo_CS));
	m_computeNoise3DPSO.Finalize();

	D3D12_INPUT_ELEMENT_DESC layout[] = 
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	m_graphicsPSO.SetRootSignature(m_graphicsRS);
	m_graphicsPSO.SetSampleMask(UINT_MAX);
	m_graphicsPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_graphicsPSO.SetRenderTargetFormat(m_backBufferFormat, DXGI_FORMAT_UNKNOWN);
	m_graphicsPSO.SetVertexShader(g_pDrawQuad_VS, sizeof(g_pDrawQuad_VS));
	m_graphicsPSO.SetPixelShader(g_pComputeDemo_PS, sizeof(g_pComputeDemo_PS));
	m_graphicsPSO.SetInputLayout(_countof(layout), layout);
	m_graphicsPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	m_graphicsPSO.SetDepthStencilState(Global::DepthStateDisabled);
	m_graphicsPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_graphicsPSO.Finalize();

	m_showNoise3DPSO = m_graphicsPSO;
	m_showNoise3DPSO.SetPixelShader(g_pCompute3DDemo_PS, sizeof(g_pCompute3DDemo_PS));
	m_showNoise3DPSO.Finalize();
}

void ComputeDemo::CreateResources()
{
	m_quad.reset(Mesh::CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.5f));
	m_noise.Create(L"Noise Texture", 1024, 1024, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	m_noise3D.Create(L"Noise3D Texture", 256, 256, 128, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	int32_t minmax[2] = { 0, 0 };
	m_minMax.Create(L"Global Min Max", 2, sizeof(float), minmax);
}

void ComputeDemo::Update(const Timer& timer)
{
}

void ComputeDemo::Draw(const Timer& timer)
{
	GraphicsContext& graphicsContext = GraphicsContext::Begin();

	if (m_isNoiseSettingDirty)
	{
		ComputeContext& context = graphicsContext.GetComputeContext();
		context.SetRootSignature(m_computeRS);
		if (m_generate3D)
		{
			context.SetPipelineState(m_computeNoise3DPSO);
			context.TransitionResource(m_noise3D, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			context.SetConstants(0, m_seed, m_frequency);
			context.SetDynamicDescriptor(1, 0, m_noise3D.GetUAV());
			context.Dispatch3D(m_noise3D.GetWidth(), m_noise3D.GetHeight(), m_noise3D.GetDepth(), 8, 8, 1);
			context.TransitionResource(m_noise3D, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		}
		else
		{
			//context.ClearUAV(m_minMax);
			context.SetPipelineState(m_computePSO);
			context.TransitionResource(m_noise, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			context.TransitionResource(m_minMax, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			context.SetConstants(0, m_seed, m_frequency);
			context.SetDynamicDescriptor(1, 1, m_minMax.GetUAV());
			context.SetDynamicDescriptor(1, 0, m_noise.GetUAV());
			context.Dispatch2D(m_noise.GetWidth(), m_noise.GetHeight());
			context.TransitionResource(m_noise, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		}
	}


	graphicsContext.TransitionResource(m_sceneBuffers[m_currBackBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_sceneBuffers[m_currBackBuffer].SetClearColor(m_clearColor);
	graphicsContext.ClearColor(m_sceneBuffers[m_currBackBuffer]);

	graphicsContext.SetRenderTarget(m_sceneBuffers[m_currBackBuffer].GetRTV());
	graphicsContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	graphicsContext.SetRootSignature(m_graphicsRS);
	if (m_generate3D)
	{
		graphicsContext.SetPipelineState(m_showNoise3DPSO);
		graphicsContext.SetDynamicDescriptor(0, 0, m_noise3D.GetSRV());
	}
	else
	{
		graphicsContext.SetPipelineState(m_graphicsPSO);
		graphicsContext.SetDynamicDescriptor(0, 0, m_noise.GetSRV());

	}
	graphicsContext.SetViewportAndScissor(m_screenViewport, m_scissorRect);
	graphicsContext.SetVertexBuffer(0, m_quad->VertexBufferView());
	graphicsContext.SetIndexBuffer(m_quad->IndexBufferView());
	graphicsContext.DrawIndexed(6, 0, 0);

	graphicsContext.TransitionResource(m_sceneBuffers[m_currBackBuffer], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	graphicsContext.Finish();
}

void ComputeDemo::UpdateUI()
{
	m_noiseGenerator.UpdateUI();

	m_isNoiseSettingDirty = m_isFirst;
	ImGui::Begin("Noise Setting");

	m_isNoiseSettingDirty |= ImGui::Checkbox("Generate 3D Noise", &m_generate3D);
	m_isNoiseSettingDirty |= ImGui::DragInt("Seed", &m_seed, 1.0f);
	m_isNoiseSettingDirty |= ImGui::DragFloat("Frequency", &m_frequency, 0.001f, 0.0, FLT_MAX, "%.3f");

	ImGui::End();
	m_isFirst = false;

	ImGui::Begin("Noise Window");
	ImVec2 window_size = ImGui::GetWindowSize();
	//ImGui::Text("CPU handle = %p", m_noise3D.GetSRV().ptr);
	ImGui::Text("Window Size: %d x % d", (int)window_size.x, (int)window_size.y);
	//ImGui::Text("Cursor Pos: %d x % d", (int)pos.x, (int)pos.y);
	if (m_generate3D)
	{
		static bool new_window = false;
		if (ImGui::VolumeImageButton((ImTextureID)(m_noise3D.GetSRV().ptr), ImVec2(128.0, 128.0), m_noise3D.GetDepth()))
		{
			new_window = !new_window;
		}
		if (new_window)
		{
			ImGui::Begin("New Window");
			window_size = ImGui::GetWindowSize();
			ImVec2 pos = ImGui::GetCursorPos();
			ImGui::TiledVolumeImage((ImTextureID)(m_noise3D.GetSRV().ptr), ImVec2(window_size.x - 2 * pos.x, window_size.y - pos.y - 10), (int)m_noise3D.GetDepth());
			ImGui::End();
		}	
	}
	else
	{
		static bool new_window = false;
		static bool is_open = false;
		if (ImGui::ImageButton((ImTextureID)(m_noise.GetSRV().ptr), ImVec2(128.0, 128.0)))
		{
			new_window = !new_window;
			is_open = true;
		}
		if (new_window)
		{
			ImGui::Begin("New Window");
			ImVec2 pos = ImGui::GetCursorPos();
			if (is_open)
				ImGui::SetWindowSize(ImVec2((float)m_noise.GetWidth() + 2.0f * pos.x, (float)m_noise.GetHeight() + pos.y + 10.0f));
			window_size = ImGui::GetWindowSize();
			ImGui::Image((ImTextureID)(m_noise.GetSRV().ptr), ImVec2(window_size.x - 2 * pos.x, window_size.y - pos.y - 10));
			//ImGui::Image((ImTextureID)(m_noise.GetSRV().ptr), ImVec2(m_noise.GetWidth(), m_noise.GetHeight()));
			ImGui::End();
			is_open = false;
		}
	}
	ImGui::End();
}