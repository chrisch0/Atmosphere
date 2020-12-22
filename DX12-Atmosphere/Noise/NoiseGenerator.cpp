#include "stdafx.h"
#include "NoiseGenerator.h"
#include "D3D12/GpuBuffer.h"
#include "D3D12/ColorBuffer.h"
#include "D3D12/CommandContext.h"
#include "imgui/imgui_internal.h"

#include "CompiledShaders/GenerateNoise_CS.h"
#include "CompiledShaders/GenerateVolumeNoise_CS.h"
#include "CompiledShaders/MapNoiseColor_CS.h"
#include "CompiledShaders/MapVolumeNoiseColor_CS.h"

NoiseState::NoiseState()
{
	seed = 1337;
	frequency = 0.01f;
	noise_type = kNoiseOpenSimplex2;
	rotation_type_3d = kRotationNone;
	fractal_type = kFractalNone;
	octaves = 5;
	lacunarity = 2.0f;
	gain = 0.5f;
	weighted_strength = 0.0f;
	ping_pong_strength = 2.0f;
	cellular_distance_func = kCellularDistanceEuclidean;
	cellular_return_type = kCellularReturnTypeDistance;
	cellular_jitter_mod = 1.0f;
	invert_visualize_warp = 0;
	domain_warp_type = kDomainWarpNone;
	domain_warp_rotation_type_3d = kRotationNone;
	domain_warp_amp = 1.0f;
	domain_warp_frequency = 0.005f;
	domain_warp_fractal_type = kFractalNone;
	domain_warp_octaves = 5;
	domain_warp_lacunarity = 2.0f;
	domain_warp_gain = 0.5f;
}

NoiseGenerator::NoiseGenerator()
{
	m_showNoiseWindow = true;
}

NoiseGenerator::~NoiseGenerator()
{
	
}

