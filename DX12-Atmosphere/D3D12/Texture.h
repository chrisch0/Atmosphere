#pragma once
#include "stdafx.h"
#include "GpuResource.h"

class Texture2D : public GpuResource
{
	friend class CommandContext;
public:
	Texture2D() { m_cpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
	Texture2D(D3D12_CPU_DESCRIPTOR_HANDLE handle) : m_cpuHandle(handle) {}

	void Create(size_t pitch, size_t width, size_t height, DXGI_FORMAT format, const void* initData);
	void Create(size_t width, size_t height, DXGI_FORMAT format, const void* initData)
	{
		Create(width, width, height, format, initData);
	}

protected:
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle;
};