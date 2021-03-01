#include "stdafx.h"
#include "VolumetricCloud.h"
#include "D3D12/GraphicsGlobal.h"
#include "D3D12/CommandContext.h"
#include "D3D12/TextureManager.h"
#include "D3D12/ColorBuffer.h"
#include "D3D12/PostProcess.h"
#include "Utils/CameraController.h"
#include "Utils/Camera.h"
#include "Mesh/Mesh.h"
#include "Atmosphere/Atmosphere.h"

#include "CompiledShaders/VolumetricCloud_VS.h"
#include "CompiledShaders/VolumetricCloud_PS.h"
#include "CompiledShaders/Skybox_VS.h"
#include "CompiledShaders/Skybox_PS.h"
#include "CompiledShaders/ComputeCloud_CS.h"
#include "CompiledShaders/PerlinWorley_CS.h"
#include "CompiledShaders/Worley_CS.h"
#include "CompiledShaders/ComputeQuarterCloud_CS.h"
#include "CompiledShaders/CombineSky_CS.h"
#include "CompiledShaders/TemporalCloud_CS.h"

using namespace Global;

VolumetricCloud::VolumetricCloud()
{

}

VolumetricCloud::~VolumetricCloud()
{

}

bool VolumetricCloud::Initialize()
{
	if (!App::Initialize())
		return false;

	Global::InitializeGlobalStates();

	m_showAnotherDemoWindow = false;
	m_showDemoWindow = false;
	m_showHelloWindow = false;

	m_cloudShapeManager.Initialize();

	InitCloudParameters();
	CreatePSO();
	CreateMeshes();
	CreateCamera();
	CreateNoise();

	m_position = Vector3(0.0f, 0.0f, 0.0f);
	m_rotation = Vector3(0.0f, 0.0f, 0.0f);
	m_scale = Vector3(1.0f, 1.0f, 1.0f);

	m_quarterBuffer = std::make_shared<ColorBuffer>();
	m_quarterBuffer->Create(L"Quarter Buffer", m_clientWidth / 4, m_clientHeight / 4, 1, m_sceneBufferFormat);
	m_cloudTempBuffer = std::make_shared<ColorBuffer>();
	m_cloudTempBuffer->Create(L"Previous Cloud Buffer", m_clientWidth, m_clientHeight, 1, m_sceneBufferFormat);

	m_mipmapTestBuffer = std::make_shared<ColorBuffer>();
	m_mipmapTestBuffer->Create(L"Generate Mips Buffer", m_clientWidth, m_clientHeight, 0, m_sceneBufferFormat);

	PostProcess::EnableHDR = false;

	Atmosphere::Precompute(4);

	XMStoreFloat3(&m_passCB.groundAlbedo, Vector3(0.0f, 0.0f, 0.04f));

	return true;
}

