#include "stdafx.h"
#include "Camera.h"
#include "imgui/imgui_internal.h"

Vector3 Camera::s_worldUp = Vector3(kYUnitVector);
Vector3 Camera::s_worldNorth = -Vector3(kZUnitVector);
Vector3 Camera::s_worldEast = Vector3(kXUnitVector);

void Camera::UpdateUI()
{
	if (ImGui::CollapsingHeader(m_name.c_str()))
	{
		ImGui::PushItemWidth(120);
		bool transform_dirty = false;
		// position
		Vector3 pos = GetPosition();
		float x = pos.GetX(), y = pos.GetY(), z = pos.GetZ();
		transform_dirty |= ImGui::DragFloat("X", &x, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f", ImGuiSliderFlags_None); ImGui::SameLine();
		transform_dirty |= ImGui::DragFloat("Y", &y, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f", ImGuiSliderFlags_None); ImGui::SameLine();
		transform_dirty |= ImGui::DragFloat("Z", &z, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f", ImGuiSliderFlags_None);

		// pitch head roll
		transform_dirty |= ImGui::DragFloat("Pitch", &m_currentPitch, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f", ImGuiSliderFlags_None); ImGui::SameLine();
		transform_dirty |= ImGui::DragFloat("Head", &m_currentHeading, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f", ImGuiSliderFlags_None); ImGui::SameLine();
		transform_dirty |= ImGui::DragFloat("Roll", &m_currentRoll, 0.05f, -FLT_MAX, +FLT_MAX, "%.2f", ImGuiSliderFlags_None);
		ImGui::PopItemWidth();

		m_currentPitch = std::fmod(m_currentPitch, 360.0f);
		m_currentHeading = std::fmod(m_currentHeading, 360.0f);
		m_currentRoll = std::fmod(m_currentRoll, 360.0f);
		pos = Vector3(x, y, z);

		// Update camera parameter
		if (transform_dirty)
		{
			Matrix3 orientation = Matrix3(s_worldEast, s_worldUp, -s_worldNorth) *
				Matrix3::MakeYRotation(ToRadian(m_currentHeading)) * Matrix3::MakeXRotation(ToRadian(m_currentPitch)) * Matrix3::MakeZRotation(ToRadian(m_currentRoll));
			SetTransform(AffineTransform(orientation, pos));
		}

		// Camera type
		bool proj_dirty = false;
		const char* cameraType[] = { "Perspective", "Orthographic" };
		int curIdx = m_isPerspective ? 0 : 1;
		if (ImGui::BeginCombo("Camera Type", cameraType[curIdx], 0))
		{
			for (int n = 0; n < IM_ARRAYSIZE(cameraType); ++n)
			{
				const bool isSelected = (curIdx == n);
				if (ImGui::Selectable(cameraType[n], isSelected))
				{
					proj_dirty = true;
					curIdx = n;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		m_isPerspective = curIdx == 0;

		// Reverse Z
		proj_dirty |= ImGui::Checkbox("Reverse Z", &m_isReverseZ);

		// Projection parameters
		proj_dirty |= ImGui::SliderFloat("Vertical FOV", &m_verticalFOV, 20.0f, 120.0f, "%.2f");
		proj_dirty |= ImGui::DragFloat("Near Plane", &m_nearClip, 0.05f, 0.01f, m_farClip, "%.2f");
		proj_dirty |= ImGui::DragFloat("Far Plane", &m_farClip, 0.05f, m_nearClip + 1.0f, FLT_MAX, "%.2f");
		if (proj_dirty)
			UpdateProjMatrix();
		
		// movement speed
		ImGui::SliderFloat("Movement Speed", &m_moveSpeed, 1, 2000, "%.2f");
		ImGui::SliderFloat("Horizontal Sensitivity", &m_horizontalSensitivity, 1.0f, 30.0f);
		ImGui::SliderFloat("Vertical Sensitivity", &m_verticalSensitivity, 1.0f, 30.0f);
	}
}

Camera::Camera(const std::string& name)
	: m_name(name), m_cameraToWorld(kIdentity), m_basis(kIdentity), m_isPerspective(true), m_isReverseZ(true)
{
	SetPrespectiveMatrix(45, 9.0f / 16.0f, 0.01f, 1000.0f);
	m_horizontalSensitivity = 10.0f;
	m_verticalSensitivity = 10.0f;
	m_moveSpeed = 20.0f;
	m_currentPitch = Sin(Dot(GetForward(), s_worldUp));

	Vector3 forward = Normalize(Cross(s_worldUp, GetRight()));
	m_currentHeading = ATan2(-Dot(forward, s_worldEast), Dot(forward, s_worldNorth));

	m_currentRoll = 0.0f;

	m_momentum = true;

	m_lastYaw = 0.0f;
	m_lastPitch = 0.0f;
	m_lastRoll = 0.0f;
	m_lastForward = 0.0f;
	m_lastStrafe = 0.0f;
	m_lastAscent = 0.0f;
}

void Camera::SetLookDirection(const Vector3& forward, const Vector3& up)
{
	Scalar forwardLenSq = LengthSquare(forward);
	// if forwardLenSq == 0.0, use -Z axis as forward
	Vector3 normalizedForward = Select(forward * RecipSqrt(forwardLenSq), -Vector3(kZUnitVector), forwardLenSq < Scalar(0.000001f));

	Vector3 right = Cross(normalizedForward, up);
	Scalar rightLenSq = LengthSquare(right);
	right = Select(right * RecipSqrt(rightLenSq), Quaternion(Vector3(kYUnitVector), -XM_PIDIV2) * forward, rightLenSq < Scalar(0.000001f));

	Vector3 normaliedUp = Cross(right, normalizedForward);

	m_basis = Matrix3(right, normaliedUp, -normalizedForward);
	m_cameraToWorld.SetRotation(Quaternion(m_basis));
}

void Camera::Update(float deltaTime)
{
	UpdateMatrixAndFrustum();
}

void Camera::UpdateMatrixAndFrustum()
{
	m_previousViewProjMatrix = m_viewProjMatrix;

	m_viewMatrix = Matrix4(~m_cameraToWorld);
	m_viewProjMatrix = m_projMatrix * m_viewMatrix;
	m_reprojectMatrix = m_previousViewProjMatrix * Invert(GetViewProjMatrix());

	m_frustumVS = Frustum(m_projMatrix);
	m_frustumWS = m_cameraToWorld * m_frustumVS;
}

void Camera::UpdateProjMatrix()
{
	if (m_isPerspective)
	{
		float Y = 1.0f / std::tanf(ToRadian(m_verticalFOV) * 0.5f);
		float X = Y * m_aspectRatio;

		float Q1, Q2;

		if (m_isReverseZ)
		{
			Q1 = m_nearClip / (m_farClip - m_nearClip);
			Q2 = Q1 * m_farClip;
		}
		else
		{
			Q1 = m_farClip / (m_nearClip - m_farClip);
			Q2 = Q1 * m_nearClip;
		}

		SetProjMatrix(Matrix4
		(
			Vector4(X, 0.0f, 0.0f, 0.0f),
			Vector4(0.0f, Y, 0.0f, 0.0f),
			Vector4(0.0f, 0.0f, Q1, -1.0f),
			Vector4(0.0f, 0.0f, Q2, 0.0f)
		));
	}
	else
	{

	}
}

void Camera::ApplyMomentum(float& oldValue, float& newValue, float deltaTime)
{
	float blendValue;
	if (Abs(newValue) > Abs(oldValue))
		blendValue = Lerp(newValue, oldValue, Pow(0.6f, deltaTime * 60.0f));
	else
		blendValue = Lerp(newValue, oldValue, Pow(0.8f, deltaTime * 60.0f));
	oldValue = blendValue;
	newValue = blendValue;
}

void SceneCamera::Update(float deltaTime)
{
	ImGuiIO& io = ImGui::GetIO();

	float yaw = 0.0f, pitch = 0.0f, forward = 0.0f, strafe = 0.0f, ascent = 0.0f;
	// if left mouse click the rendering image
	if (!io.WantCaptureKeyboard)
	{
		if (io.MouseDownDuration[0] >= 0.0f)
		{
			yaw = (float)io.MouseDelta.x * 0.0018f * m_horizontalSensitivity;
			pitch = (float)io.MouseDelta.y * -0.0018f * m_verticalSensitivity;
		}
		forward = m_moveSpeed * ((ImGui::IsKeyPressedMap(ImGuiKey_W) ? deltaTime : 0.0f) + (ImGui::IsKeyPressedMap(ImGuiKey_S) ? -deltaTime : 0.0f));
		strafe = m_moveSpeed * ((ImGui::IsKeyPressedMap(ImGuiKey_D) ? deltaTime : 0.0f) + (ImGui::IsKeyPressedMap(ImGuiKey_A) ? -deltaTime : 0.0f));
		ascent = m_moveSpeed * ((ImGui::IsKeyPressedMap(ImGuiKey_E) ? deltaTime : 0.0f) + (ImGui::IsKeyPressedMap(ImGuiKey_Q) ? -deltaTime : 0.0f));

		if (m_momentum)
		{
			//ApplyMomentum(m_lastYaw, yaw, deltaTime);
			//ApplyMomentum(m_lastPitch, pitch, deltaTime);
			ApplyMomentum(m_lastForward, forward, deltaTime);
			ApplyMomentum(m_lastStrafe, strafe, deltaTime);
			ApplyMomentum(m_lastAscent, ascent, deltaTime);
		}
	}

	m_currentPitch = std::fmod(m_currentPitch + pitch, 360.0f);
	m_currentHeading = std::fmod(m_currentHeading - yaw, 360.0f);

	Matrix3 orientation = Matrix3(s_worldEast, s_worldUp, -s_worldNorth) * Matrix3::MakeYRotation(ToRadian(m_currentHeading)) * Matrix3::MakeXRotation(ToRadian(m_currentPitch));
	Vector3 position = orientation * Vector3(strafe, ascent, -forward) + GetPosition();
	SetTransform(AffineTransform(orientation, position));
	UpdateMatrixAndFrustum();

}