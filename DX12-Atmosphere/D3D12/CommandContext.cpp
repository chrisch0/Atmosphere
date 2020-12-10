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
	m_dynamicViewDescriptorHeap.CleanupUsedHeaps(fenceValue);
	m_dynamicSamplerDescriptorHeap.CleanupUsedHeaps(fenceValue);

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

void CommandContext::InitializeTexture(GpuResource& dest, uint32_t numSubresources, D3D12_SUBRESOURCE_DATA subData[])
{
	uint64_t uploadBufferSize = GetRequiredIntermediateSize(dest.GetResource(), 0, numSubresources);
	CommandContext& initContext = CommandContext::Begin();

	auto mem = initContext.AllocateUploadBuffer(uploadBufferSize);
	UpdateSubresources(initContext.m_commandList, dest.GetResource(), mem.buffer.GetResource(), 0, 0, numSubresources, subData);
	initContext.TransitionResource(dest, D3D12_RESOURCE_STATE_GENERIC_READ);

	initContext.Finish(true);
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
		assert(m_numBarriersToFlush < 16);
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
	assert(m_numBarriersToFlush < 16);
	auto& barrierDesc = m_resourceBarrierBuffer[m_numBarriersToFlush++];

	barrierDesc = CD3DX12_RESOURCE_BARRIER::UAV(resource.GetResource());

	if (flushImmediate)
		FlushResourceBarriers();
}

void CommandContext::FlushResourceBarriers()
{
	if (m_numBarriersToFlush)
	{
		m_commandList->ResourceBarrier(m_numBarriersToFlush, m_resourceBarrierBuffer);
		m_numBarriersToFlush = 0;
	}
}

void CommandContext::BindDescriptorHeaps()
{
	uint32_t nonNullHeaps = 0;
	ID3D12DescriptorHeap* heapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* heap = m_curDescriptorHeaps[i];
		if (heap != nullptr)
			heapsToBind[nonNullHeaps++] = heap;
	}

	if (nonNullHeaps > 0)
		m_commandList->SetDescriptorHeaps(nonNullHeaps, heapsToBind);
}

void CommandContext::SetPipelineState(const PipelineState& pso)
{
	auto pipelineState = pso.GetPipelineStateObject();
	if (pipelineState == m_curPipelineState)
		return;

	m_commandList->SetPipelineState(pipelineState);
	m_curPipelineState = pipelineState;
}

void CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heapPtr)
{
	if (m_curDescriptorHeaps[type] != heapPtr)
	{
		m_curDescriptorHeaps[type] = heapPtr;
		BindDescriptorHeaps();
	}
}

void CommandContext::SetDescriptorHeaps(uint32_t heapCount, D3D12_DESCRIPTOR_HEAP_TYPE type[], ID3D12DescriptorHeap* heapPtrs[])
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

void GraphicsContext::SetRootSignature(const RootSignature& rootSig)
{
	if (rootSig.GetSignature() == m_curGraphicsRootSignature)
		return;

	m_commandList->SetGraphicsRootSignature(m_curGraphicsRootSignature = rootSig.GetSignature());

	m_dynamicViewDescriptorHeap.ParseGraphicsRootSignature(rootSig);
	m_dynamicSamplerDescriptorHeap.ParseGraphicsRootSignature(rootSig);
}

void GraphicsContext::SetRenderTargets(uint32_t numRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[])
{
	m_commandList->OMSetRenderTargets(numRTVs, rtvs, false, nullptr);
}

void GraphicsContext::SetRenderTargets(uint32_t numRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[], D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	m_commandList->OMSetRenderTargets(numRTVs, rtvs, false, &dsv);
}

void GraphicsContext::SetViewport(const D3D12_VIEWPORT& vp)
{
	m_commandList->RSSetViewports(1, &vp);
}

void GraphicsContext::SetScissor(const D3D12_RECT& rect)
{
	m_commandList->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect)
{
	m_commandList->RSSetViewports(1, &vp);
	m_commandList->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetBlendFactor(Color blendFactor)
{
	m_commandList->OMSetBlendFactor(blendFactor.GetPtr());
}

void GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
{
	m_commandList->IASetPrimitiveTopology(topology);
}

void GraphicsContext::SetConstantArray(uint32_t rootIndex, uint32_t numConstants, const void* pConstants)
{
	m_commandList->SetGraphicsRoot32BitConstants(rootIndex, numConstants, pConstants, 0);
}

void GraphicsContext::SetConstant(uint32_t rootEntry, DWParam val, uint32_t offset)
{
	m_commandList->SetGraphicsRoot32BitConstant(rootEntry, val.val_ui, offset);
}

void GraphicsContext::SetConstants(uint32_t rootIndex, DWParam x)
{
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, x.val_ui, 0);
}

void GraphicsContext::SetConstants(uint32_t rootIndex, DWParam x, DWParam y)
{
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, x.val_ui, 0);
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, y.val_ui, 1);
}

void GraphicsContext::SetConstants(uint32_t rootIndex, DWParam x, DWParam y, DWParam z)
{
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, x.val_ui, 0);
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, y.val_ui, 1);
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, z.val_ui, 2);
}

void GraphicsContext::SetConstants(uint32_t rootIndex, DWParam x, DWParam y, DWParam z, DWParam w)
{
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, x.val_ui, 0);
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, y.val_ui, 1);
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, z.val_ui, 2);
	m_commandList->SetGraphicsRoot32BitConstant(rootIndex, w.val_ui, 3);
}

