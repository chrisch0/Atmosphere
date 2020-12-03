#include "stdafx.h"
#include "ColorBuffer.h"

void ColorBuffer::CreateFromSwapChain(const std::wstring& name, ID3D12Resource* baseResource)
{
	AssociateWithResource(g_Device, name, baseResource, D3D12_RESOURCE_STATE_PRESENT);

	m_rtvHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	g_Device->CreateRenderTargetView(m_pResource.Get(), nullptr, m_rtvHandle);
}

void ColorBuffer::Create(const std::wstring& name, uint32_t width, uint32_t height, uint32_t numMips, DXGI_FORMAT format, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr /* = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN */)
{
	numMips = (numMips == 0 ? ComputeNumMips)
}