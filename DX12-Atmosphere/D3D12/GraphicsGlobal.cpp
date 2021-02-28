#include "stdafx.h"
#include "GraphicsGlobal.h"
#include "RootSignature.h"
#include "PipelineState.h"

#include "CompiledShaders/CreateVolumeTexture_CS.h"
#include "CompiledShaders/Generate2DMipsGamma_CS.h"
#include "CompiledShaders/Generate2DMipsGammaOdd_CS.h"
#include "CompiledShaders/Generate2DMipsGammaOddX_CS.h"
#include "CompiledShaders/Generate2DMipsGammaOddY_CS.h"
#include "CompiledShaders/Generate2DMipsLinear_CS.h"
#include "CompiledShaders/Generate2DMipsLinearOdd_CS.h"
#include "CompiledShaders/Generate2DMipsLinearOddX_CS.h"
#include "CompiledShaders/Generate2DMipsLinearOddY_CS.h"
#include "CompiledShaders/Generate3DMipsGamma_CS.h"
#include "CompiledShaders/Generate3DMipsLinear_CS.h"

namespace Global
{
	D3D12_STATIC_SAMPLER_DESC SamplerDefault;
	D3D12_STATIC_SAMPLER_DESC SamplerLinearWrapDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerAnisoWrapDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerShadowDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerLinearClampDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerVolumeWrapDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerPointClampDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerPointBorderDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerLinearBorderDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerLinearMirrorDesc;
	D3D12_STATIC_SAMPLER_DESC SamplerPointWrapDesc;

	//D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearWrap;
	//D3D12_CPU_DESCRIPTOR_HANDLE SamplerAnisoWrap;
	//D3D12_CPU_DESCRIPTOR_HANDLE SamplerShadow;
	//D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearClamp;
	//D3D12_CPU_DESCRIPTOR_HANDLE SamplerVolumeWrap;
	//D3D12_CPU_DESCRIPTOR_HANDLE SamplerPointClamp;
	//D3D12_CPU_DESCRIPTOR_HANDLE SamplerPointBorder;
	//D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearBorder;

	D3D12_RASTERIZER_DESC RasterizerDefault;    // Counter-clockwise
	D3D12_RASTERIZER_DESC RasterizerDefaultMsaa;
	D3D12_RASTERIZER_DESC RasterizerDefaultCw;    // Clockwise winding
	D3D12_RASTERIZER_DESC RasterizerDefaultCwMsaa;
	D3D12_RASTERIZER_DESC RasterizerTwoSided;
	D3D12_RASTERIZER_DESC RasterizerTwoSidedMsaa;
	D3D12_RASTERIZER_DESC RasterizerShadow;
	D3D12_RASTERIZER_DESC RasterizerShadowCW;
	D3D12_RASTERIZER_DESC RasterizerShadowTwoSided;

	D3D12_BLEND_DESC BlendNoColorWrite;
	D3D12_BLEND_DESC BlendDisable;
	D3D12_BLEND_DESC BlendPreMultiplied;
	D3D12_BLEND_DESC BlendTraditional;
	D3D12_BLEND_DESC BlendAdditive;
	D3D12_BLEND_DESC BlendTraditionalAdditive;

	D3D12_DEPTH_STENCIL_DESC DepthStateDisabled;
	D3D12_DEPTH_STENCIL_DESC DepthStateReadWrite;
	D3D12_DEPTH_STENCIL_DESC DepthStateReadOnly;
	D3D12_DEPTH_STENCIL_DESC DepthStateReadOnlyReversed;
	D3D12_DEPTH_STENCIL_DESC DepthStateTestEqual;

	RootSignature CreateVolumeTextureRS;
	ComputePSO CreateVolumeTexturePSO;

	RootSignature GenerateMipsRS;
	ComputePSO Generate2DMipsLinearPSO[4];
	ComputePSO Generate2DMipsGammaPSO[4];
	ComputePSO Generate3DMipsLinearPSO[4];
	ComputePSO Generate3DMipsGammaPSO[4];

