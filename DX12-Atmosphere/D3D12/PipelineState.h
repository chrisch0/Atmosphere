#pragma once
#include "stdafx.h"

class CommandContext;
class RootSignature;

class PipelineState
{
public:
	PipelineState() : m_rootSignature(nullptr) {}

	void SetRootSignature(const RootSignature& rootSignature)
	{
		m_rootSignature = &rootSignature;
	}

	const RootSignature& GetRootSignature() const 
	{
		assert(m_rootSignature != nullptr);
		return *m_rootSignature;
	}

	ID3D12PipelineState* GetPipelineStateObject() const 
	{
		return m_pso;
	}
protected:
	const RootSignature* m_rootSignature;
	ID3D12PipelineState* m_pso;
};

class GraphicsPSO : public PipelineState
{
	friend class CommandContext;
public:
	GraphicsPSO();

	void SetBlendState(const D3D12_BLEND_DESC& blendDesc);
	void SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizerDesc);
	void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depthStencilDesc);
	void SetSampleMask(UINT sampleMask);
	void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType);
	void SetRenderTargetFormat(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, UINT msaaCount = 1, UINT msaaQuality = 0);
	void SetRenderTargetFormats(UINT numRTVs, const DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat, UINT msaaCount = 1, UINT msaaQuality = 0);
	void SetInputLayout(UINT numElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs);
	void SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps);

	void SetVertexShader(const void* binary, size_t size) { m_psoDesc.VS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(binary), size); }
	void SetPixelShader(const void* binary, size_t size) { m_psoDesc.PS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(binary), size); }
	void SetGeometryShader(const void* binary, size_t size) { m_psoDesc.GS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(binary), size); }
	void SetHullShader(const void* binary, size_t size) { m_psoDesc.HS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(binary), size); }
	void SetDomainShader(const void* binary, size_t size) { m_psoDesc.DS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(binary), size); }

	void Finalize();

private:
	D3D12_GRAPHICS_PIPELINE_STATE_DESC m_psoDesc;
	std::shared_ptr<const D3D12_INPUT_ELEMENT_DESC> m_inputLayouts;
};

class ComputePSO : public PipelineState
{
	friend class CommandContext;
public:
	ComputePSO();

	void SetComputeShader(const void* binary, size_t size) 
	{
		m_psoDesc.CS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(binary), size);
	}

	void Finalize();
private:
	D3D12_COMPUTE_PIPELINE_STATE_DESC m_psoDesc;
};