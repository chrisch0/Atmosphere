#pragma once
#include "stdafx.h"
#include "LinearAllocator.h"
#include "DynamicDescriptorHeap.h"
#include "DynamicAllocator.h"
#include "GpuBuffer.h"
#include "ColorBuffer.h"

class CommandContext;
class CommandListManager;
class GpuResource;
class GraphicsContext;
class ComputeContext;
class PipelineState;

struct DWParam
{
	DWParam(float f) : val_f(f) {}
	DWParam(uint32_t ui) : val_ui(ui) {}
	DWParam(int32_t i) : val_i(i) {}

	void operator= (float f) { val_f = f; }
	void operator= (uint32_t u) { val_ui = u; }
	void operator= (int32_t i) { val_i = i; }

	union 
	{
		float val_f;
		uint32_t val_ui;
		int32_t val_i;
	};
};

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

	DynMem AllocateMemory(size_t sizeInBytes)
	{
		return m_dynamicMemoryAllocator.Allocate(sizeInBytes);
	}

	static void InitializeBuffer(GpuResource& dest, const void* bufferData, size_t numBytes, size_t offset = 0, const std::wstring& name = L"");
	static void InitializeTexture(GpuResource& dest, uint32_t numSubresources, D3D12_SUBRESOURCE_DATA subData[]);

	void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState, bool flushImmediate = false);
	void InsertUAVBarrier(GpuResource& resource, bool flushImmediate = false);
	void FlushResourceBarriers();

	void SetPipelineState(const PipelineState& pso);
	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heapPtr);
	void SetDescriptorHeaps(uint32_t heapCount, D3D12_DESCRIPTOR_HEAP_TYPE type[], ID3D12DescriptorHeap* heapPtrs[]);

protected:
	void BindDescriptorHeaps();

	CommandListManager* m_owningManager;
	ID3D12CommandAllocator* m_currAllocator;
	ID3D12GraphicsCommandList* m_commandList;

	ID3D12RootSignature* m_curGraphicsRootSignature;
	ID3D12PipelineState* m_curPipelineState;
	ID3D12RootSignature* m_curComputeRootSignature;

	// HEAP_TYPE_CBV_SRV_UAV
	DynamicDescriptorHeap m_dynamicViewDescriptorHeap;
	// HEAP_TYPE_SAMPLER
	DynamicDescriptorHeap m_dynamicSamplerDescriptorHeap;

	ID3D12DescriptorHeap* m_curDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	uint32_t m_numBarriersToFlush;
	CD3DX12_RESOURCE_BARRIER m_resourceBarrierBuffer[16];

	LinearAllocator m_cpuLinearAllocator;
	LinearAllocator m_gpuLinearAllocator;

	DynamicAllocator m_dynamicMemoryAllocator;

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

	void ClearUAV(GpuBuffer& target);
	void ClearUAV(ColorBuffer& target);
	void ClearColor(ColorBuffer& target);
	//void ClearDepth(DepthBuffer& target);
	//void ClearStencil(DepthBuffer& target);
	//void ClearDepthAndStencil(DepthBuffer& target);

	void SetRootSignature(const RootSignature& rootSig);

	void SetRenderTargets(uint32_t numRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]);
	void SetRenderTargets(uint32_t numRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[], D3D12_CPU_DESCRIPTOR_HANDLE dsv);
	void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv) { SetRenderTargets(1, &rtv); }
	void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv) { SetRenderTargets(1, &rtv, dsv); }
	void SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE dsv) { SetRenderTargets(0, nullptr, dsv); }

	void SetViewport(const D3D12_VIEWPORT& vp);
	void SetScissor(const D3D12_RECT& rect);
	void SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect);
	void SetBlendFactor(Color blendFactor);
	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);

	void SetConstantArray(uint32_t rootIndex, uint32_t numConstants, const void* pConstants);
	void SetConstant(uint32_t rootIndex, DWParam cal, uint32_t offset = 0);
	void SetConstants(uint32_t rootIndex, DWParam x);
	void SetConstants(uint32_t rootIndex, DWParam x, DWParam y);
	void SetConstants(uint32_t rootIndex, DWParam x, DWParam y, DWParam z);
	void SetConstants(uint32_t rootIndex, DWParam x, DWParam y, DWParam z, DWParam w);
	void SetConstantBuffer(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV);
	void SetDynamicConstantBufferView(uint32_t rootIndex, size_t bufferSize, const void* bufferData);
	void SetBufferSRV(uint32_t rootIndex, const GpuBuffer& SRV, UINT64 offset = 0);
	void SetBufferUAV(uint32_t rootIndex, const GpuBuffer& UAV, UINT64 offset = 0);
	void SetDescriptorTable(uint32_t rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE firstHandle);

	//void SetDynamicDescriptorDirect(D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void SetDynamicDescriptor(uint32_t rootIndex, uint32_t offset, D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void SetDynamicDescriptors(uint32_t rootIndex, uint32_t offset, uint32_t count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[]);
	void SetDynamicSampler(uint32_t rootIndex, uint32_t offset, D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void SetDynamicSamplers(uint32_t rootIndex, uint32_t offset, uint32_t count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[]);

	void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView);
	void SetVertexBuffer(uint32_t slot, const D3D12_VERTEX_BUFFER_VIEW& vbView);
	void SetVertexBuffers(uint32_t startSlot, uint32_t count, const D3D12_VERTEX_BUFFER_VIEW vbViews[]);
	void SetDynamicVB(uint32_t slot, size_t numVertices, size_t vertexStride, const void* vbData);
	void SetDynamicIB(size_t indexCount, const uint16_t* ibData);
	//void SetDynamicSRV(uint32_t rootIndex, size_t bufferSize, const void* bufferData);

	void Draw(uint32_t vertexCount, uint32_t vertexStartOffset = 0);
	void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation = 0, INT baseVertexLocation = 0);
	void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount,
		uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0);
	void DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation,
		INT baseVertexLocation, uint32_t startInstanceLocation);
	//void DrawIndirect(GpuBuffer& ArgumentBuffer, uint64_t argumentBufferOffset = 0);
};

