#pragma once
#include "stdafx.h"
#include "CommandAllocatorPool.h"

class CommandQueue
{
	friend class CommandListManager;
	friend class CommandContext;

public:
	CommandQueue(D3D12_COMMAND_LIST_TYPE type);
	~CommandQueue();

	void Create(ID3D12Device* pDevice);
	void Shutdown();

	inline bool IsReady()
	{
		return m_commandQueue != nullptr;
	}

	uint64_t IncrementFence();
	bool IsFenceComplete(uint64_t fenceValue);
	void StallForFence(uint64_t fenceValue);
	void StallForProducer(CommandQueue& producer);
	void WaitForFence(uint64_t fenceValue);
	void WaitForIdle() { WaitForFence(IncrementFence()); }

	ID3D12CommandQueue* GetCommandQueue() { return m_commandQueue; }

	uint64_t GetNextFenceValue() { return m_nextFenceValue; }

private:
	uint64_t ExecuteCommandList(ID3D12CommandList* list);
	ID3D12CommandAllocator* RequsetAllocator();
	void DiscardAllocator(uint64_t fenceValueForReset, ID3D12CommandAllocator* allocator);

	ID3D12CommandQueue* m_commandQueue;
	const D3D12_COMMAND_LIST_TYPE m_type;

	CommandAllocatorPool m_allocatorPool;
	std::mutex m_fenceMutex;
	std::mutex m_eventMutex;

	ID3D12Fence* m_pFence;
	uint64_t m_nextFenceValue;
	uint64_t m_lastCompletedFenceValue;
	HANDLE m_fenceEventHandle;
};

class CommandListManager
{
	friend class CommandContext;

public:
	CommandListManager();
	~CommandListManager();

	void Create(ID3D12Device* pDevice);
	void Shutdown();

	CommandQueue& GetGraphicsQueue() { return m_graphicsQueue; }
	CommandQueue& GetComputeQueue() { return m_computeQueue; }
	CommandQueue& GetCopyQueue() { return m_copyQueue; }

	CommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		switch (type)
		{
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: return m_computeQueue;
		case D3D12_COMMAND_LIST_TYPE_COPY: return m_copyQueue;
		default: return m_graphicsQueue;
		}
	}

	ID3D12CommandQueue* GetCommandQueue()
	{
		return m_graphicsQueue.GetCommandQueue();
	}

	void CreateNewCommandList(
		D3D12_COMMAND_LIST_TYPE type,
		ID3D12GraphicsCommandList** list,
		ID3D12CommandAllocator** allocator
	);

	bool IsFenceComplete(uint64_t fenceValue)
	{
		return GetQueue(D3D12_COMMAND_LIST_TYPE(fenceValue >> 56)).IsFenceComplete(fenceValue);
	}

	void WaitForFence(uint64_t fenceValue);

	void IdleGPU()
	{
		m_graphicsQueue.WaitForIdle();
		m_computeQueue.WaitForIdle();
		m_copyQueue.WaitForIdle();
	}

private:
	ID3D12Device* m_device;

	CommandQueue m_graphicsQueue;
	CommandQueue m_computeQueue;
	CommandQueue m_copyQueue;
};