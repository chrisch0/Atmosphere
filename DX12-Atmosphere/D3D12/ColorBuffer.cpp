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
	numMips = (numMips == 0 ? ComputeNumMips(width, height) : numMips);
	D3D12_RESOURCE_FLAGS flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC resourceDesc = DescribeTex2D(width, height, 1, numMips, format, flags);

	resourceDesc.SampleDesc.Count = m_fragmentCount;
	resourceDesc.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	clearValue.Color[0] = m_clearColor.R();
	clearValue.Color[1] = m_clearColor.G();
	clearValue.Color[2] = m_clearColor.B();
	clearValue.Color[3] = m_clearColor.A();

	CreateTextureResource(g_Device, name, resourceDesc, clearValue, vidMemPtr);
	CreateDerivedViews(g_Device, format, 1, numMips);
}

void ColorBuffer::CreateArray(const std::wstring& name, uint32_t width, uint32_t height, uint32_t arrayCount, DXGI_FORMAT format, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr /* = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN */)
{
	D3D12_RESOURCE_FLAGS flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC resourceDesc = DescribeTex2D(width, height, arrayCount, 1, format, flags);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	clearValue.Color[0] = m_clearColor.R();
	clearValue.Color[1] = m_clearColor.G();
	clearValue.Color[2] = m_clearColor.B();
	clearValue.Color[3] = m_clearColor.A();

	CreateTextureResource(g_Device, name, resourceDesc, clearValue, vidMemPtr);
	CreateDerivedViews(g_Device, format, arrayCount, 1);
}

void ColorBuffer::CreateDerivedViews(ID3D12Device* device, DXGI_FORMAT format, uint32_t arraySize, uint32_t numMips)
{
	assert(arraySize == 1 || numMips == 1);

	m_numMipMaps = numMips - 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	rtvDesc.Format = format;
	uavDesc.Format = GetUAVFormat(format);
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (arraySize > 1)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.FirstArraySlice = 0;
		rtvDesc.Texture2DArray.ArraySize = (UINT)arraySize;

		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = (UINT)arraySize;

		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MipLevels = numMips;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = (UINT)arraySize;
	}
	else if (m_fragmentCount > 1)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}
	else
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = numMips;
		srvDesc.Texture2D.MostDetailedMip = 0;
	}

	if (m_srvHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_rtvHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_srvHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	ID3D12Resource* resource = m_pResource.Get();
	device->CreateRenderTargetView(resource, &rtvDesc, m_rtvHandle);
	device->CreateShaderResourceView(resource, &srvDesc, m_srvHandle);

	if (m_fragmentCount > 1)
		return;

	for (uint32_t i = 0; i < numMips; ++i)
	{
		if (m_uavHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_uavHandle[i] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, m_uavHandle[i]);

		uavDesc.Texture2D.MipSlice++;
	}
}

void VolumeColorBuffer::Create(const std::wstring& name, uint32_t width, uint32_t height, uint32_t depth, uint32_t numMips, DXGI_FORMAT format, D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr /* = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN */)
{
	numMips = (numMips == 0 ? ComputeNumMips(width, height, depth) : numMips);
	D3D12_RESOURCE_FLAGS flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC resourceDesc = DescribeTex3D(width, height, depth, numMips, format, flags);

	resourceDesc.SampleDesc.Count = m_fragmentCount;
	resourceDesc.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	clearValue.Color[0] = m_clearColor.R();
	clearValue.Color[1] = m_clearColor.G();
	clearValue.Color[2] = m_clearColor.B();
	clearValue.Color[3] = m_clearColor.A();

	CreateTextureResource(g_Device, name, resourceDesc, clearValue, vidMemPtr);
	CreateDerivedViews(g_Device, format, numMips);
}

void VolumeColorBuffer::CreateDerivedViews(ID3D12Device* device, DXGI_FORMAT format, uint32_t numMips)
{
	m_numMipMaps = numMips - 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	rtvDesc.Format = format;
	uavDesc.Format = GetUAVFormat(format);
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;


	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
	rtvDesc.Texture3D.MipSlice = 0;
	rtvDesc.Texture3D.FirstWSlice = 0;
	rtvDesc.Texture3D.WSize = m_depth;

	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = m_depth;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	srvDesc.Texture3D.MipLevels = numMips;
	srvDesc.Texture3D.MostDetailedMip = 0;


	if (m_srvHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_rtvHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_srvHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	ID3D12Resource* resource = m_pResource.Get();
	device->CreateRenderTargetView(resource, &rtvDesc, m_rtvHandle);
	device->CreateShaderResourceView(resource, &srvDesc, m_srvHandle);

	if (m_fragmentCount > 1)
		return;

	for (uint32_t i = 0; i < numMips; ++i)
	{
		if (m_uavHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_uavHandle[i] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, m_uavHandle[i]);

		uavDesc.Texture2D.MipSlice++;
	}
}