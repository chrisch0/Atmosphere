#pragma once
#include "stdafx.h"
#include "Math/VectorMath.h"
#include "Math/Frustum.h"
#include "CameraController.h"

class Camera
{
	friend class CameraController;
public:
	Camera(const std::string& name);
	virtual void Update(float deltaTime);
	void UpdateMatrixAndFrustum();

public:
	void SetLookAt(const Vector3& eye, const Vector3& lookAt, const Vector3& up);
	void SetLookDirection(const Vector3& forward, const Vector3& up);
	void SetRotation(const Quaternion& basisRotation);
	void SetPosition(const Vector3& worldPos);
	void SetTransform(const AffineTransform& xform);

	void SetPrespectiveMatrix(float verticalFOV, float aspectRatio, float nearClip, float farClip);
	void SetFOV(float verticalFOV) { m_verticalFOV = verticalFOV; UpdateProjMatrix(); }
	void SetAspectRatio(float heightOverWidth) { m_aspectRatio = heightOverWidth; UpdateProjMatrix(); }
	void SetAspectRatio(float height, float width) { m_frustumWidth = width; m_frustumHeight = height; m_aspectRatio = height / width; UpdateProjMatrix(); }
	void SetNearClip(float nearClip) { m_nearClip = nearClip; UpdateProjMatrix(); }
	void SetFarClip(float farClip) { m_farClip = farClip; UpdateProjMatrix(); }
	void ReverseZ(bool enable) { m_isReverseZ = enable; UpdateProjMatrix(); }

	const Quaternion GetRotation() const { return m_cameraToWorld.GetRotation(); }
	const Vector3 GetRight() const { return m_basis.GetX(); }
	const Vector3 GetUp() const { return m_basis.GetY(); }
	const Vector3 GetForward() const { return -m_basis.GetZ(); }
	const Vector3 GetPosition() const { return m_cameraToWorld.GetTranslation(); }

	const Matrix4& GetViewMatrix() const { return m_viewMatrix; }
	const Matrix4& GetProjMatrix() const { return m_projMatrix; }
	const Matrix4& GetViewProjMatrix() const { return m_viewProjMatrix; }
	const Matrix4& GetPrevViewProjMatrix() const { return m_previousViewProjMatrix; }
	const Matrix4& GetReprojectionMatrix() const { return m_reprojectMatrix; }
	const Frustum& GetViewSpaceFrustum() const { return m_frustumVS; }
	const Frustum& GetWorldSpaceFrustum() const { return m_frustumWS; }

	float GetFOV() const { return m_verticalFOV; }
	float GetAspectRatio() const { return m_aspectRatio; }
	float GetNearClip() const { return m_nearClip; }
	float GetFarClip() const { return m_farClip; }

	const std::string& GetName() const { return m_name; }

protected:
	void UpdateProjMatrix();

	void SetProjMatrix(const Matrix4& projMat) { m_projMatrix = projMat; }

	virtual void UpdateUI();

	void ApplyMomentum(float& oldValue, float& newValue, float deltaTime);

	static Vector3 s_worldUp;
	static Vector3 s_worldNorth;
	static Vector3 s_worldEast;

	std::string m_name;
	bool m_isPerspective;
	// store forward, up, right
	Matrix3 m_basis;
	OrthogonalTransform m_cameraToWorld;
	// view space defined as +X right, +Y up, -Z forward
	Matrix4 m_viewMatrix;
	Matrix4 m_projMatrix;
	Matrix4 m_viewProjMatrix;
	Matrix4 m_previousViewProjMatrix;
	// Projects a clip-space coordinate to the previous frame (useful for temporal effects).
	Matrix4 m_reprojectMatrix;
	
	Frustum m_frustumVS;
	Frustum m_frustumWS;

	float m_verticalFOV;
	float m_frustumWidth;
	float m_frustumHeight;
	float m_aspectRatio;
	float m_nearClip;
	float m_farClip;

	// if true, z = 0 is the far plane
	bool m_isReverseZ;

	float m_moveSpeed;
	float m_horizontalSensitivity;
	float m_verticalSensitivity;

	float m_currentHeading;
	float m_currentPitch;
	float m_currentRoll;

	bool  m_momentum;

	float m_lastYaw;
	float m_lastPitch;
	float m_lastRoll;
	float m_lastForward;
	float m_lastStrafe;
	float m_lastAscent;
};

class SceneCamera : public Camera
{
	friend class CameraController;
public:
	SceneCamera(const std::string& name)
		: Camera(name)
	{

	}

	virtual void Update(float deltaTime) override;

};

inline void Camera::SetLookAt(const Vector3& eye, const Vector3& lookAt, const Vector3& up)
{
	SetLookDirection(lookAt - eye, up);
	SetPosition(eye);
	Vector3 forward = Normalize(Cross(s_worldUp, GetRight()));
	m_currentHeading = ToDegree(ATan2(-Dot(forward, s_worldEast), Dot(forward, s_worldNorth)));
	m_currentPitch = ToDegree(Sin(Dot(GetForward(), s_worldUp)));
}

inline void Camera::SetPosition(const Vector3& worldPos)
{
	m_cameraToWorld.SetTranslation(worldPos);
}

inline void Camera::SetTransform(const AffineTransform& xform)
{
	SetLookDirection(-xform.GetZ(), xform.GetY());
	SetPosition(xform.GetTranslation());
}

inline void Camera::SetRotation(const Quaternion& basisRotation)
{
	m_cameraToWorld.SetRotation(Normalize(basisRotation));
	m_basis = Matrix3(m_cameraToWorld.GetRotation());
}

inline void Camera::SetPrespectiveMatrix(float verticalFOV, float aspectRatio, float nearClip, float farClip)
{
	m_verticalFOV = verticalFOV;
	m_aspectRatio = aspectRatio;
	m_nearClip = nearClip;
	m_farClip = farClip;

	UpdateProjMatrix();

	m_previousViewProjMatrix = m_viewProjMatrix;
}