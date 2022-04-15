#ifndef WINDIO_HPP
#define WINDIO_HPP

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <climits>
#include <thread>
#include <atomic>
#include <vector>
#include <condition_variable>

// Windows specific
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>

#ifndef WINDIO_PI
#define WINDIO_PI 3.141592653589
#endif

#ifndef WINDIO_DEF_VOLUME
#define WINDIO_DEF_VOLUME 0.1f
#endif

#ifndef WINDIO_BLOCKS_SZ
#define WINDIO_BLOCKS_SZ 8
#endif

#ifndef WINDIO_SAMPLES_SZ
#define WINDIO_SAMPLES_SZ 256
#endif

#ifndef WINDIO_SAMPLE_RATE
#define WINDIO_SAMPLE_RATE 44100
#endif

#ifndef WINDIO_FREQ_CAP
#define WINDIO_FREQ_CAP 32
#endif

#ifndef WINDIO_TIME_STEP
#define WINDIO_TIME_STEP (1.0f / WINDIO_SAMPLE_RATE)
#endif

// TODO(#1): Different instruments, drum like etc.
// TODO(#2): Have a way to distinguish frequencies (better polyphony)
// TODO(#3): ADSR, more pleasant sounds
// TODO: Better way of checking if a struct passed through windioInitializeSetting()

enum Wave {
    WAVE_SIN = 0,
    WAVE_SQU,
    WAVE_TRI,
};

struct windio_settings
{
    // NOTE(Aiden): Controlled by user
    std::atomic<float> frequency[WINDIO_FREQ_CAP];
    std::atomic<Wave> wave;
    std::atomic<float> volume;

    // NOTE(Aiden): Implementation part
    std::atomic<bool> music_play = false;
    std::atomic<double> global_time;
    std::atomic<DWORD> free_blocks;
    std::atomic<size_t> samples_sz;
    std::condition_variable loop_again;
    std::mutex mux_play;
    std::thread music_thread;
    
    HWAVEOUT device;
    WAVEHDR *wave_hdr;
    short *block;
};

void windioPrintDevsInfo();
void windioInitializeSettings(windio_settings& settings, UINT device_num = 0);
void windioUninitializeSettings(windio_settings& settings);
void windioPlay(windio_settings& settings, float frequency, Wave wave, float volume);
void windioPlay(windio_settings& settings, float frequency, Wave wave);
void windioPlayMultiple(windio_settings& settings, const float *frequencies, size_t samples, Wave wave, float volume);
void windioPlayMultiple(windio_settings& settings, const std::vector<float>& frequencies, Wave wave, float volume);
void windioPlayMultiple(windio_settings& settings, const std::vector<float>& frequencies, Wave wave);
void windioMute(windio_settings& settings);

#endif // WINDIO_HPP
#ifdef WINDIO_IMPLEMENTATION

// NOTE(Aiden): Frequency as angular velocity
static inline float windioFav(const float& f)
{
    return f * 2.0 * WINDIO_PI;
}

static float windioGetSoundFrequency(const windio_settings& settings)
{
    float out = 0.0f;
    
    for (size_t i = 0; i < settings.samples_sz; ++i) {
        switch (settings.wave) {
            case WAVE_SIN:
                out += sin(windioFav(settings.frequency[i]) * settings.global_time);
                break;
            
            case WAVE_SQU:
                out += sin(windioFav(settings.frequency[i]) * settings.global_time) > 0.0 ? 1.0 : -1.0;
                break;
            
            case WAVE_TRI:
                out += asin(sin(windioFav(settings.frequency[i]) * settings.global_time)) * (2.0 / WINDIO_PI);
                break;
            
            default:
                assert(false && "[ERROR]: Unreachable, invalid wave provided!");
        }
    }

    return out;
}

