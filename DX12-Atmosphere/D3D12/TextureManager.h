#pragma once
#include "stdafx.h"
#include "Texture.h"

namespace TextureManager
{
	std::pair<Texture2D*, bool> FindOrLoadTexture2D(const std::wstring& name);
	std::pair<Texture3D*, bool> FindOrLoadTexture3D(const std::wstring& name);

	const Texture2D* CreateTexture2D(const std::wstring& name, uint32_t width, uint32_t height, DXGI_FORMAT format, const void* data);

	const Texture2D* LoadTexture2DFromFile(const std::wstring& fileName, bool sRGB = false);
	const Texture2D* LoadDDSFromFile(const std::wstring& fileName, bool sRGB = false);
	const Texture2D* LoadTGAFromFile(const std::wstring& fileName, bool sRGB = false);

	//static const Texture3D* LoadTexture3DFromFile(const std::wstring& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB = false);
	//static const Texture3D* LoadDDSFromFile(const std::wstring& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB = false);
	const Texture3D* LoadTGAFromFile(const std::wstring& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB = false);

	const Texture2D* LoadTexture2DFromFile(const std::string& fileName, bool sRGB = false);

	const Texture2D* LoadDDSFromFile(const std::string& fileName, bool sRGB = false);

	const Texture2D* LoadTGAFromFile(const std::string& fileName, bool sRGB = false);

	const Texture3D* LoadTGAFromFile(const std::string& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB = false);

	void Shutdown();

	template <typename T>
	const Texture& GetSolidColorTex2D(const std::wstring& name, DXGI_FORMAT format, T color[4])
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

};