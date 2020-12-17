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
			//AddNoise3D(new_texture_name, width, height, depth);
		}
		else
		{
			//AddNoise2D(new_texture_name, width, height);
		}
	}

	for (int i = 0; i < m_noiseTextureNames.size(); ++i)
	{
		//NoiseConfig()
	}

	ImGui::End();

}

void NoiseGenerator::NoiseConfig(GraphicsContext& context, size_t i)
{
	ImGui::Separator();
	
	bool dirty_flag = false;
	auto name = m_noiseTextureNames[i];
	bool tex_3d = m_isNoise3D[i];
	auto noise_state = m_noiseStates[i];
	bool domain_warp = m_isDomainWarp[i];
	auto& size = m_textureSize[i];
	auto iter = m_noiseTextures.find(m_noiseTextureNames[i]);
	assert(iter != m_noiseTextures.end());

	ImGui::BeginGroup();
	{
		ImGui::Text(name.data());
		ImGui::Text("Size: %d x %d x %d", (int)size.GetX(), (int)size.GetY(), (int)size.GetZ());
		dirty_flag |= ImGui::Checkbox("3D", &tex_3d);
		if (m_isNoise3D[i] != tex_3d)
		{

			if (tex_3d)
			{
				auto new_texture = std::make_shared<ColorBuffer3D>();
				std::wstring wname(name.begin(), name.end());
				new_texture->Create(wname, size.GetX(), size.GetY(), 64, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
				size.SetZ(64);
				iter->second = new_texture;
				//Generate(new_texture, size.GetX(), size.GetY(), size.GetZ(), noise_state.get(), domain_warp);
			}
			else
			{
				auto new_texture = std::make_shared<ColorBuffer>();
				std::wstring wname(name.begin(), name.end());
				new_texture->Create(wname, size.GetX(), size.GetY(), 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
				size.SetZ(1);
				iter->second = new_texture;
				//Generate(new_texture, size.GetX(), size.GetY(), noise_state.get(), domain_warp);
			}
		}
		m_isNoise3D[i] = tex_3d;
		dirty_flag |= ImGui::Checkbox("Visualize Domain Warp", &domain_warp);
		if (!domain_warp)
		{
			ImGui::Text("General");
			const char* noise_type[] = { "Open Simplex 2", "Open Simplex 2S", "Cellular", "Perlin", "Value Cubic", "Value" };
			dirty_flag |= ImGui::Combo("Noise Type", (int*)&(noise_state->noise_type), noise_type, IM_ARRAYSIZE(noise_type));
			if (tex_3d)
			{
				const char* rotation_type_3d[] = {"None", "Improve XY Planes", "Improve XZ Planes"};
				dirty_flag |= ImGui::Combo("Rotation Type", (int*)&(noise_state->rotation_type_3d), rotation_type_3d, IM_ARRAYSIZE(rotation_type_3d));
			}
			dirty_flag |= ImGui::DragInt("Seed", &(noise_state->seed));
			dirty_flag |= ImGui::DragFloat("Frequency", &(noise_state->frequency), 0.001f, 0.0f, FLT_MAX, "%.3f");
			
			ImGui::Text("Fractal");
			const char* fractal_type[] = { "None", "FBM", "Ridged", "Ping Pong" };
			dirty_flag |= ImGui::Combo("Fractal Type", (int*)&(noise_state->fractal_type), fractal_type, IM_ARRAYSIZE(fractal_type));
			if (noise_state->fractal_type != kFractalNone)
			{
				dirty_flag |= ImGui::DragInt("Octaves", &(noise_state->octaves), 1.0f, 0, INT_MAX);
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
		}
		else
		{
			ImGui::Text("Domain Warp");
			const char* type[] = { "None", "Open Simplex 2", "Open Simplex 2 Reduced", "Basic Grid" };
			dirty_flag |= ImGui::Combo("Type", (int*)&(noise_state->domain_warp_type), type, IM_ARRAYSIZE(type));
			if (tex_3d)
			{
				const char* rotation_type_3d[] = { "None", "Improve XY Planes", "Improve XZ Planes" };
				dirty_flag |= ImGui::Combo("Rotation Type", (int*)&(noise_state->rotation_type_3d), rotation_type_3d, IM_ARRAYSIZE(rotation_type_3d));
			}
			dirty_flag |= ImGui::DragFloat("Amplitude", &(noise_state->domain_warp_amp), 1.0f, -FLT_MAX, FLT_MAX, "%.2f");
			dirty_flag |= ImGui::DragFloat("Frequency", &(noise_state->frequency), 0.001f, -FLT_MAX, FLT_MAX, "%.3f");

			ImGui::Text("Domain Warp Fractal");
			const char* fractal_type[] = { "None", "Domain Warp Progressive", "Domain Warp Independent" };
			dirty_flag |= ImGui::Combo("Fractal Type", (int*)&(noise_state->fractal_type), fractal_type, IM_ARRAYSIZE(fractal_type));
			if (noise_state->fractal_type != kFractalNone)
			{
				dirty_flag |= ImGui::DragInt("Octaves", &(noise_state->octaves), 1.0f, 0, INT_MAX);
				dirty_flag |= ImGui::DragFloat("Lacunarity", &(noise_state->lacunarity), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
				dirty_flag |= ImGui::DragFloat("Gain", &(noise_state->gain), 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
			}
		}
	}
	ImGui::EndGroup();
	ImGui::SameLine();

	//if (tex_3d)
	//{
	//	auto tex = std::dynamic_pointer_cast<ColorBuffer3D>(iter->second);
	//	auto gpu_handle = context.SetDynamicDescriptorDirect(tex->GetSRV());
	//	ImGui::Image()
	//}
}

//void NoiseGenerator::AddNoise3D(const std::string& name, uint32_t width, uint32_t height, uint32_t depth)
//{
//	auto iter = m_noiseTextures.find(name);
//	std::shared_ptr<ColorBuffer3D> tex = std::make_shared<ColorBuffer3D>();
//	std::string n = name;
//	if (iter != m_noiseTextures.end())
//	{
//		n += "_";
//	}
//	std::wstring wn(n.begin(), n.end());
//	tex->Create(wn, width, height, depth, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
//	m_noiseTextures.insert({ n, tex });
//	m_noiseTextureNames.push_back(n);
//	m_noiseStates.emplace_back(std::make_shared<NoiseState>());
//	m_isNoise3D.push_back(true);
//	m_isDomainWarp.push_back(false);
//	m_textureSize.emplace_back((float)width, (float)height, (float)depth);
//	Generate(tex, width, height, depth, m_noiseStates.back().get(), false);
//}
//
//void NoiseGenerator::AddNoise2D(const std::string& name, uint32_t width, uint32_t height)
//{
//	auto iter = m_noiseTextures.find(name);
//	std::shared_ptr<ColorBuffer> tex = std::make_shared<ColorBuffer>();
//	std::string n = name;
//	if (iter != m_noiseTextures.end())
//	{
//		n += "_";
//	}
//	std::wstring wn(n.begin(), n.end());
//	tex->Create(wn, width, height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
//	m_noiseTextures.insert({ n, tex });
//	m_noiseTextureNames.push_back(n);
//	m_noiseStates.emplace_back(std::make_shared<NoiseState>());
//	m_isNoise3D.push_back(false);
//	m_isDomainWarp.push_back(false);
//	m_textureSize.emplace_back((float)width, (float)height, 1.0f);
//	Generate(tex, width, height, m_noiseStates.back().get(), false);
//}
//
//void NoiseGenerator::Generate(std::shared_ptr<PixelBuffer> texPtr, uint32_t width, uint32_t height, uint32_t depth, NoiseState* state, bool isDomainWarp)
//{
//	auto tex = std::dynamic_pointer_cast<ColorBuffer3D>(texPtr);
//
//	ComputeContext& context = ComputeContext::Begin();
//	context.SetRootSignature(m_noiseRS);
//	if (isDomainWarp)
//		context.SetPipelineState(m_domainWarp3DPSO);
//	else
//		context.SetPipelineState(m_noise3DPSO);
//	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
//	context.SetDynamicConstantBufferView(0, sizeof(NoiseState), state);
//	context.SetDynamicDescriptor(1, 0, tex->GetUAV());
//	context.Dispatch3D(tex->GetWidth(), tex->GetHeight(), tex->GetDepth(), 8, 8, 1);
//	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
//}
//
//void NoiseGenerator::Generate(std::shared_ptr<PixelBuffer> texPtr, uint32_t width, uint32_t height, NoiseState* state, bool isDomainWarp)
//{
//	auto tex = std::dynamic_pointer_cast<ColorBuffer>(texPtr);
//
//	ComputeContext& context = ComputeContext::Begin();
//	context.SetRootSignature(m_noiseRS);
//	if (isDomainWarp)
//		context.SetPipelineState(m_domainWarp2DPSO);
//	else
//		context.SetPipelineState(m_noise2DPSO);
//	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
//	context.SetDynamicConstantBufferView(0, sizeof(NoiseState), state);
//	context.SetDynamicDescriptor(1, 0, tex->GetUAV());
//	context.Dispatch2D(tex->GetWidth(), tex->GetHeight());
//	context.TransitionResource(*tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
//}