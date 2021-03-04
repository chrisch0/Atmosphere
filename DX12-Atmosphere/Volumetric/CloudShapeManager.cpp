#include "stdafx.h"
#include "CloudShapeManager.h"
#include "Noise/NoiseGenerator.h"
#include "D3D12/ColorBuffer.h"
#include "D3D12/CommandContext.h"
#include "D3D12/GraphicsGlobal.h"

#include "CompiledShaders/GenerateBasicShape_CS.h"
#include "CompiledShaders/GenerateGradient_CS.h"
#include "CompiledShaders/GenerateErosionNoise_CS.h"

void CloudShapeManager::Initialize()
{
	m_noiseGenerator = std::make_shared<NoiseGenerator>();
	m_noiseGenerator->Initialize();
	m_noiseGenerator->SetShowWindow(false);

	m_basicShapeRS.Reset(3, 2);
	m_basicShapeRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_basicShapeRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
	m_basicShapeRS[2].InitAsConstantBufferView(0);
	m_basicShapeRS.InitStaticSampler(0, Global::SamplerPointWrapDesc);
	m_basicShapeRS.InitStaticSampler(1, Global::SamplerLinearWrapDesc);
	m_basicShapeRS.Finalize(L"BasicShapeRS");

	m_gradientRS.Reset(2, 0);
	m_gradientRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_gradientRS[1].InitAsConstantBufferView(0);
	m_gradientRS.Finalize(L"GradientRS");

	m_erosionRS.Reset(2, 0);
	m_erosionRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
	m_erosionRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_erosionRS.Finalize(L"ErosionRS");

	m_basicShapePSO.SetRootSignature(m_basicShapeRS);
	m_basicShapePSO.SetComputeShader(g_pGenerateBasicShape_CS, sizeof(g_pGenerateBasicShape_CS));
	m_basicShapePSO.Finalize();

	m_gradientPSO.SetRootSignature(m_gradientRS);
	m_gradientPSO.SetComputeShader(g_pGenerateGradient_CS, sizeof(g_pGenerateGradient_CS));
	m_gradientPSO.Finalize();

	m_erosionPSO.SetRootSignature(m_erosionRS);
	m_erosionPSO.SetComputeShader(g_pGenerateErosionNoise_CS, sizeof(g_pGenerateErosionNoise_CS));
	m_erosionPSO.Finalize();

	m_showWindow = true;

	m_basicShapeMin = 0.5f;
	m_basicShapeMax = 2.0f;
	m_worleySampleFreq = 1.34f;
	m_perlinSampleFreq = 13.57f;
	m_worleyAmp = -8.5f;
	m_perlinAmp = 2.49f;
	m_noiseBias = 2.19f;

	m_cloudMin = 1500.0f;
	m_cloudMax = 4000.0f;

	CreateBasicCloudShape();
	CreateGradient();
}

