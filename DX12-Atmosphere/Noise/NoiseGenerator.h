#pragma once
#include "stdafx.h"
#include "D3D12/GpuBuffer.h"
#include "D3D12/RootSignature.h"
#include "D3D12/PipelineState.h"

class PixelBuffer;
class ColorBuffer;
class VolumeColorBuffer;
class ComputeContext;

enum NoiseType
{
	kNoiseOpenSimplex2 = 0,
	kNoiseOpenSimplex2S = 1,
	kNoiseCellular = 2,
	kNoisePerlin = 3,
	kNoiseValueCubic = 4,
	kNoiseValue = 5
};

enum NoiseRotationType3D
{
	kRotationNone = 0,
	kRotationImproveXYPlanes = 1,
	kRotationImproveXZPlanes = 2
};

enum NoiseFractalType
{
	kFractalNone = 0,
	kFractalFBM = 1,
	kFractalRidged = 2,
	kFractalPingPong = 3,
	kFractalDomainWarpProgressive = 4,
	kFractalDomainWarpIndependent = 5
};

enum NoiseCellularDistanceFunc
{
	kCellularDistanceEuclidean = 0,
	kCellularDistanceEuclideanSq = 1,
	kCellularDistanceManhattan = 2,
	kCellularDistanceHybrid = 3
};

enum NoiseCellularReturnType
{
	kCellularReturnTypeCellvalue = 0,
	kCellularReturnTypeDistance = 1,
	kCellularReturnTypeDistance2 = 2,
	kCellularReturnTypeDistance2Add = 3,
	kCellularReturnTypeDistance2Sub = 4,
	kCellularReturnTypeDistance2Mul = 5,
	kCellularReturnTypeDistance2Div = 6
};

enum NoiseDomainWarpType
{
	kDomainWarpNone = 0,
	kDomainWarpOpenSimplex2 = 1,
	kDomainWarpOpenSimplex2Reduced = 2,
	kDomainWarpBasicGrid = 3
};

struct NoiseState
{
	NoiseState();
	int seed;
	float frequency;
	NoiseType noise_type;
	NoiseRotationType3D rotation_type_3d;
	// fractal parameters
	NoiseFractalType fractal_type;
	int octaves;
	float lacunarity;
	float gain;
	float weighted_strength;
	float ping_pong_strength;
	// distance function
	NoiseCellularDistanceFunc cellular_distance_func;
	NoiseCellularReturnType cellular_return_type;
	float cellular_jitter_mod;
	// high 16 bit: is invert color, low 16 bit: is visualize domain warp
	int invert_visualize_warp;
	// domain warp parameters
	NoiseDomainWarpType domain_warp_type;
	NoiseRotationType3D domain_warp_rotation_type_3d;
	float domain_warp_amp;
	float domain_warp_frequency;
	NoiseFractalType domain_warp_fractal_type;
	int domain_warp_octaves;
	float domain_warp_lacunarity;
	float domain_warp_gain;
};

class NoiseGenerator
{
public:
	NoiseGenerator();
	~NoiseGenerator();

	NoiseGenerator(const NoiseGenerator&) = delete;
	NoiseGenerator& operator=(const NoiseGenerator&) = delete;

	void Initialize();
	void UpdateUI();
	void Destroy();

	size_t NumTextures() const { return m_noiseTextures.size(); }
	std::string GetTextureName(size_t idx) { return m_noiseTextureNames[idx]; }
	std::shared_ptr<PixelBuffer> GetTexture(size_t idx);
	std::shared_ptr<PixelBuffer> GetTexture(const std::string& name);

	void AddNoise(const std::string& name, uint32_t width, uint32_t height);
	void AddVolumeNoise(const std::string& name, uint32_t width, uint32_t height, uint32_t depth);
	void AddNoise(const std::string& name, std::shared_ptr<ColorBuffer> texPtr, NoiseState* state);
	void AddVolumeNoise(const std::string& name, std::shared_ptr<VolumeColorBuffer> texPtr, NoiseState* state);

	void GenerateNoise(std::shared_ptr<PixelBuffer> texPtr, NoiseState* state);
	void GenerateVolumeNoise(std::shared_ptr<PixelBuffer> texPtr, NoiseState* state);

	void GenerateRawNoiseData(ComputeContext& context, std::shared_ptr<ColorBuffer> texPtr, NoiseState* state);
	void GenerateRawNoiseData(ComputeContext& context, std::shared_ptr<VolumeColorBuffer> texPtr, NoiseState* state);

	void MapNoiseColor(ComputeContext& context, std::shared_ptr<ColorBuffer> texPtr, NoiseState* state);
	void MapNoiseColor(ComputeContext& context, std::shared_ptr<VolumeColorBuffer> texPtr, NoiseState* state);

private:
	void NoiseConfig(size_t index);

private:
	std::unordered_map<std::string, std::shared_ptr<PixelBuffer>> m_noiseTextures;
	std::vector<std::string> m_noiseTextureNames;
	std::vector<std::shared_ptr<NoiseState>> m_noiseStates;
	std::vector<bool> m_isVolumeNoise;
	std::vector<Vector3> m_textureSize;

	RootSignature m_genNoiseRS;
	RootSignature m_mapColorRS;
	ComputePSO m_genNoisePSO;
	ComputePSO m_genVolumeNoisePSO;
	ComputePSO m_mapNoiseColorPSO;
	ComputePSO m_mapVolumeNoiseColorPSO;
	StructuredBuffer m_minMax;

	bool m_showNoiseWindow;
};