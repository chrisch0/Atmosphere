#include "stdafx.h"
#include "App/App.h"
//#include "Testes/FullScreenQuad.h"
#include "Testes/CameraDemo.h"
#include <tchar.h>

int main(int argc, char** argv)
{
	CameraDemo app;
	app.Initialize();
	return app.Run();

}