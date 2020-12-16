#include "stdafx.h"
#include "NoiseGenerator.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"
#include "D3D12/GpuBuffer.h"
#include "D3D12/ColorBuffer.h"
#include "D3D12/CommandContext.h"

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
	domain_warp_amp = 1.0f;
	domain_warp_type = kDomainWarpOpenSimplex2;
}

NoiseGenerator::NoiseGenerator()
{
	m_showNoiseWindow = true;
	m_isFirst = true;
}

NoiseGenerator::~NoiseGenerator()
{

}

void NoiseGenerator::Initialize()
{

}

void NoiseGenerator::UpdateUI()
{
	if (!m_showNoiseWindow)
		return;

	ImGui::Begin("Noise Generator", &m_showNoiseWindow);

	const char* noise_dim[] = { "Texture2D", "Texture3D" };
	static int cur_dim = 0;
	ImGui::Combo("Noise Dimension", &cur_dim, noise_dim, IM_ARRAYSIZE(noise_dim)); ImGui::SameLine();
	static int width = 512, height = 512, depth = 64;
	ImGui::DragInt("Width", &width); ImGui::SameLine();
	ImGui::DragInt("Height", &height); ImGui::SameLine();
	if (cur_dim)
	{
		ImGui::DragInt("Depth", &depth); ImGui::SameLine();
	}
	static char new_texture_name[64] = "Noise";
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

	for (int i = 0; i < m_noiseTextureNames.size(); ++i)
	{
		//NoiseConfig()
	}

	ImGui::End();

	m_isFirst = false;
}

void NoiseGenerator::NoiseConfig(GraphicsContext& context, size_t i)
{
	ImGui::Separator();
	
	bool dirty_flag = m_isFirst;
	auto name = m_noiseTextureNames[i];
	bool tex_3d = m_isNoise3D[i];
	auto noise_state = m_noiseStates[i];
	bool domain_warp = m_isDomainWarp[i];
	ImGui::BeginGroup();
	{
		ImGui::Text(name.data());
		dirty_flag |= ImGui::Checkbox("3D", &tex_3d);
		m_isNoise3D[i] = tex_3d;
		dirty_flag |= ImGui::Checkbox("Visualize Domain Warp", &domain_warp);
		ImGui::Text("General");
		if (!domain_warp)
		{
			const char* noise_type[] = { "Open Simplex 2", "Open Simplex 2S", "Cellular", "Perlin", "Value Cubic", "Value" };
			dirty_flag |= ImGui::Combo("Noise Type", (int*)&(noise_state->noise_type), noise_type, IM_ARRAYSIZE(noise_type));
			if (tex_3d)
			{
				const char* rotation_type_3d[] = {"None", "Improve XY Planes", "Improve XZ Planes"};
				dirty_flag |= ImGui::Combo("Rotation Type", (int*)&(noise_state->rotation_type_3d), rotation_type_3d, IM_ARRAYSIZE(rotation_type_3d));
			}
			dirty_flag |= ImGui::DragInt("Seed", &(noise_state->seed));
			dirty_flag |= ImGui::DragFloat("Frequency", &(noise_state->frequency), 0.001f, 0.0f, FLT_MAX, "%.3f");
		}
		else
		{

		}
	}
	ImGui::EndGroup();
	ImGui::SameLine();
}

void NoiseGenerator::AddNoise3D(const std::string& name, uint32_t width, uint32_t height, uint32_t depth)
{
	auto iter = m_noiseTextures.find(name);
	std::shared_ptr<ColorBuffer3D> tex = std::make_shared<ColorBuffer3D>();
	std::string n = name;
	if (iter != m_noiseTextures.end())
	{
		n += "_";
	}
	std::wstring wn(n.begin(), n.end());
	tex->Create(wn, width, height, depth, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	m_noiseTextures.insert({ n, tex });
	m_noiseTextureNames.push_back(n);
	m_noiseStates.emplace_back(std::make_shared<NoiseState>());
	m_isNoise3D.push_back(true);
	m_isDomainWarp.push_back(false);
}

void NoiseGenerator::AddNoise2D(const std::string& name, uint32_t width, uint32_t height)
{
	auto iter = m_noiseTextures.find(name);
	std::shared_ptr<ColorBuffer> tex = std::make_shared<ColorBuffer>();
	std::string n = name;
	if (iter != m_noiseTextures.end())
	{
		n += "_";
	}
	std::wstring wn(n.begin(), n.end());
	tex->Create(wn, width, height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	m_noiseTextures.insert({ n, tex });
	m_noiseTextureNames.push_back(n);
	m_noiseStates.emplace_back(std::make_shared<NoiseState>());
	m_isNoise3D.push_back(false);
	m_isDomainWarp.push_back(false);
}