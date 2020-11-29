#include "stdafx.h"
#include "LinearAllocator.h"
#include "CommandContext.h"
#include "CommandListManager.h"

LinearAllocatorType LinearAllocatorPageManager::s_autoType = kGpuExclusive;

LinearAllocatorPageManager::LinearAllocatorPageManager()
{
	m_allocationType = s_autoType;
	s_autoType = (LinearAllocatorType)(s_autoType + 1);
	assert(s_autoType <= kNumAllocatorTypes);
}

LinearAllocationPage* LinearAllocatorPageManager::RequestPage(const std::wstring& name /* = L"" */)
{
	std::lock_guard<std::mutex> lockGuard(m_mutex);

	while (!m_retiredPages.empty() && g_CommandManager.IsFenceComplete(m_retiredPages.front().first))
	{
		m_availablePages.push(m_retiredPages.front().second);
		m_retiredPages.pop();
	}

	LinearAllocationPage* pagePtr = nullptr;

	if (!m_availablePages.empty())
	{
		pagePtr = m_availablePages.front();
		m_availablePages.pop();
	}
	else
	{
		pagePtr = CreateNewPage(0, name);
		m_pagePool.emplace_back(pagePtr);
	}

	return pagePtr;
}

LinearAllocationPage* LinearAllocatorPageManager::CreateNewPage(size_t pageSize /* = 0 */, const std::wstring& name /* = L"" */)
{
	D3D12_HEAP_TYPE heapType;
	int bufferWidth = 0;
	D3D12_RESOURCE_FLAGS flag;

	D3D12_RESOURCE_STATES defaultUsage;

	if (m_allocationType == kGpuExclusive)
	{
		heapType = D3D12_HEAP_TYPE_DEFAULT;
		bufferWidth = pageSize == 0 ? kGpuAllocatorPageSize : pageSize;
		flag = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		defaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	else
	{
		heapType = D3D12_HEAP_TYPE_UPLOAD;
		bufferWidth = pageSize == 0 ? kCpuAllocatorPageSize : pageSize;
		flag = D3D12_RESOURCE_FLAG_NONE;
		defaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;

	}

	CD3DX12_HEAP_PROPERTIES heapProps(heapType);

	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferWidth, flag);

	ID3D12Resource* pBuffer;
	ThrowIfFailed(g_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
		&resourceDesc, defaultUsage, nullptr, IID_PPV_ARGS(&pBuffer)));

	pBuffer->SetName((L"LinearAllocator Page" + name).data());

	return new LinearAllocationPage(pBuffer, defaultUsage);
}

void LinearAllocatorPageManager::DiscardPages(uint64_t fenceID, const std::vector<LinearAllocationPage *>& usedPages)
{
	std::lock_guard<std::mutex> lockGuard(m_mutex);
	for (auto iter = usedPages.begin(); iter != usedPages.end(); ++iter)
		m_retiredPages.push({ fenceID, *iter });
}

void LinearAllocatorPageManager::FreeLargePages(uint64_t fenceID, const std::vector<LinearAllocationPage *>& pages)
{
	std::lock_guard<std::mutex> lockGuard(m_mutex);

	while (!m_deletionQueue.empty() && g_CommandManager.IsFenceComplete(m_deletionQueue.front().first))
	{
		delete m_deletionQueue.front().second;
		m_deletionQueue.pop();
	}

	for (auto iter = pages.begin(); iter < pages.end(); ++iter)
	{
		(*iter)->Unmap();
		m_deletionQueue.push({ fenceID, *iter });
	}
}

LinearAllocatorPageManager LinearAllocator::s_pageManager[2];

void LinearAllocator::CleanupUsedPages(uint64_t fenceID)
{
	if (m_currPage == nullptr)
		return;

	m_retiredPages.push_back(m_currPage);
	m_currPage = nullptr;
	m_currOffset = 0;

	s_pageManager[m_allocationType].DiscardPages(fenceID, m_retiredPages);
	m_retiredPages.clear();

	s_pageManager[m_allocationType].FreeLargePages(fenceID, m_largePageList);
	m_largePageList.clear();
}

DynAlloc LinearAllocator::AllocateLargePage(size_t sizeInBytes, const std::wstring& name /* = L"" */)
{
	LinearAllocationPage* oneOff = s_pageManager[m_allocationType].CreateNewPage(sizeInBytes, name);
	m_largePageList.push_back(oneOff);

	DynAlloc ret(*oneOff, 0, sizeInBytes);
	ret.dataPtr = oneOff->m_cpuVirtualAddress;
	ret.gpuAddress = oneOff->m_gpuVirtualAddress;

	return ret;
}

DynAlloc LinearAllocator::Allocate(size_t sizeInBytes, size_t alignment /* = DEFAULT_ALIGN */, const std::wstring& name /* = L"" */)
{
	const size_t aligmentMask = alignment - 1;
	assert((aligmentMask & alignment) == 0);

	const size_t alignedSize = Math::AlignUpWithMask(sizeInBytes, aligmentMask);

	if (alignedSize > m_pageSize)
		return AllocateLargePage(alignedSize);

	m_currOffset = Math::AlignUp(m_currOffset, alignment);

	if (m_currOffset + alignedSize > m_pageSize)
	{
		assert(m_currPage != nullptr);
		m_retiredPages.push_back(m_currPage);
		m_currPage = nullptr;
	}

	if (m_currPage == nullptr)
	{
		m_currPage = s_pageManager[m_allocationType].RequestPage(name);
		m_currOffset = 0;
	}

	DynAlloc ret(*m_currPage, m_currOffset, alignedSize);
	ret.dataPtr = (uint8_t*)m_currPage->m_cpuVirtualAddress + m_currOffset;
	ret.gpuAddress = m_currPage->m_gpuVirtualAddress + m_currOffset;
	
	m_currOffset += alignedSize;

	return ret;
}