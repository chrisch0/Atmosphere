#pragma once
#include "stdafx.h"
#include "GpuResource.h"

class GpuBuffer : public GpuResource
{
public:
	~GpuBuffer() { Destroy(); }

	void Create(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& name, uint32_t numElements, uint32_t elementSize, const void* initialData = nullptr);

	void CreatePlaced(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& name, ID3D12Heap* pBackingHeap, uint32_t heapOffset, uint32_t numElements, uint32_t elementSize,
		const void* initialData = nullptr);

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_SRV; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV() const { return m_UAV; }

	D3D12_GPU_VIRTUAL_ADDRESS RootConstantBufferView() const { return m_gpuVirtualAddress; }

	D3D12_CPU_DESCRIPTOR_HANDLE CreateConstantBufferView(uint32_t offset, uint32_t size) const;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t offset, uint32_t size, uint32_t stride) const;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t baseVertexIndex = 0) const
	{
		size_t offset = baseVertexIndex * m_elementSize;
		return VertexBufferView(offset, (uint32_t)(m_bufferSize - offset), m_elementSize);
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t offset, uint32_t size, bool b32Bit = false) const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t startIndex = 0) const
	{
		size_t offset = startIndex * m_elementSize;
		return IndexBufferView(offset, (uint32_t)(m_bufferSize - offset), m_elementSize == 4);
	}

	size_t GetBufferSize() const { return m_bufferSize; }
	size_t GetElementCount() const { return m_elementCount; }
	size_t GetElementSize() const { return m_bufferSize; }

protected:

	GpuBuffer() : m_bufferSize(0), m_elementCount(0), m_elementSize(0)
	{
		m_resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		m_UAV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_SRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	D3D12_RESOURCE_DESC DescribeBuffer();
	virtual void CreateDerivedViews() = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE m_UAV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRV;

	size_t m_bufferSize;
	uint32_t m_elementCount;
	uint32_t m_elementSize;
	D3D12_RESOURCE_FLAGS m_resourceFlags;
};
