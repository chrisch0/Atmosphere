#pragma once
#include "stdafx.h"
#include "Math/VectorMath.h"
#include "Math/Frustum.h"

class Camera
{
public:
	Camera()
		: m_cameraToWorld(kIdentity), m_basis(kIdentity), m_isPerspective(true), m_isReverseZ(true)
	{

	}

	void Update();

	void SetLookAt(const Vector3& eye, const Vector3& lookAt, const Vector3& up);
	void SetLookDirection(const Vector3& forward, const Vector3& up);
	void SetRotation(const Quaternion& basisRotation);
	void SetPosition(const Vector3& worldPos);
	void SetTransform(const AffineTransform& xform);

	void SetPrespectiveMatrix(float verticalFOV, float aspectRatio, float near, float far);
	void SetFOV(float verticalFOV) { m_verticalFOV = verticalFOV; }
	void SetAspectRatio(float heightOverWidth) { m_aspectRatio = heightOverWidth; }
	void SetAspectRatio(float height, float width) { m_aspectRatio = height / width; }
	void SetNearClip(float nearClip) { m_nearClip = nearClip; }
	void SetFarClip(float farClip) { m_farClip = farClip; }
	void ReverseZ(bool enable) { m_isReverseZ = enable; UpdateProjMatrix(); }

	const Quaternion GetRotation() const { return m_cameraToWorld.GetRotation(); }
	const Vector3 GetRight() const { return m_basis.GetX(); }
	const Vector3 GetUp() const { return m_basis.GetY(); }
	const Vector3 GetForward() const { return -m_basis.GetZ(); }
	const Vector3 GetPosition() const { return m_cameraToWorld.GetTranslation(); }

	const Matrix4& GetViewMatrix() const { return m_viewMatrix; }
	const Matrix4& GetProjMatrix() const { return m_projMatrix; }
	const Matrix4& GetViewProjMatrix() const { return m_viewProjMatrix; }
	const Matrix4& GetReprojectionMatrix() const { return m_reproMatrix; }
	const Frustum& GetViewSpaceFrustum() const { return m_frustumVS; }
	const Frustum& GetWorldSpaceFrustum() const { return m_frustumWS; }

	float GetFOV() const { return m_verticalFOV; }
	float GetAspectRatio() const { return m_aspectRatio; }
	float GetNearClip() const { return m_nearClip; }
	float GetFarClip() const { return m_farClip; }


protected:
	void UpdateProjMatrix();

private:
	bool m_isPerspective;
	// store forward, up, right
	Matrix3 m_basis;
	OrthogonalTransform m_cameraToWorld;
	// view space defined as +X right, +Y up, -Z forward
	Matrix4 m_viewMatrix;
	Matrix4 m_projMatrix;
	Matrix4 m_viewProjMatrix;
	Matrix4 m_previousViewProjMatrix;
	Matrix4 m_reproMatrix;
	
	Frustum m_frustumVS;
	Frustum m_frustumWS;

	float m_verticalFOV;
	float m_aspectRatio;
	float m_nearClip;
	float m_farClip;

	// if true, z = 0 is the far plane
	bool m_isReverseZ;
};

