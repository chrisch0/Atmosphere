#pragma once
#include "PixelBuffer.h"

class DepthBuffer : public PixelBuffer
{
public:
	DepthBuffer(float clearDepth = 0.0f, uint8_t clearStencil = 0)
		: m_clearDepth(clearDepth), m_clearStencil(clearStencil)
	{
		m_DSV[0].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_DSV[1].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_DSV[2].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_DSV[3].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_depthSRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_stencilSRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	void Create(const std::wstring& name, uint32_t width, uint32_t height, DXGI_FORMAT format,
		D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

	void Create(const std::wstring& name, uint32_t width, uint32_t height, uint32_t numSamples, DXGI_FORMAT format,
		D3D12_GPU_VIRTUAL_ADDRESS vidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV() const { return m_DSV[0]; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_DepthReadOnly() const { return m_DSV[1]; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_StencilReadOnly() const { return m_DSV[2]; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_ReadOnly() const { return m_DSV[3]; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDepthSRV() const { return m_depthSRV; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetStencilSRV() const { return m_stencilSRV; }

	float GetClearDepth() const { return m_clearDepth; }
	uint8_t GetClearStencil() const { return m_clearStencil; }

private:

	void CreateDerivedViews(ID3D12Device* device, DXGI_FORMAT format);

	float m_clearDepth;
	uint8_t m_clearStencil;
	D3D12_CPU_DESCRIPTOR_HANDLE m_DSV[4];
	D3D12_CPU_DESCRIPTOR_HANDLE m_depthSRV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_stencilSRV;
};