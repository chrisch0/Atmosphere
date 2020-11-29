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