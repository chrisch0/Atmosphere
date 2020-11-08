#pragma once
#include "stdafx.h"

class FrameContext
{
public:
	FrameContext(ID3D12Device* device);
	~FrameContext();

	UINT64 GetFenceValue() const;
	void SetFenceValue(UINT64);

	ID3D12CommandAllocator* GetCmdAllocator() const;

private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	UINT64 m_fenceValue;
};