#include "stdafx.h"
#include "DynamicDescriptorHeap.h"
#include "CommandListManager.h"
#include "CommandContext.h"

std::mutex DynamicDescriptorHeap::s_mutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DynamicDescriptorHeap::s_descriptorHeapPool[2];
std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> s_retiredDescriptorHeaps[2];
std::queue<ID3D12DescriptorHeap*> s_availableDescriptorHeaps[2];

ID3D12DescriptorHeap* DynamicDescriptorHeap::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	std::lock_guard<std::mutex> lockGuard(s_mutex);

	uint32_t idx = heapType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? 1 : 0;

	while (!s_retiredDescriptorHeaps[idx].empty() && g_CommandManager.IsFenceComplete(s_retiredDescriptorHeaps[idx].front().first))
	{
		s_availableDescriptorHeaps[idx].push(s_retiredDescriptorHeaps[idx].front().second);
		s_retiredDescriptorHeaps[idx].pop();
	}

	if (!s_availableDescriptorHeaps[idx].empty())
	{
		ID3D12DescriptorHeap* heapPtr = s_availableDescriptorHeaps[idx].front();
		s_availableDescriptorHeaps[idx].pop();
		return heapPtr;
	}
	else
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = heapType;
		heapDesc.NumDescriptors = kNumDescriptorsPerHeap;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 1;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heapPtr;
		ThrowIfFailed(g_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heapPtr)));
		s_descriptorHeapPool[idx].emplace_back(heapPtr);
		return heapPtr.Get();
	}
}

void DynamicDescriptorHeap::DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint64_t fenceValueForReset, const std::vector<ID3D12DescriptorHeap *>& usedHeaps)
{
	uint32_t idx = heapType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? 1 : 0;
	std::lock_guard<std::mutex> lockGuard(s_mutex);
	for (auto iter = usedHeaps.begin(); iter != usedHeaps.end(); ++iter)
		s_retiredDescriptorHeaps[idx].push({fenceValueForReset, *iter});
}