#pragma once
#include "stdafx.h"
#include "App/App.h"
#include "D3D12/GpuBuffer.h"

class RootSignature;
class GraphicsPSO;
class ComputePSO;
class Camera;
class ColorBuffer;
class ColorBuffer3D;
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
	GraphicsPSO m_showNoise3DPSO;
	ComputePSO m_computePSO;
	ComputePSO m_computeNoise3DPSO;

	ColorBuffer m_noise;
	ColorBuffer3D m_noise3D;
	StructuredBuffer m_minMax;

	std::shared_ptr<Camera> m_camera;
	std::shared_ptr<Mesh> m_quad;

	int m_seed;
	float m_frequency;
	bool m_isNoiseSettingDirty;
	bool m_isFirst;
	bool m_generate3D;
};