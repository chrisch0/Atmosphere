#include "stdafx.h"
#include "App/App.h"
#include <tchar.h>

LRESULT WINAPI wWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


int main(int argc, char** argv)
{
	App app;
	app.Initialize();
	return app.Run();

	// Create application window
	//ImGui_ImplWin32_EnableDpiAwareness();
	//WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, wWndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"ImGui Example", NULL };
	//::RegisterClassEx(&wc);
	//HWND hwnd = ::CreateWindow(wc.lpszClassName, L"Dear ImGui DirectX12 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

	//

	//// Show the window
	//::ShowWindow(hwnd, SW_SHOWDEFAULT);
	//::UpdateWindow(hwnd);
	//return 0;

	//WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, wWndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"Atmosphere App", NULL };

	//if (!::RegisterClassEx(&wc))
	//{
	//	MessageBoxW(0, L"Register Class Failed.", 0, 0);
	//	return false;
	//}

	//RECT R = { 0, 0, 1280, 800 };
	//AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	//int width = R.right - R.left;
	//int height = R.bottom - R.top;

	//HWND hwnd = ::CreateWindow(wc.lpszClassName, L"Atmosphere", WS_OVERLAPPEDWINDOW, 100, 100, width, height, 0, 0, wc.hInstance, 0);
	//if (!hwnd)
	//{
	//	MessageBoxW(0, L"Create Window Failed.", 0, 0);
	//	return false;
	//}
	////m_hAppInst = wc.hInstance;
	//ShowWindow(hwnd, SW_SHOWDEFAULT);
	//UpdateWindow(hwnd);

	//return 0;
}

LRESULT WINAPI wWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{


	switch (msg)
	{
	case WM_SIZE:
		
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}