void CloudShapeManager::CreateBasicCloudShape()
{
	// Perlin
	NoiseState* perlin = new NoiseState();
	perlin->seed = 1567;
	perlin->frequency = 0.046f;
	perlin->noise_type = kNoisePerlin;
	perlin->fractal_type = kFractalFBM;
	perlin->octaves = 5;
	perlin->lacunarity = 3.430f;
	m_perlinNoise = m_noiseGenerator->CreateVolumeNoise("PerlinNoise", 128, 128, 128, DXGI_FORMAT_R32_FLOAT, perlin);

	// Worley FBM
	NoiseState* worleyFBM_Low = new NoiseState();
	worleyFBM_Low->seed = 2342;
	worleyFBM_Low->frequency = 0.123f;
	worleyFBM_Low->noise_type = kNoiseCellular;
	worleyFBM_Low->fractal_type = kFractalFBM;
	worleyFBM_Low->octaves = 5;
	worleyFBM_Low->lacunarity = 2.0f;
	worleyFBM_Low->SetInvert(true);
	m_worleyFBMLow = m_noiseGenerator->CreateVolumeNoise("WorleyFBMLow", 128, 128, 128, DXGI_FORMAT_R32_FLOAT, worleyFBM_Low);

	NoiseState* worleyFBM_Mid = new NoiseState(*worleyFBM_Low);
	worleyFBM_Mid->seed = 3424;
	worleyFBM_Mid->frequency = 0.277f;
	m_worleyFBMMid = m_noiseGenerator->CreateVolumeNoise("WorlyFBMMid", 128, 128, 128, DXGI_FORMAT_R32_FLOAT, worleyFBM_Mid);

	NoiseState* worleyFBM_High = new NoiseState(*worleyFBM_Mid);
	worleyFBM_High->seed = 1987;
	worleyFBM_High->frequency = 0.534f;
	m_worleyFBMHigh = m_noiseGenerator->CreateVolumeNoise("WorlyFBMHigh", 128, 128, 128, DXGI_FORMAT_R32_FLOAT, worleyFBM_High);

	if (m_basicShape == nullptr)
	{
		m_basicShape = std::make_shared<VolumeColorBuffer>();
		m_basicShape->Create(L"BasicShape", 128, 128, 128, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);
	}

	if (m_basicShapeView == nullptr)
	{
		m_basicShapeView = std::make_shared<ColorBuffer>();
		m_basicShapeView->Create(L"BasicShapeView", 128, 128, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	}

	GenerateBasicCloudShape();

	NoiseState* worleyFBM_Low32 = new NoiseState(*worleyFBM_Low);
	NoiseState* worleyFBM_Mid32 = new NoiseState(*worleyFBM_Mid);
	NoiseState* worleyFBM_High32 = new NoiseState(*worleyFBM_High);
	m_worleyFBMLow32 = m_noiseGenerator->CreateVolumeNoise("WorlyFBMLow32", 32, 32, 32, DXGI_FORMAT_R32_FLOAT, worleyFBM_Low32);
	m_worleyFBMMid32 = m_noiseGenerator->CreateVolumeNoise("WorlyFBMMid32", 32, 32, 32, DXGI_FORMAT_R32_FLOAT, worleyFBM_Mid32);
	m_worleyFBMHigh32 = m_noiseGenerator->CreateVolumeNoise("WorlyFBMHigh32", 32, 32, 32, DXGI_FORMAT_R32_FLOAT, worleyFBM_High32);
	if (m_erosion == nullptr)
	{
		m_erosion = std::make_shared<VolumeColorBuffer>();
		m_erosion->Create(L"Erosion", 32, 32, 32, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);
	}
	GenerateErosion();
}

void CloudShapeManager::CreateGradient()
{
	if (m_densityHeightGradinet == nullptr)
	{
		m_densityHeightGradinet = std::make_shared<ColorBuffer>();
		m_densityHeightGradinet->Create(L"HeightDensityGradient", 6, 64, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	}
	m_cloudTypeParams.stratus = Vector4(0.0f, 0.058f, 0.068f, 0.13f);
	m_cloudTypeParams.cumulus = Vector4(0.0f, 0.27f, 0.32f, 0.65f);
	m_cloudTypeParams.cumulonimbus = Vector4(0.0f, 0.09f, 0.72f, 1.0f);
	m_cloudTypeParams.textureSize = Vector4((float)m_densityHeightGradinet->GetWidth(), (float)m_densityHeightGradinet->GetHeight(), 0.0f, 0.0f);

	GenerateDensityHeightGradient();
}

void CloudShapeManager::GenerateBasicCloudShape()
{
	struct
	{
		float min;
		float max;
		float perlinFreq;
		float worleyFreq;
		float perlinAmp;
		float worleyAmp;
		float noiseBias;
		int method;
		Vector3 textureSize;
	} shape_setting;
	shape_setting.min = m_basicShapeMin;
	shape_setting.max = m_basicShapeMax;
	shape_setting.perlinFreq = m_perlinSampleFreq;
	shape_setting.worleyFreq = m_worleySampleFreq;
	shape_setting.perlinAmp = m_perlinAmp;
	shape_setting.worleyAmp = m_worleyAmp;
	shape_setting.noiseBias = m_noiseBias;
	shape_setting.method = m_method2;
	shape_setting.textureSize = Vector3((float)m_basicShape->GetWidth(), (float)m_basicShape->GetHeight(), (float)m_basicShape->GetDepth());

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
	context.SetDynamicConstantBufferView(2, sizeof(shape_setting), &shape_setting);
	context.Dispatch3D(m_basicShape->GetWidth(), m_basicShape->GetHeight(), m_basicShape->GetDepth(), 8, 8, 8);
	context.TransitionResource(*m_basicShape, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_basicShapeView, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.Finish();

	ComputeContext& mipContext = ComputeContext::Begin();
	m_basicShape->GenerateMipMaps(mipContext);
	mipContext.Finish();
}

void CloudShapeManager::GenerateDensityHeightGradient()
{
	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_gradientRS);
	context.SetPipelineState(m_gradientPSO);
	context.TransitionResource(*m_densityHeightGradinet, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(0, 0, m_densityHeightGradinet->GetUAV());
	context.SetDynamicConstantBufferView(1, sizeof(m_cloudTypeParams), &m_cloudTypeParams);
	context.Dispatch2D(m_densityHeightGradinet->GetWidth(), m_densityHeightGradinet->GetHeight(), 1, 8);
	context.Finish();
}

void CloudShapeManager::GenerateErosion()
{
	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_erosionRS);
	context.SetPipelineState(m_erosionPSO);
	context.TransitionResource(*m_worleyFBMLow32, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_worleyFBMMid32, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_worleyFBMHigh32, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_erosion, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(0, 0, m_worleyFBMLow32->GetSRV());
	context.SetDynamicDescriptor(0, 1, m_worleyFBMMid32->GetSRV());
	context.SetDynamicDescriptor(0, 2, m_worleyFBMHigh32->GetSRV());
	context.SetDynamicDescriptor(1, 0, m_erosion->GetUAV());
	context.Dispatch3D(m_erosion->GetWidth(), m_erosion->GetHeight(), m_erosion->GetDepth(), 8, 8, 8);
	context.Finish();

	ComputeContext& mipContext = ComputeContext::Begin();
	m_erosion->GenerateMipMaps(mipContext);
	mipContext.Finish();
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

	dirty_flag = m_noiseGenerator->IsDirty("WorlyFBMLow32");
	dirty_flag |= m_noiseGenerator->IsDirty("WorlyFBMMid32");
	dirty_flag |= m_noiseGenerator->IsDirty("WorlyFBMHigh32");
	if (dirty_flag)
	{
		GenerateErosion();
	}
}

void CloudShapeManager::UpdateUI()
{
	m_noiseGenerator->UpdateUI();

	if (!m_showWindow)
		return;

	ImGui::Begin("Cloud Shape Manager", &m_showWindow);

	// Basic Cloud Shape
	bool refresh_basic_shape = false;
	ImGui::BeginGroup();
	{
		ImGui::PushItemWidth(125.0f);
		static bool use_method2 = false;
		refresh_basic_shape |= ImGui::Checkbox("Use Method2", &use_method2);
		if (use_method2)
		{
			m_method2 = (int)use_method2;
			refresh_basic_shape |= ImGui::InputFloat("Perlin Sample Freq", &m_perlinSampleFreq, 0.01f, 0.01f);
			refresh_basic_shape |= ImGui::InputFloat("Worley Sample Freq", &m_worleySampleFreq, 0.01f, 0.01f);
			refresh_basic_shape |= ImGui::InputFloat("Perlin Amp", &m_perlinAmp, 0.01f, 0.01f);
			refresh_basic_shape |= ImGui::InputFloat("Worley Amp", &m_worleyAmp, 0.01f, 0.01f);
			refresh_basic_shape |= ImGui::InputFloat("Noise Bias", &m_noiseBias, 0.01f, 0.01f);
		}
		else
		{
			m_method2 = (int)use_method2;
			ImGui::Text("Basic Cloud Shape");
			ImGui::Text("Basic Shape Range");
			refresh_basic_shape |= ImGui::InputFloat("Basic Shape Min", &m_basicShapeMin, 0.01f, 0.1f);
			refresh_basic_shape |= ImGui::InputFloat("Basic Shape Max", &m_basicShapeMax, 0.01f, 0.1f);
		}
		ImGui::PopItemWidth();

		if (refresh_basic_shape)
		{
			GenerateBasicCloudShape();
		}
	}
	ImGui::EndGroup();

	ImGui::SameLine(300.0f);
	ImGui::BeginGroup();
	{
		static bool tiled_basic_shape_window = false;
		static bool basic_shape_window_open = false;
		ImGui::PreviewVolumeImageButton(m_basicShape.get(), ImVec2(128.0f, 128.0f), "Basic Shape Texture 3D", &tiled_basic_shape_window, &basic_shape_window_open);

		ImGui::SameLine();
		static bool basic_shape_view = false;
		ImGui::PreviewImageButton(m_basicShapeView.get(), ImVec2(128.0f, 128.0f), "Basic Shape View", &basic_shape_view, &basic_shape_window_open);
	}
	ImGui::EndGroup();

	ImGui::Text("Erosion");
	ImGui::SameLine(300.0f);
	{
		static bool erosion_detail = false;
		static bool erosion_window_opening = false;
		ImGui::PreviewVolumeImageButton(m_erosion.get(), ImVec2(128.0f, 128.0f), "Erosion", &erosion_detail, &erosion_window_opening);
	}

	ImGui::Separator();

	// Cloud Density Gradient
	ImGui::PushItemWidth(125.0f);
	ImGui::Text("Cloud Density Gradient");
	ImGui::InputFloat("Cloud Altitude Min", &m_cloudMin, 10.0f, 10.0f, "%.1f");
	ImGui::InputFloat("Cloud Altitude Max", &m_cloudMax, 10.0f, 10.0f, "%.1f");

	static bool dirty_flag = false;
	// Stratus Gradient
	{
		ImGui::BeginGroup();
		{
			static float stratus_range[4] = { 0.0f, 0.058f, 0.068f, 0.13f};
			ImGui::Text("Stratus Gradient");
			ImGui::PushID(10000);
			dirty_flag |= ImGui::InputFloat("Cloud Bottom", stratus_range, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Fade-in", stratus_range + 1, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Fade-out", stratus_range + 2, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Top", stratus_range + 3, 0.001f);
			ImGui::PopID();

			Utils::Clamp(stratus_range[0], 0.0f, stratus_range[3] - 0.03f);
			Utils::Clamp(stratus_range[1], stratus_range[0] + 0.01f, stratus_range[2] - 0.01f);
			Utils::Clamp(stratus_range[2], stratus_range[1] + 0.01f, stratus_range[3] - 0.01f);
			Utils::Clamp(stratus_range[3], stratus_range[0] + 0.03f, 1.0f);

			m_cloudTypeParams.stratus = Vector4(stratus_range);

		}
		ImGui::EndGroup(); ImGui::SameLine(300.0f);
		ImGui::ImageButton((ImTextureID)(m_densityHeightGradinet->GetSRV().ptr), ImVec2(128.0f, 128.0f), ImVec2(0.125f, 0.0f), ImVec2(0.125f, 1.0f));
	}

	// Cumulus Gradient
	{
		static bool cumulus_dirty_flag = false;
		ImGui::BeginGroup();
		{
			static float cumulus_range[4] = { 0.0f, 0.27f, 0.32f, 0.65f };
			ImGui::Text("Cumulus Gradient");
			ImGui::PushID(10001);
			dirty_flag |= ImGui::InputFloat("Cloud Bottom", cumulus_range, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Fade-in", cumulus_range + 1, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Fade-out", cumulus_range + 2, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Top", cumulus_range + 3, 0.001f);
			ImGui::PopID();

			Utils::Clamp(cumulus_range[0], 0.0f, cumulus_range[3] - 0.03f);
			Utils::Clamp(cumulus_range[1], cumulus_range[0] + 0.01f, cumulus_range[2] - 0.01f);
			Utils::Clamp(cumulus_range[2], cumulus_range[1] + 0.01f, cumulus_range[3]);
			Utils::Clamp(cumulus_range[3], cumulus_range[0] + 0.03f, 1.0f);

			m_cloudTypeParams.cumulus = Vector4(cumulus_range);
		}
		ImGui::EndGroup(); ImGui::SameLine(300.0f);
		ImGui::ImageButton((ImTextureID)(m_densityHeightGradinet->GetSRV().ptr), ImVec2(128.0f, 128.0f), ImVec2(0.5f, 0.0f), ImVec2(0.5f, 1.0f));
	}

	// Cumulonimbus Gradient
	{
		static bool cumulonimbus_dirty_flag = false;
		ImGui::BeginGroup();
		{
			static float cumulonimbus_range[4] = { 0.0f, 0.09f, 0.72f, 1.0f };
			ImGui::Text("Cumulonimbus Gradient");
			ImGui::PushID(10002);
			dirty_flag |= ImGui::InputFloat("Cloud Bottom", cumulonimbus_range, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Fade-in", cumulonimbus_range + 1, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Fade-out", cumulonimbus_range + 2, 0.001f);
			dirty_flag |= ImGui::InputFloat("Cloud Top", cumulonimbus_range + 3, 0.001f);
			ImGui::PopID();

			Utils::Clamp(cumulonimbus_range[0], 0.0f, cumulonimbus_range[3] - 0.03f);
			Utils::Clamp(cumulonimbus_range[1], cumulonimbus_range[0] + 0.01f, cumulonimbus_range[2] - 0.01f);
			Utils::Clamp(cumulonimbus_range[2], cumulonimbus_range[1] + 0.01f, cumulonimbus_range[3]);
			Utils::Clamp(cumulonimbus_range[3], cumulonimbus_range[0] + 0.03f, 1.0f);

			m_cloudTypeParams.cumulonimbus = Vector4(cumulonimbus_range);
		}
		ImGui::EndGroup(); ImGui::SameLine(300.0f);
		ImGui::ImageButton((ImTextureID)(m_densityHeightGradinet->GetSRV().ptr), ImVec2(128.0f, 128.0f), ImVec2(0.8f, 0.0f), ImVec2(0.8f, 1.0f));
	}
	ImGui::PopItemWidth();

	ImGui::End();

	if (dirty_flag)
		GenerateDensityHeightGradient();
}

void CloudShapeManager::SetShowNoiseGeneratorWindow(bool val) 
{
	m_noiseGenerator->SetShowWindow(val); 
}
