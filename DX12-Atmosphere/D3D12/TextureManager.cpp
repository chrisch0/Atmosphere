#include "stdafx.h"
#include "TextureManager.h"

std::map<std::wstring, std::unique_ptr<Texture>> TextureManager::s_textureCache;

std::pair<Texture2D*, bool> TextureManager::FindOrLoadTexture2D(const std::wstring& name)
{
	static std::mutex s_mutex;
	std::lock_guard<std::mutex> lockGuard(s_mutex);

	auto iter = s_textureCache.find(name);

	if (iter != s_textureCache.end())
		return { dynamic_cast<Texture2D*>(iter->second.get()), false };

	Texture2D* newTexture = new Texture2D(name);
	s_textureCache[name].reset(newTexture);

	return { newTexture, true };
}

const Texture2D* TextureManager::CreateTexture2D(const std::wstring& name, uint32_t width, uint32_t height, DXGI_FORMAT format, const void* data)
{
	auto tex = FindOrLoadTexture2D(name);

	Texture2D* texPtr = tex.first;
	bool needLoad = tex.second;

	if (!needLoad)
		return texPtr;

	texPtr->Create(width, height, format, data);
	return texPtr;
}

void TextureManager::Shutdown()
{
	s_textureCache.clear();
}