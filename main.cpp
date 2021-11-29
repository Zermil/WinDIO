#include <cstdint>

#define WINDIO_IMPLEMENTATION
#include "./windio.hpp"

// For compiler that implement it
#pragma comment(lib, "winmm.lib")

enum Key : uint8_t {
    KEY_A = 0x41,
    KEY_S = 0x53,
};

int main()
{
    windioInitialize();
    windioGetDevsInfo();

    bool running = true;
    
    while (running) {
	if (GetAsyncKeyState(KEY_A) & 0x01) {
	    windioPlay(440.0, Wave::SIN);
	}

	if (GetAsyncKeyState(KEY_S) & 0x01) {
	    windioPlay(264.0, Wave::SIN);
	}

	if (GetAsyncKeyState(VK_ESCAPE) & 0x01) {
	    running = false;
	}
    }

    windioUninitialize();
    return 0;
}
