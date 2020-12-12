#pragma once
#include "stdafx.h"
#include "App/App.h"
#include "D3D12/GpuBuffer.h"

class RootSignature;
class GraphicsPSO;

class CameraDemo : public App
{
public:
	CameraDemo();
	~CameraDemo();

	bool Initialize();
	void Draw(const Timer& timer) override;
	void Update(const Timer& timer) override;
	void UpdateUI() override;

private:
	void CreatePSO();
	void CreateCBV();
	void CreateGeometry();

	RootSignature m_RS;
	GraphicsPSO m_PSO;

	StructuredBuffer m_vertexBuffer;
	ByteAddressBuffer m_indexBuffer;
};