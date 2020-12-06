#pragma once
#include "stdafx.h"

class DescriptorAllocator
{
public:
	DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type) :
		m_type(type)
	{

	}

	~DescriptorAllocator() {}

	D3D12_CPU_DESCRIPTOR_HANDLE Allocate(uint32_t count);

	static void DestroyAll();

private:
	static const uint32_t s_numDescriptorsPerHeap = 256;
	static std::mutex s_allocationMutex;
	static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> s_descriptorHeapPool;
	static ID3D12DescriptorHeap* RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);

	D3D12_DESCRIPTOR_HEAP_TYPE m_type;
	ID3D12DescriptorHeap* m_curHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_curHandle;
	uint32_t m_descriptorSize;
	uint32_t m_remainingFreeHandles;
};

class DescriptorHandle
{
public:
	DescriptorHandle()
	{
		m_CpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_GpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle)
		: m_CpuHandle(CpuHandle)
	{
		m_GpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle)
		: m_CpuHandle(CpuHandle), m_GpuHandle(GpuHandle)
	{
	}

	DescriptorHandle operator+ (INT OffsetScaledByDescriptorSize) const
	{
		DescriptorHandle ret = *this;
		ret += OffsetScaledByDescriptorSize;
		return ret;
	}

	void operator += (INT OffsetScaledByDescriptorSize)
	{
		if (m_CpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_CpuHandle.ptr += OffsetScaledByDescriptorSize;
		if (m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_GpuHandle.ptr += OffsetScaledByDescriptorSize;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return m_CpuHandle; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return m_GpuHandle; }

	bool IsNull() const { return m_CpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
	bool IsShaderVisible() const { return m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE m_CpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_GpuHandle;
};