static WAVEFORMATEX windioInitializeWaveStruct()
{
    WAVEFORMATEX wave_bin_hdr = {};
    
    wave_bin_hdr.wFormatTag = WAVE_FORMAT_PCM;
    wave_bin_hdr.nSamplesPerSec = WINDIO_SAMPLE_RATE;
    wave_bin_hdr.nChannels = 1;
    wave_bin_hdr.wBitsPerSample = 16;
    wave_bin_hdr.nBlockAlign = (wave_bin_hdr.nChannels * (wave_bin_hdr.wBitsPerSample / 8));
    wave_bin_hdr.nAvgBytesPerSec = (wave_bin_hdr.nSamplesPerSec * wave_bin_hdr.nBlockAlign);
    wave_bin_hdr.cbSize = 0;
    
    return wave_bin_hdr;
}

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void) hwo;
    (void) dwParam1;
    (void) dwParam2;

    windio_settings *settings = reinterpret_cast<windio_settings*>(dwInstance);
    
    if (uMsg == WOM_DONE) {
        std::lock_guard<std::mutex> lm(settings->mux_play);
	
        settings->free_blocks++;
        settings->loop_again.notify_one();
    }
}

static void windioPlayThread(windio_settings& settings)
{
    size_t current_block = 0;
    
    while (settings.music_play) {
        // NOTE(Aiden): Instead of 'continue;' it waits until it can loop again, not looping infinitely.
        if (settings.free_blocks == 0) {
            std::unique_lock<std::mutex> lm(settings.mux_play);
            settings.loop_again.wait(lm, [&]() { return settings.free_blocks != 0; });
        }
	
        settings.free_blocks--;
    
        if (settings.wave_hdr[current_block].dwFlags & WHDR_PREPARED) {
            MMRESULT unprepare_result = waveOutUnprepareHeader(settings.device, &settings.wave_hdr[current_block], sizeof(WAVEHDR));
            assert((unprepare_result == MMSYSERR_NOERROR) && "[ERROR]: Could not clear wave header\n");
        }

        for (DWORD i = 0; i < WINDIO_SAMPLES_SZ; ++i) {
            short sample_freq = static_cast<short>((windioGetSoundFrequency(settings) * settings.volume) * SHRT_MAX);

            settings.block[(current_block * WINDIO_SAMPLES_SZ) + i] = sample_freq;
            settings.global_time = (settings.global_time + WINDIO_TIME_STEP);
        }
    
        MMRESULT prepare_result = waveOutPrepareHeader(settings.device, &settings.wave_hdr[current_block], sizeof(WAVEHDR));
        assert((prepare_result == MMSYSERR_NOERROR) && "[ERROR]: Could not prepare wave header\n");

        MMRESULT write_result = waveOutWrite(settings.device, &settings.wave_hdr[current_block], sizeof(WAVEHDR));
        assert((write_result == MMSYSERR_NOERROR) && "[ERROR]: Could not send audio to output device\n");

        current_block = (current_block + 1) % WINDIO_BLOCKS_SZ;
    }
}

void windioPrintDevsInfo()
{
    WAVEOUTCAPS caps = {};
    UINT devices = waveOutGetNumDevs();
    
    printf("Devices Found:\n");
    for (UINT i = 0; i < devices; ++i) {
        MMRESULT result = waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS));
        assert((result == MMSYSERR_NOERROR) && "[ERROR]: There was a problem retrieving information from one of the available devices!\n\n");
        
        printf("%s\n", caps.szPname);
    }
}

