// PlayerDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#include <FFMPEGBroadcastDLL.h>

const int WIDTH = 512;
const int HEIGHT = 512;
const int COLOR_CHANNELS = 3;

unsigned char texData[WIDTH * HEIGHT * COLOR_CHANNELS];
int main(int argc, char* argv[])
{
	std::cout << "Hello World!\n";

	char* filepath = argv[1];
	//rtmp://localhost:1935/myapp
	//bigbuckbunny_480x272.h265
	//SDL---------------------------
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;

	const char* filename = argv[1];
	std::cout << "Open: " << filename << std::endl;
	Receiver* rec = Receiver_Create(filename);

	screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		WIDTH, HEIGHT,
		SDL_WINDOW_RESIZABLE/*SDL_WINDOW_OPENGL*/);
	
	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = WIDTH;
	sdlRect.h = HEIGHT;

	int64_t count = 0;
	bool alive = true;

	while (alive)
	{
		if (Receiver_GetUpdatedFrame(rec, WIDTH, HEIGHT, (char*)texData, nullptr))
		{
			SDL_Event event;

			while (SDL_PollEvent(&event)) {
				switch (event.type) {
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE)
						alive = false;
					break;
				case SDL_QUIT:
					alive = false;
					break;
				}
			}

			Uint32 *dst;
			int row, col;
			void *pixels;
			int pitch;

			if (SDL_LockTexture(sdlTexture, NULL, &pixels, &pitch) < 0) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't lock texture: %s\n", SDL_GetError());
			}

			//src = MooseFrames[frame];
			for (row = 0; row < HEIGHT; ++row) {
				dst = (Uint32*)((Uint8*)pixels + row * pitch);
				for (col = 0; col < WIDTH; ++col) {
					uint32_t r, g, b;
					const int index = COLOR_CHANNELS * ((HEIGHT - 1 - row) * WIDTH + col);
					r = texData[index + 0];
					g = texData[index + 1];
					b = texData[index + 2];
					/*r = 0;
					g = 0;
					b = 255;*/
					/*r = time_now % 2 * 128;
					g = time_now % 3 * 80;
					b = time_now % 5 * 50;*/
					*dst++ = (0xFF000000 | (r << 16) | (g << 8) | b);
				}
			}

			SDL_UnlockTexture(sdlTexture);

			SDL_RenderClear(sdlRenderer);
			//if (time(NULL) % 2)
			SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
			SDL_RenderPresent(sdlRenderer);

			std::cout << "Received frame " << ++count << "\n";
			// TODO: Get and print frame data
		}
	}

	Receiver_Destroy(rec);
}