#define WINDIO_IMPLEMENTATION
#include "./windio.hpp"

// For compiler that implement it
#pragma comment(lib, "winmm.lib")

int main()
{
    windioInitialize();
    windioGetDevsInfo();

    bool running = true;
    
    while (running) {
	if (GetAsyncKeyState(0x41) & 0x01) {
	    windioPlay(440.0, Wave::SIN);
	}

	if (GetAsyncKeyState(0x53) & 0x01) {
	    windioPlay(264.0, Wave::SIN);
	}

	if (GetAsyncKeyState(VK_ESCAPE) & 0x01) {
	    running = false;
	}
    }

    windioUninitialize();
    return 0;
}
