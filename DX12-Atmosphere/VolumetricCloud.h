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
	void OnResize() override;

private:
	void InitCloudParameters();
	void CreatePSO();
	void CreateMeshes();
	void CreateCamera();
	void CreateNoise();
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

	RootSignature m_generateNoiseRS;
	ComputePSO m_generateWorleyPSO;
	ComputePSO m_generatePerlinWorleyPSO;

	ComputePSO m_temporalCloudPSO;
	ComputePSO m_quarterCloudPSO;
	ComputePSO m_combineSkyPSO;

	//GraphicsPSO m_renderCloudOnQuadPSO;

	std::shared_ptr<Camera> m_camera;
	std::shared_ptr<Mesh> m_boxMesh;
	std::shared_ptr<Mesh> m_skyboxMesh;
	std::shared_ptr<Mesh> m_quadMesh;

	const Texture2D* m_weatherTexture;
	const Texture2D* m_curlNoise2D;
	std::shared_ptr<VolumeColorBuffer> m_curlNoiseTexture;
	std::shared_ptr<VolumeColorBuffer> m_perlinWorleyUE;
	VolumeColorBuffer* m_basicCloudShape;
	VolumeColorBuffer* m_erosionTexture;

	std::shared_ptr<VolumeColorBuffer> m_perlinWorley;
	std::shared_ptr<VolumeColorBuffer> m_worley;
	
	std::shared_ptr<ColorBuffer> m_quarterBuffer;
	std::shared_ptr<ColorBuffer> m_cloudTempBuffer;

	std::shared_ptr<ColorBuffer> m_mipmapTestBuffer;

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
		float lightColor[3] = { 1.0f, 0.99995f, 0.90193f };
		int sampleCountMin = 64;
		int sampleCountMax = 128;
		float cloudCoverage = 1.0f;
		float Exposure = 1.0;
		float GroundAlbedo = 1.0;
		float cloudBottomColor[3] = {0.38235f, 0.41176f, 0.47059f};
		float crispiness = 43.238f;
		float curliness = 0.1f;
		float earthRadius = 6360.0f;
		float cloudBottomRadius = 5000.0f;
		float cloudTopRadius = 17000.0f;
		float cloudSpeed = 0.0f;
		float densityFactor = 0.003f;
		float absorption = 0.0037f;
		float hg0 = -0.08f;
		float hg1 = 0.08f;
		int enablePowder = 1;
		int enableBeer = 1;
		float rainAbsorption = 1.0f;
		float eccentricity = 0.6f;
		float sliverIntensity = 1.0f;
		float sliverSpread = 0.3f;
		float brightness = 1.0f;
		float cloudScattering[3] = { 1.0f , 0.878f, 0.745f};
		float cloudScatteringWeight = 2.193f;
		float ABC[3] = { 0.5f, 0.5f, 0.5f };
		float HGWeight = 0.5;
	}m_cloudParameterCB;

	struct
	{
		Matrix4 invView;
		Matrix4 invProj;
		Matrix4 prevViewProj;
		XMFLOAT3 cameraPosition;
		float time = 0.0f;
		XMFLOAT3 lightDir;
		uint32_t frameIndex = 0;
		XMFLOAT4 resolution;
		XMFLOAT3 groundAlbedo;
		float exposure = 1.0f;
		XMFLOAT3 whitePoint;
		float sunSize = 0.999853f;
	}m_passCB;

	bool m_useTemporal;
	bool m_computeToQuarter = false;
	bool m_autoMoveSun = false;
};