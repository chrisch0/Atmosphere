#include "stdafx.h"
#include "CommandListManager.h"

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type) :
	m_type(type),
	m_commandQueue(nullptr),
	m_pFence(nullptr),
	m_nextFenceValue((uint64_t)type << 56 | 1),
	m_lastCompletedFenceValue((uint64_t)type << 56),
	m_allocatorPool(type)
{

}

CommandQueue::~CommandQueue()
{
	Shutdown();
}

void CommandQueue::Shutdown()
{
	if (m_commandQueue == nullptr)
		return;

	m_allocatorPool.Shutdown();

	CloseHandle(m_fenceEventHandle);

	m_pFence->Release();
	m_pFence = nullptr;

	m_commandQueue->Release();
	m_commandQueue = nullptr;
}

void CommandQueue::Create(ID3D12Device* pDevice)
{
	assert(pDevice != nullptr);
	assert(!IsReady());
	assert(m_allocatorPool.Size() == 0);

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = m_type;
	queueDesc.NodeMask = 1;
	pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
	m_commandQueue->SetName(L"CommandListManager::m_CommandQueue");

	ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
	m_pFence->SetName(L"CommandListManager::m_pFence");
	m_pFence->Signal((uint64_t)m_type << 56);

	m_fenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
	assert(m_fenceEventHandle != NULL);

	m_allocatorPool.Create(pDevice);
	assert(IsReady());
}

uint64_t CommandQueue::ExecuteCommandList(ID3D12CommandList* list)
{
	std::lock_guard<std::mutex> lockGuard(m_fenceMutex);

	ThrowIfFailed(((ID3D12GraphicsCommandList*)list)->Close());

	m_commandQueue->ExecuteCommandLists(1, &list);

	m_commandQueue->Signal(m_pFence, m_nextFenceValue);

	return m_nextFenceValue++;
}

uint64_t CommandQueue::IncrementFence()
{
	std::lock_guard<std::mutex> lockGuard(m_fenceMutex);
	m_commandQueue->Signal(m_pFence, m_nextFenceValue);
	return m_nextFenceValue++;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
	if (fenceValue > m_lastCompletedFenceValue)
		m_lastCompletedFenceValue = std::max(m_lastCompletedFenceValue, m_pFence->GetCompletedValue());

	return fenceValue <= m_lastCompletedFenceValue;
}

void CommandQueue::StallForFence(uint64_t fenceValue)
{
	CommandQueue& producer = g_CommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56));
	m_commandQueue->Wait(producer.m_pFence, fenceValue);
}

void CommandQueue::StallForProducer(CommandQueue& producer)
{
	assert(producer.m_nextFenceValue > 0);
	m_commandQueue->Wait(producer.m_pFence, producer.m_nextFenceValue - 1);
}

void CommandQueue::WaitForFence(uint64_t fenceValue)
{
	if (IsFenceComplete(fenceValue))
		return;

	std::lock_guard<std::mutex> lockGuard(m_eventMutex);

	m_pFence->SetEventOnCompletion(fenceValue, m_fenceEventHandle);
	WaitForSingleObject(m_fenceEventHandle, INFINITE);
	m_lastCompletedFenceValue = fenceValue;
}

ID3D12CommandAllocator* CommandQueue::RequsetAllocator()
{
	uint64_t completedFence = m_pFence->GetCompletedValue();
	return m_allocatorPool.RequestAllocator(completedFence);
}

void CommandQueue::DiscardAllocator(uint64_t fenceValueForReset, ID3D12CommandAllocator* allocator)
{
	m_allocatorPool.DiscardAllocator(fenceValueForReset, allocator);
}

CommandListManager::CommandListManager() :
	m_device(nullptr),
	m_graphicsQueue(D3D12_COMMAND_LIST_TYPE_DIRECT),
	m_computeQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE),
	m_copyQueue(D3D12_COMMAND_LIST_TYPE_COPY)
{

}

CommandListManager::~CommandListManager()
{
	Shutdown();
}

void CommandListManager::Shutdown()
{
	m_graphicsQueue.Shutdown();
	m_computeQueue.Shutdown();
	m_copyQueue.Shutdown();
}

void CommandListManager::Create(ID3D12Device* pDevice)
{
	assert(pDevice != nullptr);
	m_device = pDevice;

	m_graphicsQueue.Create(pDevice);
	m_computeQueue.Create(pDevice);
	m_copyQueue.Create(pDevice);
}

void CommandListManager::CreateNewCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12GraphicsCommandList** list, ID3D12CommandAllocator** allocator)
{
	assert(type != D3D12_COMMAND_LIST_TYPE_BUNDLE);
	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: *allocator = m_graphicsQueue.RequsetAllocator(); break;
	case D3D12_COMMAND_LIST_TYPE_BUNDLE: break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: *allocator = m_computeQueue.RequsetAllocator(); break;
	case D3D12_COMMAND_LIST_TYPE_COPY: *allocator = m_copyQueue.RequsetAllocator(); break;
	}

	ThrowIfFailed(m_device->CreateCommandList(1, type, *allocator, nullptr, IID_PPV_ARGS(list)));
	(*list)->SetName(L"CommandList");
}

void CommandListManager::WaitForFence(uint64_t fenceValue)
{
	CommandQueue& producer = g_CommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56));
	producer.WaitForFence(fenceValue);
}