#pragma once
#include "stdafx.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"

class NoiseGenerator;

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
private:
	std::shared_ptr<NoiseGenerator> m_noiseGenerator;
};