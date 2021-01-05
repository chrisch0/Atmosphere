#pragma once
#include "App/App.h"
#include "Volumetric/CloudShapeManager.h"

class Camera;
class Mesh;

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
	void CreatePSO();
	void CreateMeshes();
	void CreateCamera();

	CloudShapeManager m_cloudShapeManager;

	RootSignature m_skyboxRS;
	GraphicsPSO m_skyboxPSO;

	RootSignature m_volumetricCloudRS;
	GraphicsPSO m_volumetricCloudPSO;

	std::shared_ptr<Camera> m_camera;
	std::shared_ptr<Mesh> m_boxMesh;
	std::shared_ptr<Mesh> m_skyboxMesh;

	Vector3 m_position;
	Vector3 m_scale;
	Vector3 m_rotation;
	Matrix4 m_modelMatrix;

	Matrix4 m_skyboxModelMatrix;
};