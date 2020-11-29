#include "stdafx.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "DescriptorHeap.h"

ID3D12Device* g_Device = nullptr;
ContextManager g_ContextManager;
CommandListManager g_CommandManager;

DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
{
	D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
	D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
	D3D12_DESCRIPTOR_HEAP_TYPE_DSV
};