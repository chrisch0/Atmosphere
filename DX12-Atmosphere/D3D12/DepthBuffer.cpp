#include "stdafx.h"
#include "DepthBuffer.h"

void DepthBuffer::Create(const std::wstring& name, uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr /* = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN */)
{
	D3D12_RESOURCE_DESC resDesc = DescribeTex2D(width, height, 1, 1, format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	CreateTextureResource(g_Device, name, resDesc, clearValue, vidMemPtr);
	CreateDerivedViews(g_Device, format);
}

void DepthBuffer::Create(const std::wstring& name, uint32_t width, uint32_t height, uint32_t numSamples, DXGI_FORMAT format, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr /* = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN */)
{
	D3D12_RESOURCE_DESC resDesc = DescribeTex2D(width, height, 1, 1, format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	resDesc.SampleDesc.Count = numSamples;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	CreateTextureResource(g_Device, name, resDesc, clearValue, vidMemPtr);
	CreateDerivedViews(g_Device, format);
}

void DepthBuffer::CreateDerivedViews(ID3D12Device* device, DXGI_FORMAT format)
{
	ID3D12Resource* resource = m_pResource.Get();

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = GetDSVFormat(format);
	if (resource->GetDesc().SampleDesc.Count == 1)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
	}
	else
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
	}

	if (m_DSV[0].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_DSV[0] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		m_DSV[1] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	device->CreateDepthStencilView(resource, &dsvDesc, m_DSV[0]);
	
	dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
	device->CreateDepthStencilView(resource, &dsvDesc, m_DSV[1]);

	DXGI_FORMAT stencilReadFormat = GetStencilFormat(format);
	if (stencilReadFormat != DXGI_FORMAT_UNKNOWN)
	{
		if (m_DSV[2].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		{
			m_DSV[2] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			m_DSV[3] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_STENCIL;
		device->CreateDepthStencilView(resource, &dsvDesc, m_DSV[2]);

		dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_STENCIL | D3D12_DSV_FLAG_READ_ONLY_DEPTH;
		device->CreateDepthStencilView(resource, &dsvDesc, m_DSV[3]);
	}
	else
	{
		m_DSV[2] = m_DSV[0];
		m_DSV[3] = m_DSV[1];
	}

	if (m_depthSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_depthSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = GetDepthFormat(format);
	if (srvDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
	}
	else
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	device->CreateShaderResourceView(resource, &srvDesc, m_depthSRV);

	if (stencilReadFormat != DXGI_FORMAT_UNKNOWN)
	{
		if (m_stencilSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_stencilSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		srvDesc.Format = stencilReadFormat;
		device->CreateShaderResourceView(resource, &srvDesc, m_stencilSRV);
	}
}