#pragma once
#include "App/App.h"
#include "Volumetric/CloudShapeManager.h"

class VolumetricCloud : public App
{
public:
	VolumetricCloud();
	~VolumetricCloud();

	bool Initialize() override;
	void Draw(const Timer& timer) override;
	void Update(const Timer& timer) override;
	void UpdateUI() override;
private:

	CloudShapeManager m_cloudShapeManager;
};