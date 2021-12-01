#include <cstdint>

#define WINDIO_IMPLEMENTATION
#include "./windio.hpp"

// For compilers that implement it
#pragma comment(lib, "winmm.lib")

enum Key : uint8_t {
    KEY_A = 0x41,
    KEY_S = 0x53,
};

int main()
{
    output_settings settings;
    
    windioInitialize(&settings);
    windioGetDevsInfo();

    bool running = true;
    
    while (running) {
	if (GetAsyncKeyState(KEY_A) & 0x01) {
	    settings.wave = Wave::SIN;
	    settings.frequency = 440.0;
	}

	if (GetAsyncKeyState(KEY_S) & 0x01) {
	    settings.wave = Wave::TRI;
	    settings.frequency = 264.0;
	}
	
	if (GetAsyncKeyState(VK_ESCAPE) & 0x01) {
	    settings.frequency = 0.0;
	    running = false;
	}
    }

    windioDestroy();
    return 0;
}
