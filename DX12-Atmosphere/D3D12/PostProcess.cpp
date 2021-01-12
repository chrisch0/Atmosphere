#include "stdafx.h"
#include "PostProcess.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "GpuBuffer.h"

#include "CompiledShaders/ExtractLuma_CS.h"
#include "CompiledShaders/GenerateHistogram_CS.h"
#include "CompiledShaders/AdaptExposure_CS.h"

namespace PostProcess
{
	const float kInitialMinLog = -12.0f;
	const float kInitialMaxLog = 4.0f;

	bool EnableHDR = false;
	bool EnableAdaptation = true;
	float Exposure = 2.0f;

	ExposureConstants ExposureCB;

	StructuredBuffer ExposureBuffer;
	ByteAddressBuffer HistogramBuffer;

	ColorBuffer LumaLR;

	RootSignature PostProcessRS;
	ComputePSO ExtractLumaPSO;
	ComputePSO GenerateHistogramPSO;
	ComputePSO AdaptExposurePSO;

	ColorBuffer* SceneColorBuffer;

	void UpdateExposure(ComputeContext&);
	void ExtractLuma(ComputeContext&);
	void ProcessHDR(ComputeContext&);
	void ProcessLDR(ComputeContext&);
}

void PostProcess::Initialize(ColorBuffer* sceneBuffer)
{
	SceneColorBuffer = sceneBuffer;

	PostProcessRS.Reset(4, 2);
	PostProcessRS.InitStaticSampler(0, Global::SamplerLinearClampDesc);
	PostProcessRS.InitStaticSampler(1, Global::SamplerLinearBorderDesc);
	PostProcessRS[0].InitAsConstants(0, 4);
	PostProcessRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
	PostProcessRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	PostProcessRS[3].InitAsConstantBufferView(1);
	PostProcessRS.Finalize(L"Post Process");

	ExtractLumaPSO.SetRootSignature(PostProcessRS);
	ExtractLumaPSO.SetComputeShader(g_pExtractLuma_CS, sizeof(g_pExtractLuma_CS));
	ExtractLumaPSO.Finalize();

	GenerateHistogramPSO.SetRootSignature(PostProcessRS);
	GenerateHistogramPSO.SetComputeShader(g_pGenerateHistogram_CS, sizeof(g_pGenerateHistogram_CS));
	GenerateHistogramPSO.Finalize();

	AdaptExposurePSO.SetRootSignature(PostProcessRS);
	AdaptExposurePSO.SetComputeShader(g_pAdaptExposure_CS, sizeof(g_pAdaptExposure_CS));
	AdaptExposurePSO.Finalize();

	float init_exposure[] =
	{
		Exposure, 1.0f / Exposure, Exposure, 0.0f,
		kInitialMinLog, kInitialMaxLog, kInitialMaxLog - kInitialMinLog, 1.0f / (kInitialMaxLog - kInitialMinLog)
	};
	ExposureBuffer.Create(L"Exposure", 8, 4, init_exposure);

	HistogramBuffer.Create(L"Histogram", 256, 4);

	uint32_t kBloomWidth = 640;
	uint32_t kBloomHeight = 384;
	LumaLR.Create(L"Luma Buffer", kBloomWidth, kBloomHeight, 1, DXGI_FORMAT_R8_UINT);
	ExposureCB.pixelCount = kBloomHeight * kBloomWidth;

	ExposureCB.targetLuminance = 0.08f;
	ExposureCB.adaptationRate = 0.05f;
	ExposureCB.minExposure = 1.0f / 64.0f;
	ExposureCB.maxExposure = 64.0f;
}

void PostProcess::ExtractLuma(ComputeContext& context)
{
	context.TransitionResource(LumaLR, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.TransitionResource(ExposureBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.SetConstants(0, 1.0f / LumaLR.GetWidth(), 1.0f / LumaLR.GetHeight());
	context.SetDynamicDescriptor(1, 0, LumaLR.GetUAV());
	context.SetDynamicDescriptor(2, 0, SceneColorBuffer->GetSRV());
	context.SetDynamicDescriptor(2, 1, ExposureBuffer.GetSRV());
	context.SetPipelineState(ExtractLumaPSO);
	context.Dispatch2D(LumaLR.GetWidth(), LumaLR.GetHeight());
}

void PostProcess::UpdateExposure(ComputeContext& context)
{
	if (!EnableAdaptation)
	{
		float init_exposure[] =
		{
			Exposure, 1.0f / Exposure, Exposure, 0.0f,
			kInitialMinLog, kInitialMaxLog, kInitialMaxLog - kInitialMinLog, 1.0f / (kInitialMaxLog - kInitialMinLog)
		};
		context.WriteBuffer(ExposureBuffer, 0, init_exposure, sizeof(init_exposure));
		context.TransitionResource(ExposureBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		return;
	}

	context.TransitionResource(HistogramBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
	context.ClearUAV(HistogramBuffer);
	context.TransitionResource(LumaLR, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.SetDynamicDescriptor(1, 0, HistogramBuffer.GetUAV());
	context.SetDynamicDescriptor(2, 0, LumaLR.GetSRV());
	context.SetPipelineState(GenerateHistogramPSO);
	context.Dispatch2D(LumaLR.GetWidth(), LumaLR.GetHeight(), 16, 384);

	context.TransitionResource(HistogramBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(ExposureBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(1, 0, ExposureBuffer.GetUAV());
	context.SetDynamicDescriptor(2, 0, HistogramBuffer.GetSRV());
	context.SetDynamicConstantBufferView(3, sizeof(ExposureCB), &ExposureCB);
	context.SetPipelineState(AdaptExposurePSO);
	context.Dispatch();
	context.TransitionResource(ExposureBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}