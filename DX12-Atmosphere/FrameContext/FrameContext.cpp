#include "stdafx.h"
#include "FrameContext.h"

FrameContext::FrameContext(ID3D12Device* device)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf())));
}

FrameContext::~FrameContext()
{

}

UINT64 FrameContext::GetFenceValue() const
{
	return m_fenceValue;
}

void FrameContext::SetFenceValue(UINT64 val)
{
	m_fenceValue = val;
}

ID3D12CommandAllocator* FrameContext::GetCmdAllocator() const
{
	return m_commandAllocator.Get();
}