#include "stdafx.h"
#include "CloudShapeManager.h"
#include "Noise/NoiseGenerator.h"
#include "D3D12/ColorBuffer.h"
#include "D3D12/CommandContext.h"
#include "D3D12/GraphicsGlobal.h"

#include "CompiledShaders/GenerateBasicShape_CS.h"

void CloudShapeManager::Initialize()
{
	m_noiseGenerator = std::make_shared<NoiseGenerator>();
	m_noiseGenerator->Initialize();

	m_basicShapeRS.Reset(3, 2);
	m_basicShapeRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_basicShapeRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
	m_basicShapeRS[2].InitAsConstantBufferView(0);
	m_basicShapeRS.InitStaticSampler(0, Global::SamplerPointBorderDesc);
	m_basicShapeRS.InitStaticSampler(1, Global::SamplerLinearBorderDesc);
	m_basicShapeRS.Finalize(L"BasicShapeRS");

	m_basicShapePSO.SetRootSignature(m_basicShapeRS);
	m_basicShapePSO.SetComputeShader(g_pGenerateBasicShape_CS, sizeof(g_pGenerateBasicShape_CS));
	m_basicShapePSO.Finalize();

	m_showWindow = true;

	m_basicShapeMin = 0.5f;
	m_basicShapeMax = 2.0f;
}

void CloudShapeManager::CreateBasicCloudShape()
{
	// Perlin
	NoiseState* perlin = new NoiseState();
	perlin->seed = 1567;
	perlin->frequency = 0.05f;
	perlin->noise_type = kNoisePerlin;
	perlin->fractal_type = kFractalNone;
	m_perlinNoise = m_noiseGenerator->CreateVolumeNoise("PerlinNoise", 128, 128, 128, DXGI_FORMAT_R32_FLOAT, perlin);

	// Worley FBM
	NoiseState* worleyFBM_Low = new NoiseState();
	worleyFBM_Low->seed = 2342;
	worleyFBM_Low->frequency = 0.05f;
	worleyFBM_Low->noise_type = kNoiseCellular;
	worleyFBM_Low->fractal_type = kFractalFBM;
	worleyFBM_Low->octaves = 5;
	worleyFBM_Low->lacunarity = 2.0f;
	worleyFBM_Low->SetInvert(true);
	m_worleyFBMLow = m_noiseGenerator->CreateVolumeNoise("WorleyFBMLow", 128, 128, 128, DXGI_FORMAT_R32_FLOAT, worleyFBM_Low);

	NoiseState* worleyFBM_Mid = new NoiseState(*worleyFBM_Low);
	worleyFBM_Mid->seed = 3424;
	worleyFBM_Mid->frequency = 0.1f;
	m_worleyFBMMid = m_noiseGenerator->CreateVolumeNoise("WorlyFBMMid", 128, 128, 128, DXGI_FORMAT_R32_FLOAT, worleyFBM_Mid);

	NoiseState* worleyFBM_High = new NoiseState(*worleyFBM_Mid);
	worleyFBM_High->seed = 1987;
	worleyFBM_High->frequency = 0.2f;
	m_worleyFBMHigh = m_noiseGenerator->CreateVolumeNoise("WorlyFBMHigh", 128, 128, 128, DXGI_FORMAT_R32_FLOAT, worleyFBM_High);

	if (m_basicShape == nullptr)
	{
		m_basicShape = std::make_shared<VolumeColorBuffer>();
		m_basicShape->Create(L"BasicShape", 128, 128, 128, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	}

	if (m_basicShapeView == nullptr)
	{
		m_basicShapeView = std::make_shared<ColorBuffer>();
		m_basicShapeView->Create(L"BasicShapeView", 128, 128, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	}

	GenerateBasicCloudShape();
}

void CloudShapeManager::GenerateBasicCloudShape()
{
	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_basicShapeRS);
	context.SetPipelineState(m_basicShapePSO);
	context.TransitionResource(*m_perlinNoise, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_worleyFBMLow, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_worleyFBMMid, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_worleyFBMHigh, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_basicShape, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.TransitionResource(*m_basicShapeView, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(0, 0, m_perlinNoise->GetSRV());
	context.SetDynamicDescriptor(0, 1, m_worleyFBMLow->GetSRV());
	context.SetDynamicDescriptor(0, 2, m_worleyFBMMid->GetSRV());
	context.SetDynamicDescriptor(0, 3, m_worleyFBMHigh->GetSRV());
	context.SetDynamicDescriptor(1, 0, m_basicShape->GetUAV());
	context.SetDynamicDescriptor(1, 1, m_basicShapeView->GetUAV());
	struct
	{
		float min;
		float max;
	} shape_range;
	shape_range.min = m_basicShapeMin;
	shape_range.max = m_basicShapeMax;
	context.SetDynamicConstantBufferView(2, sizeof(shape_range), &shape_range);
	context.Dispatch3D(128, 128, 128, 8, 8, 8);
	context.TransitionResource(*m_basicShape, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_basicShapeView, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.Finish();
}

void CloudShapeManager::Update()
{
	bool dirty_flag = m_noiseGenerator->IsDirty("PerlinNoise");
	dirty_flag |= m_noiseGenerator->IsDirty("WorleyFBMLow");
	dirty_flag |= m_noiseGenerator->IsDirty("WorlyFBMMid");
	dirty_flag |= m_noiseGenerator->IsDirty("WorlyFBMHigh");
	if (dirty_flag)
	{
		GenerateBasicCloudShape();
	}
}

void CloudShapeManager::UpdateUI()
{
	m_noiseGenerator->UpdateUI();

	if (!m_showWindow)
		return;

	ImGui::Begin("Cloud Shape Manager", &m_showWindow);

	bool refresh_basic_shape = false;
	ImGui::BeginGroup();
	{
		ImGui::PushItemWidth(125);
		ImGui::Text("Basic Cloud Shape");
		ImGui::Text("Basic Shape Range");
		refresh_basic_shape |= ImGui::InputFloat("Basic Shape Min", &m_basicShapeMin, 0.01f, 0.1f);
		refresh_basic_shape |= ImGui::InputFloat("Basic Shape Max", &m_basicShapeMax, 0.01f, 0.1f);
		ImGui::PopItemWidth();
	}
	ImGui::EndGroup();

	ImGui::SameLine();
	ImGui::BeginGroup();
	{
		static bool tiled_basic_shape_window = false;
		static bool window_open = false;
		if (ImGui::VolumeImageButton((ImTextureID)(m_basicShape->GetSRV().ptr), ImVec2(128.0f, 128.0f), m_basicShape->GetDepth()))
		{
			tiled_basic_shape_window = !tiled_basic_shape_window;
			window_open = true;
		}
		if (tiled_basic_shape_window)
		{
			ImGui::Begin("Basic Shape Texture 3D", &tiled_basic_shape_window);
			Utils::AutoResizeVolumeImage(m_basicShape.get(), window_open);
			ImGui::End();
			window_open = false;
		}

		ImGui::SameLine();
		static bool basic_shape_view = false;
		if (ImGui::ImageButton((ImTextureID)(m_basicShapeView->GetSRV().ptr), ImVec2(128.0f, 128.0f)))
		{
			basic_shape_view = !basic_shape_view;
			window_open = true;
		}
		if (basic_shape_view)
		{
			ImGui::Begin("Basic Shape View", &basic_shape_view);
			Utils::AutoResizeImage(m_basicShapeView.get(), window_open);
			ImGui::End();
			window_open = false;
		}
	}
	ImGui::EndGroup();

	ImGui::End();

	if (refresh_basic_shape)
	{
		GenerateBasicCloudShape();
	}
}