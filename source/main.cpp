#include "pch.h"

#if defined(_DEBUG)
#include "vld.h"
#endif

#undef main
#include "Renderer.h"

using namespace dae;

void ShutDown(SDL_Window* pWindow)
{
	SDL_DestroyWindow(pWindow);
	SDL_Quit();
}

int main(int argc, char* args[])
{
	//Unreferenced parameters
	(void)argc;
	(void)args;

	//Create window + surfaces
	SDL_Init(SDL_INIT_VIDEO);

	const uint32_t width = 640;
	const uint32_t height = 480;

	SDL_Window* pWindow = SDL_CreateWindow(
		"DualRasterizer - Mendel Debrabandere / 2DAE07",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width, height, 0);

	if (!pWindow)
		return 1;

	//Initialize "framework"
	const auto pTimer = new Timer();
	const auto pRenderer = new Renderer(pWindow);

	std::cout << "I added E and Q to move the camera up and down respectively (basic UE movement)\n\n";

	//Start loop
	pTimer->Start();
	float printTimer = 0.f;
	bool printFPS{ true };
	bool isLooping = true;
	while (isLooping)
	{
		//--------- Get input events ---------
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_QUIT:
				isLooping = false;
				break;
			case SDL_KEYUP:
				//Test for a key
				//if (e.key.keysym.scancode == SDL_SCANCODE_X)
				switch (e.key.keysym.scancode)
				{
					//SHARED
					case SDL_SCANCODE_F1:
						pRenderer->ToggleDirectX();
						break;
					case SDL_SCANCODE_F2:
						pRenderer->ToggleRotation();
						break;
					case SDL_SCANCODE_F9:
						pRenderer->CycleCullModes();
						break;
					case SDL_SCANCODE_F10:
						pRenderer->ToggleUniformClearColor();
						break;
					case SDL_SCANCODE_F11:
						pTimer->Reset();
						printFPS = !printFPS;
						if (printFPS)
							std::cout << "Started FPS printing\n";
						else
							std::cout << "Stopped FPS printing\n";
						break;

					//HARDWARE ONLY
					case SDL_SCANCODE_F3:
						pRenderer->ToggleFireRendering();
						break;
					case SDL_SCANCODE_F4:
						pRenderer->ToggleFilteringMethod();
						break;

					//SOFTWARE ONLY
					case SDL_SCANCODE_F5:
						pRenderer->CycleShadingMode();
						break;
					case SDL_SCANCODE_F6:
						pRenderer->ToggleNormalMap();
						break;
					case SDL_SCANCODE_F7:
						pRenderer->ToggleDepthBufferVisualization();
						break;
					case SDL_SCANCODE_F8:
						pRenderer->ToggleBoundingBoxVisualization();
						break;
				}
				break;
			default:;
			}
		}

		//--------- Update ---------
		pRenderer->Update(pTimer);

		//--------- Render ---------
		pRenderer->Render();

		//--------- Timer ---------
		if (printFPS)
		{
			pTimer->Update();
			printTimer += pTimer->GetElapsed();
			if (printTimer >= 1.f)
			{
				printTimer = 0.f;
				std::cout << "dFPS: " << pTimer->GetdFPS() << std::endl;
			}
		}
	}
	pTimer->Stop();

	//Shutdown "framework"
	delete pRenderer;
	delete pTimer;

	ShutDown(pWindow);
	return 0;
}