#include "stdafx.h"
#include "PipelineState.h"
#include "CommandContext.h"
#include "RootSignature.h"

GraphicsPSO::GraphicsPSO()
{
	ZeroMemory(&m_psoDesc, sizeof(m_psoDesc));
	m_psoDesc.NodeMask = 1;
	m_psoDesc.SampleMask = 0xFFFFFFFFu;
	m_psoDesc.SampleDesc.Count = 1;
	m_psoDesc.InputLayout.NumElements = 0;
}

void GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& blendDesc)
{
	m_psoDesc.BlendState = blendDesc;
}

void GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizerDesc)
{
	m_psoDesc.RasterizerState = rasterizerDesc;
}

void GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depthStencilDesc)
{
	m_psoDesc.DepthStencilState = depthStencilDesc;
}

void GraphicsPSO::SetSampleMask(UINT sampleMask)
{
	m_psoDesc.SampleMask = sampleMask;
}

void GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType)
{
	assert(topologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED);
	m_psoDesc.PrimitiveTopologyType = topologyType;
}

void GraphicsPSO::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps)
{
	m_psoDesc.IBStripCutValue = IBProps;
}

void GraphicsPSO::SetRenderTargetFormat(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, UINT msaaCount /* = 1 */, UINT msaaQuality /* = 0 */)
{
	SetRenderTargetFormats(1, &rtvFormat, dsvFormat, msaaCount, msaaQuality);
}

void GraphicsPSO::SetRenderTargetFormats(UINT numRTVs, const DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat, UINT msaaCount /* = 1 */, UINT msaaQuality /* = 0 */)
{
	assert(numRTVs == 0 || rtvFormats != nullptr);
	for (uint32_t i = 0; i < numRTVs; ++i)
		m_psoDesc.RTVFormats[i] = rtvFormats[i];
	for (uint32_t i = numRTVs; i < m_psoDesc.NumRenderTargets; ++i)
		m_psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
	m_psoDesc.NumRenderTargets = numRTVs;
	m_psoDesc.DSVFormat = dsvFormat;
	m_psoDesc.SampleDesc.Count = msaaCount;
	m_psoDesc.SampleDesc.Quality = msaaQuality;
}

void GraphicsPSO::SetInputLayout(UINT numElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
{
	m_psoDesc.InputLayout.NumElements = numElements;
	if (numElements > 0)
	{
		D3D12_INPUT_ELEMENT_DESC* newElements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * numElements);
		memcpy(newElements, pInputElementDescs, numElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
		m_inputLayouts.reset((const D3D12_INPUT_ELEMENT_DESC*)newElements);
	}
	else
		m_inputLayouts = nullptr;
}

void GraphicsPSO::Finalize()
{
	m_psoDesc.InputLayout.pInputElementDescs = m_inputLayouts.get();
	m_psoDesc.pRootSignature = m_rootSignature->GetSignature();
	assert(m_psoDesc.pRootSignature != nullptr);

	ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&m_psoDesc, IID_PPV_ARGS(&m_pso)));
}

void ComputePSO::Finalize()
{
	m_psoDesc.pRootSignature = m_rootSignature->GetSignature();
	assert(m_psoDesc.pRootSignature != nullptr);

	ThrowIfFailed(g_Device->CreateComputePipelineState(&m_psoDesc, IID_PPV_ARGS(&m_pso)));
}