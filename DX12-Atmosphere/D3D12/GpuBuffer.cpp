#include "stdafx.h"
#include "GpuBuffer.h"

void GpuBuffer::Create(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& name, uint32_t numElements, uint32_t elementSize, const void* initialData /* = nullptr */)
{
	Destroy();

	m_elementCount = numElements;
	m_elementSize = elementSize;
	m_bufferSize = numElements * elementSize;

	D3D12_RESOURCE_DESC resourceDesc = DescribeBuffer();
	
	m_usageState = D3D12_RESOURCE_STATE_COMMON;

	D3D12_HEAP_PROPERTIES heapProps;
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	ThrowIfFailed(
		device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, 
			&resourceDesc, m_usageState, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_gpuVirtualAddress = m_pResource->GetGPUVirtualAddress();

	if (initialData)
	{
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = initialData;
		subResourceData.RowPitch = m_bufferSize;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition()
	}

}