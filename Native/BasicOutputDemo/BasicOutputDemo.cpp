// BasicOutputDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>

#include <FFMPEGBroadcastDLL.h>

const int WIDTH = 1024;
const int HEIGHT = 1024;
const int COLOR_CHANNELS = 3;

char texData[WIDTH * HEIGHT * COLOR_CHANNELS];

void fillTexture(unsigned char* texture, int64_t timeDiff)
{
	for (int y = 0; y < HEIGHT; ++y)
		for (int x = 0; x < WIDTH; ++x)
		{
			int index = COLOR_CHANNELS * (y * WIDTH + x);

			int bias = timeDiff / 20000;

			texture[index + 0] = (x + y + bias) % 255;
			texture[index + 1] = (128 + y + bias) % 255;
			texture[index + 2] = (64 + x + bias) % 255;
		}
}

int main(int argc, char* argv[])
{
	std::cout << "Hello World!\n";

	const char* filename = argv[1];
	std::cout << "Open: " << filename << std::endl;

	const char* format = "flv";
	const char* codec = "";

	TransmittingOptions options;
	options.bit_rate = 40000000;
	options.output_width = WIDTH;
	options.output_height = HEIGHT;
	options.frame_units_in_sec = 1000;
	options.max_framerate = 60;
	options.gop_size = 12;
	options.has_audio = true;

	Transmitter* transmitter = Transmitter_Create(filename, format, codec, WIDTH, HEIGHT, options, 0, nullptr, nullptr);
	
	int64_t startTime = GetMicrosecondsTimeRelative();
	int64_t count = 0;
	int64_t count_audio = 0;

	while (true)
	{
		int64_t currentTime = GetMicrosecondsTimeRelative();
		int64_t timeDiff = currentTime - startTime;

		while (Transmitter_ShellWriteAudioNow(transmitter, timeDiff))
		{
			if (!Transmitter_WriteAudioFrame(transmitter, timeDiff, 0, nullptr, nullptr))
				break;

			std::cout << "Transmitted audio frame " << ++count_audio << "\n";
		}

		if (Transmitter_ShellWriteVideoNow(transmitter, timeDiff))
		{
			fillTexture((unsigned char*)texData, timeDiff);

			if (!Transmitter_WriteVideoFrame(transmitter, timeDiff, WIDTH, HEIGHT, texData, nullptr))
				break;
			// otherwise

			std::cout << "Transmitted video frame " << ++count << "\n";
			// TODO: Get and print frame data
		}
	}

	Transmitter_Destroy(transmitter);
}