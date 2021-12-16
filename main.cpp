#include <cstdint>

#define WINDIO_IMPLEMENTATION
#include "./windio.hpp"

// For compilers that implement it
#pragma comment(lib, "winmm.lib")

enum Key : uint8_t {
    KEY_A = 0x41,
    KEY_D = 0x44,
    KEY_S = 0x53,
};

int main()
{
    output_settings settings;
    settings.windioGetDevsInfo();

    std::vector<double> frequencies = {
	261.63,
	329.63,
	392.00,
	493.88
    };
    
    bool running = true;
    while (running) {
	if (GetAsyncKeyState(KEY_A) & 0x01) {
	    settings.windioPlay(440.0);
	}
	
	if (GetAsyncKeyState(KEY_S) & 0x01) {
	    settings.wave = Wave::TRI;
	    settings.frequency = 261.63;
	}

	if (GetAsyncKeyState(KEY_D) & 0x01) {
	    settings.windioPlayMultiple(frequencies);
	}
	
	if (GetAsyncKeyState(VK_ESCAPE) & 0x01) {
	    settings.frequency = 0.0;
	    running = false;
	}
    }
    
    return 0;
}
