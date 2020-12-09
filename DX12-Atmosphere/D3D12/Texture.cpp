#include "stdafx.h"
#include "Texture.h"
#include "DDSTextureLoader.h"
#include "CommandContext.h"

uint32_t BytesPerPixel(DXGI_FORMAT format)
{
	return (uint32_t)BitsPerPixel(format) / 8;
}

void Texture2D::Create(size_t pitch, size_t width, size_t height, DXGI_FORMAT format, const void* initData)
{
	m_width = (uint32_t)width;
	m_height = (uint32_t)height;
	m_usageState = D3D12_RESOURCE_STATE_COPY_DEST;

	D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);

	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ThrowIfFailed(g_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
		m_usageState, nullptr, IID_PPV_ARGS(m_pResource.ReleaseAndGetAddressOf())));

	m_pResource->SetName((m_textureName + L"_Resource").data());

	D3D12_SUBRESOURCE_DATA texResource;
	texResource.pData = initData;
	texResource.RowPitch = pitch * BytesPerPixel(format);
	texResource.SlicePitch = texResource.RowPitch * height;

	CommandContext::InitializeTexture(*this, 1, &texResource);

	if (m_cpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_cpuHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(m_pResource.Get(), nullptr, m_cpuHandle);
}