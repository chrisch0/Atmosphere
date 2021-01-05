#include "stdafx.h"
#include "VolumetricCloud.h"
#include "D3D12/GraphicsGlobal.h"
#include "D3D12/CommandContext.h"
#include "Utils/CameraController.h"
#include "Utils/Camera.h"
#include "Mesh/Mesh.h"

#include "CompiledShaders/VolumetricCloud_VS.h"
#include "CompiledShaders/VolumetricCloud_PS.h"
#include "CompiledShaders/Skybox_VS.h"
#include "CompiledShaders/Skybox_PS.h"

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

	CreatePSO();
	CreateMeshes();
	CreateCamera();

	m_position = Vector3(0.0f, 0.0f, 0.0f);
	m_rotation = Vector3(0.0f, 0.0f, 0.0f);
	m_scale = Vector3(1.0f, 1.0f, 1.0f);

	return true;
}

void VolumetricCloud::CreatePSO()
{
	m_volumetricCloudRS.Reset(3, 1);
	m_volumetricCloudRS[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_volumetricCloudRS[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_volumetricCloudRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_volumetricCloudRS.InitStaticSampler(0, SamplerLinearWrapDesc);
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
	m_volumetricCloudPSO.SetRenderTargetFormat(m_backBufferFormat, DXGI_FORMAT_UNKNOWN);
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
	GraphicsContext& graphicsContext = GraphicsContext::Begin();
	graphicsContext.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_displayBuffer[m_currBackBuffer].SetClearColor({0.0f, 0.0f, 0.0f, 1.0f});
	graphicsContext.ClearColor(m_displayBuffer[m_currBackBuffer]);
	graphicsContext.SetRenderTarget(m_displayBuffer[m_currBackBuffer].GetRTV());
	graphicsContext.SetViewportAndScissor(m_screenViewport, m_scissorRect);

	{
		struct
		{
			Matrix4 modelMatrix;
			Matrix4 mvpMatrix;
		}skyboxConstants;
		skyboxConstants.modelMatrix = m_skyboxModelMatrix;
		skyboxConstants.mvpMatrix = m_camera->GetViewProjMatrix() * m_skyboxModelMatrix;

		struct  
		{
			Vector3 viewerPos;
			float time;
		}passConstants;
		passConstants.viewerPos = m_camera->GetPosition();
		passConstants.time = timer.TotalTime();

		graphicsContext.SetRootSignature(m_skyboxRS);
		graphicsContext.SetPipelineState(m_skyboxPSO);
		graphicsContext.SetDynamicConstantBufferView(0, sizeof(skyboxConstants), &skyboxConstants);
		graphicsContext.SetDynamicConstantBufferView(1, sizeof(passConstants), &passConstants);
		graphicsContext.TransitionResource(*m_cloudShapeManager.GetBasicCloudShape(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		graphicsContext.SetDynamicDescriptor(2, 0, m_cloudShapeManager.GetBasicCloudShape()->GetSRV());
		graphicsContext.SetVertexBuffer(0, m_skyboxMesh->VertexBufferView());
		graphicsContext.SetIndexBuffer(m_skyboxMesh->IndexBufferView());
		graphicsContext.SetPrimitiveTopology(m_skyboxMesh->Topology());
		graphicsContext.DrawIndexed(m_skyboxMesh->IndexCount());
	}

	/*{
		struct
		{
			Matrix4 mvpMatrix;
			Matrix4 modelMatrix;
			Vector3 modelScale;
			Vector3 viewerPos;
		} objectConstants;

		struct
		{
			Matrix4 viewMatrix;
			Matrix4 projMatrix;
			Matrix4 viewProjMatrix;
			Matrix4 invViewMatrix;
			Matrix4 invProjMatrix;
			Matrix4 invViewProjMatrix;
			Vector3 viewerPos;
			float time;
		} passConstants;

		objectConstants.mvpMatrix = m_camera->GetViewProjMatrix() * m_modelMatrix;
		objectConstants.modelMatrix = m_modelMatrix;
		objectConstants.modelScale = m_scale;
		objectConstants.viewerPos = m_camera->GetPosition();

		passConstants.viewMatrix = m_camera->GetViewMatrix();
		passConstants.projMatrix = m_camera->GetProjMatrix();
		passConstants.viewProjMatrix = m_camera->GetViewProjMatrix();
		passConstants.invViewMatrix = Invert(m_camera->GetViewMatrix());
		passConstants.invProjMatrix = Invert(m_camera->GetProjMatrix());
		passConstants.invViewProjMatrix = Invert(m_camera->GetViewProjMatrix());
		passConstants.viewerPos = m_camera->GetPosition();
		passConstants.time = timer.TotalTime();
		graphicsContext.SetRootSignature(m_volumetricCloudRS);
		graphicsContext.SetPipelineState(m_volumetricCloudPSO);
		graphicsContext.SetDynamicConstantBufferView(0, sizeof(objectConstants), &objectConstants);
		graphicsContext.SetDynamicConstantBufferView(1, sizeof(passConstants), &passConstants);
		graphicsContext.TransitionResource(*m_cloudShapeManager.GetBasicCloudShape(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		graphicsContext.TransitionResource(*m_cloudShapeManager.GetStratusGradient(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		graphicsContext.TransitionResource(*m_cloudShapeManager.GetCumulusGradinet(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		graphicsContext.TransitionResource(*m_cloudShapeManager.GetCumulonimbusGradient(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		graphicsContext.SetDynamicDescriptor(2, 0, m_cloudShapeManager.GetBasicCloudShape()->GetSRV());
		graphicsContext.SetDynamicDescriptor(2, 1, m_cloudShapeManager.GetStratusGradient()->GetSRV());
		graphicsContext.SetDynamicDescriptor(2, 2, m_cloudShapeManager.GetCumulusGradinet()->GetSRV());
		graphicsContext.SetDynamicDescriptor(2, 3, m_cloudShapeManager.GetCumulonimbusGradient()->GetSRV());
		graphicsContext.SetVertexBuffer(0, m_boxMesh->VertexBufferView());
		graphicsContext.SetIndexBuffer(m_boxMesh->IndexBufferView());
		graphicsContext.SetPrimitiveTopology(m_boxMesh->Topology());
		graphicsContext.DrawIndexed(m_boxMesh->IndexCount());
	}*/
	graphicsContext.Finish();
}

void VolumetricCloud::UpdateUI()
{
	g_CameraController.UpdateUI();
	m_cloudShapeManager.UpdateUI();

	ImGui::Begin("Volumetric Cloud Configuration");
	
	if (ImGui::BeginTabBar("##TabBar"))
	{
		if (ImGui::BeginTabItem("Box"))
		{
			float pos[3] = { m_position.GetX(), m_position.GetY(), m_position.GetZ() };
			float scale[3] = { m_scale.GetX(), m_scale.GetY(), m_scale.GetZ() };
			float rotate[3] = { m_rotation.GetX(), m_rotation.GetY(), m_rotation.GetZ() };

			ImGui::DragFloat3("Position", pos, 0.05f, -FLT_MAX, FLT_MAX, "%.2f");
			ImGui::DragFloat3("Rotation", rotate, 0.05f, -FLT_MAX, FLT_MAX, "%.2f");
			ImGui::DragFloat3("Scale", scale, 0.05f, 0.0f, FLT_MAX, "%.2f");

			m_position = Vector3(pos);
			m_scale = Vector3(scale);
			m_rotation = Vector3(rotate);

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}