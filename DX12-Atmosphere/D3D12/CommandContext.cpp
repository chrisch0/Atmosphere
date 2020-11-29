#include "stdafx.h"
#include "CommandContext.h"
#include "CommandListManager.h"

void ContextManager::DestroyAllContexts()
{
	for (uint32_t i = 0; i < 4; ++i)
	{
		sm_contextPool[i].clear();
	}
}

CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
	std::lock_guard<std::mutex> LockGuard(sm_contextAllocationMutex);

	auto& AvailableContexts = sm_availableContexts[Type];

	CommandContext* ret = nullptr;
	if (AvailableContexts.empty())
	{
		ret = new CommandContext(Type);
		sm_contextPool[Type].emplace_back(ret);
		ret->Initialize();
	}
	else
	{
		ret = AvailableContexts.front();
		AvailableContexts.pop();
		ret->Reset();
	}
	assert(ret != nullptr);

	assert(ret->m_type == Type);

	return ret;
}

void ContextManager::FreeContext(CommandContext* usedContext)
{
	assert(usedContext != nullptr);
	std::lock_guard<std::mutex> LockGuard(sm_contextAllocationMutex);
	sm_availableContexts[usedContext->m_type].push(usedContext);
}

CommandContext::CommandContext(D3D12_COMMAND_LIST_TYPE type) :
	m_type(type),
	m_cpuLinearAllocator(kCpuWritable),
	m_gpuLinearAllocator(kGpuExclusive)
{
	m_owningManager = nullptr;
	m_commandList = nullptr;
	m_currAllocator = nullptr;
	ZeroMemory(m_curDescriptorHeaps, sizeof(m_curDescriptorHeaps));

	m_curGraphicsRootSignature = nullptr;
	m_curPipelineState = nullptr;
	m_curComputeRootSignature = nullptr;
	m_numBarriersToFlush = 0;
}

CommandContext::~CommandContext()
{
	if (m_commandList != nullptr)
		m_commandList->Release();
}

void CommandContext::Initialize()
{
	g_CommandManager.CreateNewCommandList(m_type, &m_commandList, &m_currAllocator);
}

void CommandContext::Reset()
{
	assert(m_commandList != nullptr && m_currAllocator == nullptr);
	m_currAllocator = g_CommandManager.GetQueue(m_type).RequsetAllocator();
	m_commandList->Reset(m_currAllocator, nullptr);

	m_curGraphicsRootSignature = nullptr;
	m_curPipelineState = nullptr;
	m_curComputeRootSignature = nullptr;
	m_numBarriersToFlush = 0;
	
	BindDescriptorHeaps();
}

CommandContext& CommandContext::Begin(const std::wstring& ID /* = L"" */)
{
	CommandContext* newContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	newContext->SetID(ID);
	return *newContext;
}

void CommandContext::DestroyAllContexts()
{
	g_ContextManager.DestroyAllContexts();
}

uint64_t CommandContext::Flush(bool waitForCompletion /* = false */)
{
	FlushResourceBarriers();

	assert(m_currAllocator != nullptr);

	uint64_t fenceValue = g_CommandManager.GetQueue(m_type).ExecuteCommandList(m_commandList);

	if (waitForCompletion)
		g_CommandManager.WaitForFence(fenceValue);

	// Reset the command list and restore previous state
	m_commandList->Reset(m_currAllocator, nullptr);

	if (m_curGraphicsRootSignature)
	{
		m_commandList->SetGraphicsRootSignature(m_curGraphicsRootSignature);
	}
	if (m_curComputeRootSignature)
	{
		m_commandList->SetComputeRootSignature(m_curComputeRootSignature);
	}
	if (m_curPipelineState)
	{
		m_commandList->SetPipelineState(m_curPipelineState);
	}

	BindDescriptorHeaps();

	return fenceValue;
}

uint64_t CommandContext::Finish(bool waitForCompletion /* = false */)
{
	assert(m_type == D3D12_COMMAND_LIST_TYPE_DIRECT || m_type == D3D12_COMMAND_LIST_TYPE_COMPUTE);

	FlushResourceBarriers();

	assert(m_currAllocator != nullptr);

	CommandQueue& queue = g_CommandManager.GetQueue(m_type);

	uint64_t fenceValue = queue.ExecuteCommandList(m_commandList);
	queue.DiscardAllocator(fenceValue, m_currAllocator);
	m_currAllocator = nullptr;

	m_cpuLinearAllocator.CleanupUsedPages(fenceValue);
	m_gpuLinearAllocator.CleanupUsedPages(fenceValue);

	if (waitForCompletion)
		g_CommandManager.WaitForFence(fenceValue);

	g_ContextManager.FreeContext(this);

	return fenceValue;
}

void CommandContext::BindDescriptorHeaps()
{
	UINT nonNullHeaps = 0;
	ID3D12DescriptorHeap* heapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* heap = m_curDescriptorHeaps[i];
		if (heap != nullptr)
			heapsToBind[nonNullHeaps++] = heap;
	}

	if (nonNullHeaps > 0)
		m_commandList->SetDescriptorHeaps(nonNullHeaps, heapsToBind);
}

void CommandContext::InitializeBuffer(GpuResource& dest, const void* bufferData, size_t numBytes, size_t offset)
{
	CommandContext& initContext = CommandContext::Begin();

	// Create upload buffer
	auto uploadBuffer = initContext.AllocateUploadBuffer(numBytes);

	// Memory copy data to upload buffer
	memcpy(uploadBuffer.dataPtr, bufferData, numBytes);

	// Copy data from upload buffer to default buffer
}

inline void CommandContext::FlushResourceBarriers()
{
	if (m_numBarriersToFlush)
	{
		m_commandList->ResourceBarrier(m_numBarriersToFlush, m_resourceBarrierBuffer);
		m_numBarriersToFlush = 0;
	}
}
