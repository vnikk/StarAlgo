#pragma once
#pragma message("Compiling precompiled headers (you should see this only once)")
#define WIN32_LEAN_AND_MEAN	// exclude rarely-used stuff from Windows headers
#define NOMINMAX			// avoid collisions with min() max()

#include <Windows.h>
#include <random>
#include <fstream>

extern unsigned int combatsSimulated; // TODO define in srcC

extern std::mt19937 gen; // random number generator


