#pragma once
#include "stdafx.h"

class RootParameter : public CD3DX12_ROOT_PARAMETER
{
	friend class RootSignature;
public:

	RootParameter()
	{
		ParameterType = (D3D12_ROOT_PARAMETER_TYPE)0xFFFFFFFF;
	}

	~RootParameter()
	{
		Clear();
	}

	void Clear()
	{
		if (ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			delete[] DescriptorTable.pDescriptorRanges;

		ParameterType = (D3D12_ROOT_PARAMETER_TYPE)0xFFFFFFFF;
	}

	// Descriptor table but has only one descriptor range
	void InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT shaderRegister, UINT numDescriptors, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		InitAsDescriptorTable(1, visibility);
		SetTableRange(0, type, shaderRegister, numDescriptors);
	}

	void InitAsDescriptorTable(UINT numDescriptorRanges, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		CD3DX12_ROOT_PARAMETER
			::InitAsDescriptorTable(numDescriptorRanges, new D3D12_DESCRIPTOR_RANGE[numDescriptorRanges], visibility);
	}

	void SetTableRange(UINT rangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT shaderRegister, UINT numDescriptors, UINT space = 0)
	{
		D3D12_DESCRIPTOR_RANGE* range = const_cast<D3D12_DESCRIPTOR_RANGE*>(DescriptorTable.pDescriptorRanges + rangeIndex);
		range->RangeType = type;
		range->NumDescriptors = numDescriptors;
		range->BaseShaderRegister = shaderRegister;
		range->RegisterSpace = space;
		range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}
};

class RootSignature
{
	friend class DynamicDescriptorHeap;
public:
	RootSignature(uint32_t numRootParams = 0, uint32_t numStaticSamplers = 0)
		: m_finalized(false), m_numParameters(numRootParams)
	{
		Reset(numRootParams, numStaticSamplers);
	}

	~RootSignature()
	{
	}

	void Reset(uint32_t numRootParames, uint32_t numStaticSamplers = 0)
	{
		if (numRootParames > 0)
			m_paramArray.reset(new RootParameter[numRootParames]);
		else
			m_paramArray = nullptr;
		m_numParameters = numRootParames;

		if (numStaticSamplers > 0)
			m_samplerArray.reset(new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers]);
		else
			m_samplerArray = nullptr;

		m_numSamplers = numStaticSamplers;
		m_numInitializedStaticSamplers = 0;
	}

	RootParameter& operator[](size_t index)
	{
		assert(index < m_numParameters);
		return m_paramArray.get()[index];
	}

	const RootParameter& operator[](size_t index) const
	{
		assert(index < m_numParameters);
		return m_paramArray.get()[index];
	}

	void InitStaticSampler(uint32_t shaderRegister, const D3D12_SAMPLER_DESC& nonStaticSamplerDesc,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

	void InitStaticSampler(uint32_t shaderRegister, const D3D12_STATIC_SAMPLER_DESC& staticSamplerDesc, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

	void Finalize(const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

	ID3D12RootSignature* GetSignature() const { return m_signature; }

private:
	bool m_finalized;
	uint32_t m_numParameters;
	uint32_t m_numSamplers;
	uint32_t m_numInitializedStaticSamplers;
	// One bit is set for root parameters that are non-sampler descriptor tables
	uint32_t m_descriptorTableBitMap;
	// One bit is set for root parameters that are sampler descriptor tables
	uint32_t m_samplerTableBitMap;
	uint32_t m_descriptorTableSize[16];
	std::unique_ptr<RootParameter[]> m_paramArray;
	std::unique_ptr<D3D12_STATIC_SAMPLER_DESC[]> m_samplerArray;
	ID3D12RootSignature* m_signature;
};