#pragma once
#include "stdafx.h"
#include "App/App.h"

class RootSignature;
class GraphicsPSO;
class ComputePSO;
class Camera;
class ColorBuffer;

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
	RootSignature m_graphicsRS;
	RootSignature m_computeRS;
	GraphicsPSO m_graphicsPSO;
	ComputePSO m_computePSO;

	ColorBuffer m_computeResult;

	std::shared_ptr<Camera> m_camera;
};