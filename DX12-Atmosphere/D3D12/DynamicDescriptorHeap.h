#pragma once
#include "stdafx.h"
#include "DescriptorHeap.h"
#include "RootSignature.h"

class CommandContext;

class DynamicDescriptorHeap
{
public:

private:
	static const uint32_t kNumDescriptorsPerHeap = 1024;
	static std::mutex s_mutex;
	static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> s_descriptorHeapPool[2];
	static std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> s_retiredDescriptorHeaps[2];
	static std::queue<ID3D12DescriptorHeap*> s_availableDescriptorHeaps[2];

	static ID3D12DescriptorHeap* RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType);
	static void DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint64_t fenceValueForReset,
		const std::vector<ID3D12DescriptorHeap*>& usedHeaps);

	CommandContext& m_owningContext;
	ID3D12DescriptorHeap* m_curHeapPtr;
	const D3D12_DESCRIPTOR_HEAP_TYPE m_descriptorType;
	uint32_t m_descriptorSize;
	uint32_t m_curOffset;
	DescriptorHandle m_firsetDescriptor;
	std::vector<ID3D12DescriptorHeap*> m_retiredHeaps;
};