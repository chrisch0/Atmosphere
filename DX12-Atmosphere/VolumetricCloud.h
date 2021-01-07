#pragma once
#include "App/App.h"
#include "Volumetric/CloudShapeManager.h"

class Camera;
class Mesh;
class Texture2D;
class Texture3D;

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
	void InitCloudParameters();
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

	const Texture2D* m_weatherTexture;
	const Texture3D* m_erosionTexture;
	const Texture3D* m_noiseShapeTexture;

	Vector3 m_position;
	Vector3 m_scale;
	Vector3 m_rotation;
	Matrix4 m_modelMatrix;

	Matrix4 m_skyboxModelMatrix;

	// Cloud Setting
	int m_sampleCountMin;
	int m_sampleCountMax;
	// atmosphere setting
	float m_extinct;
	float m_hgCoeff;
	float m_scatterCoeff;
	
	// sun light setting
	Vector3 m_sunLightRotation;
	Vector3 m_sunLightColor;
	int m_lightSampleCount;

	// cloud shape setting
	// TODO: replace by weather texture and density gradient texture
	float m_altitudeMin;
	float m_altitudeMax;
	float m_farDistance;
};