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

	void CreateArray(const std::wstring& name, uint32_t width, uint32_t height, uint32_t arrayCount,
		DXGI_FORMAT format, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

	void CreateDerivedViews(ID3D12Device* device, DXGI_FORMAT format, uint32_t arraySize, uint32_t numMips);

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_srvHandle; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV() const { return m_rtvHandle; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV() const { return m_uavHandle[0]; }

	void SetClearColor(const Color& clearColor) { m_clearColor = clearColor; }

	void SetMsaaMode(uint32_t numColorSamples, uint32_t numCoverageSamples)
	{
		assert(numCoverageSamples >= numColorSamples);
		m_fragmentCount = numColorSamples;
		m_sampleCount = numCoverageSamples;
	}

	Color GetClearColor() const { return m_clearColor; }

protected:

	D3D12_RESOURCE_FLAGS CombineResourceFlags() const
	{
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

		if (flags == D3D12_RESOURCE_FLAG_NONE && m_fragmentCount == 1)
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | flags;
	}

	static inline uint32_t ComputeNumMips(uint32_t width, uint32_t height)
	{
		uint32_t highBit;
		_BitScanReverse((unsigned long*)&highBit, width | height);
		return highBit + 1;
	}

	Color m_clearColor;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_uavHandle[12];  // use for each mip level
	uint32_t m_numMipMaps;
	uint32_t m_fragmentCount;
	uint32_t m_sampleCount;
};

class VolumeColorBuffer : public PixelBuffer
{
public:
	VolumeColorBuffer(Color clearColor = Color(0.0f, 0.0f, 0.0f, 0.0f))
		: m_clearColor(clearColor), m_numMipMaps(0), m_fragmentCount(1), m_sampleCount(1)
	{
		m_srvHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		std::memset(m_uavHandle, 0xFF, sizeof(m_uavHandle));
	}

	void Create(const std::wstring& name, uint32_t width, uint32_t height, uint32_t depth, uint32_t numMips,
		DXGI_FORMAT format, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

	void CreateDerivedViews(ID3D12Device* device, DXGI_FORMAT format, uint32_t numMips);

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_srvHandle; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV() const { return m_uavHandle[0]; }

	void SetClearColor(Color clearColor) { m_clearColor = clearColor; }

	void SetMsaaMode(uint32_t numColorSamples, uint32_t numCoverageSamples)
	{
		assert(numCoverageSamples >= numColorSamples);
		m_fragmentCount = numColorSamples;
		m_sampleCount = numCoverageSamples;
	}

	Color GetClearColor() const { return m_clearColor; }

protected:
	D3D12_RESOURCE_FLAGS CombineResourceFlags() const
	{
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

		if (flags == D3D12_RESOURCE_FLAG_NONE && m_fragmentCount == 1)
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | flags;
	}

	static inline uint32_t ComputeNumMips(uint32_t width, uint32_t height, uint32_t depth)
	{
		uint32_t highBit;
		_BitScanReverse((unsigned long*)&highBit, width | height | depth);
		return highBit + 1;
	}

	Color m_clearColor;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_uavHandle[12];
	uint32_t m_numMipMaps;
	uint32_t m_fragmentCount;
	uint32_t m_sampleCount;
};