#pragma once
#include "stdafx.h"
#include "App/App.h"

class FullScreenQuad : public App
{
public:
	FullScreenQuad();
	~FullScreenQuad();

	bool Initialize();
	void Draw(const Timer& timer) override;
	void Update(const Timer& timer) override;
	void DrawUI() override;

private:
	void CreateRootSignature();
	void CreatePipelineStates();
	void CreateConstantBufferView();

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBufferGPU = nullptr;

	bool m_useUVAsColor = false;
	DirectX::XMFLOAT3 m_backgroundColor{0.0, 0.0, 0.5};

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_constantUploadBuffers;
	BYTE* m_constantMappedData[c_swapChainBufferCount];
	D3D12_GPU_VIRTUAL_ADDRESS m_cbGPUAddress[c_swapChainBufferCount];

	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
};