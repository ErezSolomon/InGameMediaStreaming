// FFMPEGBBroadcastDLLDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>

#include <FFMPEGBroadcastDLL.h>

const int WIDTH = 1024;
const int HEIGHT = 1024;
const int COLOR_CHANNELS = 3;

char texData[WIDTH * HEIGHT * COLOR_CHANNELS];
int main(int argc, char* argv[])
{
	std::cout << "Hello World!\n";

	const char* filename = argv[1];
	std::cout << "Open: " << filename << std::endl;
	Receiver* rec = Receiver_Create(filename);

	int count = 0;
	while (true)
	{
		Receiver_GetUpdatedFrame(rec, WIDTH, HEIGHT, texData, nullptr);
		std::cout << "Received frame " << ++count << "\n";
	}

	Receiver_Destroy(rec);
}