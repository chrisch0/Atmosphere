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
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBufferGPU = nullptr;

	bool m_useUVAsColor = false;
};