#include "stdafx.h"
#include "DynamicDescriptorHeap.h"
#include "CommandListManager.h"
#include "CommandContext.h"

std::mutex DynamicDescriptorHeap::s_mutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DynamicDescriptorHeap::s_descriptorHeapPool[2];
std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> DynamicDescriptorHeap::s_retiredDescriptorHeaps[2];
std::queue<ID3D12DescriptorHeap*> DynamicDescriptorHeap::s_availableDescriptorHeaps[2];

ID3D12DescriptorHeap* DynamicDescriptorHeap::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	std::lock_guard<std::mutex> lockGuard(s_mutex);

	uint32_t idx = heapType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? 1 : 0;

	while (!s_retiredDescriptorHeaps[idx].empty() && g_CommandManager.IsFenceComplete(s_retiredDescriptorHeaps[idx].front().first))
	{
		s_availableDescriptorHeaps[idx].push(s_retiredDescriptorHeaps[idx].front().second);
		s_retiredDescriptorHeaps[idx].pop();
	}

	if (!s_availableDescriptorHeaps[idx].empty())
	{
		ID3D12DescriptorHeap* heapPtr = s_availableDescriptorHeaps[idx].front();
		s_availableDescriptorHeaps[idx].pop();
		return heapPtr;
	}
	else
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = heapType;
		heapDesc.NumDescriptors = kNumDescriptorsPerHeap;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 1;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heapPtr;
		ThrowIfFailed(g_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heapPtr)));
		s_descriptorHeapPool[idx].emplace_back(heapPtr);
		return heapPtr.Get();
	}
}

void DynamicDescriptorHeap::DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint64_t fenceValueForReset, const std::vector<ID3D12DescriptorHeap *>& usedHeaps)
{
	uint32_t idx = heapType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ? 1 : 0;
	std::lock_guard<std::mutex> lockGuard(s_mutex);
	for (auto iter = usedHeaps.begin(); iter != usedHeaps.end(); ++iter)
		s_retiredDescriptorHeaps[idx].push({fenceValueForReset, *iter});
}

