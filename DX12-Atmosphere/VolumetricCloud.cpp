#include "stdafx.h"
#include "VolumetricCloud.h"
#include "D3D12/GraphicsGlobal.h"
#include "D3D12/CommandContext.h"

VolumetricCloud::VolumetricCloud()
{

}

VolumetricCloud::~VolumetricCloud()
{

}

uint32_t UintMin(uint32_t a, float b)
{
	uint32_t t = *reinterpret_cast<uint32_t*>(&b);
	uint32_t mask = t >> 31;
	mask = (1 + ~mask) | 0x80000000;

	t ^= mask;
	return std::min<uint32_t>(a, t);
}

uint32_t UintMax(uint32_t a, float b)
{
	uint32_t t = *reinterpret_cast<uint32_t*>(&b);
	uint32_t mask = t >> 31;
	mask = (1 + ~mask) | 0x80000000;
	//mask += 1;
	t ^= mask;
	//uint32_t mask = (1 + ~(t >> 31) | 0x80000000);
	//t ^= (1 + ~(t >> 31) | 0x80000000);
	return std::max<uint32_t>(a, t);
}

float Tofloat(uint32_t a)
{
	uint32_t t = (a >> 31) - 1;
	t |= 0x80000000;
	a ^= t;
	return *reinterpret_cast<float*>(&a);
}

bool VolumetricCloud::Initialize()
{
	if (!App::Initialize())
		return false;

	Global::InitializeGlobalStates();

	m_showAnotherDemoWindow = false;
	m_showDemoWindow = false;
	m_showHelloWindow = false;

	m_cloudShapeManager.Initialize();
	//m_cloudShapeManager.CreateBasicCloudShape();

	uint32_t min = 0xffffffff;
	uint32_t max = 0;

	float f = Tofloat(max);
	f = Tofloat(min);

	max = UintMax(max, -3.30282347E+38);
	f = Tofloat(max);
	max = UintMax(max, -0.98);
	f = Tofloat(max);
	max = UintMax(max, 0.89);
	f = Tofloat(max);

	min = UintMin(min, 3.40282347E+38);
	f = Tofloat(min);
	min = UintMin(min, 3.40282E+38);
	f = Tofloat(min);
	min = UintMin(min, 0.3);
	f = Tofloat(min);
	min = UintMin(min, 0.0);
	f = Tofloat(min);
	min = UintMin(min, -0.3);
	f = Tofloat(min);
	min = UintMin(min, -12134.0f);
	f = Tofloat(min);
	min = UintMin(min, 3.40282347E+38);
	f = Tofloat(min);
	min = UintMin(min, -3.40282347E+38);
	f = Tofloat(min);
	return true;
}

void VolumetricCloud::Update(const Timer& timer)
{
	//m_cloudShapeManager.GenerateBasicCloudShape();
}

void VolumetricCloud::Draw(const Timer& timer)
{
	GraphicsContext& graphicsContext = GraphicsContext::Begin();
	graphicsContext.TransitionResource(m_displayBuffer[m_currBackBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_displayBuffer[m_currBackBuffer].SetClearColor(m_clearColor);
	graphicsContext.ClearColor(m_displayBuffer[m_currBackBuffer]);
	graphicsContext.Finish();

}

void VolumetricCloud::UpdateUI()
{
	m_cloudShapeManager.UpdateUI();
}