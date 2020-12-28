#pragma once
#include "stdafx.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"
#include "D3D12/GpuBuffer.h"

class NoiseGenerator;
class ColorBuffer;
class VolumeColorBuffer;

class CloudShapeManager
{
public:
	void Initialize();
	void Update();
	void UpdateUI();

	void CreateStratusGradient();
	void CreateCumulusGradient();
	void CreateCumulonimbusGradient();

	void CreateBasicCloudShape();
	void GenerateBasicCloudShape();
private:
	RootSignature m_basicShapeRS;
	ComputePSO m_basicShapePSO;

	std::shared_ptr<NoiseGenerator> m_noiseGenerator;

	std::shared_ptr<VolumeColorBuffer> m_perlinNoise;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMLow;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMMid;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMHigh;
	std::shared_ptr<VolumeColorBuffer> m_basicShape;
	std::shared_ptr<ColorBuffer> m_basicShapeView;
	float m_basicShapeMin;
	float m_basicShapeMax;

	bool m_showWindow;
};