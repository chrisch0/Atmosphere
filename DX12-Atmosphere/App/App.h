#pragma once
#include "stdafx.h"
#include "Utils/Timer.h"
#include "FrameContext/FrameContext.h"
#include "D3D12/ColorBuffer.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"
#include "D3D12/Texture.h"

struct ImGui_RenderBuffers;
struct ImGui_FrameContext;
struct ImGuiViewportDataDx12;
class GraphicsContext;

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
	virtual void Update(const Timer& t);
	virtual void Draw(const Timer& t);
	virtual void UpdateUI();

protected:
	bool InitMainWindow();
	bool InitDirect3D();
	void InitImgui();
	void CreateSwapChain();

	void CreateAppRootSignature();
	void CreateAppPipelineState();
	void CreateFontTexture();

	void UpdateImGuiDemo();
	void Display();

	void SwapBackBuffer();

	void RenderUI(GraphicsContext& context, ImDrawData* drawData);
	//void SetupRenderState(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx, ImGui_RenderBuffers* fr);

	void ShutdownWindow();

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
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;

	static int const c_swapChainBufferCount = 3;
	int m_currBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;

	ColorBuffer m_displayBuffer[c_swapChainBufferCount];

	int m_frameIndex = 0;

	D3D12_VIEWPORT m_screenViewport;
	D3D12_RECT m_scissorRect;

	std::wstring m_mainWndCaption = L"Atmosphere";
	D3D_DRIVER_TYPE m_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int m_clientWidth = 1280;
	int m_clientHeight = 800;

	// Imgui
	RootSignature m_displayRootSignature;
	GraphicsPSO m_displayPSO;

	const Texture2D* m_fontColorBuffer;

	bool m_showDemoWindow = true;
	bool m_showAnotherDemoWindow = false;

	ImVec4 m_clearColor;
};