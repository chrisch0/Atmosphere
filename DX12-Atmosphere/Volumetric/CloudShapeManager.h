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

	void CreateBasicCloudShape();
	void CreateGradient();

	void GenerateBasicCloudShape();
	void GenerateGradient(std::shared_ptr<ColorBuffer> texPtr, float cloudMin, float cloudMax, float solidMin, float solidMax);

	void SetShowWindow(bool val) { m_showWindow = val; }
	void SetShowNoiseGeneratorWindow(bool val);

	VolumeColorBuffer* GetBasicCloudShape() const { return m_basicShape.get(); }
	VolumeColorBuffer* GetPerlinNoise() const { return m_perlinNoise.get(); }
	ColorBuffer* GetStratusGradient() const { return m_stratusGradient.get(); }
	ColorBuffer* GetCumulusGradinet() const { return m_cumulusGradient.get(); }
	ColorBuffer* GetCumulonimbusGradient() const { return m_cumulonimbusGradient.get(); }

private:
	RootSignature m_basicShapeRS;
	RootSignature m_gradientRS;

	ComputePSO m_basicShapePSO;
	ComputePSO m_gradientPSO;

	std::shared_ptr<NoiseGenerator> m_noiseGenerator;

	std::shared_ptr<VolumeColorBuffer> m_perlinNoise;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMLow;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMMid;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMHigh;
	std::shared_ptr<VolumeColorBuffer> m_basicShape;
	std::shared_ptr<ColorBuffer> m_basicShapeView;
	std::shared_ptr<ColorBuffer> m_stratusGradient;
	std::shared_ptr<ColorBuffer> m_cumulusGradient;
	std::shared_ptr<ColorBuffer> m_cumulonimbusGradient;

	float m_basicShapeMin;
	float m_basicShapeMax;
	float m_worleySampleFreq;
	float m_perlinSampleFreq;
	float m_worleyAmp;
	float m_perlinAmp;
	float m_noiseBias;
	int m_method2;

	bool m_showWindow;
};