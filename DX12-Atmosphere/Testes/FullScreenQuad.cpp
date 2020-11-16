#include "stdafx.h"
#include "FullScreenQuad.h"
#include "CompiledShaders/FullScreenQuad_VS.h"
#include "CompiledShaders/FullScreenQuad_PS.h"

using namespace Microsoft::WRL;

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

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	m_currentFence++;
	m_commandQueue->Signal(m_fence.Get(), m_currentFence);
	if (m_fence->GetCompletedValue() < m_currentFence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

}

void FullScreenQuad::CreateRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		std::cout << (const char*)errorBlob->GetBufferPointer() << std::endl;
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(m_d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.GetAddressOf())
	));
}

void FullScreenQuad::CreatePipelineStates()
{
	m_inputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD0", 0, DXGI_FORMAT_R8_UINT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	float quad_verts[] = 
	{
		-1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
		 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
		-1.0f,-1.0f, 1.0f, 0.0f, 1.0f,
		 1.0f,-1.0f, 1.0f, 1.0f, 1.0f
	};

	size_t vertexBufferSize = sizeof(quad_verts) * sizeof(float);

	uint16_t quad_indices[] = 
	{
		0, 1, 2,
		2, 1, 3
	};

	size_t indexBufferSize = sizeof(quad_indices) * sizeof(uint16_t);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso;
	ZeroMemory(&pso, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	pso.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	pso.pRootSignature = m_rootSignature.Get();
	pso.VS =
	{
		g_pFullScreenQuad_VS,
		sizeof(g_pFullScreenQuad_VS)
	};
	pso.PS =
	{
		g_pFullScreenQuad_PS,
		sizeof(g_pFullScreenQuad_PS)
	};
	pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	CD3DX12_DEPTH_STENCIL_DESC depth_always = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depth_always.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	pso.DepthStencilState = depth_always;
	pso.SampleMask = UINT_MAX;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = m_backBufferFormat;
	pso.SampleDesc.Count = m_4xMSAAState ? 4 : 1;
	pso.SampleDesc.Quality = m_4xMSAAState ? (m_4xMSAAQuality - 1) : 0;
	pso.DSVFormat = m_depthStencilBufferFormat;


	m_vertexBufferGPU = D3DUtils::CreateDefaultBuffer(
		m_d3dDevice.Get(),
		m_commandList.Get(),
		quad_verts,
		vertexBufferSize,
		m_vertexBufferUploader
	);

	m_indexBufferGPU = D3DUtils::CreateDefaultBuffer(
		m_d3dDevice.Get(),
		m_commandList.Get(),
		quad_indices,
		indexBufferSize,
		m_indexBufferUploader
	);
}

void FullScreenQuad::CreateConstantBufferView()
{

}

void FullScreenQuad::Update(const Timer& timer)
{

}

void FullScreenQuad::Draw(const Timer& timer)
{

}

void FullScreenQuad::DrawUI()
{

}