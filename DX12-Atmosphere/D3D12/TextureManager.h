#pragma once
#include "stdafx.h"
#include "Texture.h"

class TextureManager
{
public:
	static std::pair<Texture2D*, bool> FindOrLoadTexture2D(const std::wstring& name);
	template <typename T>
	static const Texture& GetSolidColorTex2D(const std::wstring& name, DXGI_FORMAT format, T color[4]);
	static const Texture2D* CreateTexture2D(const std::wstring& name, uint32_t width, uint32_t height, DXGI_FORMAT format, const void* data);
	static void Shutdown();
private:
	static std::map<std::wstring, std::unique_ptr<Texture>> s_textureCache;
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