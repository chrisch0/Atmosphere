#include "stdafx.h"
#include "HelperFuncs.h"
#include "D3D12/ColorBuffer.h"

namespace Utils
{
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
	)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

		// Create the actual default buffer resource.
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


		// Describe the data we want to copy into the default buffer.
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = initData;
		subResourceData.RowPitch = byteSize;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
		// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
		// the intermediate upload heap data will be copied to mBuffer.
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
		UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

		// Note: uploadBuffer has to be kept alive after the above function calls because
		// the command list has not been executed yet that performs the actual copy.
		// The caller can Release the uploadBuffer after it knows the copy has been executed.


		return defaultBuffer;
	}

	ImVec2 GetTiledVolumeTextureSize(uint32_t width, uint32_t height, uint32_t depth)
	{
		uint32_t y = (uint32_t)std::sqrt(depth);
		y = 1 << ((uint32_t)std::log2(y));
		uint32_t x = depth / y;

		return ImVec2((float)width * x, (float)height * y);
	}

	void AutoResizeVolumeImage(VolumeColorBuffer* image, bool isWindowOpen)
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

	void AutoResizeImage(ColorBuffer* image, bool isWindowOpen)
	{
		auto cursor_pos = ImGui::GetCursorPos();
		if (isWindowOpen)
			ImGui::SetWindowSize(ImVec2(image->GetWidth() + 2.0f * cursor_pos.x, image->GetHeight() + cursor_pos.y + 10.0f));
		auto window_size = ImGui::GetWindowSize();
		ImGui::Image((ImTextureID)(image->GetSRV().ptr), ImVec2(window_size.x - 2.0f * cursor_pos.x, window_size.y - cursor_pos.y - 10.0f));
	}
}

