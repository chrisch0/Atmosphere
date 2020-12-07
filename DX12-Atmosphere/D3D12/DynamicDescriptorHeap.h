#pragma once
#include "stdafx.h"
#include "DescriptorHeap.h"
#include "RootSignature.h"

class CommandContext;

class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap(CommandContext& owningContext, D3D12_DESCRIPTOR_HEAP_TYPE heapType);
	~DynamicDescriptorHeap();

	static void DestoryAll()
	{
		s_descriptorHeapPool[0].clear();
		s_descriptorHeapPool[1].clear();
	}

	// Deduce cache layout needed to support the descriptor tables needed by the root signature.
	void ParseGraphicsRootSignature(const RootSignature& rootSig)
	{
		m_graphicsHandleCache.ParseRootSignature(m_descriptorType, rootSig);
	}

	void ParseComputeRootSignature(const RootSignature& rootSig)
	{
		m_computeHandleCache.ParseRootSignature(m_descriptorType, rootSig);
	}

	// Copy multiple handles into the cache area reserved for the specified root parameter.
	void SetGraphicsDescriptorHandles(uint32_t rootIdx, uint32_t offset, uint32_t numHandles, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
	{
		m_graphicsHandleCache.StageDescriptorHandles(rootIdx, offset, numHandles, handles);
	}

	void SetComputeDescriptorHandles(uint32_t rootIdx, uint32_t offset, uint32_t numHandles, const D3D12_CPU_DESCRIPTOR_HANDLE handles[])
	{
		m_computeHandleCache.StageDescriptorHandles(rootIdx, offset, numHandles, handles);
	}

	inline void CommitGraphicsRootDescriptorTables(ID3D12GraphicsCommandList* cmdList)
	{
		if (m_graphicsHandleCache.m_staleRootParamsBitMap != 0)
			CopyAndBindStagedTables(m_graphicsHandleCache, cmdList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
	}

	inline void CommitComputeRootDescriptorTables(ID3D12GraphicsCommandList* cmdList)
	{
		if (m_computeHandleCache.m_staleRootParamsBitMap != 0)
			CopyAndBindStagedTables(m_computeHandleCache, cmdList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
	}

private:
	static const uint32_t kNumDescriptorsPerHeap = 1024;
	static std::mutex s_mutex;
	static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> s_descriptorHeapPool[2];
	static std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> s_retiredDescriptorHeaps[2];
	static std::queue<ID3D12DescriptorHeap*> s_availableDescriptorHeaps[2];

	static ID3D12DescriptorHeap* RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType);
	static void DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint64_t fenceValueForReset,
		const std::vector<ID3D12DescriptorHeap*>& usedHeaps);

	CommandContext& m_owningContext;
	ID3D12DescriptorHeap* m_curHeapPtr;
	const D3D12_DESCRIPTOR_HEAP_TYPE m_descriptorType;
	uint32_t m_descriptorSize;
	uint32_t m_curOffset;
	DescriptorHandle m_firsetDescriptor;
	std::vector<ID3D12DescriptorHeap*> m_retiredHeaps;

	struct DescriptorTableCache
	{
		DescriptorTableCache() : assignedHandlesBitMap(0) {}
		uint32_t assignedHandlesBitMap;
		D3D12_CPU_DESCRIPTOR_HANDLE* tableStart;
		uint32_t tableSize;
	};

	struct DescriptorHandleCache
	{
		DescriptorHandleCache()
		{
			ClearCache();
		}

		void ClearCache()
		{
			m_rootDescriptorTablesBitMap = 0;
			m_maxCachedDescriptors = 0;
		}

		void ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE type, const RootSignature& rootSig);
		void StageDescriptorHandles(uint32_t rootIndex, uint32_t offset, uint32_t numHandles, const D3D12_CPU_DESCRIPTOR_HANDLE handles[]);

		uint32_t ComputeStagedSize();
		void CopyAndBindStaleTables(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorSize, DescriptorHandle destHandleStart, ID3D12GraphicsCommandList* cmdList,
			void (ID3D12GraphicsCommandList::*SetFunc)(uint32_t, D3D12_GPU_DESCRIPTOR_HANDLE));

		void UnbindAllValid();

		// static member
		static const uint32_t kMaxNumDescriptors = 256;
		static const uint32_t kMaxNumDescriptorTables = 16;

		DescriptorTableCache m_rootDescriptorTable[kMaxNumDescriptorTables];
		D3D12_CPU_DESCRIPTOR_HANDLE m_handleCache[kMaxNumDescriptors];
		uint32_t m_rootDescriptorTablesBitMap;
		uint32_t m_staleRootParamsBitMap;
		uint32_t m_maxCachedDescriptors;
	};

	// update while resetting root signature
	DescriptorHandleCache m_graphicsHandleCache;
	DescriptorHandleCache m_computeHandleCache;

	bool HasSpace(uint32_t count)
	{
		return (m_curHeapPtr != nullptr && m_curOffset + count <= kNumDescriptorsPerHeap);
	}

	void RetireCurrentHeap();
	void RetireUsedHeaps(uint64_t fenceValue);
	ID3D12DescriptorHeap* GetHeapPointer();

	DescriptorHandle Allocate(uint32_t count)
	{
		DescriptorHandle ret = m_firsetDescriptor + m_curOffset * m_descriptorSize;
		m_curOffset += count;
		return ret;
	}

	void CopyAndBindStagedTables(DescriptorHandleCache& handleCache, ID3D12GraphicsCommandList* cmdList,
		void (ID3D12GraphicsCommandList::*SetFunc)(uint32_t, D3D12_GPU_DESCRIPTOR_HANDLE));

	void UnbindAllValid();
};