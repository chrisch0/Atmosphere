#pragma once
#include "stdafx.h"
#include "App/App.h"

class ComputeShader : public App
{
public:
	ComputeShader();
	~ComputeShader();

	bool Initialize() override;
	void Draw(const Timer& timer) override;
	void Update(const Timer& timer) override;
	void UpdateUI() override;

private:

private:

};