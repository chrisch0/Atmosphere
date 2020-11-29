#pragma once
#include "stdafx.h"
#include "LinearAllocator.h"

class CommandContext;
class CommandListManager;
class GpuResource;

class ContextManager
{
public: 
	ContextManager() {}

	CommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE type);
	void FreeContext(CommandContext*);
	void DestroyAllContexts();

private:
	std::vector<std::unique_ptr<CommandContext>> sm_contextPool[4];
	std::queue<CommandContext*> sm_availableContexts[4];
	std::mutex sm_contextAllocationMutex;
};

struct NonCopyable
{
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

class CommandContext : NonCopyable
{
	friend ContextManager;
private:
	CommandContext(D3D12_COMMAND_LIST_TYPE type);

	void Reset();

public:
	~CommandContext();

	static void DestroyAllContexts();

	static CommandContext& Begin(const std::wstring& ID = L"");

	void Initialize();

	// Flush existing commands and release the current context
	uint64_t CommandContext::Finish(bool waitForCompletion = false);

	// Flush existing commands to the GPU but keep the context alive
	uint64_t Flush(bool waitForCompletion = false);

	inline void FlushResourceBarriers();

	DynAlloc AllocateUploadBuffer(size_t sizeInBytes)
	{
		return m_cpuLinearAllocator.Allocate(sizeInBytes);
	}

	static void InitializeBuffer(GpuResource& dest, const void* bufferData, size_t numBytes, size_t offset);

protected:
	void BindDescriptorHeaps();

	CommandListManager* m_owningManager;
	ID3D12CommandAllocator* m_currAllocator;
	ID3D12GraphicsCommandList* m_commandList;

	ID3D12RootSignature* m_curGraphicsRootSignature;
	ID3D12PipelineState* m_curPipelineState;
	ID3D12RootSignature* m_curComputeRootSignature;

	ID3D12DescriptorHeap* m_curDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	UINT m_numBarriersToFlush;
	CD3DX12_RESOURCE_BARRIER m_resourceBarrierBuffer[16];

	LinearAllocator m_cpuLinearAllocator;
	LinearAllocator m_gpuLinearAllocator;

	std::wstring m_ID;
	void SetID(const std::wstring& ID) { m_ID = ID; }
	D3D12_COMMAND_LIST_TYPE m_type;
};