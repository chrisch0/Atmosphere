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

// Buffers used during the rendering of a frame
//struct ImGui_RenderBuffers
//{
//	ID3D12Resource*     IndexBuffer;
//	ID3D12Resource*     VertexBuffer;
//	int                 IndexBufferSize;
//	int                 VertexBufferSize;
//};

// Buffers used for secondary viewports created by the multi-viewports systems
//struct ImGui_FrameContext
//{
//	ID3D12CommandAllocator*         CommandAllocator;
//	ID3D12Resource*                 RenderTarget;
//	D3D12_CPU_DESCRIPTOR_HANDLE     RenderTargetCpuDescriptors;
//};

// Helper structure we store in the void* RendererUserData field of each ImGuiViewport to easily retrieve our backend data.
// Main viewport created by application will only use the Resources field.
// Secondary viewports created by this backend will use all the fields (including Window fields),
struct ImGuiViewportDataDx12
{
	// Window
	//ID3D12CommandQueue*             CommandQueue;
	//ID3D12GraphicsCommandList*      CommandList;
	//ID3D12DescriptorHeap*           RtvDescHeap;
	IDXGISwapChain3*                SwapChain;
	ColorBuffer RenderTargetBuffer[s_numFrameContexts];
	std::unique_ptr<ImDrawVert[]> VertexBuffer;
	size_t VertexCount = 0;
	std::unique_ptr<ImDrawIdx[]> IndexBuffer;
	size_t IndexCount = 0;
	//ID3D12Fence*                    Fence;
	//UINT64                          FenceSignaledValue;
	//HANDLE                          FenceEvent;
	//ImGui_FrameContext*    FrameCtx;

	// Render buffers
	UINT                            FrameIndex;
	//ImGui_RenderBuffers*   FrameRenderBuffers;

	ImGuiViewportDataDx12()
	{
		//CommandQueue = NULL;
		//CommandList = NULL;
		//RtvDescHeap = NULL;
		SwapChain = NULL;
		//Fence = NULL;
		//FenceSignaledValue = 0;
		//FenceEvent = NULL;
		//FrameCtx = new ImGui_FrameContext[s_numFrameContexts];
		FrameIndex = UINT_MAX;
		//FrameRenderBuffers = new ImGui_RenderBuffers[s_numFrameContexts];

		//for (UINT i = 0; i < s_numFrameContexts; ++i)
		//{
		//	FrameCtx[i].CommandAllocator = NULL;
		//	FrameCtx[i].RenderTarget = NULL;

		//	// Create buffers with a default size (they will later be grown as needed)
		//	FrameRenderBuffers[i].IndexBuffer = NULL;
		//	FrameRenderBuffers[i].VertexBuffer = NULL;
		//	FrameRenderBuffers[i].VertexBufferSize = 5000;
		//	FrameRenderBuffers[i].IndexBufferSize = 10000;
		//}
	}
	~ImGuiViewportDataDx12()
	{
		//IM_ASSERT(CommandQueue == NULL && CommandList == NULL);
		//IM_ASSERT(RtvDescHeap == NULL);
		IM_ASSERT(SwapChain == NULL);
		//IM_ASSERT(Fence == NULL);
		//IM_ASSERT(FenceEvent == NULL);

		/*for (UINT i = 0; i < s_numFrameContexts; ++i)
		{
			IM_ASSERT(FrameCtx[i].CommandAllocator == NULL && FrameCtx[i].RenderTarget == NULL);
			IM_ASSERT(FrameRenderBuffers[i].IndexBuffer == NULL && FrameRenderBuffers[i].VertexBuffer == NULL);
		}*/

		/*delete[] FrameCtx; FrameCtx = NULL;
		delete[] FrameRenderBuffers; FrameRenderBuffers = NULL;*/
	}
};

template<typename T>
static void SafeRelease(T*& res)
{
	if (res)
		res->Release();
	res = NULL;
}