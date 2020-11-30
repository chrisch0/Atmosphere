#include "stdafx.h"
#include "GpuBuffer.h"
#include "CommandContext.h"

void GpuBuffer::Create(const std::wstring& name, uint32_t numElements, uint32_t elementSize, const void* initialData /* = nullptr */)
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
		g_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, 
			&resourceDesc, m_usageState, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_gpuVirtualAddress = m_pResource->GetGPUVirtualAddress();

	if (initialData)
	{
		CommandContext::InitializeBuffer(*this, initialData, m_bufferSize, 0, name);
	}

#ifdef RELEASE
	(name)
#else
	m_pResource->SetName(name.c_str());
#endif

	CreateDerivedViews();
}

// Sub-Allocate a buffer out of a pre-allocated heap.  If initial data is provided, it will be copied into the buffer using the default command context.
void GpuBuffer::CreatePlaced(const std::wstring& name, ID3D12Heap* pBackingHeap, uint32_t heapOffset, uint32_t numElements, uint32_t elementSize, const void* initialData /* = nullptr */)
{
	m_elementCount = numElements;
	m_elementSize = elementSize;
	m_bufferSize = numElements * elementSize;

	D3D12_RESOURCE_DESC resourceDesc = DescribeBuffer();

	m_usageState = D3D12_RESOURCE_STATE_COMMON;

	ThrowIfFailed(g_Device->CreatePlacedResource(pBackingHeap, heapOffset, &resourceDesc, m_usageState, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_gpuVirtualAddress = m_pResource->GetGPUVirtualAddress();

	if (initialData)
		CommandContext::InitializeBuffer(*this, initialData, m_bufferSize);

#ifdef RELEASE
	(name);
#else
	m_pResource->SetName(name.c_str());
#endif

	CreateDerivedViews();
}

D3D12_CPU_DESCRIPTOR_HANDLE GpuBuffer::CreateConstantBufferView(uint32_t offset, uint32_t size) const
{
	assert(size + offset <= m_bufferSize);
	size = Math::AlignUp(size, 16);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = m_gpuVirtualAddress + (size_t)offset;
	cbvDesc.SizeInBytes = size;

	D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateConstantBufferView(&cbvDesc, cbvHandle);
	return cbvHandle;
}

D3D12_RESOURCE_DESC GpuBuffer::DescribeBuffer()
{
	assert(m_bufferSize != 0);

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer((UINT64)m_bufferSize, m_resourceFlags);

	return desc;
}

void ByteAddressBuffer::CreateDerivedViews()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = (UINT)m_bufferSize / 4;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	if (m_SRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_SRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(m_pResource.Get(), &srvDesc, m_SRV);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.Buffer.NumElements = (UINT)m_bufferSize / 4;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	if (m_UAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_UAV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateUnorderedAccessView(m_pResource.Get(), nullptr, &uavDesc, m_UAV);
}

void StructuredBuffer::CreateDerivedViews()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = m_elementCount;
	srvDesc.Buffer.StructureByteStride = m_elementSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	if (m_SRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_SRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(m_pResource.Get(), &srvDesc, m_SRV);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.NumElements = m_elementCount;
	uavDesc.Buffer.StructureByteStride = m_elementSize;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	m_counterBuffer.Create(L"StructuredBuffer::Counter", 1, 4);

	if (m_UAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_UAV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateUnorderedAccessView(m_pResource.Get(), m_counterBuffer.GetResource(), &uavDesc, m_UAV);
}

const D3D12_CPU_DESCRIPTOR_HANDLE& StructuredBuffer::GetCounterSRV(CommandContext& Context)
{
	Context.TransitionResource(m_counterBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
	return m_counterBuffer.GetSRV();
}

const D3D12_CPU_DESCRIPTOR_HANDLE& StructuredBuffer::GetCounterUAV(CommandContext& Context)
{
	Context.TransitionResource(m_counterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	return m_counterBuffer.GetUAV();
}