#pragma once
#include "stdafx.h"

class GpuResource
{
	friend class CommandContext;
	friend class GraphicsContext;
	friend class ComputeContext;
public:
	GpuResource() :
		m_usageState(D3D12_RESOURCE_STATE_COMMON),
		m_transitioningState((D3D12_RESOURCE_STATES)-1),
		m_gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL)
	{

	}

	GpuResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES currentState) :
		m_pResource(pResource),
		m_usageState(currentState),
		m_transitioningState((D3D12_RESOURCE_STATES)-1),
		m_gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL)
	{

	}

	virtual void Destroy()
	{
		m_pResource = nullptr;
		m_gpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
	}

	ID3D12Resource* operator->() { return m_pResource.Get(); }
	const ID3D12Resource* operator->() const { return m_pResource.Get(); }

	ID3D12Resource* GetResource() { return m_pResource.Get(); }
	const ID3D12Resource* GetResource() const { return m_pResource.Get(); }

	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const { return m_gpuVirtualAddress; }

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
	D3D12_RESOURCE_STATES m_usageState;
	D3D12_RESOURCE_STATES m_transitioningState;
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuVirtualAddress;
};