	void SetTextureAddressMode(D3D12_STATIC_SAMPLER_DESC& desc, D3D12_TEXTURE_ADDRESS_MODE mode)
	{
		desc.AddressU = mode;
		desc.AddressV = mode;
		desc.AddressW = mode;
	}

	void InitializeGlobalStates()
	{
		SamplerDefault = {};
		SamplerDefault.Filter = D3D12_FILTER_ANISOTROPIC;
		SamplerDefault.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		SamplerDefault.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		SamplerDefault.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		SamplerDefault.MipLODBias = 0.0f;
		SamplerDefault.MaxAnisotropy = 16;
		SamplerDefault.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		SamplerDefault.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		SamplerDefault.MinLOD = 0.0f;
		SamplerDefault.MaxLOD = D3D12_FLOAT32_MAX;

		SamplerLinearWrapDesc = SamplerDefault;
		SamplerLinearWrapDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

		SamplerAnisoWrapDesc = SamplerDefault;
		SamplerAnisoWrapDesc.MaxAnisotropy = 4;

		SamplerShadowDesc = SamplerDefault;
		SamplerShadowDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		SamplerShadowDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		SetTextureAddressMode(SamplerShadowDesc, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		SamplerLinearClampDesc = SamplerDefault;
		SamplerLinearClampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		SetTextureAddressMode(SamplerLinearClampDesc, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		SamplerVolumeWrapDesc = SamplerDefault;
		SamplerVolumeWrapDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

		SamplerPointClampDesc = SamplerDefault;
		SamplerPointClampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		SetTextureAddressMode(SamplerPointClampDesc, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		
		SamplerPointBorderDesc = SamplerDefault;
		SamplerPointBorderDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		SetTextureAddressMode(SamplerPointBorderDesc, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
		SamplerPointBorderDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

		SamplerLinearBorderDesc = SamplerDefault;
		SamplerLinearBorderDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		SetTextureAddressMode(SamplerLinearBorderDesc, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
		SamplerLinearBorderDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

		SamplerLinearMirrorDesc = SamplerLinearWrapDesc;
		SetTextureAddressMode(SamplerLinearMirrorDesc, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);

		SamplerPointWrapDesc = SamplerPointBorderDesc;
		SetTextureAddressMode(SamplerPointWrapDesc, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

		// Default rasterizer states
		RasterizerDefault.FillMode = D3D12_FILL_MODE_SOLID;
		RasterizerDefault.CullMode = D3D12_CULL_MODE_BACK;
		RasterizerDefault.FrontCounterClockwise = TRUE;
		RasterizerDefault.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		RasterizerDefault.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		RasterizerDefault.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		RasterizerDefault.DepthClipEnable = TRUE;
		RasterizerDefault.MultisampleEnable = FALSE;
		RasterizerDefault.AntialiasedLineEnable = FALSE;
		RasterizerDefault.ForcedSampleCount = 0;
		RasterizerDefault.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		RasterizerDefaultMsaa = RasterizerDefault;
		RasterizerDefaultMsaa.MultisampleEnable = TRUE;

		RasterizerDefaultCw = RasterizerDefault;
		RasterizerDefaultCw.FrontCounterClockwise = FALSE;

		RasterizerDefaultCwMsaa = RasterizerDefaultCw;
		RasterizerDefaultCwMsaa.MultisampleEnable = TRUE;

		RasterizerTwoSided = RasterizerDefault;
		RasterizerTwoSided.CullMode = D3D12_CULL_MODE_NONE;

		RasterizerTwoSidedMsaa = RasterizerTwoSided;
		RasterizerTwoSidedMsaa.MultisampleEnable = TRUE;

		// Shadows need their own rasterizer state so we can reverse the winding of faces
		RasterizerShadow = RasterizerDefault;
		//RasterizerShadow.CullMode = D3D12_CULL_FRONT;  // Hacked here rather than fixing the content
		RasterizerShadow.SlopeScaledDepthBias = -1.5f;
		RasterizerShadow.DepthBias = -100;

		RasterizerShadowTwoSided = RasterizerShadow;
		RasterizerShadowTwoSided.CullMode = D3D12_CULL_MODE_NONE;

		RasterizerShadowCW = RasterizerShadow;
		RasterizerShadowCW.FrontCounterClockwise = FALSE;

		DepthStateDisabled.DepthEnable = FALSE;
		DepthStateDisabled.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		DepthStateDisabled.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		DepthStateDisabled.StencilEnable = FALSE;
		DepthStateDisabled.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		DepthStateDisabled.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		DepthStateDisabled.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		DepthStateDisabled.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		DepthStateDisabled.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		DepthStateDisabled.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		DepthStateDisabled.BackFace = DepthStateDisabled.FrontFace;

		DepthStateReadWrite = DepthStateDisabled;
		DepthStateReadWrite.DepthEnable = TRUE;
		DepthStateReadWrite.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		DepthStateReadWrite.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

		DepthStateReadOnly = DepthStateReadWrite;
		DepthStateReadOnly.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		DepthStateReadOnlyReversed = DepthStateReadOnly;
		DepthStateReadOnlyReversed.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

		DepthStateTestEqual = DepthStateReadOnly;
		DepthStateTestEqual.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;

		D3D12_BLEND_DESC alphaBlend = {};
		alphaBlend.IndependentBlendEnable = FALSE;
		alphaBlend.RenderTarget[0].BlendEnable = FALSE;
		alphaBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		alphaBlend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		alphaBlend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		alphaBlend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		alphaBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		alphaBlend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		alphaBlend.RenderTarget[0].RenderTargetWriteMask = 0;
		BlendNoColorWrite = alphaBlend;

		alphaBlend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		BlendDisable = alphaBlend;

		alphaBlend.RenderTarget[0].BlendEnable = TRUE;
		BlendTraditional = alphaBlend;

		alphaBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		BlendPreMultiplied = alphaBlend;

		alphaBlend.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		BlendAdditive = alphaBlend;

		alphaBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		BlendTraditionalAdditive = alphaBlend;

		CreateVolumeTextureRS.Reset(3);
		CreateVolumeTextureRS[0].InitAsConstants(4, 0);
		CreateVolumeTextureRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		CreateVolumeTextureRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
		CreateVolumeTextureRS.Finalize(L"CreateVolumeTextureRS");

		CreateVolumeTexturePSO.SetRootSignature(CreateVolumeTextureRS);
		CreateVolumeTexturePSO.SetComputeShader(g_pCreateVolumeTexture_CS, sizeof(g_pCreateVolumeTexture_CS));
		CreateVolumeTexturePSO.Finalize();

		GenerateMipsRS.Reset(3, 1);
		GenerateMipsRS[0].InitAsConstants(8, 0);
		GenerateMipsRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		GenerateMipsRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
		GenerateMipsRS.InitStaticSampler(0, SamplerLinearClampDesc);
		GenerateMipsRS.Finalize(L"Generate Mips");

#define CreatePSO(ObjName, ShaderByteCode ) \
		ObjName.SetRootSignature(GenerateMipsRS); \
		ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
		ObjName.Finalize();

		CreatePSO(Generate2DMipsLinearPSO[0], g_pGenerate2DMipsLinear_CS);
		CreatePSO(Generate2DMipsLinearPSO[1], g_pGenerate2DMipsLinearOddX_CS);
		CreatePSO(Generate2DMipsLinearPSO[2], g_pGenerate2DMipsLinearOddY_CS);
		CreatePSO(Generate2DMipsLinearPSO[3], g_pGenerate2DMipsLinearOdd_CS);
		CreatePSO(Generate2DMipsGammaPSO[0], g_pGenerate2DMipsGamma_CS);
		CreatePSO(Generate2DMipsGammaPSO[1], g_pGenerate2DMipsGammaOddX_CS);
		CreatePSO(Generate2DMipsGammaPSO[2], g_pGenerate2DMipsGammaOddY_CS);
		CreatePSO(Generate2DMipsGammaPSO[3], g_pGenerate2DMipsGammaOdd_CS);
		CreatePSO(Generate3DMipsLinearPSO[0], g_pGenerate3DMipsLinear_CS);
		CreatePSO(Generate3DMipsGammaPSO[0], g_pGenerate3DMipsGamma_CS);
	}

	void DestroyGlobalStates()
	{

	}
}