class ComputeContext : public CommandContext
{
public:
	static ComputeContext& Begin(const std::wstring& ID = L"", bool async = false);

	void ClearUAV(GpuBuffer& target);
	void ClearUAV(ColorBuffer& target);

	void SetRootSignature(const RootSignature& rootSig);

	void SetConstantArray(uint32_t rootIndex, uint32_t numConstants, const void* pConstants);
	void SetConstant(uint32_t rootIndex, DWParam val, uint32_t offset = 0);
	void SetConstants(uint32_t rootIndex, DWParam x);
	void SetConstants(uint32_t rootIndex, DWParam x, DWParam y);
	void SetConstants(uint32_t rootIndex, DWParam x, DWParam y, DWParam z);
	void SetConstants(uint32_t rootIndex, DWParam x, DWParam y, DWParam z, DWParam w);
	void SetConstantBuffer(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS cbv);
	void SetDynamicConstantBufferView(uint32_t rootIndex, size_t bufferSize, const void* bufferData);
	void SetDynamicSRV(uint32_t rootIndex, size_t bufferSize, const void* bufferData);
	void SetBufferSRV(uint32_t rootIndex, const GpuBuffer& srv, uint64_t offset = 0);
	void SetBufferUAV(uint32_t rootIndex, const GpuBuffer& uav, uint64_t offset = 0);
	void SetDescriptorTable(uint32_t rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE firstHandle);

