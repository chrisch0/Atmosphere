#pragma once
#include "stdafx.h"
#include "LinearAllocator.h"

class CommandContext;
class CommandListManager;
class GpuResource;
class GraphicsContext;
class ComputeContext;

#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
    | D3D12_RESOURCE_STATE_COPY_DEST \
    | D3D12_RESOURCE_STATE_COPY_SOURCE )

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

	// Flush existing commands to the GPU but keep the context alive
	uint64_t Flush(bool waitForCompletion = false);

	// Flush existing commands and release the current context
	uint64_t CommandContext::Finish(bool waitForCompletion = false);

	void Initialize();

	GraphicsContext& GetGraphicsContext()
	{
		assert(m_type != D3D12_COMMAND_LIST_TYPE_COMPUTE);
		return reinterpret_cast<GraphicsContext&>(*this);
	}

	ComputeContext& GetComputeContext()
	{
		return reinterpret_cast<ComputeContext&>(*this);
	}

	ID3D12GraphicsCommandList* GetCommandList()
	{
		return m_commandList;
	}

	DynAlloc AllocateUploadBuffer(size_t sizeInBytes, const std::wstring& name = L"")
	{
		return m_cpuLinearAllocator.Allocate(sizeInBytes, name);
	}

	static void InitializeBuffer(GpuResource& dest, const void* bufferData, size_t numBytes, size_t offset = 0, const std::wstring& name = L"");

	void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState, bool flushImmediate = false);
	void InsertUAVBarrier(GpuResource& resource, bool flushImmediate = false);
	inline void FlushResourceBarriers();


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

class GraphicsContext : public CommandContext
{
public:
	static GraphicsContext& Begin(const std::wstring& ID = L"")
	{
		return CommandContext::Begin(ID).GetGraphicsContext();
	}




};