void VolumetricCloud::InitCloudParameters()
{
	m_sampleCountMin = 32;
	m_sampleCountMax = 64;

	m_extinct = 0.0025f;
	m_hgCoeff = 0.5f;
	m_scatterCoeff = 0.009f;

	m_sunLightRotation = Vector3(30.0f, -180.0f, 0.0f);
	m_sunLightColor = Vector3(1.0f, 0.9568627f, 0.8392157f);
	m_lightSampleCount = 6;

	m_farDistance = 22000.0f;

	//m_weatherTexture = TextureManager::LoadTGAFromFile("CloudWeatherTexture.TGA");
	m_weatherTexture = TextureManager::LoadDDSFromFile("Weather_Texture.dds");
	//auto* curl_2d = TextureManager::LoadDDSFromFile("CurlNoise_Volume_16by8.DDS");
	m_curlNoise2D = TextureManager::LoadDDSFromFile("CurlNoise_2D.dds");
	//m_curlNoiseTexture = std::make_shared<VolumeColorBuffer>();
	//m_curlNoiseTexture->CreateFromTexture2D(L"CurlVolumeNoise", curl_2d, 16, 8, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	auto* perlin_worley = TextureManager::LoadDDSFromFile("PerlinWorley.DDS");
	m_perlinWorleyUE = std::make_shared<VolumeColorBuffer>();
	m_perlinWorleyUE->CreateFromTexture2D(L"PerlinWorleyUE", perlin_worley, 16, 8, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
	//m_erosionTexture = TextureManager::LoadTGAFromFile("volume_test.TGA", 8, 2);
	//m_noiseShapeTexture = TextureManager::LoadTGAFromFile("CloudWeatherTexture.tga", 8, 8);

}

void VolumetricCloud::CreatePSO()
{
	m_volumetricCloudRS.Reset(4, 2);
	m_volumetricCloudRS[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_volumetricCloudRS[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_volumetricCloudRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_volumetricCloudRS[3].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_volumetricCloudRS.InitStaticSampler(0, SamplerLinearClampDesc);
	m_volumetricCloudRS.InitStaticSampler(1, SamplerLinearMirrorDesc);
	m_volumetricCloudRS.Finalize(L"VolumetricCloudRS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	D3D12_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	m_volumetricCloudPSO.SetRootSignature(m_volumetricCloudRS);
	m_volumetricCloudPSO.SetSampleMask(UINT_MAX);
	m_volumetricCloudPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_volumetricCloudPSO.SetRenderTargetFormat(m_sceneBufferFormat, DXGI_FORMAT_UNKNOWN);
	m_volumetricCloudPSO.SetVertexShader(g_pVolumetricCloud_VS, sizeof(g_pVolumetricCloud_VS));
	m_volumetricCloudPSO.SetPixelShader(g_pVolumetricCloud_PS, sizeof(g_pVolumetricCloud_PS));
	m_volumetricCloudPSO.SetInputLayout(_countof(layout), layout);
	m_volumetricCloudPSO.SetBlendState(BlendPreMultiplied);
	m_volumetricCloudPSO.SetDepthStencilState(DepthStateDisabled);
	m_volumetricCloudPSO.SetRasterizerState(RasterizerDefault);
	m_volumetricCloudPSO.Finalize();

	m_skyboxRS.Reset(3, 1);
	m_skyboxRS[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_skyboxRS[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_skyboxRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_skyboxRS.InitStaticSampler(0, SamplerLinearWrapDesc);
	m_skyboxRS.Finalize(L"SkyboxRS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_skyboxPSO = m_volumetricCloudPSO;
	m_skyboxPSO.SetRootSignature(m_skyboxRS);
	m_skyboxPSO.SetVertexShader(g_pSkybox_VS, sizeof(g_pSkybox_VS));
	m_skyboxPSO.SetPixelShader(g_pSkybox_PS, sizeof(g_pSkybox_PS));
	m_skyboxPSO.Finalize();

	m_computeCloudOnQuadRS.Reset(5, 2);
	m_computeCloudOnQuadRS[0].InitAsConstantBufferView(0);
	m_computeCloudOnQuadRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_computeCloudOnQuadRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
	m_computeCloudOnQuadRS[3].InitAsConstantBufferView(1);
	m_computeCloudOnQuadRS[4].InitAsConstantBufferView(2);
	m_computeCloudOnQuadRS.InitStaticSampler(0, SamplerLinearClampDesc);
	m_computeCloudOnQuadRS.InitStaticSampler(1, SamplerLinearWrapDesc);
	m_computeCloudOnQuadRS.Finalize(L"ComputeCloudOnQuadRS");

	m_computeCloudOnQuadPSO.SetRootSignature(m_computeCloudOnQuadRS);
	m_computeCloudOnQuadPSO.SetComputeShader(g_pComputeCloud_CS, sizeof(g_pComputeCloud_CS));
	m_computeCloudOnQuadPSO.Finalize();

	m_generateNoiseRS.Reset(1, 0);
	m_generateNoiseRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_generateNoiseRS.Finalize(L"GenerateNoiseRS");

	m_generateWorleyPSO.SetRootSignature(m_generateNoiseRS);
	m_generateWorleyPSO.SetComputeShader(g_pWorley_CS, sizeof(g_pWorley_CS));
	m_generateWorleyPSO.Finalize();

	m_generatePerlinWorleyPSO.SetRootSignature(m_generateNoiseRS);
	m_generatePerlinWorleyPSO.SetComputeShader(g_pPerlinWorley_CS, sizeof(g_pPerlinWorley_CS));
	m_generatePerlinWorleyPSO.Finalize();

	m_temporalCloudPSO.SetRootSignature(m_computeCloudOnQuadRS);
	m_temporalCloudPSO.SetComputeShader(g_pTemporalCloud_CS, sizeof(g_pTemporalCloud_CS));
	m_temporalCloudPSO.Finalize();

	m_quarterCloudPSO.SetRootSignature(m_computeCloudOnQuadRS);
	m_quarterCloudPSO.SetComputeShader(g_pComputeQuarterCloud_CS, sizeof(g_pComputeQuarterCloud_CS));
	m_quarterCloudPSO.Finalize();

	m_combineSkyPSO.SetRootSignature(m_computeCloudOnQuadRS);
	m_combineSkyPSO.SetComputeShader(g_pCombineSky_CS, sizeof(g_pCombineSky_CS));
	m_combineSkyPSO.Finalize();
}

void VolumetricCloud::CreateMeshes()
{
	auto* box_mesh = Mesh::CreateBox(0);
	m_boxMesh = std::make_shared<Mesh>();
	m_boxMesh.reset(box_mesh);

	auto* skybox_mesh = Mesh::CreateSkybox();
	m_skyboxMesh = std::make_shared<Mesh>();
	m_skyboxMesh.reset(skybox_mesh);
}

void VolumetricCloud::CreateCamera()
{
	m_camera = CameraController::CreateCamera<SceneCamera>("Scene Camera");
	m_camera->SetLookAt({ 2.25f, 1.75f, 2.10f }, { 2.25f - 0.659f, 1.75f + 0.409f, 2.10f - 0.632f }, { 0.0f, 1.0f, 0.0f });
	m_camera->SetNearClip(0.01f);
	m_camera->SetFarClip(10000.0f);
	Atmosphere::SetCamera(m_camera.get());
}

void VolumetricCloud::CreateNoise()
{
	m_perlinWorley = std::make_shared<VolumeColorBuffer>();
	m_perlinWorley->Create(L"Perlin Worley", 128, 128, 128, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_worley = std::make_shared<VolumeColorBuffer>();
	m_worley->Create(L"Worley", 32, 32, 32, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);

	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_generateNoiseRS);
	context.SetPipelineState(m_generatePerlinWorleyPSO);
	context.TransitionResource(*m_perlinWorley, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(0, 0, m_perlinWorley->GetUAV());
	context.Dispatch3D(m_perlinWorley->GetWidth(), m_perlinWorley->GetHeight(), m_perlinWorley->GetDepth(), 8, 8, 8);

	context.SetPipelineState(m_generateWorleyPSO);
	context.TransitionResource(*m_worley, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(0, 0, m_worley->GetUAV());
	context.Dispatch3D(m_worley->GetWidth(), m_worley->GetHeight(), m_worley->GetDepth(), 8, 8, 8);
	context.Finish();

	ComputeContext& mipContext = ComputeContext::Begin();
	m_perlinWorley->GenerateMipMaps(context);
	m_worley->GenerateMipMaps(mipContext);
	m_perlinWorleyUE->GenerateMipMaps(context);
	mipContext.Finish();
}

void VolumetricCloud::SwitchBasicCloudShape(int idx)
{
	switch (idx)
	{
	default:
		break;
	case 0:
		m_basicCloudShape = m_cloudShapeManager.GetBasicCloudShape();
		break;
	case 1:
		m_basicCloudShape = m_perlinWorleyUE.get();
		break;
	}
}

void VolumetricCloud::Update(const Timer& timer)
{
	m_cloudShapeManager.Update();
	g_CameraController.Update(timer.DeltaTime());

	Matrix3 scale_rotation = Matrix3::MakeScale(m_scale) *
		Matrix3::MakeYRotation(ToRadian(m_rotation.GetY())) * Matrix3::MakeXRotation(ToRadian(m_rotation.GetX())) * Matrix3::MakeZRotation(ToRadian(m_rotation.GetZ()));
	AffineTransform trans(scale_rotation, m_position);
	m_modelMatrix = Matrix4(trans);

	Matrix3 skybox_scale = Matrix3::MakeScale({ 10000.0f, 10000.0f, 10000.0f });
	AffineTransform skybox_trans(skybox_scale, m_camera->GetPosition());
	m_skyboxModelMatrix = Matrix4(skybox_trans);

	m_passCB.invView = Invert(m_camera->GetViewMatrix());
	m_passCB.invProj = Invert(m_camera->GetProjMatrix());
	m_passCB.time = timer.TotalTime();
	auto dir = Quaternion(ToRadian(m_sunLightRotation.GetX()), ToRadian(m_sunLightRotation.GetY()), ToRadian(m_sunLightRotation.GetZ())) * Vector3(0.0f, 0.0f, 1.0f);
	XMStoreFloat3(&m_passCB.lightDir, -dir);
	XMStoreFloat3(&m_passCB.cameraPosition, m_camera->GetPosition());
	Vector4 res{ (float)m_sceneColorBuffer->GetWidth(), (float)m_sceneColorBuffer->GetHeight(), 1.0f / m_sceneColorBuffer->GetWidth(), 1.0f / m_sceneColorBuffer->GetHeight() };
	XMStoreFloat4(&m_passCB.resolution, res);
	m_passCB.frameIndex = m_frameIndex;
	m_passCB.prevViewProj = m_camera->GetPrevViewProjMatrix();

	XMStoreFloat3(&m_passCB.whitePoint, Vector3(1.0f, 1.0f, 1.0f));
	//Atmosphere::Update(dir, res);
}

void VolumetricCloud::Draw(const Timer& timer)
{
	//DrawOnSkybox(timer);
	//Atmosphere::Draw();
	DrawOnQuad(timer);

	/*GraphicsContext& mipsContext = GraphicsContext::Begin();

	mipsContext.TransitionResource(*m_sceneColorBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
	mipsContext.TransitionResource(*m_mipmapTestBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
	mipsContext.CopySubresource(*m_mipmapTestBuffer, 0, *m_sceneColorBuffer, 0);

	m_mipmapTestBuffer->GenerateMipMaps(mipsContext);
	m_cloudShapeManager.GetBasicCloudShape()->GenerateMipMaps(mipsContext);

	mipsContext.Finish();*/
}

void VolumetricCloud::UpdateUI()
{
	g_CameraController.UpdateUI();
	m_cloudShapeManager.UpdateUI();

	ImGui::Begin("Volumetric Cloud Configuration");
	
	if (ImGui::BeginTabBar("##TabBar"))
	{
		if (ImGui::BeginTabItem("Cloud Setting"))
		{
			ImGui::Text("Sample Setting");
			ImGui::InputInt("Sample Count Min", &m_sampleCountMin);
			ImGui::InputInt("Sample Count Max", &m_sampleCountMax);

			ImGui::Text("Atmosphere Setting");
			ImGui::InputFloat("Extinct", &m_extinct, 0.0001f, 0.0f, "%.4f");
			ImGui::InputFloat("HG Coefficient", &m_hgCoeff, 0.01f);
			ImGui::InputFloat("Scatter Coefficient", &m_scatterCoeff, 0.001f);

			ImGui::Text("Sun Light");
			float rotate[3] = { m_sunLightRotation.GetX(), m_sunLightRotation.GetY(), m_sunLightRotation.GetZ() };
			ImGui::DragFloat3("Rotation", rotate, 0.05f, -FLT_MAX, FLT_MAX, "%.2f");
			ImGui::ColorEdit3("Light Color", m_cloudParameterCB.lightColor);
			m_sunLightRotation = Vector3(rotate);
			//Vector3 light_dir = Matrix3::MakeYRotation(ToRadian(m_sunLightRotation.GetY())) * Matrix3::MakeXRotation(ToRadian(m_sunLightRotation.GetX())) * Matrix3::MakeZRotation(ToRadian(m_sunLightRotation.GetZ())) * Vector3(0.0, 1.0, 0.0);
			ImGui::InputInt("Light Sample Count", &m_lightSampleCount);

			ImGui::Text("Raymarch Max Distance");
			ImGui::InputFloat("Far Distance", &m_farDistance, 100.0f);

			//ImGui::Text("Basic Cloud Shape Texture");
			ImGui::SetNextItemWidth(150.0f);
			const char* cloud_shape_textures[] = { "Perlin-Worley", "Perlin-Worley_UE", "Perlin-Worley-New" };
			static VolumeColorBuffer* cloud_shape_texture_ptrs[] = { m_cloudShapeManager.GetBasicCloudShape(), m_perlinWorleyUE.get(), m_perlinWorley.get() };
			static int cur_basic_shape = 0;
			ImGui::Combo("Basic Cloud Shape", &cur_basic_shape, cloud_shape_textures, IM_ARRAYSIZE(cloud_shape_textures));
			//SwitchBasicCloudShape(cur_basic_shape);
			m_basicCloudShape = cloud_shape_texture_ptrs[cur_basic_shape];
			static bool basic_cloud_shape_detail;
			static bool basic_cloud_shapw_window_opening;
			ImGui::SameLine(400.0);
			ImGui::PreviewVolumeImageButton(m_basicCloudShape, ImVec2(128.0f, 128.0f), "Basic Cloud Shape", &basic_cloud_shape_detail, &basic_cloud_shapw_window_opening);
			
			//ImGui::Text("Erosion Texture");
			ImGui::SetNextItemWidth(150.0f);
			const char* erosion_textures[] = { "Worley", "Erosion"};
			static VolumeColorBuffer* erosion_texture_ptrs[] = { m_worley.get(), m_cloudShapeManager.GetErosion() };
			static int cur_erosion = 0;
			ImGui::Combo("Erosion Texture", &cur_erosion, erosion_textures, IM_ARRAYSIZE(erosion_textures));
			m_erosionTexture = erosion_texture_ptrs[cur_erosion];
			static bool erosion_window = false;
			static bool erosion_window_open = false;
			ImGui::SameLine(400.0f);
			ImGui::PreviewVolumeImageButton(m_erosionTexture, ImVec2(128.0f, 128.0f), "Basic Cloud Shape", &basic_cloud_shape_detail, &basic_cloud_shapw_window_opening);

			ImGui::Text("Weather Texture");
			static bool weather_detail;
			static bool weather_window_opening;
			ImGui::SameLine(400.0f);
			ImGui::PreviewImageButton(m_weatherTexture, ImVec2(128.0f, 128.0f), "Weather Texture", &weather_detail, &weather_window_opening);

			ImGui::Text("Curl Noise");
			static bool curl_noise_window = false;
			static bool curl_noise_window_open = false;
			ImGui::SameLine(400.0f);
			ImGui::PreviewImageButton(m_curlNoise2D, ImVec2(128.0f, 128.0f), "Curl Noise", &curl_noise_window, &curl_noise_window_open);

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Compute Cloud Setting"))
		{
			ImGui::InputInt("Sample Count Min", &m_cloudParameterCB.sampleCountMin);
			ImGui::InputInt("Sample Count Max", &m_cloudParameterCB.sampleCountMax);
			ImGui::Separator();
			ImGui::Text("Cloud Rendering");
			ImGui::SliderFloat("Cloud Coverage", &m_cloudParameterCB.cloudCoverage, 0.0f, 1.0f);
			ImGui::SliderFloat("Cloud Speed", &m_cloudParameterCB.cloudSpeed, 0.0f, 5.0e3f);
			ImGui::SliderFloat("Crispiness", &m_cloudParameterCB.crispiness, 0.0f, 120.0f);
			ImGui::SliderFloat("Curliness", &m_cloudParameterCB.curliness, 0.0f, 3.0f);
			ImGui::SliderFloat("Density Factor", &m_cloudParameterCB.densityFactor, 0.0f, 0.1f);
			ImGui::DragFloat("Light Absorption", &m_cloudParameterCB.absorption, 0.0001f, 0.0f, 1.5f, "%.4f");
			ImGui::DragFloat("HG Coeff0", &m_cloudParameterCB.hg0, 0.0005f);
			ImGui::DragFloat("HG Coeff1", &m_cloudParameterCB.hg1, 0.0005f);
			ImGui::ColorEdit3("Cloud Bottom Color", m_cloudParameterCB.cloudBottomColor);
			ImGui::ColorEdit3("Light Color", m_cloudParameterCB.lightColor);
			bool enable_powder = (bool)m_cloudParameterCB.enablePowder;
			ImGui::Checkbox("Enable Powder", &enable_powder);
			m_cloudParameterCB.enablePowder = (int)enable_powder;
			static bool enable_beer = (bool)m_cloudParameterCB.enableBeer;
			ImGui::Checkbox("Enable Beer", &enable_beer);
			m_cloudParameterCB.enableBeer = enable_beer;
			ImGui::DragFloat("Rain Absorption", &m_cloudParameterCB.rainAbsorption, 0.01f, 0.01f, 20.0f);
			ImGui::DragFloat("Eccentricity", &m_cloudParameterCB.eccentricity, 0.001f, -FLT_MAX, FLT_MAX);
			ImGui::DragFloat("Sliver Intensity", &m_cloudParameterCB.sliverIntensity, 0.001f, -FLT_MAX, FLT_MAX);
			ImGui::DragFloat("Sliver Spread", &m_cloudParameterCB.sliverSpread, 0.001f, -FLT_MAX, FLT_MAX);
			ImGui::DragFloat("Brightness", &m_cloudParameterCB.brightness, 0.001f, -FLT_MAX, FLT_MAX);
			ImGui::Checkbox("Enable Temporal", &m_useTemporal);
			if (m_useTemporal)
				ImGui::Checkbox("Render To Quarter Buffer", &m_computeToQuarter);

			ImGui::Separator();
			ImGui::Text("Cloud Distribution");
			ImGui::SliderFloat("Earth Radius", &m_cloudParameterCB.earthRadius, 10000.0f, 5000000.0f);
			ImGui::SliderFloat("Cloud Bottom Altitude", &m_cloudParameterCB.cloudBottomRadius, 1000.0f, 15000.0f);
			ImGui::SliderFloat("Cloud Top Altitude", &m_cloudParameterCB.cloudTopRadius, 1000.0f, 40000.0f);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Atmosphere Setting"))
		{
			static float ground_albedo[3] = { 0.0f, 0.0f, 0.04f };
			ImGui::DragFloat3("Ground Albedo", ground_albedo);
			Vector3 g(ground_albedo);
			XMStoreFloat3(&m_passCB.groundAlbedo, g);
			ImGui::DragFloat("Exposure", &m_passCB.exposure);
			
			//ImGui::DragFloat3("White Point")
			//ImGui::DragFloat("Sun Size", &m_passCB.sunSize, 0.0000001f, 0.999f, 1.0f);

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void VolumetricCloud::DrawOnSkybox(const Timer& timer)
{
	GraphicsContext& graphicsContext = GraphicsContext::Begin();
	graphicsContext.TransitionResource(*m_sceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	graphicsContext.ClearColor(*m_sceneColorBuffer);
	graphicsContext.SetRenderTarget(m_sceneColorBuffer->GetRTV());
	graphicsContext.SetViewportAndScissor(m_screenViewport, m_scissorRect);

	{
		struct
		{
			Matrix4 modelMatrix;
			Matrix4 mvpMatrix;
		}skyboxConstants;
		skyboxConstants.modelMatrix = m_skyboxModelMatrix;
		skyboxConstants.mvpMatrix = m_camera->GetViewProjMatrix() * m_skyboxModelMatrix;

		__declspec(align(16)) struct
		{
			XMFLOAT3 viewerPos;
			float time;
			int sampleCountMin;
			int sampleCountMax;
			float extinct;
			float hgCoeff;
			float scatterCoeff;
			XMFLOAT3 lightDir;
			int lightSampleCount;
			XMFLOAT3 lightColor;
			float altitudeMin;
			float altitudeMax;
			float farDistance;
		}passConstants;
		XMStoreFloat3(&passConstants.viewerPos, m_camera->GetPosition());
		passConstants.time = timer.TotalTime();
		passConstants.sampleCountMin = m_sampleCountMin;
		passConstants.sampleCountMax = m_sampleCountMax;
		passConstants.extinct = m_extinct;
		passConstants.hgCoeff = m_hgCoeff;
		passConstants.scatterCoeff = m_scatterCoeff;
		auto dir = Quaternion(ToRadian(m_sunLightRotation.GetX()), ToRadian(m_sunLightRotation.GetY()), ToRadian(m_sunLightRotation.GetZ())) * Vector3(0.0f, 0.0f, 1.0f);
		XMStoreFloat3(&passConstants.lightDir, -dir);
		passConstants.lightSampleCount = m_lightSampleCount;
		XMStoreFloat3(&passConstants.lightColor, m_sunLightColor);
		passConstants.altitudeMin = m_cloudShapeManager.GetAltitudeMin();
		passConstants.altitudeMax = m_cloudShapeManager.GetAltitudeMax();
		passConstants.farDistance = m_farDistance;

		graphicsContext.SetRootSignature(m_skyboxRS);
		graphicsContext.SetPipelineState(m_skyboxPSO);
		graphicsContext.SetDynamicConstantBufferView(0, sizeof(skyboxConstants), &skyboxConstants);
		graphicsContext.SetDynamicConstantBufferView(1, sizeof(passConstants), &passConstants);
		graphicsContext.TransitionResource(*m_basicCloudShape, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		graphicsContext.SetDynamicDescriptor(2, 0, m_basicCloudShape->GetSRV());
		graphicsContext.TransitionResource(*m_cloudShapeManager.GetDensityHeightGradient(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		graphicsContext.SetDynamicDescriptor(2, 1, m_cloudShapeManager.GetDensityHeightGradient()->GetSRV());
		graphicsContext.TransitionResource(*m_cloudShapeManager.GetPerlinNoise(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		graphicsContext.SetDynamicDescriptor(2, 4, m_cloudShapeManager.GetPerlinNoise()->GetSRV());
		graphicsContext.SetDynamicDescriptor(2, 5, m_weatherTexture->GetSRV());
		graphicsContext.SetVertexBuffer(0, m_skyboxMesh->VertexBufferView());
		graphicsContext.SetIndexBuffer(m_skyboxMesh->IndexBufferView());
		graphicsContext.SetPrimitiveTopology(m_skyboxMesh->Topology());
		graphicsContext.DrawIndexed(m_skyboxMesh->IndexCount());
	}
	graphicsContext.Finish();
}

void VolumetricCloud::DrawOnQuad(const Timer& timer)
{
	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_computeCloudOnQuadRS);
	context.TransitionResource(*m_basicCloudShape, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_worley, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*const_cast<Texture2D*>(m_weatherTexture), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_sceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	if (m_useTemporal)
	{
		context.TransitionResource(*m_cloudTempBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.SetPipelineState(m_temporalCloudPSO);
		context.SetDynamicConstantBufferView(0, sizeof(m_passCB), &m_passCB);
		context.SetDynamicDescriptor(1, 0, m_basicCloudShape->GetSRV());
		context.SetDynamicDescriptor(1, 1, m_erosionTexture->GetSRV());
		context.SetDynamicDescriptor(1, 2, m_weatherTexture->GetSRV());
		context.SetDynamicDescriptor(1, 3, m_cloudTempBuffer->GetSRV());
		context.SetDynamicDescriptor(1, 4, m_curlNoise2D->GetSRV());
		context.SetDynamicDescriptor(1, 5, Atmosphere::GetTransmittance()->GetSRV());
		context.SetDynamicDescriptor(1, 6, Atmosphere::GetScattering()->GetSRV());
		context.SetDynamicDescriptor(1, 7, Atmosphere::GetIrradiance()->GetSRV());
		if (!Atmosphere::UseCombinedScatteringTexture())
			context.SetDynamicDescriptor(1, 8, Atmosphere::GetOptionalScattering()->GetSRV());
		context.SetDynamicDescriptor(2, 0, m_sceneColorBuffer->GetUAV());
		context.SetDynamicConstantBufferView(3, sizeof(Atmosphere::AtmosphereCB), Atmosphere::GetAtmosphereCB());
		context.SetDynamicConstantBufferView(4, sizeof(m_cloudParameterCB), &m_cloudParameterCB);
		context.Dispatch2D(m_sceneColorBuffer->GetWidth(), m_sceneColorBuffer->GetHeight());

		context.TransitionResource(*m_cloudTempBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
		context.TransitionResource(*m_sceneColorBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		context.CopySubresource(*m_cloudTempBuffer, 0, *m_sceneColorBuffer, 0);
	}
	else
	{
		context.SetPipelineState(m_computeCloudOnQuadPSO);
		//context.TransitionResource(*m_cloudShapeManager.GetErosion(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		context.SetDynamicConstantBufferView(0, sizeof(m_passCB), &m_passCB);
		context.SetDynamicDescriptor(1, 0, m_basicCloudShape->GetSRV());
		//context.SetDynamicDescriptor(1, 1, m_cloudShapeManager.GetErosion()->GetSRV());
		context.SetDynamicDescriptor(1, 1, m_erosionTexture->GetSRV());
		context.SetDynamicDescriptor(1, 2, m_weatherTexture->GetSRV());
		context.SetDynamicDescriptor(2, 0, m_sceneColorBuffer->GetUAV());
		context.SetDynamicConstantBufferView(4, sizeof(m_cloudParameterCB), &m_cloudParameterCB);
		context.Dispatch2D(m_sceneColorBuffer->GetWidth(), m_sceneColorBuffer->GetHeight());
	}
	context.Finish();
}

void VolumetricCloud::OnResize()
{
	App::OnResize();
	m_quarterBuffer->Destroy();
	m_quarterBuffer->Create(L"Quarter Buffer", m_clientWidth / 4, m_clientHeight / 4, 1, m_sceneBufferFormat);
	m_cloudTempBuffer->Destroy();
	m_cloudTempBuffer->Create(L"Cloud Temp Buffer", m_clientWidth, m_clientHeight, 1, m_sceneBufferFormat);
	m_mipmapTestBuffer->Destroy();
	m_mipmapTestBuffer->Create(L"Generate Mips Buffer", m_clientWidth, m_clientHeight, 0, m_sceneBufferFormat);
}