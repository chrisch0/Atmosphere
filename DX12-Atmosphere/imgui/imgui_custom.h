#pragma once
#include "stdafx.h"

namespace ImGui
{
	ImVec2 GetTiledVolumeTextureSize(uint32_t width, uint32_t height, uint32_t depth);

	template <typename VT>
	void AutoResizeVolumeImage(VT* image, bool isWindowOpen)
	{
		auto cursor_pos = ImGui::GetCursorPos();
		if (isWindowOpen)
		{
			auto tiled_size = ImGui::GetTiledVolumeTextureSize(image->GetWidth(), image->GetHeight(), image->GetDepth());
			float x_max = 1024.0f, y_max = 1024.0f * (tiled_size.y / tiled_size.x);
			tiled_size.x = std::min(tiled_size.x, x_max);
			tiled_size.y = std::min(tiled_size.y, y_max);
			ImGui::SetWindowSize(ImVec2(tiled_size.x + 2.0f * cursor_pos.x, tiled_size.y + cursor_pos.y + 10.0f));
		}
		auto window_size = ImGui::GetWindowSize();
		ImGui::TiledVolumeImage((ImTextureID)(image->GetSRV().ptr), ImVec2(window_size.x - 2.0f * cursor_pos.x, window_size.y - cursor_pos.y - 10.0f), image->GetDepth());

	}

	template <typename T>
	void AutoResizeImage(T* image, bool isWindowOpen)
	{
		auto cursor_pos = ImGui::GetCursorPos();
		if (isWindowOpen)
			ImGui::SetWindowSize(ImVec2(image->GetWidth() + 2.0f * cursor_pos.x, image->GetHeight() + cursor_pos.y + 10.0f));
		auto window_size = ImGui::GetWindowSize();
		ImGui::Image((ImTextureID)(image->GetSRV().ptr), ImVec2(window_size.x - 2.0f * cursor_pos.x, window_size.y - cursor_pos.y - 10.0f));
	}

	template <typename T>
	void PreviewImageButton(T* image, const ImVec2& size, const std::string& window_name,
		bool* open_detail, bool* window_opening,
		const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1),
		int frame_padding = -1, const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1))
	{
		if (ImGui::ImageButton((ImTextureID)(image->GetSRV().ptr), size))
		{
			*open_detail = !(*open_detail);
			*window_opening = true;
		}
		if (*open_detail)
		{
			ImGui::Begin(window_name.c_str(), open_detail);
			ImGui::AutoResizeImage(image, *window_opening);
			ImGui::End();
			*window_opening = false;
		}
	}

	template <typename VT>
	void PreviewVolumeImageButton(VT* volumeImage, const ImVec2& size, const std::string& window_name,
		bool* open_detail, bool* window_opening,
		const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1),
		int frame_padding = -1, const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1))
	{
		if (ImGui::VolumeImageButton((ImTextureID)(volumeImage->GetSRV().ptr), size, volumeImage->GetDepth()))
		{
			*open_detail = !(*open_detail);
			*window_opening = true;
		}
		if (*open_detail)
		{
			ImGui::Begin(window_name.c_str(), open_detail);
			ImGui::AutoResizeVolumeImage(volumeImage, *window_opening);
			ImGui::End();
			*window_opening = false;
		}
	}
}