void GraphicsContext::SetConstantBuffer(uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV)
{
	m_commandList->SetGraphicsRootConstantBufferView(rootIndex, CBV);
}

void GraphicsContext::SetDynamicConstantBufferView(uint32_t rootIndex, size_t bufferSize, const void* bufferData)
{
	assert(bufferData != nullptr);
	DynAlloc cb = m_cpuLinearAllocator.Allocate(bufferSize);
	//SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
	memcpy(cb.dataPtr, bufferData, bufferSize);
	m_commandList->SetGraphicsRootConstantBufferView(rootIndex, cb.gpuAddress);
}

void GraphicsContext::SetBufferSRV(uint32_t rootIndex, const GpuBuffer& SRV, UINT64 offset /* = 0 */)
{
	assert((SRV.m_usageState & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) != 0);
	m_commandList->SetGraphicsRootShaderResourceView(rootIndex, SRV.GetGpuVirtualAddress() + offset);
}

void GraphicsContext::SetBufferUAV(uint32_t rootIndex, const GpuBuffer& UAV, UINT64 offset /* = 0 */)
{
	assert((UAV.m_usageState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0);
	m_commandList->SetGraphicsRootUnorderedAccessView(rootIndex, UAV.GetGpuVirtualAddress() + offset);
}

void GraphicsContext::SetDescriptorTable(uint32_t rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE firstHandle)
{
	m_commandList->SetGraphicsRootDescriptorTable(rootIndex, firstHandle);
}

void GraphicsContext::SetDynamicDescriptor(uint32_t rootIndex, uint32_t offset, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	SetDynamicDescriptors(rootIndex, offset, 1, &handle);
}

void GraphicsContext::SetDynamicDescriptors(uint32_t rootIndex, uint32_t offset, uint32_t count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
{
	m_dynamicViewDescriptorHeap.SetGraphicsDescriptorHandles(rootIndex, offset, count, handles);
}

void GraphicsContext::SetDynamicSampler(uint32_t rootIndex, uint32_t offset, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	m_dynamicSamplerDescriptorHeap.SetGraphicsDescriptorHandles(rootIndex, offset, 1, &handle);
}

void GraphicsContext::SetDynamicSamplers(uint32_t rootIndex, uint32_t offset, uint32_t count, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
{
	m_dynamicSamplerDescriptorHeap.SetGraphicsDescriptorHandles(rootIndex, offset, count, handles);
}

void GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView)
{
	m_commandList->IASetIndexBuffer(&ibView);
}

void GraphicsContext::SetVertexBuffer(uint32_t slot, const D3D12_VERTEX_BUFFER_VIEW& vbView)
{
	SetVertexBuffers(slot, 1, &vbView);
}

void GraphicsContext::SetVertexBuffers(uint32_t startSlot, uint32_t count, const D3D12_VERTEX_BUFFER_VIEW vbViews[])
{
	m_commandList->IASetVertexBuffers(startSlot, count, vbViews);
}

void GraphicsContext::SetDynamicVB(uint32_t slot, size_t numVertices, size_t vertexStride, const void* vbData)
{
	assert(vbData != nullptr);

	size_t bufferSize = numVertices * vertexStride;
	auto vb = m_cpuLinearAllocator.Allocate(bufferSize);

	memcpy(vb.dataPtr, vbData, bufferSize);

	D3D12_VERTEX_BUFFER_VIEW VBView;
	VBView.BufferLocation = vb.gpuAddress;
	VBView.SizeInBytes = (uint32_t)bufferSize;
	VBView.StrideInBytes = (uint32_t)vertexStride;

	m_commandList->IASetVertexBuffers(slot, 1, &VBView);
}

void GraphicsContext::SetDynamicIB(size_t indexCount, const uint16_t* ibData)
{
	assert(ibData != nullptr);

	size_t bufferSize = indexCount * sizeof(uint16_t);
	auto ib = m_cpuLinearAllocator.Allocate(bufferSize);

	memcpy(ib.dataPtr, ibData, bufferSize);

	D3D12_INDEX_BUFFER_VIEW IBView;
	IBView.BufferLocation = ib.gpuAddress;
	IBView.SizeInBytes = (uint32_t)(indexCount * sizeof(uint16_t));
	IBView.Format = DXGI_FORMAT_R16_UINT;

	m_commandList->IASetIndexBuffer(&IBView);
}

void GraphicsContext::Draw(uint32_t vertexCount, uint32_t vertexStartOffset /* = 0 */)
{
	DrawInstanced(vertexCount, 1, vertexStartOffset, 0);
}

void GraphicsContext::DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation /* = 0 */, uint32_t startInstanceLocation /* = 0 */)
{
	FlushResourceBarriers();
	m_dynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_commandList);
	m_dynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_commandList);
	m_commandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

void GraphicsContext::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation /* = 0 */, INT baseVertexLocation /* = 0 */)
{
	DrawIndexedInstanced(indexCount, 1, startIndexLocation, baseVertexLocation, 0);
}

void GraphicsContext::DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, INT baseVertexLocation, uint32_t startInstanceLocation)
{
	FlushResourceBarriers();
	m_dynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_commandList);
	m_dynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_commandList);
	m_commandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}