#pragma once
#include "App/App.h"

class ModelLoading : public App
{
public:
	bool Initialize() override;
	void Update(const Timer& t) override;
	void Draw(const Timer& t) override;
private:

};