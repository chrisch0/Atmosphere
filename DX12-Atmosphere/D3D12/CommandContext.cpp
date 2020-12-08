#include "stdafx.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "PipelineState.h"

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
	m_dynamicViewDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_dynamicSamplerDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
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

void CommandContext::InitializeBuffer(GpuResource& dest, const void* bufferData, size_t numBytes, size_t offset, const std::wstring& name /* = L""*/)
{
	CommandContext& initContext = CommandContext::Begin();

	// Create upload buffer
	auto uploadBuffer = initContext.AllocateUploadBuffer(numBytes);

	// Memory copy data to upload buffer
	memcpy(uploadBuffer.dataPtr, bufferData, numBytes);

	// Copy data from upload buffer to default buffer
	initContext.TransitionResource(dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
	initContext.m_commandList->CopyBufferRegion(dest.GetResource(), offset, uploadBuffer.buffer.GetResource(), 0, numBytes);
	initContext.TransitionResource(dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

	initContext.Flush(true);
}

void CommandContext::TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES newState, bool flushImmediate /* = false */)
{
	D3D12_RESOURCE_STATES oldState = resource.m_usageState;

	if (m_type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
	{
		assert((oldState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == oldState);
		assert((newState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == newState);
	}

	if (oldState != newState)
	{
		assert(m_numBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
		auto& barrierDesc = m_resourceBarrierBuffer[m_numBarriersToFlush++];

		barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(resource.GetResource(), oldState, newState);

		// Check to see if we already started the transition
		if (newState == resource.m_transitioningState)
		{
			barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
			resource.m_transitioningState = (D3D12_RESOURCE_STATES)-1;
		}
		else
			barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		resource.m_usageState = newState;
	}
	else if (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		InsertUAVBarrier(resource, flushImmediate);

	if (flushImmediate || m_numBarriersToFlush == 16)
		FlushResourceBarriers();
}

void CommandContext::InsertUAVBarrier(GpuResource& resource, bool flushImmediate /* = false */)
{
	assert(m_numBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
	auto& barrierDesc = m_resourceBarrierBuffer[m_numBarriersToFlush++];

	barrierDesc = CD3DX12_RESOURCE_BARRIER::UAV(resource.GetResource());

	if (flushImmediate)
		FlushResourceBarriers();
}

inline void CommandContext::FlushResourceBarriers()
{
	if (m_numBarriersToFlush)
	{
		m_commandList->ResourceBarrier(m_numBarriersToFlush, m_resourceBarrierBuffer);
		m_numBarriersToFlush = 0;
	}
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

inline void CommandContext::SetPipelineState(const PipelineState& pso)
{
	auto pipelineState = pso.GetPipelineStateObject();
	if (pipelineState == m_curPipelineState)
		return;

	m_commandList->SetPipelineState(pipelineState);
	m_curPipelineState = pipelineState;
}

inline void CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heapPtr)
{
	if (m_curDescriptorHeaps[type] != heapPtr)
	{
		m_curDescriptorHeaps[type] = heapPtr;
		BindDescriptorHeaps();
	}
}

inline void CommandContext::SetDescriptorHeaps(uint32_t heapCount, D3D12_DESCRIPTOR_HEAP_TYPE type[], ID3D12DescriptorHeap* heapPtrs[])
{
	bool anyChanged = false;
	for (uint32_t i = 0; i < heapCount; ++i)
	{
		if (m_curDescriptorHeaps[type[i]] != heapPtrs[i])
		{
			m_curDescriptorHeaps[type[i]] = heapPtrs[i];
			anyChanged = true;
		}
	}

	if (anyChanged)
		BindDescriptorHeaps();
}

void GraphicsContext::ClearUAV(GpuBuffer& target)
{
	// After binding a UAV, we can get a GPU handle that is required to clear it as a UAV (because it essentially runs
	// a shader to set all of the values).
	D3D12_GPU_DESCRIPTOR_HANDLE gpuVisibleHandle = m_dynamicViewDescriptorHeap.UploadDirect(target.GetUAV());
	const uint32_t clearColor[4] = {};
	m_commandList->ClearUnorderedAccessViewUint(gpuVisibleHandle, target.GetUAV(), target.GetResource(), clearColor, 0, nullptr);
}

void GraphicsContext::ClearUAV(ColorBuffer& target)
{
	D3D12_GPU_DESCRIPTOR_HANDLE gpuVisibleHandle = m_dynamicViewDescriptorHeap.UploadDirect(target.GetUAV());
	CD3DX12_RECT clearRect(0, 0, (long)target.GetWidth(), (long)target.GetHeight());

	const float* clearColor = target.GetClearColor().GetPtr();
	m_commandList->ClearUnorderedAccessViewFloat(gpuVisibleHandle, target.GetUAV(), target.GetResource(), clearColor, 1, &clearRect);
}

void GraphicsContext::ClearColor(ColorBuffer& target)
{
	m_commandList->ClearRenderTargetView(target.GetRTV(), target.GetClearColor().GetPtr(), 0, nullptr);
}

inline void GraphicsContext::SetRootSignature(const RootSignature& rootSig)
{
	if (rootSig.GetSignature() == m_curGraphicsRootSignature)
		return;

	m_commandList->SetGraphicsRootSignature(m_curGraphicsRootSignature = rootSig.GetSignature());

	m_dynamicViewDescriptorHeap.ParseGraphicsRootSignature(rootSig);
	m_dynamicSamplerDescriptorHeap.ParseGraphicsRootSignature(rootSig);
}

inline void GraphicsContext::SetRenderTargets(UINT numRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[])
{
	m_commandList->OMSetRenderTargets(numRTVs, rtvs, false, nullptr);
}

inline void GraphicsContext::SetRenderTargets(UINT numRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[], D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	m_commandList->OMSetRenderTargets(numRTVs, rtvs, false, &dsv);
}

inline void GraphicsContext::SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect)
{
	m_commandList->RSSetViewports(1, &vp);
	m_commandList->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetDynamicDescriptor(UINT rootIndex, UINT offset, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	SetDynamicDescriptors(rootIndex, offset, 1, &handle);
}

void GraphicsContext::SetDynamicDescriptors(UINT rootIndex, UINT offset, UINT count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
{
	m_dynamicViewDescriptorHeap.SetGraphicsDescriptorHandles(rootIndex, offset, count, handles);
}

void GraphicsContext::SetDynamicSampler(UINT rootIndex, UINT offset, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	m_dynamicSamplerDescriptorHeap.SetGraphicsDescriptorHandles(rootIndex, offset, 1, &handle);
}

void GraphicsContext::SetDynamicSamplers(UINT rootIndex, UINT offset, UINT count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
{
	m_dynamicSamplerDescriptorHeap.SetGraphicsDescriptorHandles(rootIndex, offset, count, handles);
}

inline void GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView)
{
	m_commandList->IASetIndexBuffer(&ibView);
}

inline void GraphicsContext::SetVertexBuffer(UINT slot, const D3D12_VERTEX_BUFFER_VIEW& vbView)
{
	SetVertexBuffers(slot, 1, &vbView);
}

inline void GraphicsContext::SetVertexBuffers(UINT startSlot, UINT count, const D3D12_VERTEX_BUFFER_VIEW vbViews[])
{
	m_commandList->IASetVertexBuffers(startSlot, count, vbViews);
}

inline void GraphicsContext::SetDynamicVB(UINT slot, size_t numVertices, size_t vertexStride, const void* vbData)
{
	assert(vbData != nullptr);

	size_t bufferSize = numVertices * vertexStride;
	auto vb = m_cpuLinearAllocator.Allocate(bufferSize);

	memcpy(vb.dataPtr, vbData, bufferSize);

	D3D12_VERTEX_BUFFER_VIEW VBView;
	VBView.BufferLocation = vb.gpuAddress;
	VBView.SizeInBytes = (UINT)bufferSize;
	VBView.StrideInBytes = (UINT)vertexStride;

	m_commandList->IASetVertexBuffers(slot, 1, &VBView);
}

inline void GraphicsContext::SetDynamicIB(size_t indexCount, const uint16_t* ibData)
{
	assert(ibData != nullptr);

	size_t bufferSize = indexCount * sizeof(uint16_t);
	auto ib = m_cpuLinearAllocator.Allocate(bufferSize);

	memcpy(ib.dataPtr, ibData, bufferSize);

	D3D12_INDEX_BUFFER_VIEW IBView;
	IBView.BufferLocation = ib.gpuAddress;
	IBView.SizeInBytes = (UINT)(indexCount * sizeof(uint16_t));
	IBView.Format = DXGI_FORMAT_R16_UINT;

	m_commandList->IASetIndexBuffer(&IBView);
}

inline void GraphicsContext::Draw(UINT vertexCount, UINT vertexStartOffset /* = 0 */)
{
	DrawInstanced(vertexCount, 1, vertexStartOffset, 0);
}

inline void GraphicsContext::DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation /* = 0 */, UINT startInstanceLocation /* = 0 */)
{
	FlushResourceBarriers();
	m_dynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_commandList);
	m_dynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_commandList);
	m_commandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

inline void GraphicsContext::DrawIndexed(UINT indexCount, UINT startIndexLocation /* = 0 */, INT baseVertexLocation /* = 0 */)
{
	DrawIndexedInstanced(indexCount, 1, startIndexLocation, baseVertexLocation, 0);
}

inline void GraphicsContext::DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation, INT baseVertexLocation, UINT startInstanceLocation)
{
	FlushResourceBarriers();
	m_dynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_commandList);
	m_dynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_commandList);
	m_commandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}