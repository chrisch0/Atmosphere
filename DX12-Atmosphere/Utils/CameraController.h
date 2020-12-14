#pragma once
#include "stdafx.h"

class Camera;

class CameraController
{
	friend class Camera;
public:
	template <typename T>
	static std::shared_ptr<T> CreateCamera(const std::string& name);
	static std::shared_ptr<Camera> GetCamera(const std::string& name);
	static void Destroy();

	CameraController() :m_showCameraUI(false) {}
	~CameraController() {}

	void UpdateUI();
	void Update(float deltaTime);

private:
	bool m_showCameraUI;

private:
	static std::unordered_map<std::string, std::shared_ptr<Camera>> s_cameras;
};

template <typename T>
std::shared_ptr<T> CameraController::CreateCamera(const std::string& name)
{
	static_assert(std::is_base_of_v<Camera, T>);
	auto iter = s_cameras.find(name);
	std::string n = name;
	if (iter != s_cameras.end())
	{
		n = name + "_";
		std::cout << "Warning: Camera \"" << name << "\" is already exist, rename to \"" << n << "\"" << std::endl;
	}
	auto cameraPtr = std::make_shared<T>(n);
	s_cameras.insert({ n, cameraPtr });
	return cameraPtr;
}