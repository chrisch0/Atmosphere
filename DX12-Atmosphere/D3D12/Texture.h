#pragma once
#include "stdafx.h"
#include "GpuResource.h"

class Texture : public GpuResource
{
	friend class TextureManager;
public:
	Texture(const std::wstring& name) 
		: m_textureName(name), m_isValid(true), m_width(0), m_height(0), m_depth(1)
	{ 
		m_cpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; 
	}

	virtual void Destroy() override
	{
		GpuResource::Destroy();
		m_cpuHandle.ptr = 0;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_cpuHandle; }
	bool operator!() { return m_cpuHandle.ptr == 0; }
	bool isValid() const { return m_isValid; }

protected:
	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_depth;
	std::wstring m_textureName;
	bool m_isValid;
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle;
};

class Texture2D : public Texture
{
	friend class CommandContext;
	friend class TextureManager;
public:
	Texture2D(const std::wstring& name) : Texture(name) { }
	Texture2D(D3D12_CPU_DESCRIPTOR_HANDLE handle) : Texture(L"") { m_cpuHandle = handle; }


	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }

protected:
	void Create(size_t pitch, size_t width, size_t height, DXGI_FORMAT format, const void* initData);
	void Create(size_t width, size_t height, DXGI_FORMAT format, const void* initData)
	{
		Create(width, width, height, format, initData);
	}

	void CreateTGAFromMemory(const void* memBuffer, size_t fileSize, bool sRGB);
	bool CreateDDSFromMemory(const void* memBuffer, size_t fileSize, bool sRGB);

protected:

};

class Texture3D : public Texture
{
	friend class CommandContext;
	friend class TextureManager;
public:
	Texture3D(const std::wstring& name) : Texture(name) {}

	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
	uint32_t GetDepth() const { return m_depth; }

protected:
	void Create(size_t pitch, size_t width, size_t height, size_t depth, DXGI_FORMAT format, const void* initData);
	void Create(size_t width, size_t height, size_t depth, DXGI_FORMAT format, const void* initData)
	{
		Create(width, width, height, depth, format, initData);
	}

	void CreateTGAFromMemory(const void* memBuffer, size_t fileSize, uint16_t numSliceX, uint16_t numSliceY, bool sRGB);
	void CreateDDSFromMemory(const void* memBuffer, size_t fileSize, uint16_t numSilceX, uint16_t numSliceY, bool sRGB);
};