#include "stdafx.h"
#include "FullScreenQuad.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"
#include "D3D12/CommandContext.h"
#include "CompiledShaders/FullScreenQuad_VS.h"
#include "CompiledShaders/FullScreenQuad_PS.h"

using namespace Microsoft::WRL;
using namespace DirectX;

struct ConstantBuffer
{
	XMFLOAT3 color;
	uint32_t use_uv;
};

FullScreenQuad::FullScreenQuad()
{

}

FullScreenQuad::~FullScreenQuad()
{

}

bool FullScreenQuad::Initialize()
{
	if (!App::Initialize())
		return false;

	CreateRootSignature();
	CreatePipelineStates();
	CreateConstantBufferView();

	return true;
}

void FullScreenQuad::CreateRootSignature()
{
	m_rootSignature.Reset(1, 0);
	m_rootSignature[0].InitAsConstantBufferView(0);
	m_rootSignature.Finalize(L"FullScreenQuadRS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

void FullScreenQuad::CreatePipelineStates()
{
	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	float quad_verts[] = 
	{
		-1.0f, 1.0f, 0.1f, 0.0f, 0.0f,
		 1.0f, 1.0f, 0.1f, 1.0f, 0.0f,
		-1.0f,-1.0f, 0.1f, 0.0f, 1.0f,
		 1.0f,-1.0f, 0.1f, 1.0f, 1.0f
	};

	m_vertexBuffer.Create(L"QuadVertexBuffer", 4, sizeof(float) * 5, quad_verts);

	uint16_t quad_indices[] = 
	{
		0, 1, 2,
		2, 1, 3
	};

	m_indexBuffer.Create(L"QuadIndexBuffer", 6, sizeof(uint16_t), quad_indices);

	auto depth_stencil_disable = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depth_stencil_disable.DepthEnable = FALSE;
	depth_stencil_disable.StencilEnable = FALSE;

	m_pipelineState.SetRootSignature(m_rootSignature);
	m_pipelineState.SetSampleMask(UINT_MAX);
	m_pipelineState.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_pipelineState.SetRenderTargetFormat(m_backBufferFormat, DXGI_FORMAT_UNKNOWN);
	m_pipelineState.SetVertexShader(g_pFullScreenQuad_VS, sizeof(g_pFullScreenQuad_VS));
	m_pipelineState.SetPixelShader(g_pFullScreenQuad_PS, sizeof(g_pFullScreenQuad_PS));
	m_pipelineState.SetInputLayout(_countof(layout), layout);
	m_pipelineState.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	m_pipelineState.SetDepthStencilState(depth_stencil_disable);
	m_pipelineState.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_pipelineState.Finalize();
}

void FullScreenQuad::CreateConstantBufferView()
{
}

void FullScreenQuad::Update(const Timer& timer)
{
	
}

void FullScreenQuad::Draw(const Timer& timer)
{
	GraphicsContext& context = GraphicsContext::Begin(L"FullScreenQuad");

	ConstantBuffer cb;
	cb.color = m_backgroundColor;
	cb.use_uv = (uint32_t)m_useUVAsColor;

	// Clear back buffer
	context.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_displayBuffer[m_currBackBuffer].SetClearColor(m_clearColor);
	context.ClearColor(m_displayBuffer[m_currBackBuffer]);

	context.SetRenderTarget(m_displayBuffer[m_currBackBuffer].GetRTV());
	context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.SetPipelineState(m_pipelineState);
	context.SetRootSignature(m_rootSignature);
	context.SetDynamicConstantBufferView(0, sizeof(cb), &cb);
	context.SetViewportAndScissor(m_screenViewport, m_scissorRect);
	context.SetVertexBuffer(0, m_vertexBuffer.VertexBufferView());
	context.SetIndexBuffer(m_indexBuffer.IndexBufferView());
	context.DrawIndexed(6, 0, 0);

	context.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_COMMON);
	context.Finish();
}

void FullScreenQuad::UpdateUI()
{
	ImGui::Begin("Color Setting");

	ImGui::Checkbox("Use uv as background color", &m_useUVAsColor);
	
	float tempColor[3] = { m_backgroundColor.x, m_backgroundColor.y, m_backgroundColor.z };
	ImGui::ColorEdit3("Background Color", tempColor);
	m_backgroundColor = XMFLOAT3(tempColor);

	ImGui::End();
}