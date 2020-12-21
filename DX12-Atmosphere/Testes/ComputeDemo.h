#pragma once
#include "stdafx.h"
#include "App/App.h"
#include "D3D12/GpuBuffer.h"
#include "Noise/NoiseGenerator.h"

class Camera;
class ColorBuffer;
class VolumeColorBuffer;
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
	VolumeColorBuffer m_noise3D;
	StructuredBuffer m_minMax;

	D3D12_GPU_DESCRIPTOR_HANDLE m_tex2DHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_tex3DHandle;

	std::shared_ptr<Camera> m_camera;
	std::shared_ptr<Mesh> m_quad;

	NoiseGenerator m_noiseGenerator;

	int m_seed;
	float m_frequency;
	bool m_isNoiseSettingDirty;
	bool m_isFirst;
	bool m_generate3D;
};