#pragma once
#include "stdafx.h"

class ColorBuffer;
class VolumeColorBuffer;

class AtmosphereModel
{
public:
	AtmosphereModel();
private:
	uint32_t m_numPrecomputedWavelengths;
	bool m_halfPrecision;
	
	std::shared_ptr<ColorBuffer> m_transmittance;
	std::shared_ptr<VolumeColorBuffer> m_scattering;
	std::shared_ptr<VolumeColorBuffer> m_optionalSingleMieScattering;
	std::shared_ptr<ColorBuffer> m_irradiance;

	std::shared_ptr<ColorBuffer> m_interIrradiance;
	std::shared_ptr<VolumeColorBuffer> m_rayleighScattering;
	std::shared_ptr<VolumeColorBuffer> m_mieScattering;
	std::shared_ptr<VolumeColorBuffer> m_interScattering;
};