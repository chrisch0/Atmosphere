#pragma once
#include "stdafx.h"
#include "App/App.h"
#include "D3D12/GpuBuffer.h"

class RootSignature;
class GraphicsPSO;

class FullScreenQuad : public App
{
public:
	FullScreenQuad();
	~FullScreenQuad();

	bool Initialize();
	void Draw(const Timer& timer) override;
	void Update(const Timer& timer) override;
	void UpdateUI() override;

private:
	void CreateRootSignature();
	void CreatePipelineStates();
	void CreateConstantBufferView();

private:
	RootSignature m_rootSignature;
	GraphicsPSO m_pipelineState;

	StructuredBuffer m_vertexBuffer;
	ByteAddressBuffer m_indexBuffer;

	bool m_useUVAsColor = false;
	DirectX::XMFLOAT3 m_backgroundColor{0.0, 0.0, 0.5};
};