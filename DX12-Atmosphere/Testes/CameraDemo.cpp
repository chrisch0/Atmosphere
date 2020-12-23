#include "stdafx.h"
#include "CameraDemo.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"
#include "Utils/Camera.h"
#include "Utils/CameraController.h"
#include "D3D12/CommandContext.h"

#include "CompiledShaders/CameraDemo_VS.h"
#include "CompiledShaders/CameraDemo_PS.h"

using namespace Microsoft::WRL;
using namespace DirectX;

CameraDemo::CameraDemo()
{

}

CameraDemo::~CameraDemo()
{

}

bool CameraDemo::Initialize()
{
	if (!App::Initialize())
		return false;

	CreatePSO();
	CreateGeometry();

	m_camera = CameraController::CreateCamera<SceneCamera>("Scene Camera");
	m_camera->SetLookAt({ 3,5,3 }, { 0,0,0 }, { 0,1,0 });
	m_camera->SetNearClip(1.0f);
	m_camera->SetFarClip(10000.0f);
	m_position = Vector3(0.0, 0.0, 0.0);
	m_rotation = Vector3(0.0, 0.0, 0.0);
	m_scale = Vector3(1.0, 1.0, 1.0);
	m_color = Vector4(1.0, 1.0, 1.0, 1.0);

	m_showAnotherDemoWindow = false;
	m_showDemoWindow = false;
	m_showHelloWindow = false;

	return true;
}

void CameraDemo::CreatePSO()
{
	m_RS.Reset(2, 0);
	m_RS[0].InitAsConstantBufferView(0);
	m_RS[1].InitAsConstantBufferView(1);
	m_RS.Finalize(L"CameraDemoRS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	D3D12_INPUT_ELEMENT_DESC layout[] = 
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	auto depth_stencil_disable = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depth_stencil_disable.DepthEnable = FALSE;
	depth_stencil_disable.StencilEnable = FALSE;

	auto blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0] = transparencyBlendDesc;

	m_PSO.SetRootSignature(m_RS);
	m_PSO.SetSampleMask(UINT_MAX);
	m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_PSO.SetRenderTargetFormat(m_backBufferFormat, DXGI_FORMAT_UNKNOWN);
	m_PSO.SetVertexShader(g_pCameraDemo_VS, sizeof(g_pCameraDemo_VS));
	m_PSO.SetPixelShader(g_pCameraDemo_PS, sizeof(g_pCameraDemo_PS));
	m_PSO.SetInputLayout(_countof(layout), layout);
	m_PSO.SetBlendState(blendDesc);
	m_PSO.SetDepthStencilState(depth_stencil_disable);
	m_PSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_PSO.Finalize();
}

void CameraDemo::CreateGeometry()
{
	float box_verts[] =
	{
		// position       normal          tangent        uv        color
		-0.5, -0.5, -0.5, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0,
		-0.5, +0.5, -0.5, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
		+0.5, +0.5, -0.5, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0,
		+0.5, -0.5, -0.5, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0,

		-0.5, -0.5, +0.5, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0,
		+0.5, -0.5, +0.5, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0,
		+0.5, +0.5, +0.5, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
		-0.5, +0.5, +0.5, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0,

		-0.5, +0.5, -0.5, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0,
		-0.5, +0.5, +0.5, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,
		+0.5, +0.5, +0.5, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
		+0.5, +0.5, -0.5, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0,

		-0.5, -0.5, -0.5, 0.0, -1.0, 0.0, -1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0,
		+0.5, -0.5, -0.5, 0.0, -1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0,
		+0.5, -0.5, +0.5, 0.0, -1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0,
		-0.5, -0.5, +0.5, 0.0, -1.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0,

		-0.5, -0.5, +0.5, -1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 1.0, 0.0, 1.0, 1.0,
		-0.5, +0.5, +0.5, -1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 1.0, 1.0,
		-0.5, +0.5, -0.5, -1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 1.0, 1.0,
		-0.5, -0.5, -0.5, -1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 1.0, 1.0, 0.0, 1.0, 1.0,

		+0.5, -0.5, -0.5, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0,
		+0.5, +0.5, -0.5, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0,
		+0.5, +0.5, +0.5, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0,
		+0.5, -0.5, +0.5, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
	};

	m_vertexBuffer.Create(L"BoxVertexBuffer", 24, sizeof(float) * 14, box_verts);

	uint16_t box_indices[] =
	{
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11,
		12, 13, 14, 12, 14, 15,
		16, 17, 18, 16, 18, 19,
		20, 21, 22, 20, 22, 23
	};
	m_indexBuffer.Create(L"BoxIndexBuffer", 36, sizeof(uint16_t), box_indices);
}