	void SetDynamicDescriptor(uint32_t rootIndex, uint32_t offset, D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void SetDynamicDescriptors(uint32_t rootIndex, uint32_t offset, uint32_t count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[]);
	void SetDynamicSampler(uint32_t rootIndex, uint32_t offset, D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void SetDynamicSamplers(uint32_t rootIndex, uint32_t offset, uint32_t count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[]);

	void Dispatch(size_t groupX = 1, size_t groupY = 1, size_t groupZ = 1);
	void Dispatch1D(size_t threadCountX, size_t groupSizeX = 64);
	void Dispatch2D(size_t threadCountX, size_t threadCountY, size_t groupSizeX = 8, size_t groupSizeY = 8);
	void Dispatch3D(size_t threadCountX, size_t threadCountY, size_t threadCountZ, size_t groupSizeX, size_t groupSizeY, size_t groupSizeZ);
	//void DispatchIndirect(GpuBuffer& argumentBuffer, uint64_t argumentBufferOffset = 0);
	//void ExecuteIndirect(CommandSignature& commandSig, GpuBuff)
};

inline void ComputeContext::SetRootSignature(const RootSignature& rootSig)
{
	if (rootSig.GetSignature() == m_curComputeRootSignature)
		return;

	m_commandList->SetComputeRootSignature(m_curComputeRootSignature = rootSig.GetSignature());

	m_dynamicViewDescriptorHeap.ParseComputeRootSignature(rootSig);
	m_dynamicSamplerDescriptorHeap.ParseComputeRootSignature(rootSig);
}

inline void ComputeContext::SetConstantArray(uint32_t rootIndex, uint32_t numConstants, const void* pConstants)
{
	m_commandList->SetComputeRoot32BitConstants(rootIndex, numConstants, pConstants, 0);
}

inline void ComputeContext::SetConstant(uint32_t rootIndex, DWParam val, uint32_t offset /* = 0 */)
{
	m_commandList->SetComputeRoot32BitConstant(rootIndex, val.val_ui, offset);
}

inline void ComputeContext::SetConstants(uint32_t rootIndex, DWParam x)
{
	m_commandList->SetComputeRoot32BitConstant(rootIndex, x.val_ui, 0);
}

inline void ComputeContext::SetConstants(uint32_t rootIndex, DWParam x, DWParam y)
{
	m_commandList->SetComputeRoot32BitConstant(rootIndex, x.val_ui, 0);
	m_commandList->SetComputeRoot32BitConstant(rootIndex, y.val_ui, 1);
}

inline void ComputeContext::SetConstants(uint32_t rootIndex, DWParam x, DWParam y, DWParam z)
{
	m_commandList->SetComputeRoot32BitConstant(rootIndex, x.val_ui, 0);
	m_commandList->SetComputeRoot32BitConstant(rootIndex, y.val_ui, 1);
	m_commandList->SetComputeRoot32BitConstant(rootIndex, z.val_ui, 2);
}

inline void ComputeContext::SetConstants(uint32_t rootIndex, DWParam x, DWParam y, DWParam z, DWParam w)
{
	m_commandList->SetComputeRoot32BitConstant(rootIndex, x.val_ui, 0);
	m_commandList->SetComputeRoot32BitConstant(rootIndex, y.val_ui, 1);
	m_commandList->SetComputeRoot32BitConstant(rootIndex, z.val_ui, 2);
	m_commandList->SetComputeRoot32BitConstant(rootIndex, w.val_ui, 3);
}

inline void ComputeContext::SetConstantBuffer(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS cbv)
{
	m_commandList->SetComputeRootConstantBufferView(rootIndex, cbv);
}

inline void ComputeContext::SetDynamicConstantBufferView(uint32_t rootIndex, size_t bufferSize, const void* bufferData)
{
	assert(bufferData != nullptr);
	DynAlloc cb = m_cpuLinearAllocator.Allocate(bufferSize);
	memcpy(cb.dataPtr, bufferData, bufferSize);
	m_commandList->SetComputeRootConstantBufferView(rootIndex, cb.gpuAddress);
}

inline void ComputeContext::SetDynamicSRV(uint32_t rootIndex, size_t bufferSize, const void* bufferData)
{
	assert(bufferData != nullptr);
	DynAlloc cb = m_cpuLinearAllocator.Allocate(bufferSize);
	memcpy(cb.dataPtr, bufferData, bufferSize);
	m_commandList->SetComputeRootConstantBufferView(rootIndex, cb.gpuAddress);
}

inline void ComputeContext::SetBufferSRV(uint32_t rootIndex, const GpuBuffer& srv, uint64_t offset /* = 0 */)
{
	assert((srv.m_usageState & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) != 0);
	m_commandList->SetComputeRootShaderResourceView(rootIndex, srv.GetGpuVirtualAddress() + offset);
}

inline void ComputeContext::SetBufferUAV(uint32_t rootIndex, const GpuBuffer& uav, uint64_t offset /* = 0 */)
{
	assert((uav.m_usageState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0);
	m_commandList->SetComputeRootUnorderedAccessView(rootIndex, uav.GetGpuVirtualAddress() + offset);
}

inline void ComputeContext::SetDescriptorTable(uint32_t rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE firstHandle)
{
	m_commandList->SetComputeRootDescriptorTable(rootIndex, firstHandle);
}

inline void ComputeContext::SetDynamicDescriptor(uint32_t rootIndex, uint32_t offset, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	SetDynamicDescriptors(rootIndex, offset, 1, &handle);
}

inline void ComputeContext::SetDynamicDescriptors(uint32_t rootIndex, uint32_t offset, uint32_t count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
{
	m_dynamicViewDescriptorHeap.SetComputeDescriptorHandles(rootIndex, offset, count, handles);
}

inline void ComputeContext::SetDynamicSampler(uint32_t rootIndex, uint32_t offset, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	SetDynamicSamplers(rootIndex, offset, 1, &handle);
}

inline void ComputeContext::SetDynamicSamplers(uint32_t rootIndex, uint32_t offset, uint32_t count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
{
	m_dynamicSamplerDescriptorHeap.SetComputeDescriptorHandles(rootIndex, offset, count, handles);
}

inline void ComputeContext::Dispatch(size_t groupX /* = 1 */, size_t groupY /* = 1 */, size_t groupZ /* = 1 */)
{
	FlushResourceBarriers();
	m_dynamicViewDescriptorHeap.CommitComputeRootDescriptorTables(m_commandList);
	m_dynamicSamplerDescriptorHeap.CommitComputeRootDescriptorTables(m_commandList);
	m_commandList->Dispatch((UINT)groupX, (UINT)groupY, (UINT)groupZ);
}

inline void ComputeContext::Dispatch1D(size_t threadCountX, size_t groupSizeX /* = 64 */)
{
	Dispatch(DivideByMultiple(threadCountX, groupSizeX), 1, 1);
}

inline void ComputeContext::Dispatch2D(size_t threadCountX, size_t threadCountY, size_t groupSizeX /* = 8 */, size_t groupSizeY /* = 8 */)
{
	Dispatch(DivideByMultiple(threadCountX, groupSizeX), DivideByMultiple(threadCountY, groupSizeY), 1);
}

inline void ComputeContext::Dispatch3D(size_t threadCountX, size_t threadCountY, size_t threadCountZ, size_t groupSizeX, size_t groupSizeY, size_t groupSizeZ)
{
	Dispatch(DivideByMultiple(threadCountX, groupSizeX), DivideByMultiple(threadCountY, groupSizeY), DivideByMultiple(threadCountZ, groupSizeZ));
}

//inline void ComputeContext::DispatchIndirect(GpuBuffer& argumentBuffer, uint64_t argumentBufferOffset /* = 0 */)
//{
//
//}