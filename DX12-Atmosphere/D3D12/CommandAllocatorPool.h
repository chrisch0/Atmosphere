#pragma once
#include "stdafx.h"

class CommandAllocatorPool
{
public:
	CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type);
	~CommandAllocatorPool();

	void Create(ID3D12Device* pDevice);
	void Shutdown();

	ID3D12CommandAllocator* RequestAllocator(uint64_t completedFenceValue);
	void DiscardAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator);

	inline size_t Size() { return m_allocatorPool.size(); }

private:
	const D3D12_COMMAND_LIST_TYPE m_type;
	ID3D12Device* m_device;
	std::vector<ID3D12CommandAllocator*> m_allocatorPool;
	std::queue<std::pair<uint64_t, ID3D12CommandAllocator*>> m_readyAllocators;
	std::mutex m_allocatorMutex;
};