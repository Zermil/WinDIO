#include <cstdint>

#define WINDIO_IMPLEMENTATION
#include "./windio.hpp"

// For compilers that implement it (MSVC and similar)
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")

enum Key : uint8_t {
    KEY_A = 0x41,
    KEY_D = 0x44,
    KEY_S = 0x53,
};

enum KeyState : uint16_t {
    PRESSED = 0x01,
    HELD = 0x8000,
};

int main()
{
    bool running = true;
    windio_settings settings = {};
    std::vector<double> frequencies = {
        261.63,
        329.63,
        392.00,
        493.88
    };
    
    windioInitializeSettings(settings);
    windioPrintDevsInfo(); // Helper function, show available audio devices
    
    while (running) {
        bool key_pressed = false;
        
        if (GetAsyncKeyState(KEY_A) & HELD) {
            windioPlay(settings, 440.0, WAVE_SIN);
            
            key_pressed = true;
        }
	
        if (GetAsyncKeyState(KEY_S) & HELD) {
            windioPlay(settings, 261.63, WAVE_TRI, 0.4);
            
            key_pressed = true;
        }

        if (GetAsyncKeyState(KEY_D) & HELD) {
            windioPlayMultiple(settings, frequencies, WAVE_SIN);
            
            key_pressed = true;
        }
	
        if (GetAsyncKeyState(VK_ESCAPE) & PRESSED) {
            windioMute(settings);
            running = false;
        }

        if (!key_pressed) {
            windioMute(settings);
        }
    }

    windioUninitializeSettings(settings);
    
    return 0;
}
