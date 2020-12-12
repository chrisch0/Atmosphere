#pragma once
#include "stdafx.h"
#include "App.h"

// multi-vieweport function
static void CreateSubWindow(ImGuiViewport* viewport)
{
	App::GetApp()->CreateAppSubWindow(viewport);
}

static void DestroySubWindow(ImGuiViewport* viewport)
{
	App::GetApp()->DestroyAppSubWindow(viewport);
}

static void SetSubWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	App::GetApp()->SetAppSubWindowSize(viewport, size);
}

static void RenderSubWindow(ImGuiViewport* viewport, void*)
{
	App::GetApp()->RenderAppSubWindow(viewport, nullptr);
}

static void SwapSubWindowBuffers(ImGuiViewport* viewport, void*)
{
	App::GetApp()->SwapAppSubWindowBuffers(viewport, nullptr);
}

static const int s_numFrameContexts = 3;

// Helper structure we store in the void* RendererUserData field of each ImGuiViewport to easily retrieve our backend data.
// Main viewport created by application will only use the Resources field.
// Secondary viewports created by this backend will use all the fields (including Window fields),
struct ImGuiViewportDataDx12
{
	IDXGISwapChain3*                SwapChain;
	ColorBuffer RenderTargetBuffer[s_numFrameContexts];
	//std::unique_ptr<ImDrawVert[]> VertexBuffer;
	size_t VertexCount = 0;
	//std::unique_ptr<ImDrawIdx[]> IndexBuffer;
	size_t IndexCount = 0;

	ImGuiViewportDataDx12()
	{
		SwapChain = NULL;	
	}
	~ImGuiViewportDataDx12()
	{
		IM_ASSERT(SwapChain == NULL);
	}
};