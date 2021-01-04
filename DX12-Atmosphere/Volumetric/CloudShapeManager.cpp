#include "stdafx.h"
#include "CloudShapeManager.h"
#include "Noise/NoiseGenerator.h"
#include "D3D12/ColorBuffer.h"
#include "D3D12/CommandContext.h"
#include "D3D12/GraphicsGlobal.h"

#include "CompiledShaders/GenerateBasicShape_CS.h"
#include "CompiledShaders/GenerateGradient_CS.h"

void CloudShapeManager::Initialize()
{
	m_noiseGenerator = std::make_shared<NoiseGenerator>();
	m_noiseGenerator->Initialize();
	m_noiseGenerator->SetShowWindow(false);

	m_basicShapeRS.Reset(3, 2);
	m_basicShapeRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_basicShapeRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
	m_basicShapeRS[2].InitAsConstantBufferView(0);
	m_basicShapeRS.InitStaticSampler(0, Global::SamplerPointBorderDesc);
	m_basicShapeRS.InitStaticSampler(1, Global::SamplerLinearBorderDesc);
	m_basicShapeRS.Finalize(L"BasicShapeRS");

	m_gradientRS.Reset(2, 0);
	m_gradientRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_gradientRS[1].InitAsConstantBufferView(0);
	m_gradientRS.Finalize(L"GradientRS");

	m_basicShapePSO.SetRootSignature(m_basicShapeRS);
	m_basicShapePSO.SetComputeShader(g_pGenerateBasicShape_CS, sizeof(g_pGenerateBasicShape_CS));
	m_basicShapePSO.Finalize();

	m_gradientPSO.SetRootSignature(m_gradientRS);
	m_gradientPSO.SetComputeShader(g_pGenerateGradient_CS, sizeof(g_pGenerateGradient_CS));
	m_gradientPSO.Finalize();

	m_showWindow = true;

	m_basicShapeMin = 0.5f;
	m_basicShapeMax = 2.0f;

	CreateBasicCloudShape();
	CreateGradient();
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

void CloudShapeManager::CreateGradient()
{
	if (m_stratusGradient == nullptr)
	{
		m_stratusGradient = std::make_shared<ColorBuffer>();
		m_stratusGradient->Create(L"StratusGradient", 1, 128, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	}
	GenerateGradient(m_stratusGradient, 0.05f, 0.18f, 0.1168f, 0.119f);

	if (m_cumulusGradient == nullptr)
	{
		m_cumulusGradient = std::make_shared<ColorBuffer>();
		m_cumulusGradient->Create(L"CumulusGradient", 1, 128, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	}
	GenerateGradient(m_cumulusGradient, 0.03f, 0.68f, 0.3f, 0.35f);

	if (m_cumulonimbusGradient == nullptr)
	{
		m_cumulonimbusGradient = std::make_shared<ColorBuffer>();
		m_cumulonimbusGradient->Create(L"CumulonimbusGradient", 1, 128, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	}
	GenerateGradient(m_cumulonimbusGradient, 0.01f, 1.0f, 0.1f, 0.73f);
}

void CloudShapeManager::GenerateBasicCloudShape()
{
	struct
	{
		float min;
		float max;
	} shape_range;
	shape_range.min = m_basicShapeMin;
	shape_range.max = m_basicShapeMax;

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
	context.SetDynamicConstantBufferView(2, sizeof(shape_range), &shape_range);
	context.Dispatch3D(m_basicShape->GetWidth(), m_basicShape->GetHeight(), m_basicShape->GetDepth(), 8, 8, 8);
	context.TransitionResource(*m_basicShape, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.TransitionResource(*m_basicShapeView, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.Finish();
}

void CloudShapeManager::GenerateGradient(std::shared_ptr<ColorBuffer> texPtr, float cloudMin, float cloudMax, float solidMin, float solidMax)
{
	struct
	{
		float cloudMin;
		float cloudMax;
		float solidCloudMin;
		float solidCloudMax;
		Vector4 textureSize;
	}cloudRange;
	cloudRange.cloudMin = cloudMin;
	cloudRange.cloudMax = cloudMax;
	cloudRange.solidCloudMin = solidMin;
	cloudRange.solidCloudMax = solidMax;
	cloudRange.textureSize = Vector4(texPtr->GetWidth(), texPtr->GetHeight(), 0.0f, 0.0f);

	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_gradientRS);
	context.SetPipelineState(m_gradientPSO);
	context.TransitionResource(*texPtr, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(0, 0, texPtr->GetUAV());
	context.SetDynamicConstantBufferView(1, sizeof(cloudRange), &cloudRange);
	context.Dispatch2D(texPtr->GetWidth(), texPtr->GetHeight(), 1, 8);
	context.TransitionResource(*texPtr, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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

	// Basic Cloud Shape
	bool refresh_basic_shape = false;
	ImGui::BeginGroup();
	{
		ImGui::PushItemWidth(125.0f);
		ImGui::Text("Basic Cloud Shape");
		ImGui::Text("Basic Shape Range");
		refresh_basic_shape |= ImGui::InputFloat("Basic Shape Min", &m_basicShapeMin, 0.01f, 0.1f);
		refresh_basic_shape |= ImGui::InputFloat("Basic Shape Max", &m_basicShapeMax, 0.01f, 0.1f);
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
		if (ImGui::VolumeImageButton((ImTextureID)(m_basicShape->GetSRV().ptr), ImVec2(128.0f, 128.0f), m_basicShape->GetDepth()))
		{
			tiled_basic_shape_window = !tiled_basic_shape_window;
			basic_shape_window_open = true;
		}
		if (tiled_basic_shape_window)
		{
			ImGui::Begin("Basic Shape Texture 3D", &tiled_basic_shape_window);
			Utils::AutoResizeVolumeImage(m_basicShape.get(), basic_shape_window_open);
			ImGui::End();
			basic_shape_window_open = false;
		}

		ImGui::SameLine();
		static bool basic_shape_view = false;
		if (ImGui::ImageButton((ImTextureID)(m_basicShapeView->GetSRV().ptr), ImVec2(128.0f, 128.0f)))
		{
			basic_shape_view = !basic_shape_view;
			basic_shape_window_open = true;
		}
		if (basic_shape_view)
		{
			ImGui::Begin("Basic Shape View", &basic_shape_view);
			Utils::AutoResizeImage(m_basicShapeView.get(), basic_shape_window_open);
			ImGui::End();
			basic_shape_window_open = false;
		}
	}
	ImGui::EndGroup();

	ImGui::Separator();

	// Cloud Density Gradient
	ImGui::SetNextItemWidth(125.0f);
	ImGui::Text("Cloud Density Gradient");
	ImGui::SetNextItemWidth(125.0f);
	ImGui::Text("Thickness of Troposphere: 10km");

	// Stratus Gradient
	{
		static bool stratus_dirty_flag = false;
		ImGui::BeginGroup();
		{
			static float stratus_range[4] = { 0.5f, 1.8f, 1.168f, 1.19f };
			ImGui::PushItemWidth(125.0f);
			ImGui::Text("Stratus Gradient");
			ImGui::PushID(10000);
			stratus_dirty_flag |= ImGui::InputFloat("Cloud Bottom(km)", stratus_range, 0.01f);
			stratus_dirty_flag |= ImGui::InputFloat("Cloud Top(km)", stratus_range + 1, 0.01f, 10.0f);
			stratus_dirty_flag |= ImGui::InputFloat("Solid Bottom(km)", stratus_range + 2, 0.01f);
			stratus_dirty_flag |= ImGui::InputFloat("Solid Top(km)", stratus_range + 3, 0.01f);
			ImGui::PopID();
			Utils::Clamp(stratus_range[0], 0.01f, stratus_range[1] - 0.02f);
			Utils::Clamp(stratus_range[1], 0.52f, 10.0f);
			Utils::Clamp(stratus_range[2], stratus_range[0] + 0.01f, stratus_range[3]);
			Utils::Clamp(stratus_range[3], stratus_range[2], stratus_range[1] - 0.01f);

			if (stratus_dirty_flag)
				GenerateGradient(m_stratusGradient, stratus_range[0] / 10.0f, stratus_range[1] / 10.0f, stratus_range[2] / 10.0f, stratus_range[3] / 10.0f);
		}
		ImGui::EndGroup(); ImGui::SameLine(300.0f);
		ImGui::ImageButton((ImTextureID)(m_stratusGradient->GetSRV().ptr), ImVec2(128.0f, 128.0f));
	}

	// Cumulus Gradient
	{
		static bool cumulus_dirty_flag = false;
		ImGui::BeginGroup();
		{
			static float cumulus_range[4] = { 0.3f, 6.8f, 3.0f, 3.5f };
			ImGui::PushItemWidth(125.0f);
			ImGui::Text("Cumulus Gradient");
			ImGui::PushID(10001);
			cumulus_dirty_flag |= ImGui::InputFloat("Cloud Bottom(km)", cumulus_range, 0.01f);
			cumulus_dirty_flag |= ImGui::InputFloat("Cloud Top(km)", cumulus_range + 1, 0.01f);
			cumulus_dirty_flag |= ImGui::InputFloat("Solid Bottom(km)", cumulus_range + 2, 0.01f);
			cumulus_dirty_flag |= ImGui::InputFloat("Solid Top(km)", cumulus_range + 3, 0.01f);
			ImGui::PopID();
			Utils::Clamp(cumulus_range[0], 0.01f, cumulus_range[1] - 0.02f);
			Utils::Clamp(cumulus_range[1], 0.52f, 10.0f);
			Utils::Clamp(cumulus_range[2], cumulus_range[0] + 0.01f, cumulus_range[3]);
			Utils::Clamp(cumulus_range[3], cumulus_range[2], cumulus_range[1] - 0.01f);

			if (cumulus_dirty_flag)
				GenerateGradient(m_cumulusGradient, cumulus_range[0] / 10.0f, cumulus_range[1] / 10.0f, cumulus_range[2] / 10.0f, cumulus_range[3] / 10.0f);
		}
		ImGui::EndGroup(); ImGui::SameLine(300.0f);
		ImGui::ImageButton((ImTextureID)(m_cumulusGradient->GetSRV().ptr), ImVec2(128.0f, 128.0f));
	}

	// Cumulonimbus Gradient
	{
		static bool cumulonimbus_dirty_flag = false;
		ImGui::BeginGroup();
		{
			static float cumulonimbus_range[4] = { 0.1f, 10.0f, 1.0f, 7.3f };
			ImGui::PushItemWidth(125.0f);
			ImGui::Text("Cumulonimbus Gradient");
			ImGui::PushID(10002);
			cumulonimbus_dirty_flag |= ImGui::InputFloat("Cloud Bottom(km)", cumulonimbus_range, 0.01f);
			cumulonimbus_dirty_flag |= ImGui::InputFloat("Cloud Top(km)", cumulonimbus_range + 1, 0.01f);
			cumulonimbus_dirty_flag |= ImGui::InputFloat("Solid Bottom(km)", cumulonimbus_range + 2, 0.01f);
			cumulonimbus_dirty_flag |= ImGui::InputFloat("Solid Top(km)", cumulonimbus_range + 3, 0.01f);
			ImGui::PopID();
			Utils::Clamp(cumulonimbus_range[0], 0.01f, cumulonimbus_range[1] - 0.02f);
			Utils::Clamp(cumulonimbus_range[1], 0.52f, 10.0f);
			Utils::Clamp(cumulonimbus_range[2], cumulonimbus_range[0] + 0.01f, cumulonimbus_range[3]);
			Utils::Clamp(cumulonimbus_range[3], cumulonimbus_range[2], cumulonimbus_range[1] - 0.01f);

			if (cumulonimbus_dirty_flag)
				GenerateGradient(m_cumulonimbusGradient, cumulonimbus_range[0] / 10.0f, cumulonimbus_range[1] / 10.0f, cumulonimbus_range[2] / 10.0f, cumulonimbus_range[3] / 10.0f);
		}
		ImGui::EndGroup(); ImGui::SameLine(300.0f);
		ImGui::ImageButton((ImTextureID)(m_cumulonimbusGradient->GetSRV().ptr), ImVec2(128.0f, 128.0f));
	}

	ImGui::End();


}

void CloudShapeManager::SetShowNoiseGeneratorWindow(bool val) 
{
	m_noiseGenerator->SetShowWindow(val); 
}
