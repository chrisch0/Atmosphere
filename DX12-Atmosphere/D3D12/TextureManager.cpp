#include "stdafx.h"
#include "TextureManager.h"
#include "Utils/FileUtility.h"

namespace TextureManager
{
	std::map<std::wstring, std::unique_ptr<Texture>> s_textureCache;
	std::wstring s_rootPath = L"Textures/";

	std::pair<Texture2D*, bool> FindOrLoadTexture2D(const std::wstring& name)
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

	std::pair<Texture3D*, bool> FindOrLoadTexture3D(const std::wstring& name)
	{
		static std::mutex s_mutex;
		std::lock_guard<std::mutex> lockGuard(s_mutex);

		auto iter = s_textureCache.find(name);

		if (iter != s_textureCache.end())
			return { dynamic_cast<Texture3D*>(iter->second.get()), false };

		Texture3D* newTexture = new Texture3D(name);
		s_textureCache[name].reset(newTexture);

		return { newTexture, true };
	}

	const Texture2D* CreateTexture2D(const std::wstring& name, uint32_t width, uint32_t height, DXGI_FORMAT format, const void* data)
	{
		auto tex = FindOrLoadTexture2D(name);

		Texture2D* texPtr = tex.first;
		const bool needLoad = tex.second;

		if (!needLoad)
			return texPtr;

		texPtr->Create(width, height, format, data);
		return texPtr;
	}

	const Texture2D* LoadTexture2DFromFile(const std::wstring& fileName, bool sRGB /* = false */)
	{
		std::wstring catPath = fileName;

		const Texture2D* texPtr = LoadDDSFromFile(catPath + L".dds", sRGB);
		if (!texPtr->isValid())
			texPtr = LoadTGAFromFile(catPath + L".tga", sRGB);

		return texPtr;
	}

	const Texture2D* LoadDDSFromFile(const std::wstring& fileName, bool sRGB /* = false */)
	{
		auto tex = FindOrLoadTexture2D(fileName);

		Texture2D* texPtr = tex.first;
		const bool needLoad = tex.second;
		if (!needLoad)
			return texPtr;

		Utils::ByteArray ba = Utils::ReadFileSync(s_rootPath + fileName);
		if (ba->size() > 0)
		{
			texPtr->CreateDDSFromMemory(ba->data(), ba->size(), sRGB);
			texPtr->GetResource()->SetName(fileName.c_str());
		}
		return texPtr;
	}

	const Texture2D* LoadTGAFromFile(const std::wstring& fileName, bool sRGB /* = false */)
	{
		auto tex = FindOrLoadTexture2D(fileName);

		Texture2D* texPtr = tex.first;
		const bool needLoad = tex.second;
		if (!needLoad)
			return texPtr;

		Utils::ByteArray ba = Utils::ReadFileSync(s_rootPath + fileName);
		if (ba->size() > 0)
		{
			texPtr->CreateTGAFromMemory(ba->data(), ba->size(), sRGB);
			texPtr->GetResource()->SetName(fileName.c_str());
		}

		return texPtr;
	}

	const Texture3D* LoadTGAFromFile(const std::wstring& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB)
	{
		auto tex = FindOrLoadTexture3D(fileName);

		Texture3D* texPtr = tex.first;
		const bool needLoad = tex.second;
		if (!needLoad)
			return texPtr;

		Utils::ByteArray ba = Utils::ReadFileSync(s_rootPath + fileName);
		if (ba->size() > 0)
		{
			texPtr->CreateTGAFromMemory(ba->data(), ba->size(), numSliceX, numSliceY, sRGB);
			texPtr->GetResource()->SetName(fileName.c_str());
		}
		return texPtr;
	}

	const Texture2D* LoadTexture2DFromFile(const std::string& fileName, bool sRGB)
	{
		return LoadTexture2DFromFile(std::wstring(fileName.begin(), fileName.end()), sRGB);
	}

	const Texture2D* LoadDDSFromFile(const std::string& fileName, bool sRGB)
	{
		return LoadDDSFromFile(std::wstring(fileName.begin(), fileName.end()), sRGB);
	}

	const Texture2D* LoadTGAFromFile(const std::string& fileName, bool sRGB)
	{
		return LoadTGAFromFile(std::wstring(fileName.begin(), fileName.end()), sRGB);
	}

	const Texture3D* LoadTGAFromFile(const std::string& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB)
	{
		return LoadTGAFromFile(std::wstring(fileName.begin(), fileName.end()), numSliceX, numSliceY, sRGB);
	}

	void Shutdown()
	{
		s_textureCache.clear();
	}
}