void NoiseGenerator::Initialize()
{
	m_genNoiseRS.Reset(2, 0);
	m_genNoiseRS[0].InitAsConstantBufferView(0);
	m_genNoiseRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 2);
	m_genNoiseRS.Finalize(L"GenNoiseRS");

	m_mapColorRS.Reset(3, 0);
	m_mapColorRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_mapColorRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_mapColorRS[2].InitAsConstants(1, 0);
	m_mapColorRS.Finalize(L"MapNoiseColorRS");

	m_genNoisePSO.SetRootSignature(m_genNoiseRS);
	m_genNoisePSO.SetComputeShader(g_pGenerateNoise_CS, sizeof(g_pGenerateNoise_CS));
	m_genNoisePSO.Finalize();

	m_genVolumeNoisePSO.SetRootSignature(m_genNoiseRS);
	m_genVolumeNoisePSO.SetComputeShader(g_pGenerateVolumeNoise_CS, sizeof(g_pGenerateVolumeNoise_CS));
	m_genVolumeNoisePSO.Finalize();

	m_mapNoiseColorPSO.SetRootSignature(m_mapColorRS);
	m_mapNoiseColorPSO.SetComputeShader(g_pMapNoiseColor_CS, sizeof(g_pMapNoiseColor_CS));
	m_mapNoiseColorPSO.Finalize();

	m_mapVolumeNoiseColorPSO.SetRootSignature(m_mapColorRS);
	m_mapVolumeNoiseColorPSO.SetComputeShader(g_pMapVolumeNoiseColor_CS, sizeof(g_pMapVolumeNoiseColor_CS));
	m_mapVolumeNoiseColorPSO.Finalize();

	uint32_t minmax[2] = { 0, 0 };
	m_minMax.Create(L"MinMaxBuffer", 2, sizeof(uint32_t), minmax);
	m_testTexture = std::make_shared<VolumeColorBuffer>();
	m_testTexture->Create(L"Test texture", 128, 128, 32, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void NoiseGenerator::Destroy()
{

}

void NoiseGenerator::UpdateUI()
{
	if (ImGui::IsKeyPressedMap(ImGuiKey_N))
		m_showNoiseWindow = !m_showNoiseWindow;

	if (!m_showNoiseWindow)
		return;

	ImGui::SetNextWindowSizeConstraints(ImVec2(710, 0), ImVec2(710, FLT_MAX));
	ImGui::Begin("Noise Generator", &m_showNoiseWindow);
	ImGui::BeginGroup();
	{
		ImGui::PushItemWidth(250);
		const char* noise_dim[] = { "Texture2D", "Texture3D" };
		static int cur_dim = 0;
		ImGui::Combo("Noise Dimension", &cur_dim, noise_dim, IM_ARRAYSIZE(noise_dim));

		static int width = 512, height = 512, depth = 64;
		ImVec2 size = ImGui::GetItemRectSize();
		ImGui::PushItemWidth(130);
		ImGui::InputInt("Width", &width); ImGui::SameLine();
		ImGui::InputInt("Height", &height);
		if (cur_dim)
		{
			ImGui::SameLine(); ImGui::InputInt("Depth", &depth);
		}
		ImGui::PopItemWidth();

		static char new_texture_name[64] = "Noise";
		ImGui::SetNextItemWidth(125);
		ImGui::InputText("New Texture Name", new_texture_name, 64); ImGui::SameLine();


		if (ImGui::Button("Add"))
		{
			if (cur_dim)
			{
				AddNoise3D(new_texture_name, width, height, depth);
			}
			else
			{
				AddNoise2D(new_texture_name, width, height);
			}
		}
		ImGui::PopItemWidth();
	}
	ImGui::EndGroup();

	for (int i = 0; i < m_noiseTextureNames.size(); ++i)
	{
		ImGui::PushID(i);
		NoiseConfig(i);
		ImGui::PopID();
	}

	ImGui::End();

}

void NoiseGenerator::NoiseConfig(size_t i)
{
	ImGui::Separator();
	
	bool dirty_flag = false;
	auto name = m_noiseTextureNames[i];
	bool is_volume_texture = m_isVolumeNoise[i];
	auto noise_state = m_noiseStates[i];
	auto& size = m_textureSize[i];
	auto iter = m_noiseTextures.find(m_noiseTextureNames[i]);
	assert(iter != m_noiseTextures.end());

	noise_state->invert_visualize_warp = 0;

	ImGui::BeginGroup();
	{
		ImGui::PushItemWidth(250);
		ImGui::Text(name.data());
		ImGui::Text("Size: %d x %d x %d", (int)size.GetX(), (int)size.GetY(), (int)size.GetZ());
		dirty_flag |= ImGui::Checkbox("3D", &is_volume_texture);
		if (m_isVolumeNoise[i] != is_volume_texture)
		{
			if (is_volume_texture)
			{
				auto new_texture = std::make_shared<VolumeColorBuffer>();
				std::wstring wname(name.begin(), name.end());
				new_texture->Create(wname, (uint32_t)size.GetX(), (uint32_t)size.GetY(), 64, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
				size.SetZ(64);
				iter->second = new_texture;
			}
			else
			{
				auto new_texture = std::make_shared<ColorBuffer>();
				std::wstring wname(name.begin(), name.end());
				new_texture->Create(wname, (uint32_t)size.GetX(), (uint32_t)size.GetY(), 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
				size.SetZ(1);
				iter->second = new_texture;
			}
		}
		m_isVolumeNoise[i] = is_volume_texture;

		static bool visualize_domain_warp = false;
		dirty_flag |= ImGui::Checkbox("Visualize Domain Warp", &visualize_domain_warp);
		noise_state->invert_visualize_warp |= (int)visualize_domain_warp;

		ImGui::Text("General");
		const char* noise_type[] = { "Open Simplex 2", "Open Simplex 2S", "Cellular", "Perlin", "Value Cubic", "Value" };
		dirty_flag |= ImGui::Combo("Noise Type", (int*)&(noise_state->noise_type), noise_type, IM_ARRAYSIZE(noise_type));
		if (is_volume_texture)
		{
			const char* rotation_type_3d[] = { "None", "Improve XY Planes", "Improve XZ Planes" };
			dirty_flag |= ImGui::Combo("Rotation Type", (int*)&(noise_state->rotation_type_3d), rotation_type_3d, IM_ARRAYSIZE(rotation_type_3d));
		}
		dirty_flag |= ImGui::DragInt("Seed", &(noise_state->seed));
		dirty_flag |= ImGui::DragFloat("Frequency", &(noise_state->frequency), 0.001f, 0.0f, FLT_MAX, "%.3f");

		ImGui::Text("Fractal");
		const char* fractal_type[] = { "None", "FBM", "Ridged", "Ping Pong" };
		dirty_flag |= ImGui::Combo("Fractal Type", (int*)&(noise_state->fractal_type), fractal_type, IM_ARRAYSIZE(fractal_type));
		if (noise_state->fractal_type != kFractalNone)
		{
			dirty_flag |= ImGui::DragInt("Octaves", &(noise_state->octaves), 0.05f, 1, INT_MAX);
			dirty_flag |= ImGui::DragFloat("Lacunarity", &(noise_state->lacunarity), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
			dirty_flag |= ImGui::DragFloat("Gain", &(noise_state->gain), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
			dirty_flag |= ImGui::DragFloat("Weighted Strength", &(noise_state->weighted_strength), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
			if (noise_state->fractal_type == kFractalPingPong)
			{
				dirty_flag |= ImGui::DragFloat("Ping Pong Strength", &(noise_state->ping_pong_strength), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
			}
		}

		if (noise_state->noise_type == kNoiseCellular)
		{
			ImGui::Text("Cellular");
			const char* distance_function[] = { "Euclidean", "Euclidean Sq", "Manhattan", "Hybrid" };
			dirty_flag |= ImGui::Combo("Distance Function", (int*)&(noise_state->cellular_distance_func), distance_function, IM_ARRAYSIZE(distance_function));
			const char* return_type[] = { "Cell Value", "Distance", "Distance 2", "Distance 2 Add", "Distance 2 Sub", "Distance 2 Mul", "Distance 2 Div" };
			dirty_flag |= ImGui::Combo("Return Type", (int*)&(noise_state->cellular_return_type), return_type, IM_ARRAYSIZE(return_type));
			dirty_flag |= ImGui::DragFloat("Jitter", &(noise_state->cellular_jitter_mod), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
		}

		// Domain warp setting
		{
			ImGui::PushID((int)(i * 1000 + i));
			ImGui::Text("Domain Warp");
			const char* type[] = { "None", "Open Simplex 2", "Open Simplex 2 Reduced", "Basic Grid" };
			dirty_flag |= ImGui::Combo("Type", (int*)&(noise_state->domain_warp_type), type, IM_ARRAYSIZE(type));
			if (is_volume_texture)
			{
				const char* rotation_type_3d[] = { "None", "Improve XY Planes", "Improve XZ Planes" };
				dirty_flag |= ImGui::Combo("Rotation Type", (int*)&(noise_state->rotation_type_3d), rotation_type_3d, IM_ARRAYSIZE(rotation_type_3d));
			}
			dirty_flag |= ImGui::DragFloat("Amplitude", &(noise_state->domain_warp_amp), 1.0f, -FLT_MAX, FLT_MAX, "%.2f");
			dirty_flag |= ImGui::DragFloat("Frequency", &(noise_state->domain_warp_frequency), 0.001f, -FLT_MAX, FLT_MAX, "%.3f");

			ImGui::Text("Domain Warp Fractal");
			const char* fractal_type[] = { "None", "Domain Warp Progressive", "Domain Warp Independent" };
			dirty_flag |= ImGui::Combo("Fractal Type", (int*)&(noise_state->domain_warp_fractal_type), fractal_type, IM_ARRAYSIZE(fractal_type));
			if (noise_state->domain_warp_fractal_type != kFractalNone)
			{
				dirty_flag |= ImGui::DragInt("Octaves", &(noise_state->domain_warp_octaves), 0.05f, 1, INT_MAX);
				dirty_flag |= ImGui::DragFloat("Lacunarity", &(noise_state->domain_warp_lacunarity), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
				dirty_flag |= ImGui::DragFloat("Gain", &(noise_state->domain_warp_gain), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
			}
			ImGui::PopID();
		}
		static bool invert = false;
		dirty_flag |= ImGui::Checkbox("Invert Color", &invert);
		noise_state->invert_visualize_warp |= ((int)invert << 16);
		ImGui::PopItemWidth();
	}
	ImGui::EndGroup();
	ImGui::SameLine(420);

	if (dirty_flag)
	{
		if (is_volume_texture)
			Generate(iter->second, (uint32_t)size.GetX(), (uint32_t)size.GetY(), (uint32_t)size.GetZ(), noise_state.get());
		else
			Generate(iter->second, (uint32_t)size.GetX(), (uint32_t)size.GetY(), noise_state.get());
	}

	if (is_volume_texture)
	{
		static bool image_view = false;
		static bool window_open = false;
		auto volume_tex = std::dynamic_pointer_cast<VolumeColorBuffer>(iter->second);
		if (ImGui::VolumeImageButton((ImTextureID)(volume_tex->GetSRV().ptr), ImVec2(256.0f, 256.0f), volume_tex->GetDepth()))
		{
			image_view = !image_view;
			window_open = true;
		}
		if (image_view)
		{
			ImGui::Begin(name.c_str());
			auto cursor_pos = ImGui::GetCursorPos();
			if (window_open)
			{
				auto tiled_size = D3DUtils::GetTiledVolumeTextureSize(volume_tex->GetWidth(), volume_tex->GetHeight(), volume_tex->GetDepth());
				tiled_size.x = std::min(tiled_size.x, 1024.0f);
				tiled_size.y = std::min(tiled_size.y, 1024.0f);
				ImGui::SetWindowSize(ImVec2(tiled_size.x + 2.0f * cursor_pos.x, tiled_size.y + cursor_pos.y + 10.0f));
			}
			auto window_size = ImGui::GetWindowSize();
			ImGui::TiledVolumeImage((ImTextureID)(volume_tex->GetSRV().ptr), ImVec2(window_size.x - 2.0f * cursor_pos.x, window_size.y - cursor_pos.y - 10.0f), volume_tex->GetDepth());
			ImGui::End();
			window_open = false;
		}
	}
	else
	{
		static bool image_view = false;
		static bool window_open = false;
		auto tex = std::dynamic_pointer_cast<ColorBuffer>(iter->second);
		if (ImGui::ImageButton((ImTextureID)(tex->GetSRV().ptr), ImVec2(256.0f, 256.0f)))
		{
			image_view = !image_view;
			window_open = true;
		}
		if (image_view)
		{
			ImGui::Begin(name.c_str());
			auto cursor_pos = ImGui::GetCursorPos();
			if (window_open)
				ImGui::SetWindowSize(ImVec2(tex->GetWidth() + 2.0f * cursor_pos.x, tex->GetHeight() + cursor_pos.y + 10.0f));
			auto window_size = ImGui::GetWindowSize();
			ImGui::Image((ImTextureID)(tex->GetSRV().ptr), ImVec2(window_size.x - 2.0f * cursor_pos.x, window_size.y - cursor_pos.y - 10.0f));
			ImGui::End();
			window_open = false;
		}
	}
}

void NoiseGenerator::AddNoise3D(const std::string& name, uint32_t width, uint32_t height, uint32_t depth)
{
	auto iter = m_noiseTextures.find(name);
	std::shared_ptr<VolumeColorBuffer> tex = std::make_shared<VolumeColorBuffer>();
	std::string n = name;
	while (iter != m_noiseTextures.end())
	{
		n += "_";
		iter = m_noiseTextures.find(n);
	}
	std::wstring wn(n.begin(), n.end());
	tex->Create(wn, width, height, depth, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	m_noiseTextures.insert({ n, tex });
	m_noiseTextureNames.push_back(n);
	m_noiseStates.emplace_back(std::make_shared<NoiseState>());
	m_isVolumeNoise.push_back(true);
	m_textureSize.emplace_back((float)width, (float)height, (float)depth);
	Generate(tex, width, height, depth, m_noiseStates.back().get());
}

void NoiseGenerator::AddNoise2D(const std::string& name, uint32_t width, uint32_t height)
{
	auto iter = m_noiseTextures.find(name);
	std::shared_ptr<ColorBuffer> tex = std::make_shared<ColorBuffer>();
	std::string n = name;
	while (iter != m_noiseTextures.end())
	{
		n += "_";
		iter = m_noiseTextures.find(n);
	}
	std::wstring wn(n.begin(), n.end());
	tex->Create(wn, width, height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	m_noiseTextures.insert({ n, tex });
	m_noiseTextureNames.push_back(n);
	m_noiseStates.emplace_back(std::make_shared<NoiseState>());
	m_isVolumeNoise.push_back(false);
	m_textureSize.emplace_back((float)width, (float)height, 1.0f);
	Generate(tex, width, height, m_noiseStates.back().get());
}

void NoiseGenerator::Generate(std::shared_ptr<PixelBuffer> texPtr, uint32_t width, uint32_t height, uint32_t depth, NoiseState* state)
{
	auto tex = std::dynamic_pointer_cast<VolumeColorBuffer>(texPtr);

	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_genNoiseRS);
	context.SetPipelineState(m_genVolumeNoisePSO);
	context.TransitionResource(m_minMax, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicConstantBufferView(0, sizeof(NoiseState), state);
	context.SetDynamicDescriptor(1, 0, tex->GetUAV());
	context.SetDynamicDescriptor(1, 1, m_minMax.GetUAV());
	context.Dispatch3D(tex->GetWidth(), tex->GetHeight(), tex->GetDepth(), 8, 8, 1);
	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
	context.TransitionResource(m_minMax, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

	context.SetRootSignature(m_mapColorRS);
	context.SetPipelineState(m_mapVolumeNoiseColorPSO);
	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(0, 0, tex->GetUAV());
	context.SetDynamicDescriptor(1, 0, m_minMax.GetSRV());
	context.SetConstant(2, state->invert_visualize_warp, 0);
	context.Dispatch3D(tex->GetWidth(), tex->GetHeight(), tex->GetDepth(), 8, 8, 1);
	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.Finish(true);
}

void NoiseGenerator::Generate(std::shared_ptr<PixelBuffer> texPtr, uint32_t width, uint32_t height, NoiseState* state)
{
	auto tex = std::dynamic_pointer_cast<ColorBuffer>(texPtr);

	ComputeContext& context = ComputeContext::Begin();
	context.SetRootSignature(m_genNoiseRS);
	context.SetPipelineState(m_genNoisePSO);
	context.TransitionResource(m_minMax, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicConstantBufferView(0, sizeof(NoiseState), state);
	context.SetDynamicDescriptor(1, 0, tex->GetUAV());
	context.SetDynamicDescriptor(1, 1, m_minMax.GetUAV());
	context.Dispatch2D(tex->GetWidth(), tex->GetHeight());
	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
	context.TransitionResource(m_minMax, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	
	context.SetRootSignature(m_mapColorRS);
	context.SetPipelineState(m_mapNoiseColorPSO);
	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	context.SetDynamicDescriptor(0, 0, tex->GetUAV());
	context.SetDynamicDescriptor(1, 0, m_minMax.GetSRV());
	context.SetConstant(2, state->invert_visualize_warp, 0);
	context.Dispatch2D(tex->GetWidth(), tex->GetHeight());
	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.Finish(true);
}