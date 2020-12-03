#pragma once
#include "stdafx.h"
#include "GpuResource.h"

class PixelBuffer : public GpuResource
{
public:
	PixelBuffer() : m_width(0), m_height(0), m_arraySize(0), m_format(DXGI_FORMAT_UNKNOWN)
	{
	}

	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
	uint32_t GetDepth() const { return m_arraySize; }
	const DXGI_FORMAT& GetFormat() const{ return m_format; }

	// void ExportToFile()
protected:
	D3D12_RESOURCE_DESC DescribeTex2D(uint32_t width, uint32_t height, uint32_t depthOrArraySize, uint32_t numMips, DXGI_FORMAT format, UINT flags);

	void AssociateWithResource(ID3D12Device* device, const std::wstring& name, ID3D12Resource* resource, D3D12_RESOURCE_STATES currentState);

	void CreateTextureResource(ID3D12Device* device, const std::wstring& name, const D3D12_RESOURCE_DESC& resourceDesc,
		D3D12_CLEAR_VALUE clearValue, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

	static DXGI_FORMAT GetBaseFormat(DXGI_FORMAT Format);
	static DXGI_FORMAT GetUAVFormat(DXGI_FORMAT Format);
	static DXGI_FORMAT GetDSVFormat(DXGI_FORMAT Format);
	static DXGI_FORMAT GetDepthFormat(DXGI_FORMAT Format);
	static DXGI_FORMAT GetStencilFormat(DXGI_FORMAT Format);
	static size_t BytesPerPixel(DXGI_FORMAT Format);

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_arraySize;
	DXGI_FORMAT m_format;
};