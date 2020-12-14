#pragma once
#include "stdafx.h"
#include "App/App.h"

class ComputeDemo : public App
{
public:
	ComputeDemo();
	~ComputeDemo();

	bool Initialize() override;
	void Draw(const Timer& timer) override;
	void Update(const Timer& timer) override;
	void UpdateUI() override;

private:

private:

};