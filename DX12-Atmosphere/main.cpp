#include "stdafx.h"
#include "App/App.h"
#include "Testes/FullScreenQuad.h"
#include <tchar.h>

int main(int argc, char** argv)
{
	FullScreenQuad app;
	app.Initialize();
	return app.Run();

}