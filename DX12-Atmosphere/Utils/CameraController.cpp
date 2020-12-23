#include "stdafx.h"
#include "CameraController.h"
#include "Camera.h"
#include "imgui/imgui_internal.h"

std::unordered_map<std::string, std::shared_ptr<Camera>> CameraController::s_cameras;

std::shared_ptr<Camera> CameraController::GetCamera(const std::string& name)
{
	auto iter = s_cameras.find(name);
	if (iter == s_cameras.end())
	{
		std::cout << "Error: Camera \"" << name << "\" is not exist" << std::endl;
		return nullptr;
	}
	return iter->second;
}

void CameraController::Destroy()
{
	s_cameras.clear();
}

void CameraController::UpdateUI()
{
	if (m_showCameraUI)
	{
		ImGui::Begin("Camera Controller", &m_showCameraUI);

		int i = 0;
		for (auto& pair : s_cameras)
		{
			ImGui::PushID(i);
			pair.second->UpdateUI();
			ImGui::PopID();
			i++;
		}

		ImGui::End();
	}
}

void CameraController::Update(float deltaTime)
{
	//UpdateUI();

	if (ImGui::IsKeyPressedMap(ImGuiKey_C))
	{
		m_showCameraUI = !m_showCameraUI;
	}

	for (auto& pair : s_cameras)
	{
		pair.second->Update(deltaTime);
	}
}