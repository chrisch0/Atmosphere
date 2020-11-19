#include "stdafx.h"
#include "FullScreenQuad.h"
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
	for (int i = 0; i < m_numFrameContexts; ++i)
	{
		m_constantUploadBuffers[i]->Release();
	}
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

	return true;
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
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
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
	CD3DX12_DEPTH_STENCIL_DESC depth_stencil_disable = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depth_stencil_disable.DepthEnable = false;
	depth_stencil_disable.StencilEnable = false;
	pso.DepthStencilState = depth_stencil_disable;
	pso.SampleMask = UINT_MAX;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = m_backBufferFormat;
	pso.SampleDesc.Count = m_4xMSAAState ? 4 : 1;
	pso.SampleDesc.Quality = m_4xMSAAState ? (m_4xMSAAQuality - 1) : 0;
	pso.DSVFormat = m_depthStencilBufferFormat;

	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(m_pso.GetAddressOf())));

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

	m_vertexBufferView.BufferLocation = m_vertexBufferGPU->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(float) * 5;
	m_vertexBufferView.SizeInBytes = sizeof(quad_verts);

	m_indexBufferView.BufferLocation = m_indexBufferGPU->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_indexBufferView.SizeInBytes = sizeof(quad_indices);
}

void FullScreenQuad::CreateConstantBufferView()
{
	UINT cbByteSize = sizeof(ConstantBuffer);

	for (int frameIndex = 0; frameIndex < m_numFrameContexts; ++frameIndex)
	{
		m_constantUploadBuffers[frameIndex] = NULL;

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(256),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantUploadBuffers[frameIndex])
		));

		ThrowIfFailed(m_constantUploadBuffers[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&m_constantMappedData[frameIndex])));

		D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddress = m_constantUploadBuffers[frameIndex]->GetGPUVirtualAddress();

		int heapIndex = 1 + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbGpuAddress;
		cbvDesc.SizeInBytes = 256;

		m_d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void FullScreenQuad::Update(const Timer& timer)
{
	ConstantBuffer cb;
	cb.color = m_backgroundColor;
	cb.use_uv = (uint32_t)m_useUVAsColor;

	memcpy(&m_constantMappedData[m_currFrameContextIndex][0], &cb, sizeof(ConstantBuffer));
}

void FullScreenQuad::Draw(const Timer& timer)
{
	// Rendering
	{
		WaitForNextFrameResource();
		m_currFrameContext->GetCmdAllocator()->Reset();

		m_commandList->Reset(m_currFrameContext->GetCmdAllocator(), m_pso.Get());
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		m_commandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&m_clearColor, 0, NULL);
		m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), FALSE, NULL);

		m_commandList->RSSetViewports(1, &m_screenViewport);
		m_commandList->RSSetScissorRects(1, &m_scissorRect);
	}

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	int passCbvIndex = 1 + m_currFrameContextIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, m_cbvSrvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(0, passCbvHandle);

	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->IASetIndexBuffer(&m_indexBufferView);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void FullScreenQuad::DrawUI()
{
	ImGui::Begin("Color Setting");

	ImGui::Checkbox("Use uv as background color", &m_useUVAsColor);
	
	float tempColor[3] = { m_backgroundColor.x, m_backgroundColor.y, m_backgroundColor.z };
	ImGui::ColorEdit3("Background Color", tempColor);
	m_backgroundColor = XMFLOAT3(tempColor);

	ImGui::End();
}