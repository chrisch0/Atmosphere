#include "stdafx.h"
#include "RootSignature.h"

void RootSignature::InitStaticSampler(uint32_t shaderRegister, const D3D12_SAMPLER_DESC& nonStaticSamplerDesc, D3D12_SHADER_VISIBILITY visibility /* = D3D12_SHADER_VISIBILITY_ALL */)
{
	assert(m_numInitializedStaticSamplers < m_numSamplers);
	D3D12_STATIC_SAMPLER_DESC& staticSamplerDesc = m_samplerArray[m_numInitializedStaticSamplers++];

	staticSamplerDesc.Filter = nonStaticSamplerDesc.Filter;
	staticSamplerDesc.AddressU = nonStaticSamplerDesc.AddressU;
	staticSamplerDesc.AddressV = nonStaticSamplerDesc.AddressV;
	staticSamplerDesc.AddressW = nonStaticSamplerDesc.AddressW;
	staticSamplerDesc.MipLODBias = nonStaticSamplerDesc.MipLODBias;
	staticSamplerDesc.MaxAnisotropy = nonStaticSamplerDesc.MaxAnisotropy;
	staticSamplerDesc.ComparisonFunc = nonStaticSamplerDesc.ComparisonFunc;
	staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	staticSamplerDesc.MinLOD = nonStaticSamplerDesc.MinLOD;
	staticSamplerDesc.MaxLOD = nonStaticSamplerDesc.MaxLOD;
	staticSamplerDesc.ShaderRegister = shaderRegister;
	staticSamplerDesc.RegisterSpace = 0;
	staticSamplerDesc.ShaderVisibility = visibility;

	if (staticSamplerDesc.AddressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
		staticSamplerDesc.AddressV == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
		staticSamplerDesc.AddressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER)
	{
		if (nonStaticSamplerDesc.BorderColor[2] == 1.0f)
		{
			if (nonStaticSamplerDesc.BorderColor[0] == 1.0f)
				staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			else
				staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		}
		else
		{
			staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		}
	}
}

void RootSignature::Finalize(const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS flags /* = D3D12_ROOT_SIGNATURE_FLAG_NONE */)
{
	if (m_finalized)
		return;

	assert(m_numInitializedStaticSamplers == m_numSamplers);

	D3D12_ROOT_SIGNATURE_DESC rootDesc;
	rootDesc.NumParameters = m_numParameters;
	rootDesc.pParameters = (const D3D12_ROOT_PARAMETER*)m_paramArray.get();
	rootDesc.NumStaticSamplers = m_numSamplers;
	rootDesc.pStaticSamplers = (const D3D12_STATIC_SAMPLER_DESC*)m_samplerArray.get();
	rootDesc.Flags = flags;

	m_descriptorTableBitMap = 0;
	m_samplerTableBitMap = 0;

	// set sampler/descriptor table bit map, and calculate total descriptor numbers of every descriptor ranges in each descriptor table 
	for (uint32_t param = 0; param < m_numParameters; ++param)
	{
		const D3D12_ROOT_PARAMETER& rootParam = rootDesc.pParameters[param];
		m_descriptorTableSize[param] = 0;

		if (rootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			assert(rootParam.DescriptorTable.pDescriptorRanges != nullptr);

			if (rootParam.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
				m_samplerTableBitMap |= (1 << param);
			else
				m_descriptorTableBitMap |= (1 << param);

			for (uint32_t tableRange = 0; tableRange < rootParam.DescriptorTable.NumDescriptorRanges; ++tableRange)
				m_descriptorTableSize[param] += rootParam.DescriptorTable.pDescriptorRanges[tableRange].NumDescriptors;
		}
	}

	Microsoft::WRL::ComPtr<ID3DBlob> pOutBlob, pErrorBlob;

	ThrowIfFailed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, pOutBlob.GetAddressOf(), pErrorBlob.GetAddressOf()));
	ThrowIfFailed(g_Device->CreateRootSignature(1, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_signature)));

	m_signature->SetName(name.c_str());

	m_finalized = true;
}