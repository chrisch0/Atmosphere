#pragma once
#include "App/App.h"
#include "Volumetric/CloudShapeManager.h"

class Camera;
class Mesh;
class Texture2D;
class Texture3D;
class VolumeColorBuffer;

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
	void SwitchBasicCloudShape(int idx);

	void DrawOnSkybox(const Timer& timer);
	void DrawOnQuad(const Timer& timer);

	CloudShapeManager m_cloudShapeManager;

	RootSignature m_skyboxRS;
	GraphicsPSO m_skyboxPSO;

	RootSignature m_volumetricCloudRS;
	GraphicsPSO m_volumetricCloudPSO;

	RootSignature m_computeCloudOnQuadRS;
	ComputePSO m_computeCloudOnQuadPSO;

	//GraphicsPSO m_renderCloudOnQuadPSO;

	std::shared_ptr<Camera> m_camera;
	std::shared_ptr<Mesh> m_boxMesh;
	std::shared_ptr<Mesh> m_skyboxMesh;
	std::shared_ptr<Mesh> m_quadMesh;

	const Texture2D* m_weatherTexture;
	std::shared_ptr<VolumeColorBuffer> m_curlNoiseTexture;
	std::shared_ptr<VolumeColorBuffer> m_perlinWorleyUE;
	VolumeColorBuffer* m_basicCloudShape;
	std::shared_ptr<VolumeColorBuffer> m_erosionTexture;

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
	float m_farDistance;

	struct
	{
		Matrix4 invView;
		Matrix4 invProj;
		float lightColor[3] = { 1.0f, 0.99995f, 0.90193f };
		float time = 0.0f;
		XMFLOAT3 lightDir;
		int sampleCount = 64;
		XMFLOAT3 cameraPosition;
		float cloudCoverage = 0.45f;
		float cloudBottomColor[3] = {0.38235f, 0.41176f, 0.47059f};
		float crispiness = 40.0f;
		float curliness = 0.1f;
		float earthRadius = 600000.0f;
		float cloudBottomRadius = 5000.0f;
		float cloudTopRadius = 17000.0f;
		XMFLOAT4 resolution;
		float cloudSpeed = 0.0f;
		float densityFactor = 0.02f;
		float absorption = 0.0035f;
		float hg0 = -0.08f;
		float hg1 = 0.08f;
		int enablePowder = 1;
	}m_cloudOnQuadCB;
};