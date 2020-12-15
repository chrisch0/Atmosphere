#include "stdafx.h"
#include "ComputeDemo.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"
#include "D3D12/CommandContext.h"
#include "D3D12/GraphicsGlobal.h"
#include "Mesh/Mesh.h"

#include "CompiledShaders/NoiseDemo_CS.h"
#include "CompiledShaders/DrawQuad_VSd.h"
#include "CompiledShaders/ComputeDemo_PSd.h"

ComputeDemo::ComputeDemo()
{

}

ComputeDemo::~ComputeDemo()
{

}

bool ComputeDemo::Initialize()
{
	m_clientHeight = 1024;
	m_clientWidth = 1024;
	if (!App::Initialize())
		return false;

	Global::InitializeGlobalStates();

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
	m_graphicsPSO.SetVertexShader(g_pDrawQuad_VSd, sizeof(g_pDrawQuad_VSd));
	m_graphicsPSO.SetPixelShader(g_pComputeDemo_PSd, sizeof(g_pComputeDemo_PSd));
	m_graphicsPSO.SetInputLayout(_countof(layout), layout);
	m_graphicsPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	m_graphicsPSO.SetDepthStencilState(Global::DepthStateDisabled);
	m_graphicsPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_graphicsPSO.Finalize();
}

void ComputeDemo::CreateResources()
{
	m_quad.reset(Mesh::CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.5f));
	m_noise.Create(L"Noise Texture", 1024, 1024, 1, DXGI_FORMAT_R11G11B10_FLOAT);
	uint32_t minmax[2] = { (uint32_t)2.0f, (uint32_t)-2.0f };
	m_minMax.Create(L"Global Min Max", 2, sizeof(float), minmax);
}

void ComputeDemo::Update(const Timer& timer)
{

}

void ComputeDemo::Draw(const Timer& timer)
{
	GraphicsContext& graphicsContext = GraphicsContext::Begin();

	//if (m_isNoiseSettingDirty)
	{
		ComputeContext& context = graphicsContext.GetComputeContext();
		context.SetRootSignature(m_computeRS);
		context.SetPipelineState(m_computePSO);
		context.TransitionResource(m_noise, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.TransitionResource(m_minMax, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.SetConstants(0, m_seed, m_frequency);
		context.SetDynamicDescriptor(1, 0, m_noise.GetUAV());
		context.SetDynamicDescriptor(1, 1, m_minMax.GetUAV());
		context.Dispatch2D(m_noise.GetWidth(), m_noise.GetHeight());

		context.TransitionResource(m_noise, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	}

	graphicsContext.TransitionResource(m_noise, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	graphicsContext.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_displayBuffer[m_currBackBuffer].SetClearColor(m_clearColor);
	graphicsContext.ClearColor(m_displayBuffer[m_currBackBuffer]);

	graphicsContext.SetRenderTarget(m_displayBuffer[m_currBackBuffer].GetRTV());
	graphicsContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	graphicsContext.SetPipelineState(m_graphicsPSO);
	graphicsContext.SetRootSignature(m_graphicsRS);
	graphicsContext.SetDynamicDescriptor(0, 0, m_noise.GetSRV());
	graphicsContext.SetViewportAndScissor(m_screenViewport, m_scissorRect);
	graphicsContext.SetVertexBuffer(0, m_quad->VertexBufferView());
	graphicsContext.SetIndexBuffer(m_quad->IndexBufferView());
	graphicsContext.DrawIndexed(6, 0, 0);

	graphicsContext.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_COMMON);
	graphicsContext.Finish();
}

void ComputeDemo::UpdateUI()
{
	m_isNoiseSettingDirty = m_isFirst;
	ImGui::Begin("Noise Setting");

	m_isNoiseSettingDirty |= ImGui::DragInt("Seed", &m_seed, 1.0f);
	m_isNoiseSettingDirty |= ImGui::DragFloat("Frequency", &m_frequency, 0.005f, 0.0, FLT_MAX, "%.3f");

	ImGui::End();
	m_isFirst = false;
}