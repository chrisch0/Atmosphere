#include "stdafx.h"
#include "DescriptorHeap.h"

std::mutex DescriptorAllocator::s_allocationMutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DescriptorAllocator::s_descriptorHeapPool;

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t count)
{
	if (m_curHeap == nullptr || m_remainingFreeHandles < count)
	{
		m_curHeap = RequestNewHeap(m_type);
		m_curHandle = m_curHeap->GetCPUDescriptorHandleForHeapStart();
		m_remainingFreeHandles = s_numDescriptorsPerHeap;

		if (m_descriptorSize == 0)
			m_descriptorSize = g_Device->GetDescriptorHandleIncrementSize(m_type);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ret = m_curHandle;
	m_curHandle.ptr += count * m_descriptorSize;
	m_remainingFreeHandles -= count;
	return ret;
}

ID3D12DescriptorHeap* DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	std::lock_guard<std::mutex> lockGuard(s_allocationMutex);

	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.Type = type;
	desc.NodeMask = 1;
	desc.NumDescriptors = s_numDescriptorsPerHeap;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
	ThrowIfFailed(g_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)));
	s_descriptorHeapPool.emplace_back(pHeap);
	return pHeap.Get();
}

void DescriptorAllocator::DestroyAll()
{
	s_descriptorHeapPool.clear();
}