void CameraDemo::Update(const Timer& timer)
{
	g_CameraController.Update(timer.DeltaTime());
	
	Matrix3 scale_rotation = Matrix3::MakeScale(m_scale) *
		Matrix3::MakeYRotation(ToRadian(m_rotation.GetY())) * Matrix3::MakeXRotation(ToRadian(m_rotation.GetX())) * Matrix3::MakeZRotation(ToRadian(m_rotation.GetZ()));
	AffineTransform trans(scale_rotation, m_position);
	m_modelMatrix = Matrix4(trans);
}

void CameraDemo::UpdateUI()
{
	g_CameraController.UpdateUI();

	ImGui::Begin("Box Setting");
	
	float pos[3] = { m_position.GetX(), m_position.GetY(), m_position.GetZ() };
	float scale[3] = { m_scale.GetX(), m_scale.GetY(), m_scale.GetZ() };
	float rotate[3] = { m_rotation.GetX(), m_rotation.GetY(), m_rotation.GetZ() };
	float color[4] = { m_color.GetX(), m_color.GetY(), m_color.GetZ(), m_color.GetW() };

	ImGui::DragFloat3("Position", pos, 0.05f, -FLT_MAX, FLT_MAX, "%.2f");
	ImGui::DragFloat3("Rotation", rotate, 0.05f, -FLT_MAX, FLT_MAX, "%.2f");
	ImGui::DragFloat3("Scale", scale, 0.05f, 0.0f, FLT_MAX, "%.2f");
	ImGui::ColorEdit4("Color", color);

	m_position = Vector3(pos);
	m_scale = Vector3(scale);
	m_rotation = Vector3(rotate);
	m_color = Vector4(color);

	ImGui::End();
}

struct PassConstant
{
	Matrix4 ViewMatrix;
	Matrix4 ProjMatrix;
	Matrix4 ViewProjMatrix;
	Matrix4 InvViewMatrix;
	Matrix4 InvProjMatrix;
	Matrix4 InvViewProjMatrix;
};

struct ObjectConstant
{
	Matrix4 ModelMatrix;
	Vector4 ModelColor;
};

void CameraDemo::Draw(const Timer& timer)
{
	GraphicsContext& context = GraphicsContext::Begin(L"CameraDemo");

	
	PassConstant passCB;
	passCB.ViewMatrix = m_camera->GetViewMatrix();
	passCB.ProjMatrix = m_camera->GetProjMatrix();
	passCB.ViewProjMatrix = m_camera->GetViewProjMatrix();
	passCB.InvViewMatrix = Invert(m_camera->GetViewMatrix());
	passCB.InvProjMatrix = Invert(m_camera->GetProjMatrix());
	passCB.InvViewProjMatrix = Invert(m_camera->GetViewProjMatrix());

	

	ObjectConstant objCB;
	objCB.ModelMatrix = m_modelMatrix;
	objCB.ModelColor = m_color;

	// Clear back buffer
	context.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_displayBuffer[m_currBackBuffer].SetClearColor(m_clearColor);
	context.ClearColor(m_displayBuffer[m_currBackBuffer]);

	context.SetRenderTarget(m_displayBuffer[m_currBackBuffer].GetRTV());
	context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.SetPipelineState(m_PSO);
	context.SetRootSignature(m_RS);
	context.SetDynamicConstantBufferView(0, sizeof(objCB), &objCB);
	context.SetDynamicConstantBufferView(1, sizeof(passCB), &passCB);
	context.SetViewportAndScissor(m_screenViewport, m_scissorRect);
	context.SetVertexBuffer(0, m_vertexBuffer.VertexBufferView());
	context.SetIndexBuffer(m_indexBuffer.IndexBufferView());
	context.DrawIndexed(36, 0, 0);

	context.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_COMMON);
	context.Finish();
}