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

	D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, (UINT)height, 1, 1);

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

void Texture2D::CreateTGAFromMemory(const void* _filePtr, size_t fileSize, bool sRGB)
{
	const uint8_t* filePtr = (const uint8_t*)_filePtr;

	// Skip first two bytes
	filePtr += 2;

	/*uint8_t imageTypeCode =*/ *filePtr++;
	
	// Ignore another 9 bytes
	filePtr += 9;
	
	uint16_t imageWidth = *(uint16_t*)filePtr;
	filePtr += sizeof(uint16_t);
	uint16_t imageHeight = *(uint16_t*)filePtr;
	filePtr += sizeof(uint16_t);
	uint8_t bitCount = *filePtr++;

	// Ignore another byte
	filePtr++;

	uint32_t* formattedData = new uint32_t[imageWidth * imageHeight];
	uint32_t* iter = formattedData;

	uint8_t numChannels = bitCount / 8;
	uint32_t numBytes = imageWidth * imageHeight * numChannels;

	switch (numChannels)
	{
	default:
		break;
	case 3:
		for (uint32_t byteIdx = 0; byteIdx < numBytes; byteIdx += 3)
		{
			*iter++ = 0xff000000 | filePtr[0] << 16 | filePtr[1] << 8 | filePtr[2];
			filePtr += 3;
		}
		break;
	case 4:
		for (uint32_t byteIdx = 0; byteIdx < numBytes; byteIdx += 4)
		{
			*iter++ = filePtr[3] << 24 | filePtr[0] << 16 | filePtr[1] << 8 | filePtr[2];
			filePtr += 4;
		}
		break;
	}
	Create(imageWidth, imageHeight, sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM, formattedData);
	delete[] formattedData;
}

bool Texture2D::CreateDDSFromMemory(const void* filePtr, size_t fileSize, bool sRGB)
{
	if (m_cpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_cpuHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	HRESULT hr = CreateDDSTextureFromMemory(g_Device, (const uint8_t*)filePtr, fileSize, 0, sRGB, &m_pResource, m_cpuHandle);

	return SUCCEEDED(hr);
}

void Texture3D::Create(size_t pitch, size_t width, size_t height, size_t depth, DXGI_FORMAT format, const void* initData)
{
	m_width = (uint32_t)width;
	m_height = (uint32_t)height;
	m_depth = (uint32_t)depth;
	m_usageState = D3D12_RESOURCE_STATE_COPY_DEST;

	D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex3D(format, width, m_height, (uint16_t)depth);

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

void Texture3D::CreateTGAFromMemory(const void* _filePtr, size_t fileSize, uint16_t numSliceX, uint16_t numSliceY, bool sRGB)
{
	const uint8_t* filePtr = (const uint8_t*)_filePtr;

	// Skip first two bytes
	filePtr += 2;

	/*uint8_t imageTypeCode =*/ *filePtr++;

	// Ignore another 9 bytes
	filePtr += 9;

	uint16_t imageWidth = *(uint16_t*)filePtr;
	filePtr += sizeof(uint16_t);
	uint16_t imageHeight = *(uint16_t*)filePtr;
	filePtr += sizeof(uint16_t);
	uint8_t bitCount = *filePtr++;

	size_t sliceWidth = imageWidth / numSliceX;
	size_t sliceHeight = imageHeight / numSliceY;
	size_t depth = numSliceX * numSliceY;

	// Ignore another byte
	filePtr++;

	uint32_t* formattedData = new uint32_t[imageWidth * imageHeight];
	uint32_t* iter = formattedData;

	uint8_t numChannels = bitCount / 8;
	uint32_t numBytes = imageWidth * imageHeight * numChannels;

	switch (numChannels)
	{
	default:
		break;
	case 3:
		const uint8_t* fileStart = filePtr;
		for (int sliceX = 0; sliceX < numSliceX; ++sliceX)
		{
			for (int sliceY = 0; sliceY < numSliceY; ++sliceY)
			{
				for (int height = 0; height < sliceHeight; ++height)
				{
					for (int width = 0; width < sliceWidth; ++width)
					{
						filePtr = fileStart + 
						{
							*iter++ = 0xff000000 | filePtr[0] << 16 | filePtr[1] << 8 | filePtr[2];
							filePtr += 3;
						}
					}
				}
				
			}
		}
		
		break;
	case 4:
		for (uint32_t byteIdx = 0; byteIdx < numBytes; byteIdx += 4)
		{
			*iter++ = filePtr[3] << 24 | filePtr[0] << 16 | filePtr[1] << 8 | filePtr[2];
			filePtr += 4;
		}
		break;
	}

	Create(sliceWidth, sliceHeight, depth, sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM, formattedData);

	delete[] formattedData;
}