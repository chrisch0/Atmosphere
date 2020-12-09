#pragma once
#include "stdafx.h"
#include "DescriptorHeap.h"

class ContextManager;
class CommandListManager;

extern ID3D12Device* g_Device;
extern ContextManager g_ContextManager;
extern CommandListManager g_CommandManager;

extern DescriptorAllocator g_DescriptorAllocator[];
inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count = 1)
{
	return g_DescriptorAllocator[type].Allocate(count);
}