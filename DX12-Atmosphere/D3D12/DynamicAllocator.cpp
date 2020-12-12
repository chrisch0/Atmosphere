#include "stdafx.h"
#include "DynamicAllocator.h"
#include "D3D12/CommandListManager.h"

DynamicAllocationPage* DynamicAllocatorManager::RequestPage()
{
	std::lock_guard<std::mutex> lockGuard(m_mutex);

	while (!m_retiredPages.empty() && g_CommandManager.IsFenceComplete(m_retiredPages.front().first))
	{
		m_availablePages.push(m_retiredPages.front().second);
		m_retiredPages.pop();
	}

	DynamicAllocationPage* pagePtr = nullptr;

	if (!m_availablePages.empty())
	{
		pagePtr = m_availablePages.front();
		m_availablePages.pop();
	}
	else
	{
		pagePtr = CreateNewPage(0);
		m_pagePool.emplace_back(pagePtr);
	}
	return pagePtr;
}

DynamicAllocationPage* DynamicAllocatorManager::CreateNewPage(size_t pageSize /* = 0 */)
{
	pageSize = pageSize == 0 ? 0x200000 : pageSize;
	void* pagePtr = (void*)new uint8_t[pageSize];
	return new DynamicAllocationPage(pagePtr);
}

void DynamicAllocatorManager::DiscardPages(uint64_t fenceID, const std::vector<DynamicAllocationPage *>& usedPage)
{
	std::lock_guard<std::mutex> lockGuard(m_mutex);
	for (auto iter = usedPage.begin(); iter != usedPage.end(); ++iter)
	{
		m_retiredPages.push({ fenceID, *iter });
	}
}

void DynamicAllocatorManager::FreeLargePages(uint64_t fenceID, const std::vector<DynamicAllocationPage *>& pages)
{
	std::lock_guard<std::mutex> lockGuard(m_mutex);
	
	while (!m_deletionQueue.empty() && g_CommandManager.IsFenceComplete(m_deletionQueue.front().first))
	{
		delete m_deletionQueue.front().second;
		m_deletionQueue.pop();
	}

	for (auto iter = pages.begin(); iter != pages.end(); ++iter)
	{
		m_deletionQueue.push({ fenceID, *iter });
	}
}

DynamicAllocatorManager DynamicAllocator::s_pageManager;

void DynamicAllocator::CleanupUsedPages(uint64_t fenceID)
{
	if (m_currPage == nullptr)
		return;

	m_retiredPages.push_back(m_currPage);
	m_currPage = nullptr;
	m_currOffset = 0;

	s_pageManager.DiscardPages(fenceID, m_retiredPages);
	m_retiredPages.clear();

	s_pageManager.FreeLargePages(fenceID, m_largePageList);
	m_largePageList.clear();
}

DynMem DynamicAllocator::AllocateLargePage(size_t sizeInBytes)
{
	DynamicAllocationPage* oneOff = s_pageManager.CreateNewPage(sizeInBytes);
	m_largePageList.push_back(oneOff);

	DynMem ret;
	ret.offset = 0;
	ret.dataPtr = oneOff->m_pagePtr;
	ret.size = sizeInBytes;

	return ret;
}

DynMem DynamicAllocator::Allocate(size_t sizeInBytes, size_t alignment /* = 64 */)
{
	const size_t alignmentMask = alignment - 1;
	assert((alignmentMask & alignment) == 0);

	const size_t alignedSize = Math::AlignUpWithMask(sizeInBytes, alignmentMask);

	if (alignedSize > m_pageSize)
		return AllocateLargePage(alignedSize);

	if (m_currOffset + alignedSize > m_pageSize)
	{
		assert(m_currPage != nullptr);
		m_retiredPages.push_back(m_currPage);
		m_currPage = nullptr;
	}
	
	if (m_currPage == nullptr)
	{
		m_currPage = s_pageManager.RequestPage();
		m_currOffset = 0;
	}

	DynMem ret;
	ret.dataPtr = (uint8_t*)m_currPage->m_pagePtr + m_currOffset;
	ret.offset = m_currOffset;
	ret.size = alignedSize;

	m_currOffset += alignedSize;
	
	return ret;
}