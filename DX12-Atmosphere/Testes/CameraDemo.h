#pragma once
#include "stdafx.h"
#include "App/App.h"
#include "D3D12/GpuBuffer.h"
#include "Math/VectorMath.h"

class RootSignature;
class GraphicsPSO;
class Camera;

class CameraDemo : public App
{
public:
	CameraDemo();
	~CameraDemo();

	bool Initialize();
	void Draw(const Timer& timer) override;
	void Update(const Timer& timer) override;
	void UpdateUI() override;

private:
	void CreatePSO();
	void CreateGeometry();

	RootSignature m_RS;
	GraphicsPSO m_PSO;

	StructuredBuffer m_vertexBuffer;
	ByteAddressBuffer m_indexBuffer;

	std::shared_ptr<Camera> m_camera;

	Vector3 m_position;
	Vector3 m_scale;
	Vector3 m_rotation;
	Vector4 m_color;
	Matrix4 m_modelMatrix;
};