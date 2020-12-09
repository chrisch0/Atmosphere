#pragma once
#include "stdafx.h"
//#include "App/App.h"
//#include "D3D12/GpuBuffer.h"

//class FullScreenQuad : public App
//{
//public:
//	FullScreenQuad();
//	~FullScreenQuad();
//
//	bool Initialize();
//	void Draw(const Timer& timer) override;
//	void Update(const Timer& timer) override;
//	void DrawUI() override;
//
//private:
//	void CreateRootSignature();
//	void CreatePipelineStates();
//	void CreateConstantBufferView();
//
//private:
//	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
//	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
//	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
//
//	StructuredBuffer m_vertexBuffer;
//	ByteAddressBuffer m_indexBuffer;
//
//	bool m_useUVAsColor = false;
//	DirectX::XMFLOAT3 m_backgroundColor{0.0, 0.0, 0.5};
//
//	ID3D12Resource* m_constantUploadBuffers[m_numFrameContexts];
//	BYTE* m_constantMappedData[m_numFrameContexts];
//	D3D12_GPU_VIRTUAL_ADDRESS m_cbGPUAddress[m_numFrameContexts];
//};