DynamicDescriptorHeap::DynamicDescriptorHeap(CommandContext& owningContext, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
	: m_owningContext(owningContext), m_descriptorType(heapType)
{
	m_curHeapPtr = nullptr;
	m_curOffset = 0;
	m_descriptorSize = g_Device->GetDescriptorHandleIncrementSize(heapType);
}

DynamicDescriptorHeap::~DynamicDescriptorHeap()
{

}

inline ID3D12DescriptorHeap* DynamicDescriptorHeap::GetHeapPointer()
{
	if (m_curHeapPtr == nullptr)
	{
		assert(m_curOffset == 0);
		m_curHeapPtr = RequestDescriptorHeap(m_descriptorType);
		m_firstDescriptor = DescriptorHandle(m_curHeapPtr->GetCPUDescriptorHandleForHeapStart(), m_curHeapPtr->GetGPUDescriptorHandleForHeapStart());
	}
	return m_curHeapPtr;
}

void DynamicDescriptorHeap::RetireCurrentHeap()
{
	if (m_curOffset == 0)
	{
		assert(m_curHeapPtr == nullptr);
		return;
	}
	assert(m_curHeapPtr != nullptr);
	m_retiredHeaps.push_back(m_curHeapPtr);
	m_curHeapPtr = nullptr;
	m_curOffset = 0;
}

void DynamicDescriptorHeap::UnbindAllValid()
{
	m_graphicsHandleCache.UnbindAllValid();
	m_computeHandleCache.UnbindAllValid();
}

void DynamicDescriptorHeap::RetireUsedHeaps(uint64_t fenceValue)
{
	DiscardDescriptorHeaps(m_descriptorType, fenceValue, m_retiredHeaps);
	m_retiredHeaps.clear();
}

void DynamicDescriptorHeap::CleanupUsedHeaps(uint64_t fenceValue)
{
	RetireCurrentHeap();
	RetireUsedHeaps(fenceValue);
	m_graphicsHandleCache.ClearCache();
	m_computeHandleCache.ClearCache();
}

void DynamicDescriptorHeap::CopyAndBindStagedTables(DescriptorHandleCache& handleCache, ID3D12GraphicsCommandList* cmdList, void (ID3D12GraphicsCommandList::*SetFunc)(uint32_t, D3D12_GPU_DESCRIPTOR_HANDLE))
{
	uint32_t neededSize = handleCache.ComputeStagedSize();
	if (!HasSpace(neededSize))
	{
		RetireCurrentHeap();
		UnbindAllValid();
		neededSize = handleCache.ComputeStagedSize();
	}

	m_owningContext.SetDescriptorHeap(m_descriptorType, GetHeapPointer());
	handleCache.CopyAndBindStaleTables(m_descriptorType, m_descriptorSize, Allocate(neededSize), cmdList, SetFunc);
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::UploadDirect(D3D12_CPU_DESCRIPTOR_HANDLE handles)
{
	if (!HasSpace(1))
	{
		RetireCurrentHeap();
		UnbindAllValid();
	}

	m_owningContext.SetDescriptorHeap(m_descriptorType, GetHeapPointer());

	DescriptorHandle destHandle = m_firstDescriptor + m_curOffset * m_descriptorSize;
	m_curOffset += 1;

	g_Device->CopyDescriptorsSimple(1, destHandle.GetCpuHandle(), handles, m_descriptorType);

	return destHandle.GetGpuHandle();
}

void DynamicDescriptorHeap::DescriptorHandleCache::ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE type, const RootSignature& rootSig)
{
	uint32_t currentOffset = 0;

	assert(rootSig.m_numParameters <= 16);

	m_staleRootParamsBitMap = 0;
	m_rootDescriptorTablesBitMap = (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? rootSig.m_samplerTableBitMap : rootSig.m_descriptorTableBitMap);

	// record descriptor handle counts and copy dest in m_handleCache in the descriptor table cache
	unsigned long tableParams = m_rootDescriptorTablesBitMap;
	unsigned long rootIndex;
	while (_BitScanReverse(&rootIndex, tableParams))
	{
		tableParams ^= (1 << rootIndex);

		uint32_t tableSize = rootSig.m_descriptorTableSize[rootIndex];
		assert(tableSize > 0);

		DescriptorTableCache& rootDescriptorTable = m_rootDescriptorTable[rootIndex];
		rootDescriptorTable.assignedHandlesBitMap = 0;
		rootDescriptorTable.tableStart = m_handleCache + currentOffset;
		rootDescriptorTable.tableSize = tableSize;

		currentOffset += tableSize;
	}

	m_maxCachedDescriptors = currentOffset;
	assert(m_maxCachedDescriptors <= kMaxNumDescriptors);
}

void DynamicDescriptorHeap::DescriptorHandleCache::StageDescriptorHandles(uint32_t rootIndex, uint32_t offset, uint32_t numHandles, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
{
	assert(((1 << rootIndex) & m_rootDescriptorTablesBitMap) != 0);
	assert(offset + numHandles <= m_rootDescriptorTable[rootIndex].tableSize);

	// copy handles to m_handleCache according to the start location record in ParseRootSignature and the offset
	DescriptorTableCache& tableCache = m_rootDescriptorTable[rootIndex];
	D3D12_CPU_DESCRIPTOR_HANDLE* copyDest = tableCache.tableStart + offset;
	for (uint32_t i = 0; i < numHandles; ++i)
		copyDest[i] = handles[i];
	// set bitmap 0b0000...11100 numHandles = 3, offset = 2
	// the handle set later will set the higher bit 
	tableCache.assignedHandlesBitMap |= ((1 << numHandles) - 1) << offset;
	m_staleRootParamsBitMap |= (1 << rootIndex);
}

uint32_t DynamicDescriptorHeap::DescriptorHandleCache::ComputeStagedSize()
{
	uint32_t neededSpace = 0;
	uint32_t rootIndex;
	uint32_t staleParams = m_staleRootParamsBitMap;
	// from lower bit to high bit
	while (_BitScanForward((unsigned long*)&rootIndex, staleParams))
	{
		staleParams ^= (1 << rootIndex);
		uint32_t maxSetHandle = 0;
		// return true if any bit set to 1, and the highest bit which is non-zero decides the max space requires
		assert(TRUE == _BitScanReverse((unsigned long*)&maxSetHandle, m_rootDescriptorTable[rootIndex].assignedHandlesBitMap));
		neededSpace += maxSetHandle + 1;
	}
	// return the max needed handle space of the whole root signature
	return neededSpace;
}

void DynamicDescriptorHeap::DescriptorHandleCache::UnbindAllValid()
{
	// if any handles already copy to m_handleCache, set m_staleRootParamsBitMap to match the whole root signature
	m_staleRootParamsBitMap = 0;

	unsigned long tableParams = m_rootDescriptorTablesBitMap;
	unsigned long rootIndex;
	while (_BitScanForward(&rootIndex, tableParams))
	{
		tableParams ^= (1 << rootIndex);
		if (m_rootDescriptorTable[rootIndex].assignedHandlesBitMap != 0)
			m_staleRootParamsBitMap |= (1 << rootIndex);
	}
}

void DynamicDescriptorHeap::DescriptorHandleCache::CopyAndBindStaleTables(
	D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorSize, 
	DescriptorHandle destHandleStart, ID3D12GraphicsCommandList* cmdList, 
	void (ID3D12GraphicsCommandList::*SetFunc)(uint32_t, D3D12_GPU_DESCRIPTOR_HANDLE))
{
	uint32_t staleParamCount = 0;
	uint32_t tableSize[kMaxNumDescriptorTables];
	uint32_t rootIndices[kMaxNumDescriptorTables];
	uint32_t neededSpace = 0;
	uint32_t rootIndex;

	// Sum the maximum assigned offsets of stale descriptor tables to determine total needed space.
	uint32_t staleParams = m_staleRootParamsBitMap;
	while (_BitScanForward((unsigned long*)&rootIndex, staleParams))
	{
		// record the root signature index of the set descriptor table
		rootIndices[staleParamCount] = rootIndex;
		staleParams ^= (1 << rootIndex);

		uint32_t maxSetHandle = 0;
		assert(TRUE == _BitScanReverse((unsigned long*)&maxSetHandle, m_rootDescriptorTable[rootIndex].assignedHandlesBitMap));

		// record the max needed space of the descriptor table
		neededSpace += maxSetHandle + 1;
		tableSize[staleParamCount] = maxSetHandle + 1;

		++staleParamCount;
	}

	assert(staleParamCount <= DescriptorHandleCache::kMaxNumDescriptorTables);

	m_staleRootParamsBitMap = 0;

	static const uint32_t kMaxDescriptorPerCopy = 16;
	uint32_t numDestDescriptorRanges = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[kMaxDescriptorPerCopy];
	uint32_t pDestDescriptorRangeSizes[kMaxDescriptorPerCopy];

	uint32_t numSrcDescriptorRanges = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE pSrcDescriptorRangeStarts[kMaxDescriptorPerCopy];
	uint32_t pSrcDescriptorRangeSizes[kMaxDescriptorPerCopy];

	// for each descriptor table need copy and bind
	for (uint32_t i = 0; i < staleParamCount; ++i)
	{
		// set continuous descriptor handle range to the descriptor table
		rootIndex = rootIndices[i];
		(cmdList->*SetFunc)(rootIndex, destHandleStart.GetGpuHandle());

		DescriptorTableCache& rootDescTable = m_rootDescriptorTable[rootIndex];

		D3D12_CPU_DESCRIPTOR_HANDLE* srcHandles = rootDescTable.tableStart;
		uint64_t setHandles = (uint64_t)rootDescTable.assignedHandlesBitMap;
		D3D12_CPU_DESCRIPTOR_HANDLE curDest = destHandleStart.GetCpuHandle();
		destHandleStart += tableSize[i] * descriptorSize;

		unsigned long skipCount;
		while (_BitScanForward64(&skipCount, setHandles))
		{
			// skip over unset descriptor handles (the lower 0 bit)
			setHandles >>= skipCount;
			srcHandles += skipCount;
			curDest.ptr += skipCount * descriptorSize;

			// calculate set descriptor count (the lower 1 bit count)
			unsigned long descriptorCount;
			_BitScanForward64(&descriptorCount, ~setHandles);
			setHandles >>= descriptorCount;

			// if we run out of temp room, copy what we've got so far
			if (numSrcDescriptorRanges + descriptorCount > kMaxDescriptorPerCopy)
			{
				g_Device->CopyDescriptors(
					numDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
					numSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
					type);
				numDestDescriptorRanges = 0;
				numSrcDescriptorRanges = 0;
			}

			// record copy destination range start address and contiguous descriptor handle count for each location
			pDestDescriptorRangeStarts[numDestDescriptorRanges] = curDest;
			pDestDescriptorRangeSizes[numDestDescriptorRanges] = descriptorCount;
			++numDestDescriptorRanges;

			// Setup source ranges (one descriptor each because we don't assume they are contiguous)
			for (uint32_t j = 0; j < descriptorCount; ++j)
			{
				pSrcDescriptorRangeStarts[numSrcDescriptorRanges] = srcHandles[j];
				pSrcDescriptorRangeSizes[numSrcDescriptorRanges] = 1;
				++numSrcDescriptorRanges;
			}

			// Move the destination pointer forward by the number of descriptors we will copy
			srcHandles += descriptorCount;
			curDest.ptr += descriptorCount * descriptorSize;
		}
	}

	g_Device->CopyDescriptors(
		numDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
		numSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
		type);
}