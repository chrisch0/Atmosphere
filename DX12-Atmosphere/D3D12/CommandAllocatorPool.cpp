#include "stdafx.h"
#include "CommandAllocatorPool.h"

CommandAllocatorPool::CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type) :
	m_type(type),
	m_device(nullptr)
{

}

CommandAllocatorPool::~CommandAllocatorPool()
{
	Shutdown();
}

void CommandAllocatorPool::Create(ID3D12Device* pDevice)
{
	m_device = pDevice;
}

void CommandAllocatorPool::Shutdown()
{
	for (size_t i = 0; i < m_allocatorPool.size(); ++i)
		m_allocatorPool[i]->Release();

	m_allocatorPool.clear();
}

ID3D12CommandAllocator* CommandAllocatorPool::RequestAllocator(uint64_t completedFenceValue)
{
	std::lock_guard<std::mutex> lockGuard(m_allocatorMutex);

	ID3D12CommandAllocator* pAllocator = nullptr;

	if (!m_readyAllocators.empty())
	{
		auto& allocatorPair = m_readyAllocators.front();
		if (allocatorPair.first <= completedFenceValue)
		{
			pAllocator = allocatorPair.second;
			ThrowIfFailed(pAllocator->Reset());
			m_readyAllocators.pop();
		}
	}

	if (pAllocator == nullptr)
	{
		ThrowIfFailed(m_device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&pAllocator)));
		std::wstringstream allocatorName;
		allocatorName << "CommandAllocator " << m_allocatorPool.size();
		pAllocator->SetName(allocatorName.str().data());
		m_allocatorPool.push_back(pAllocator);
	}

	return pAllocator;
}

void CommandAllocatorPool::DiscardAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator)
{
	std::lock_guard<std::mutex> lockGuard(m_allocatorMutex);
	m_readyAllocators.push({ fenceValue, allocator });
}