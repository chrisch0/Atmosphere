#pragma once
#include "stdafx.h"
#include "App/App.h"
#include "D3D12/GpuBuffer.h"

class RootSignature;
class GraphicsPSO;
class ComputePSO;
class Camera;
class ColorBuffer;
class Mesh;

class ComputeDemo : public App
{
public:
	ComputeDemo();
	~ComputeDemo();

	bool Initialize() override;
	void Draw(const Timer& timer) override;
	void Update(const Timer& timer) override;
	void UpdateUI() override;

private:
	void CreateRootSignature();
	void CreatePipelineState();
	void CreateResources();

private:
	RootSignature m_graphicsRS;
	RootSignature m_computeRS;
	GraphicsPSO m_graphicsPSO;
	ComputePSO m_computePSO;

	ColorBuffer m_noise;
	StructuredBuffer m_minMax;

	std::shared_ptr<Camera> m_camera;
	std::shared_ptr<Mesh> m_quad;

	int m_seed;
	float m_frequency;
	bool m_isNoiseSettingDirty;
	bool m_isFirst;
};