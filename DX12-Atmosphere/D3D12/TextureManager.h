#pragma once
#include "stdafx.h"
#include "Texture.h"

class TextureManager
{
public:
	static std::pair<Texture2D*, bool> FindOrLoadTexture2D(const std::wstring& name);
	static std::pair<Texture3D*, bool> FindOrLoadTexture3D(const std::wstring& name);
	template <typename T>
	static const Texture& GetSolidColorTex2D(const std::wstring& name, DXGI_FORMAT format, T color[4]);
	static const Texture2D* CreateTexture2D(const std::wstring& name, uint32_t width, uint32_t height, DXGI_FORMAT format, const void* data);

	static const Texture2D* LoadTexture2DFromFile(const std::wstring& fileName, bool sRGB = false);
	static const Texture2D* LoadDDSFromFile(const std::wstring& fileName, bool sRGB = false);
	static const Texture2D* LoadTGAFromFile(const std::wstring& fileName, bool sRGB = false);

	//static const Texture3D* LoadTexture3DFromFile(const std::wstring& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB = false);
	//static const Texture3D* LoadDDSFromFile(const std::wstring& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB = false);
	static const Texture3D* LoadTGAFromFile(const std::wstring& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB = false);

	static const Texture2D* LoadTexture2DFromFile(const std::string& fileName, bool sRGB = false)
	{
		return LoadTexture2DFromFile(std::wstring(fileName.begin(), fileName.end()), sRGB);
	}

	static const Texture2D* LoadDDSFromFile(const std::string& fileName, bool sRGB = false)
	{
		return LoadDDSFromFile(std::wstring(fileName.begin(), fileName.end()), sRGB);
	}

	static const Texture2D* LoadTGAFromFile(const std::string& fileName, bool sRGB = false)
	{
		return LoadTGAFromFile(std::wstring(fileName.begin(), fileName.end()), sRGB);
	}

	static const Texture3D* LoadTGAFromFile(const std::string& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB = false)
	{
		return LoadTGAFromFile(std::wstring(fileName.begin(), fileName.end()), numSliceX, numSliceY, sRGB);
	}

	static void Shutdown();
private:
	static std::mutex s_mutex;
	static std::map<std::wstring, std::unique_ptr<Texture>> s_textureCache;
	static std::wstring s_rootPath;
};

template <typename T>
const Texture& TextureManager::GetSolidColorTex2D(const std::wstring& name, DXGI_FORMAT format, T color[4])
{
	auto tex = FindOrLoadTexture2D(name);

	Texture2D* tex2D = tex.first;
	const bool needLoad = tex.second;

	if (!needLoad)
	{
		//tex2D->WaitForLoad();
		return *tex2D;
	}

	tex2D->Create(1, 1, format, color);
	return *tex2D;
}