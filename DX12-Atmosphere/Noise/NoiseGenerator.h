#pragma once
#include "stdafx.h"

class RootSignature;
class GraphicsPSO;
class ComputePSO;
class PixelBuffer;
class GraphicsContext;

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
	NoiseDomainWarpType domain_warp_type;
	float domain_warp_amp;
	// is reverse color
	int invert;
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

private:
	void NoiseConfig(GraphicsContext& context, size_t index);

	void AddNoise2D(const std::string& name, uint32_t width, uint32_t height);
	void AddNoise3D(const std::string& name, uint32_t width, uint32_t height, uint32_t depth);
	void Generate(std::shared_ptr<PixelBuffer> texPtr, uint32_t width, uint32_t height, NoiseState* state);
	void Generate(std::shared_ptr<PixelBuffer> texPtr, uint32_t width, uint32_t height, uint32_t depth, NoiseState* state);

	void TileTexture3D();
private:
	std::unordered_map<std::string, std::shared_ptr<PixelBuffer>> m_noiseTextures;
	std::vector<std::string> m_noiseTextureNames;
	std::vector<std::shared_ptr<NoiseState>> m_noiseStates;
	std::vector<bool> m_isVolumeNoise;
	std::vector<Vector3> m_textureSize;

	RootSignature m_genNoiseRS;
	ComputePSO m_genNoisePSO;
	ComputePSO m_genVolumeNoisePSO;

	bool m_showNoiseWindow;
};