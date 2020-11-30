#pragma once
#include "stdafx.h"
#include "GpuResource.h"

#define DEFAULT_ALIGN 256

struct DynAlloc
{
	DynAlloc(GpuResource& baseResource, size_t thisOffset, size_t thisSize)
		: buffer(baseResource), offset(thisOffset), size(thisSize)
	{}
	
	GpuResource& buffer;
	size_t offset;
	size_t size;
	void* dataPtr;
	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
};

class LinearAllocationPage : public GpuResource
{
public:
	LinearAllocationPage(ID3D12Resource* pResource, D3D12_RESOURCE_STATES usage) : GpuResource()
	{
		m_pResource.Attach(pResource);
		m_usageState = usage;
		m_gpuVirtualAddress = m_pResource->GetGPUVirtualAddress();
		m_pResource->Map(0, nullptr, &m_cpuVirtualAddress);
	}

	~LinearAllocationPage()
	{
		Unmap();
	}

	void Map()
	{
		if (m_cpuVirtualAddress == nullptr)
		{
			m_pResource->Map(0, nullptr, &m_cpuVirtualAddress);
		}
	}

	void Unmap()
	{
		if (m_cpuVirtualAddress != nullptr)
		{
			m_pResource->Unmap(0, nullptr);
			m_cpuVirtualAddress = nullptr;
		}
	}


	void* m_cpuVirtualAddress;
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuVirtualAddress;
};

enum LinearAllocatorType
{
	kInvalidAllocator = -1,

	kGpuExclusive = 0,
	kCpuWritable = 1,

	kNumAllocatorTypes
};

enum
{
	kGpuAllocatorPageSize = 0x10000, // 64k
	kCpuAllocatorPageSize = 0x200000 // 2MB
};

class LinearAllocatorPageManager
{
public:
	LinearAllocatorPageManager();
	LinearAllocationPage* RequestPage(const std::wstring& name = L"");
	LinearAllocationPage* CreateNewPage(size_t pageSize = 0, const std::wstring& name = L"");

	// Discarded pages will get recycled.  This is for fixed size pages.
	void DiscardPages(uint64_t fenceID, const std::vector<LinearAllocationPage*>& usedPages);

	void FreeLargePages(uint64_t fenceID, const std::vector<LinearAllocationPage*>& pages);

	void Destroy() { m_pagePool.clear(); }

private:
	static LinearAllocatorType s_autoType;

	LinearAllocatorType m_allocationType;
	std::vector <std::unique_ptr<LinearAllocationPage>> m_pagePool;
	std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_retiredPages;
	std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_deletionQueue;
	std::queue<LinearAllocationPage*> m_availablePages;
	std::mutex m_mutex;
};

class LinearAllocator
{
public:
	LinearAllocator(LinearAllocatorType type) 
		: m_allocationType(type), m_pageSize(0), m_currOffset(~(size_t)0), m_currPage(nullptr)
	{
		assert(type > kInvalidAllocator && type < kNumAllocatorTypes);
		m_pageSize = (type == kGpuExclusive ? kGpuAllocatorPageSize : kCpuAllocatorPageSize);
	}

	DynAlloc Allocate(size_t sizeInBytes, const std::wstring& name = L"", size_t alignment = DEFAULT_ALIGN);

	void CleanupUsedPages(uint64_t fenceID);

	static void DestroyAll()
	{
		s_pageManager[0].Destroy();
		s_pageManager[1].Destroy();
	}

private:
	DynAlloc AllocateLargePage(size_t sizeInBytes, const std::wstring& name = L"");

	static LinearAllocatorPageManager s_pageManager[2];

	LinearAllocatorType m_allocationType;
	size_t m_pageSize;
	size_t m_currOffset;
	LinearAllocationPage* m_currPage;
	std::vector<LinearAllocationPage*> m_retiredPages;
	std::vector<LinearAllocationPage*> m_largePageList;
};