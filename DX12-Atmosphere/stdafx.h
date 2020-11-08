#pragma once

#define WIN32_LEAN_AND_MEAN			//Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include "d3dx12.h"

#include "imgui/imgui.h"

#include <string>
#include <memory>
#include <tchar.h>

#include "Utils/HelperFuncs.h"
