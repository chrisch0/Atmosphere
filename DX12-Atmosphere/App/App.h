#pragma once
#include "stdafx.h"
#include "Utils/Timer.h"
#include "FrameContext/FrameContext.h"

struct ImGui_RenderBuffers;
struct ImGui_FrameContext;
struct ImGuiViewportDataDx12;

class App
{
public:
	App(const App& rhs) = delete;
	App& operator=(const App& rhs) = delete;
public:
	static App* GetApp();

	App();
	virtual ~App();

	HINSTANCE AppInst() const;
	HWND MainWnd() const;
	float AspectRatio() const;
	ID3D12Device* GetDevice() const;
	int GetWidth() const { return m_clientWidth; }
	int GetHeight() const { return m_clientHeight; }
	void SetWidth(int width) { m_clientWidth = width; }
	void SetHeight(int height) { m_clientHeight = height; }

	bool Get4xMSAAState() const;
	void Set4xMSAAState(bool v);

	int Run();

	virtual bool Initialize();
	LRESULT CALLBACK MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	virtual void OnResize();

protected:
	virtual void CreateSrvRtvAndDsvDescriptorHeaps();
	virtual void Update(const Timer& t);
	virtual void Draw(const Timer& t);
	virtual void DrawUI();

protected:
	bool InitMainWindow();
	bool InitDirect3D();
	void InitFrameContext();
	void InitImgui();
	void CreateCommandObjects();
	void CreateSwapChain();

	void CreateAppRootSignature();
	void CreateAppPipelineState();
	void CreateFontTexture();

	void WaitForLastSubmittedFrame();

	void WaitForNextFrameResource();

	void DrawImGuiDemo();
	void RenderImGui();

	void SwapBackBuffer();

	void RenderDrawData(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx);
	void SetupRenderState(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx, ImGui_RenderBuffers* fr);

	void ShutdownWindow();

	ID3D12Resource* CurrentBackBuffer() const {
		return m_swapChainBuffer[m_currBackBuffer].Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_currBackBuffer, m_rtvDescriptorSize);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const
	{
		return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	void CalculateFrameStats();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

public:
	// multi-viewport function
	void CreateAppSubWindow(ImGuiViewport* viewport);
	void DestroyAppSubWindow(ImGuiViewport* viewport);
	void SetAppSubWindowSize(ImGuiViewport* viewport, ImVec2 size);
	void RenderAppSubWindow(ImGuiViewport* viewport, void*);
	void SwapAppSubWindowBuffers(ImGuiViewport* viewport, void*);
private:
	void FlushSubWindowCommandQueue(ImGuiViewportDataDx12* data);

protected:
	static App* m_app;

	HINSTANCE m_hAppInst = nullptr;
	HWND m_hMainWnd = nullptr;
	bool m_appPaused = false;
	bool m_minimized = false;
	bool m_maximized = false;
	bool m_resizing = false;
	bool m_fullScreenState = false;

	bool m_4xMSAAState = false;
	UINT m_4xMSAAQuality = 0;

	Timer m_timer;

	Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
	//Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
	HANDLE m_swapChainWaitableOject = NULL;
	Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent = NULL;
	UINT64 m_currentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
	//Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_directCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

	static int const c_swapChainBufferCount = 3;
	int m_currBackBuffer = 0;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_swapChainRTVDespcriptorHandle[c_swapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_swapChainBuffer[c_swapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;

	std::vector<std::unique_ptr<FrameContext>> m_frameContexts;
	static const int m_numFrameContexts = 3;
	FrameContext* m_currFrameContext = nullptr;
	int m_currFrameContextIndex = 0;

	int m_frameIndex = 0;

	D3D12_VIEWPORT m_screenViewport;
	D3D12_RECT m_scissorRect;

	UINT m_rtvDescriptorSize = 0;
	UINT m_dsvDescriptorSize = 0;
	UINT m_cbvSrvUavDescriptorSize = 0;

	std::wstring m_mainWndCaption = L"Atmosphere";
	D3D_DRIVER_TYPE m_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int m_clientWidth = 1280;
	int m_clientHeight = 800;

	// Imgui
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_appRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_appPipelineState;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_fontResource;
	D3D12_CPU_DESCRIPTOR_HANDLE m_fontSrvCpuDescHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_fontSrvGpuDescHandle = {};

	bool m_showDemoWindow = true;
	bool m_showAnotherDemoWindow = false;

	ImVec4 m_clearColor;
};