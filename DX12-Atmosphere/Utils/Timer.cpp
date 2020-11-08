#include "stdafx.h"
#include "Timer.h"

Timer::Timer()
	: m_secondsPerCount(0.0), m_deltaTime(-1.0),
	  m_baseTime(0), m_pausedTime(0), m_stopTime(0), m_prevTime(0), m_currTime(0),
	  m_stopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	m_secondsPerCount = 1.0 / (double)countsPerSec;
}

Timer::~Timer()
{

}

float Timer::TotalTime() const
{
	if (m_stopped)
	{
		return (float)((m_stopTime - m_baseTime - m_pausedTime) * m_secondsPerCount);
	}
	else
	{
		return (float)((m_currTime - m_baseTime - m_pausedTime) * m_secondsPerCount);
	}
}

float Timer::DeltaTime() const
{
	return (float)m_deltaTime;
}

void Timer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	m_baseTime = currTime;
	m_prevTime = currTime;
	m_stopTime = 0;
	m_stopped = false;
}

void Timer::Start()
{
	if (m_stopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		m_pausedTime += currTime - m_stopTime;
		m_stopTime = 0;
		m_stopTime = false;
	}
}

void Timer::Stop()
{
	if (!m_stopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		m_stopTime = currTime;
		m_stopped = true;
	}
}

void Timer::Tick()
{
	if (m_stopped)
	{
		m_deltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	m_currTime = currTime;
	m_deltaTime = (m_currTime - m_prevTime) * m_secondsPerCount;
	m_prevTime = m_currTime;
	m_deltaTime = m_deltaTime < 0.0 ? 0.0 : m_deltaTime;
}