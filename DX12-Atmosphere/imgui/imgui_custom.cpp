#include "stdafx.h"
#include "imgui_custom.h"

namespace ImGui
{
	ImVec2 GetTiledVolumeTextureSize(uint32_t width, uint32_t height, uint32_t depth)
	{
		uint32_t y = (uint32_t)std::sqrt(depth);
		y = 1 << ((uint32_t)std::log2(y));
		uint32_t x = depth / y;

		return ImVec2((float)width * x, (float)height * y);
	}

	void PreviewImageButton(D3D12_CPU_DESCRIPTOR_HANDLE srv, const ImVec2& size, 
		bool& open_detail, bool& window_opening,
		const ImVec2& uv0, const ImVec2& uv1,
		int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
	{

	}

	void PreviewVolumeImageButton(D3D12_CPU_DESCRIPTOR_HANDLE srv, const ImVec2& size, const unsigned int texture_depth, 
		bool& open_detail, bool& window_opening, 
		const ImVec2& uv0 /* = ImVec2(0, 0) */, const ImVec2& uv1 /* = ImVec2(1, 1) */, 
		int frame_padding /* = -1 */, const ImVec4& bg_col /* = ImVec4(0, 0, 0, 0) */, const ImVec4& tint_col /* = ImVec4(1, 1, 1, 1) */)
	{
		
	}
}