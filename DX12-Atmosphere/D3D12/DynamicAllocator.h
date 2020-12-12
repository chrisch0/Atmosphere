#pragma once
#include "stdafx.h"

struct DynMem
{
	size_t offset;
	size_t size;
	void* dataPtr;
};

class DynamicAllocationPage
{
	friend class DynamicAllocator;
public:
	DynamicAllocationPage(void* ptr)
	{
		m_pagePtr = ptr;
	}

	~DynamicAllocationPage()
	{
		delete[] m_pagePtr;
	}
private:
	void* m_pagePtr;
};

class DynamicAllocatorManager
{
public: 
	DynamicAllocatorManager() {}
	DynamicAllocationPage* RequestPage();
	DynamicAllocationPage* CreateNewPage(size_t pageSize = 0);

	void DiscardPages(uint64_t fenceID, const std::vector<DynamicAllocationPage*>& usedPage);

	void FreeLargePages(uint64_t fenceID, const std::vector<DynamicAllocationPage*>& pages);

	void Destroy() { m_pagePool.clear(); }
private:
	std::vector<std::unique_ptr<DynamicAllocationPage>> m_pagePool;
	std::queue<std::pair<uint64_t, DynamicAllocationPage*>> m_retiredPages;
	std::queue<std::pair<uint64_t, DynamicAllocationPage*>> m_deletionQueue;
	std::queue<DynamicAllocationPage*> m_availablePages;
	std::mutex m_mutex;
};

class DynamicAllocator
{
public:
	DynamicAllocator() : m_pageSize(0x200000), m_currOffset(0), m_currPage(nullptr)
	{

	}

	DynMem Allocate(size_t sizeInBytes, size_t alignment = 256);

	void CleanupUsedPages(uint64_t fenceID);

	static void Destroy()
	{
		s_pageManager.Destroy();
	}
private:
	DynMem AllocateLargePage(size_t sizeInBytes);

	static DynamicAllocatorManager s_pageManager;

	size_t m_pageSize;
	size_t m_currOffset;
	DynamicAllocationPage* m_currPage;
	std::vector<DynamicAllocationPage*> m_retiredPages;
	std::vector<DynamicAllocationPage*> m_largePageList;
};