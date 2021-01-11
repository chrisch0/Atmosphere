#include "stdafx.h"
#include "TextureManager.h"
#include "FreeImage.h"
#include "Utils/FileUtility.h"

std::mutex TextureManager::s_mutex;
std::map<std::wstring, std::unique_ptr<Texture>> TextureManager::s_textureCache;
std::wstring TextureManager::s_rootPath = L"Textures/";

std::pair<Texture2D*, bool> TextureManager::FindOrLoadTexture2D(const std::wstring& name)
{
	std::lock_guard<std::mutex> lockGuard(s_mutex);

	auto iter = s_textureCache.find(name);

	if (iter != s_textureCache.end())
		return { dynamic_cast<Texture2D*>(iter->second.get()), false };

	Texture2D* newTexture = new Texture2D(name);
	s_textureCache[name].reset(newTexture);

	return { newTexture, true };
}

std::pair<Texture3D*, bool> TextureManager::FindOrLoadTexture3D(const std::wstring& name)
{
	std::lock_guard<std::mutex> lockGuard(s_mutex);

	auto iter = s_textureCache.find(name);

	if (iter != s_textureCache.end())
		return { dynamic_cast<Texture3D*>(iter->second.get()), false };

	Texture3D* newTexture = new Texture3D(name);
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

const Texture2D* TextureManager::LoadTGAFromFile(const std::wstring& fileName, bool sRGB /* = false */)
{
	auto tex = FindOrLoadTexture2D(fileName);

	Texture2D* texPtr = tex.first;
	bool needLoad = tex.second;
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

const Texture2D* TextureManager::LoadTextureFromFile(const std::wstring& fileName, bool sRGB /* = false */)
{
	auto tex = FindOrLoadTexture2D(fileName);

	Texture2D* texPtr = tex.first;
	bool needLoad = tex.second;
	if (!needLoad)
		return texPtr;

	//image format
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	//pointer to the image, once loaded
	FIBITMAP *dib(0);
	//pointer to the image data
	BYTE* bits(0);
	//image width and height
	unsigned int width(0), height(0);

	std::string filename(fileName.begin(), fileName.end());
	filename = "Textures/" + filename;
	//check the file signature and deduce its format
	fif = FreeImage_GetFileType(filename.c_str(), 0);
	//if still unknown, try to guess the file format from the file extension
	if (fif == FIF_UNKNOWN)
		fif = FreeImage_GetFIFFromFilename(filename.c_str());

	//check that the plugin has reading capabilities and load the file
	if (FreeImage_FIFSupportsReading(fif))
		dib = FreeImage_Load(fif, filename.c_str());

	//retrieve the image data
	bits = FreeImage_GetBits(dib);
	//get the image width and height
	width = FreeImage_GetWidth(dib);
	height = FreeImage_GetHeight(dib);
	
	texPtr->Create(width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, bits);
	texPtr->GetResource()->SetName(fileName.c_str());
	return texPtr;

}

const Texture3D* TextureManager::LoadTGAFromFile(const std::wstring& fileName, uint16_t numSliceX, uint16_t numSliceY, bool sRGB)
{
	auto tex = FindOrLoadTexture3D(fileName);

	Texture3D* texPtr = tex.first;
	bool needLoad = tex.second;
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

void TextureManager::Shutdown()
{
	s_textureCache.clear();
}