#pragma once
#include "stdafx.h"
#include <stdexcept>

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}

class ColorBuffer;
class VolumeColorBuffer;

namespace Utils
{
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
	);

	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		// Constant buffers must be a multiple of the minimum hardware
		// allocation size (usually 256 bytes).  So round up to nearest
		// multiple of 256.  We do this by adding 255 and then masking off
		// the lower 2 bytes which store all bits < 256.
		// Example: Suppose byteSize = 300.
		// (300 + 255) & ~255
		// 555 & ~255
		// 0x022B & ~0x00ff
		// 0x022B & 0xff00
		// 0x0200
		// 512
		return (byteSize + 255) & ~255;
	}

	ImVec2 GetTiledVolumeTextureSize(uint32_t width, uint32_t height, uint32_t depth);

	template <typename VT>
	void AutoResizeVolumeImage(VT* image, bool isWindowOpen)
	{
		auto cursor_pos = ImGui::GetCursorPos();
		if (isWindowOpen)
		{
			auto tiled_size = Utils::GetTiledVolumeTextureSize(image->GetWidth(), image->GetHeight(), image->GetDepth());
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
	T Clamp(T& val, T min, T max)
	{
		val = (val < min ? min : (val > max ? max : val));
		return val;
	}
}
