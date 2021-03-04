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
	void GenerateDensityHeightGradient();
	void GenerateErosion();

	void SetShowWindow(bool val) { m_showWindow = val; }
	void SetShowNoiseGeneratorWindow(bool val);

	VolumeColorBuffer* GetBasicCloudShape() const { return m_basicShape.get(); }
	VolumeColorBuffer* GetPerlinNoise() const { return m_perlinNoise.get(); }
	VolumeColorBuffer* GetErosion() const { return m_erosion.get(); }

	float GetAltitudeMin() const { return m_cloudMin; }
	float GetAltitudeMax() const { return m_cloudMax; }
	ColorBuffer* GetDensityHeightGradient() const { return m_densityHeightGradinet.get(); }

	NoiseGenerator* GetNoiseGenerator() { return m_noiseGenerator.get(); }

private:
	RootSignature m_basicShapeRS;
	RootSignature m_gradientRS;
	RootSignature m_erosionRS;

	ComputePSO m_basicShapePSO;
	ComputePSO m_gradientPSO;
	ComputePSO m_erosionPSO;

	std::shared_ptr<NoiseGenerator> m_noiseGenerator;

	std::shared_ptr<VolumeColorBuffer> m_perlinNoise;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMLow;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMMid;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMHigh;
	std::shared_ptr<VolumeColorBuffer> m_basicShape;
	std::shared_ptr<ColorBuffer> m_basicShapeView;

	std::shared_ptr<VolumeColorBuffer> m_worleyFBMLow32;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMMid32;
	std::shared_ptr<VolumeColorBuffer> m_worleyFBMHigh32;
	std::shared_ptr<VolumeColorBuffer> m_erosion;

	float m_cloudMin;
	float m_cloudMax;
	std::shared_ptr<ColorBuffer> m_densityHeightGradinet;

	struct
	{
		Vector4 stratus;
		Vector4 cumulus;
		Vector4 cumulonimbus;
		Vector4 textureSize;
	}m_cloudTypeParams;

	float m_basicShapeMin;
	float m_basicShapeMax;
	float m_worleySampleFreq;
	float m_perlinSampleFreq;
	float m_worleyAmp;
	float m_perlinAmp;
	float m_noiseBias;
	int m_method2 = 0;

	bool m_showWindow;
};