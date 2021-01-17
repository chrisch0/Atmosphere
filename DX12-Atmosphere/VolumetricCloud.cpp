#include "stdafx.h"
#include "VolumetricCloud.h"
#include "D3D12/GraphicsGlobal.h"
#include "D3D12/CommandContext.h"
#include "D3D12/TextureManager.h"
#include "D3D12/ColorBuffer.h"
#include "Utils/CameraController.h"
#include "Utils/Camera.h"
#include "Mesh/Mesh.h"

#include "CompiledShaders/VolumetricCloud_VS.h"
#include "CompiledShaders/VolumetricCloud_PS.h"
#include "CompiledShaders/Skybox_VS.h"
#include "CompiledShaders/Skybox_PS.h"
#include "CompiledShaders/ComputeCloud_CS.h"

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

	m_position = Vector3(0.0f, 0.0f, 0.0f);
	m_rotation = Vector3(0.0f, 0.0f, 0.0f);
	m_scale = Vector3(1.0f, 1.0f, 1.0f);

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
	auto* curl_2d = TextureManager::LoadDDSFromFile("CurlNoise_Volume_16by8.DDS");
	m_curlNoiseTexture = std::make_shared<VolumeColorBuffer>();
	m_curlNoiseTexture->CreateFromTexture2D(L"CurlVolumeNoise", curl_2d, 16, 8, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	auto* perlin_worley = TextureManager::LoadDDSFromFile("PerlinWorley.DDS");
	m_perlinWorleyUE = std::make_shared<VolumeColorBuffer>();
	m_perlinWorleyUE->CreateFromTexture2D(L"PerlinWorleyUE", perlin_worley, 16, 8, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	//m_erosionTexture = TextureManager::LoadTGAFromFile("volume_test.TGA", 8, 2);
	//m_noiseShapeTexture = TextureManager::LoadTGAFromFile("CloudWeatherTexture.tga", 8, 8);

}

void VolumetricCloud::CreatePSO()
{
	m_volumetricCloudRS.Reset(3, 1);
	m_volumetricCloudRS[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_volumetricCloudRS[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_volumetricCloudRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_volumetricCloudRS.InitStaticSampler(0, SamplerLinearMirrorDesc);
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

	m_computeCloudOnQuadRS.Reset(3, 1);
	m_computeCloudOnQuadRS[0].InitAsConstantBufferView(0);
	m_computeCloudOnQuadRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_computeCloudOnQuadRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
	m_computeCloudOnQuadRS.InitStaticSampler(0, SamplerLinearWrapDesc);
	m_computeCloudOnQuadRS.Finalize(L"ComputeCloudOnQuadRS");

	m_computeCloudOnQuadPSO.SetRootSignature(m_computeCloudOnQuadRS);
	m_computeCloudOnQuadPSO.SetComputeShader(g_pComputeCloud_CS, sizeof(g_pComputeCloud_CS));
	m_computeCloudOnQuadPSO.Finalize();
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
	m_camera->SetLookAt({ 2.25f, 1.75f, 2.10f }, { 0, 0, 0 }, { 0, 1, 0 });
	m_camera->SetNearClip(0.01f);
	m_camera->SetFarClip(10000.0f);
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
}

void VolumetricCloud::Draw(const Timer& timer)
{
	DrawOnQuad(timer);
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
			float color[3] = { m_sunLightColor.GetX(), m_sunLightColor.GetY(), m_sunLightColor.GetZ() };
			ImGui::DragFloat3("Rotation", rotate, 0.05f, -FLT_MAX, FLT_MAX, "%.2f");
			ImGui::ColorEdit3("Light Color", color);
			m_sunLightRotation = Vector3(rotate);
			m_sunLightColor = Vector3(color);
			ImGui::InputInt("Light Sample Count", &m_lightSampleCount);

			ImGui::Text("Raymarch Max Distance");
			ImGui::InputFloat("Far Distance", &m_farDistance, 100.0f);

			ImGui::Text("Basic Cloud Shape Texture");
			ImGui::SetNextItemWidth(150.0f);
			const char* cloud_shape_texture[] = { "Perlin-Worley", "Perlin-Worley_UE" };
			static int cur_basic_shape = 0;
			ImGui::Combo("Basic Cloud Shape", &cur_basic_shape, cloud_shape_texture, IM_ARRAYSIZE(cloud_shape_texture));
			SwitchBasicCloudShape(cur_basic_shape);
			static bool basic_cloud_shape_detail;
			static bool basic_cloud_shapw_window_opening;
			ImGui::SameLine(400.0);
			ImGui::PreviewVolumeImageButton(m_basicCloudShape, ImVec2(128.0f, 128.0f), "Basic Cloud Shape", &basic_cloud_shape_detail, &basic_cloud_shapw_window_opening);
			
			ImGui::Text("Erosion Texture");
			static bool erosion_window = false;
			static bool erosion_window_open = false;
			

			ImGui::Text("Weather Texture");
			static bool weather_detail;
			static bool weather_window_opening;
			ImGui::SameLine(400.0f);
			ImGui::PreviewImageButton(m_weatherTexture, ImVec2(128.0f, 128.0f), "Weather Texture", &weather_detail, &weather_window_opening);

			ImGui::Text("Curl Noise");
			static bool curl_noise_window = false;
			static bool curl_noise_window_open = false;
			ImGui::SameLine(400.0f);
			ImGui::PreviewVolumeImageButton(m_curlNoiseTexture.get(), ImVec2(128.0f, 128.0f), "Curl Noise", &curl_noise_window, &curl_noise_window_open);

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
	m_cloudOnQuadCB.invView = Invert(m_camera->GetViewMatrix());
	m_cloudOnQuadCB.invProj = Invert(m_camera->GetProjMatrix());
	auto dir = Quaternion(ToRadian(m_sunLightRotation.GetX()), ToRadian(m_sunLightRotation.GetY()), ToRadian(m_sunLightRotation.GetZ())) * Vector3(0.0f, 0.0f, 1.0f);
	XMStoreFloat3(&m_cloudOnQuadCB.lightDir, -dir);
	XMStoreFloat3(&m_cloudOnQuadCB.cameraPosition, m_camera->GetPosition());
	Vector4 res{ (float)m_sceneColorBuffer->GetWidth(), (float)m_sceneColorBuffer->GetHeight(), 1.0f / m_sceneColorBuffer->GetWidth(), 1.0f / m_sceneColorBuffer->GetHeight() };
	XMStoreFloat4(&m_cloudOnQuadCB.resolution, res);

	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_computeCloudOnQuadRS);
	context.SetPipelineState(m_computeCloudOnQuadPSO);
	context.TransitionResource(*m_basicCloudShape, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_cloudShapeManager.GetErosion(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*const_cast<Texture2D*>(m_weatherTexture), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_sceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicConstantBufferView(0, sizeof(m_cloudOnQuadCB), &m_cloudOnQuadCB);
	context.SetDynamicDescriptor(1, 0, m_basicCloudShape->GetSRV());
	context.SetDynamicDescriptor(1, 1, m_cloudShapeManager.GetErosion()->GetSRV());
	context.SetDynamicDescriptor(1, 2, m_weatherTexture->GetSRV());
	context.SetDynamicDescriptor(2, 0, m_sceneColorBuffer->GetUAV());
	context.Dispatch2D(m_sceneColorBuffer->GetWidth(), m_sceneColorBuffer->GetHeight());
	context.Finish();
}