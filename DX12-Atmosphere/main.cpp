#include "stdafx.h"
#include "App/App.h"
//#include "Testes/FullScreenQuad.h"
//#include "Testes/CameraDemo.h"
#include "Testes/ComputeDemo.h"
#include <tchar.h>

int main(int argc, char** argv)
{
	ComputeDemo app;
	app.Initialize();
	return app.Run();

}