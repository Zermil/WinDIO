#ifndef WINDIO_HPP
#define WINDIO_HPP

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <climits>
#include <thread>
#include <atomic>
#include <condition_variable>

// Windows specific
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>

enum class Wave {
    SIN = 0,
    SQA,
    TRI,
};

struct output_settings {
    std::atomic<double> frequency;
    std::atomic<double> volume;
    std::atomic<Wave> wave;

    // If you know what you're doing, you can modify these, otherwise better leave them alone.
    WAVEHDR* wave_hdr;
    short* block;
};

void windioInitialize(output_settings* settings);
void windioDestroy();
void windioPlay(double frequency, Wave wave, double volume);
void windioStop();
void windioGetDevsInfo();

#endif // WINDIO_HPP

#ifdef WINDIO_IMPLEMENTATION

static const double PI = 2.0 * acos(0.0);
static constexpr DWORD BLOCKS_SZ = 8;
static constexpr DWORD SAMPLES_SZ = 256;
static constexpr DWORD SAMPLE_RATE = 44100;
static constexpr double TIME_STEP = 1.0 / SAMPLE_RATE;

static UINT av_devs = 0;
static HWAVEOUT device;
static output_settings* instance_settings;

static std::atomic<DWORD> free_blocks = BLOCKS_SZ;
static std::atomic<double> global_time = 0.0;
static std::atomic<bool> play_music;
static std::thread music_thread;
static std::mutex mux_play;
static std::condition_variable loop_again;

static void windioPlayThread();

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void) hwo;
    (void) dwInstance;
    (void) dwParam1;
    (void) dwParam2;

    if (uMsg == WOM_DONE) {
	free_blocks++;
	
	std::lock_guard<std::mutex> lm(mux_play);
	loop_again.notify_one();
    }
}

static void error(const char* err_msg)
{
    fprintf(stderr, err_msg);
    windioDestroy();
    
    exit(1);
}

void windioInitialize(output_settings* settings)
{
    UINT devices = waveOutGetNumDevs();
    WAVEFORMATEX wave = {};
    
    if (devices == 0) {
	error("ERROR: No output devices were found!\n");
    }
    
    av_devs = devices;

    wave.wFormatTag = WAVE_FORMAT_PCM;
    wave.nSamplesPerSec = SAMPLE_RATE;
    wave.nChannels = 1;
    wave.wBitsPerSample = 16;
    wave.nBlockAlign = (wave.nChannels * (wave.wBitsPerSample / 8));
    wave.nAvgBytesPerSec = (wave.nSamplesPerSec * wave.nBlockAlign);
    wave.cbSize = 0;
    
    MMRESULT open_result = waveOutOpen(&device, 0, &wave, reinterpret_cast<DWORD_PTR>(waveOutProc), 0, CALLBACK_FUNCTION);

    if (open_result != MMSYSERR_NOERROR) {
	error("ERROR: Default audio output device could not be properly opened!\n");
    }

    settings->wave_hdr = new WAVEHDR[BLOCKS_SZ];

    if (settings->wave_hdr == nullptr) {
	error("ERROR: Could not allocate enough memory for WAVEHDR\n");
    }

    settings->block = new short[BLOCKS_SZ * SAMPLES_SZ];

    if (settings->block == nullptr) {
	error("ERROR: Could not allocate enough memory for a block of samples\n");
    }
    
    memset(settings->wave_hdr, 0, sizeof(WAVEHDR) * BLOCKS_SZ);
    memset(settings->block, 0, sizeof(short) * BLOCKS_SZ * SAMPLES_SZ);

    // wave_hdr will be pointing to data from block
    for (DWORD i = 0; i < BLOCKS_SZ; ++i) {
	settings->wave_hdr[i].dwBufferLength = SAMPLES_SZ * sizeof(short);
	settings->wave_hdr[i].lpData = reinterpret_cast<LPSTR>(((settings->block + (i * SAMPLES_SZ))));
    }

    // Start music output thread
    settings->frequency = 0.0;
    settings->volume = 0.2;
    settings->wave = Wave::SIN;

    instance_settings = settings;

    play_music = true;
    music_thread = std::thread(windioPlayThread);
}

void windioDestroy()
{
    if (instance_settings->block) {
	delete[] instance_settings->block;
    }

    if (instance_settings->wave_hdr) {
	delete[] instance_settings->wave_hdr;
    }

    play_music = false;
    music_thread.join();

    waveOutReset(device);
    waveOutClose(device);
}

void windioGetDevsInfo()
{
    WAVEOUTCAPS caps = {};
    
    printf("Devices Found:\n");
    for (UINT i = 0; i < av_devs; ++i) {
	MMRESULT result = waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS));

	if (result != MMSYSERR_NOERROR) {
	    error("ERROR: There was a problem retrieving information from one of the available devices!\n");
	}
	
	printf("%s\n", caps.szPname);
    }
}

void windioPlay(double frequency, Wave wave = Wave::SIN, double volume = 0.2)
{
    instance_settings->volume = volume;
    instance_settings->wave = wave;
    instance_settings->frequency = frequency;
}

void windioStop()
{
    instance_settings->frequency = 0.0;
}

// Frequency as angular velocity
static inline double fav(const double& f) noexcept
{
    return f * 2.0 * PI;
}

static double get_sample_from_settings()
{
    double out_freq = 0.0;
    
    switch (instance_settings->wave) {
	case Wave::SIN:
	    out_freq = sin(fav(instance_settings->frequency) * global_time);
	    break;
	case Wave::SQA:
	    out_freq = sin(fav(instance_settings->frequency) * global_time) > 0.0 ? 1.0 : -1.0;
	    break;
	case Wave::TRI:
	    out_freq = asin(sin(fav(instance_settings->frequency) * global_time)) * (2.0 / PI);
	    break;
	default:
	    assert(false && "Unreachable, invalid wave provided!");
    }

    return out_freq * instance_settings->volume;
}

static void windioPlayThread()
{
    size_t current_block = 0;
    
    while (play_music) {
	// Instead of 'continue;' it waits until it can loop again,
	// not looping infinitely.
	if (free_blocks == 0) {
	    std::unique_lock<std::mutex> lm(mux_play);
	    loop_again.wait(lm, []() { return free_blocks != 0; });
	}
	
	free_blocks--;
    
	if (instance_settings->wave_hdr[current_block].dwFlags & WHDR_PREPARED) {
	    MMRESULT unprepare_result = waveOutUnprepareHeader(device, &instance_settings->wave_hdr[current_block], sizeof(WAVEHDR));
	
	    if (unprepare_result != MMSYSERR_NOERROR) {
		error("ERROR: Could not clear wave header\n");
	    }
	}

	for (DWORD i = 0; i < SAMPLES_SZ; ++i) {
	    short sample_freq = static_cast<short>(get_sample_from_settings() * SHRT_MAX);

	    instance_settings->block[(current_block * SAMPLES_SZ) + i] = sample_freq;
	    global_time = (global_time + TIME_STEP);
	}
    
	MMRESULT prepare_result = waveOutPrepareHeader(device, &instance_settings->wave_hdr[current_block], sizeof(WAVEHDR));

	if (prepare_result != MMSYSERR_NOERROR) {
	    error("ERROR: Could not prepare wave header\n");
	}

	MMRESULT write_result = waveOutWrite(device, &instance_settings->wave_hdr[current_block], sizeof(WAVEHDR));
    
	if (write_result != MMSYSERR_NOERROR) {
	    error("ERROR: Could not send audio to output device\n");
	}

	current_block = (current_block + 1) % BLOCKS_SZ;
    }
}

#endif // WINDIO_IMPLEMENTATION
