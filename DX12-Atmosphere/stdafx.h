#pragma once

#define WIN32_LEAN_AND_MEAN			//Exclude rarely-used stuff from Windows headers

#ifndef NOMINMAX
#define NOMINMAX
#endif

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
#else
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include "d3dx12.h"

#include "imgui/imgui.h"

#include <string>
#include <sstream>
#include <memory>
#include <tchar.h>
#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <utility>
#include <cassert>
#include <stdint.h>
#include <cmath>

#include "Utils/HelperFuncs.h"
#include "D3D12/GraphicsCore.h"
#include "Math/Common.h"


#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
