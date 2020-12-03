#pragma once
#include "stdafx.h"
#include "PixelBuffer.h"
#include "Color.h"

class ColorBuffer : public PixelBuffer
{
public:
	ColorBuffer(Color clearColor = Color(0.0f, 0.0f, 0.0f, 0.0f))
		: m_clearColor(clearColor), m_numMipMaps(0), m_fragmentCount(1), m_sampleCount(1)
	{
		m_srvHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_rtvHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		std::memset(m_uavHandle, 0xFF, sizeof(m_uavHandle));
	}

	void CreateFromSwapChain(const std::wstring& name, ID3D12Resource* baseResource);

	void Create(const std::wstring& name, uint32_t width, uint32_t height, uint32_t numMips,
		DXGI_FORMAT format, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

protected:

	Color m_clearColor;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_uavHandle[12];
	uint32_t m_numMipMaps;
	uint32_t m_fragmentCount;
	uint32_t m_sampleCount;
};