void windioInitializeSettings(windio_settings& settings, UINT device_num)
{
    settings.frequency[0] = 0.0f;
    settings.samples_sz = 1;
    settings.wave = WAVE_SIN;
    settings.volume = WINDIO_DEF_VOLUME;
    settings.global_time = 0.0;
    settings.free_blocks = WINDIO_BLOCKS_SZ;

    UINT devices = waveOutGetNumDevs();
    assert((devices != 0) && "[ERROR]: No output devices were found\n");

    WAVEFORMATEX wave_bin_hdr = windioInitializeWaveStruct();
    
    MMRESULT open_result = waveOutOpen(&settings.device, device_num, &wave_bin_hdr,
                                       reinterpret_cast<DWORD_PTR>(waveOutProc),
                                       reinterpret_cast<DWORD_PTR>(&settings), CALLBACK_FUNCTION);
    assert((open_result == MMSYSERR_NOERROR) && "[ERROR]: Default audio output device could not be properly opened!\n");

    settings.wave_hdr = new WAVEHDR[WINDIO_BLOCKS_SZ];
    settings.block = new short[WINDIO_BLOCKS_SZ * WINDIO_SAMPLES_SZ];
     
    assert((settings.wave_hdr != nullptr) && "[ERROR]: Could not allocate memory for wave header\n");
    assert((settings.block != nullptr) && "[ERROR]: Could not allocate memory for audio block\n");

    memset(settings.wave_hdr, 0, sizeof(WAVEHDR) * WINDIO_BLOCKS_SZ);
    memset(settings.block, 0, sizeof(short) * WINDIO_BLOCKS_SZ * WINDIO_SAMPLES_SZ);

    // NOTE(Aiden): wave_hdr will be pointing to data from block
    for (DWORD i = 0; i < WINDIO_BLOCKS_SZ; ++i) {
        settings.wave_hdr[i].dwBufferLength = WINDIO_SAMPLES_SZ * sizeof(short);
        settings.wave_hdr[i].lpData = reinterpret_cast<LPSTR>(((settings.block + (i * WINDIO_SAMPLES_SZ))));
    }

    settings.music_play = true;
    settings.music_thread = std::thread(windioPlayThread, std::ref(settings));
}

void windioUninitializeSettings(windio_settings& settings)
{
    assert((settings.music_play) && "[ERROR]: Trying to clear uninitialized instance of windio_settings\n");
    
    settings.music_play = false;
    settings.music_thread.join();

    delete[] settings.wave_hdr;
    delete[] settings.block;
    
    waveOutReset(settings.device);
    waveOutClose(settings.device);
}

void windioMute(windio_settings& settings)
{
    settings.frequency[0] = 0.0f;
    settings.samples_sz = 1;
    settings.volume = 0.0f;
}

void windioPlay(windio_settings& settings, float frequency, Wave wave)
{
    windioPlay(settings, frequency, wave, WINDIO_DEF_VOLUME);
}

void windioPlay(windio_settings& settings, float frequency, Wave wave, float volume)
{
    assert((settings.music_play) && "[ERROR]: windio_settings not initialized, did you forget to call windioInitializeSetting(windio_settings& settings)?\n"); 

    settings.frequency[0] = frequency;
    settings.samples_sz = 1;
    settings.wave = wave;
    settings.volume = volume;
}

void windioPlayMultiple(windio_settings& settings, const std::vector<float>& frequencies, Wave wave)
{
    windioPlayMultiple(settings, frequencies.data(), frequencies.size(), wave, WINDIO_DEF_VOLUME);
}

void windioPlayMultiple(windio_settings& settings, const std::vector<float>& frequencies, Wave wave, float volume)
{
    windioPlayMultiple(settings, frequencies.data(), frequencies.size(), wave, volume);
}

void windioPlayMultiple(windio_settings& settings, const float *frequencies, size_t samples, Wave wave, float volume)
{
    assert((settings.music_play) && "[ERROR]: windio_settings not initialized, did you forget to call windioInitializeSetting(windio_settings& settings)?\n");
    assert((samples <= WINDIO_FREQ_CAP) && "[ERROR]: Trying to assign more samples than current max capacity\n");

    for (size_t i = 0; i < samples; ++i) {
        settings.frequency[i] = frequencies[i];
    }
    
    settings.samples_sz = samples;
    settings.wave = wave;
    settings.volume = volume;
}

#endif // WINDIO_IMPLEMENTATION
