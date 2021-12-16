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

// TODO(Aiden): Different sounds, piano like, drum like etc.
// TODO(Aiden): Have a way to distinguish frequencies (better polyphony).
// TODO(Aiden): ADSR, more pleasant sounds.
// TODO(Aiden): Ensure that chords sound fine.

#ifndef WINDIO_PI
#define WINDIO_PI 3.141592653
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

#ifndef WINDIO_TIME_STEP
#define WINDIO_TIME_STEP (1.0f / WINDIO_SAMPLE_RATE)
#endif

enum class Wave {
    SIN = 0,
    SQA,
    TRI,
};

struct output_settings {
    output_settings();
    ~output_settings();

    inline double fav(const double& f) const noexcept { return f * 2.0 * WINDIO_PI; } // Frequency as angular velocity
    double get_sound_frequency();
    void windioPlay(double frequency, Wave wave, double volume);
    void windioPlayMultiple(const std::vector<double>& frequencies, Wave wave, double volume);
    void windioStop();
    void windioGetDevsInfo();

    std::atomic<double> frequency;
    std::atomic<double> volume;
    std::atomic<Wave> wave;

    // If you _really_ know what you're doing, you can modify anything below, otherwise, leave them alone.
    // We are all consenting adults :)
    void windioPlayThread();
    static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    void error(const char* err_msg);
    WAVEFORMATEX initialize_wave_struct();
    
    HWAVEOUT device;
    WAVEHDR* wave_hdr;
    short* block;
    std::atomic<DWORD> free_blocks;
    std::atomic<double> global_time;
    std::atomic<bool> play_music;
    std::thread music_thread;
    std::mutex mux_play;
    std::condition_variable loop_again;    
};

#endif // WINDIO_HPP

#ifdef WINDIO_IMPLEMENTATION

void CALLBACK output_settings::waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void) hwo;
    (void) dwParam1;
    (void) dwParam2;

    output_settings *settings = reinterpret_cast<output_settings*>(dwInstance);
    
    if (uMsg == WOM_DONE) {
	settings->free_blocks++;
	
	std::lock_guard<std::mutex> lm(settings->mux_play);
	settings->loop_again.notify_one();
    }
}

output_settings::output_settings()
{
    UINT devices = waveOutGetNumDevs();
    
    if (devices == 0) {
	error("ERROR: No output devices were found!\n");
    }
    
    WAVEFORMATEX wave_bin_hdr = initialize_wave_struct();
    MMRESULT open_result = waveOutOpen(&device, 0, &wave_bin_hdr, reinterpret_cast<DWORD_PTR>(&output_settings::waveOutProc), reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);

    if (open_result != MMSYSERR_NOERROR) {
	error("ERROR: Default audio output device could not be properly opened!\n");
    }

    wave_hdr = new WAVEHDR[WINDIO_BLOCKS_SZ];

    if (wave_hdr == nullptr) {
	error("ERROR: Could not allocate enough memory for WAVEHDR\n");
    }

    block = new short[WINDIO_BLOCKS_SZ * WINDIO_SAMPLES_SZ];

    if (block == nullptr) {
	error("ERROR: Could not allocate enough memory for a block of samples\n");
    }
    
    memset(wave_hdr, 0, sizeof(WAVEHDR) * WINDIO_BLOCKS_SZ);
    memset(block, 0, sizeof(short) * WINDIO_BLOCKS_SZ * WINDIO_SAMPLES_SZ);

    // wave_hdr will be pointing to data from block
    for (DWORD i = 0; i < WINDIO_BLOCKS_SZ; ++i) {
	wave_hdr[i].dwBufferLength = WINDIO_SAMPLES_SZ * sizeof(short);
	wave_hdr[i].lpData = reinterpret_cast<LPSTR>(((block + (i * WINDIO_SAMPLES_SZ))));
    }

    // Start music output thread
    global_time = 0.0;
    free_blocks = WINDIO_BLOCKS_SZ;
    frequency = 0.0;
    volume = 0.2;
    wave = Wave::SIN;
    
    play_music = true;
    music_thread = std::thread(windioPlayThread, this);
}

output_settings::~output_settings()
{
    play_music = false;
    music_thread.join();

    if (block) {
	delete[] block;
    }

    if (wave_hdr) {
	delete[] wave_hdr;
    }

    waveOutReset(device);
    waveOutClose(device);
}

WAVEFORMATEX output_settings::initialize_wave_struct()
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

void output_settings::error(const char* err_msg)
{
    fprintf(stderr, err_msg);
    this->~output_settings();
    
    exit(1);
}

void output_settings::windioGetDevsInfo()
{
    WAVEOUTCAPS caps = {};
    UINT devices = waveOutGetNumDevs();
    
    printf("Devices Found:\n");
    for (UINT i = 0; i < devices; ++i) {
	MMRESULT result = waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS));

	if (result != MMSYSERR_NOERROR) {
	    error("ERROR: There was a problem retrieving information from one of the available devices!\n");
	}
	
	printf("%s\n", caps.szPname);
    }
}

void output_settings::windioPlay(double frequency, Wave wave = Wave::SIN, double volume = 0.2)
{
    this->volume = volume;
    this->wave = wave;
    this->frequency = frequency;
}

void output_settings::windioPlayMultiple(const std::vector<double>& frequencies, Wave wave = Wave::SIN, double volume = 0.2)
{
    double frequency = 0.0;

    for (const double& freq : frequencies) {
	frequency += freq;
    }
    
    this->volume = volume;
    this->wave = wave;
    this->frequency = frequency;
}

void output_settings::windioStop()
{
    this->frequency = 0.0;
}

double output_settings::get_sound_frequency()
{    
    switch (wave) {
	case Wave::SIN:
	    return sin(fav(frequency) * global_time);
	case Wave::SQA:
	    return sin(fav(frequency) * global_time) > 0.0 ? 1.0 : -1.0;
	case Wave::TRI:
	    return asin(sin(fav(frequency) * global_time)) * (2.0 / WINDIO_PI);
	default:
	    assert(false && "Unreachable, invalid wave provided!");
    }
    
    // Realistically this should never happened, but I still don't trust computers.
    return 0.0;
}

void output_settings::windioPlayThread()
{
    size_t current_block = 0;
    
    while (play_music) {
	// Instead of 'continue;' it waits until it can loop again,
	// not looping infinitely.
	if (free_blocks == 0) {
	    std::unique_lock<std::mutex> lm(mux_play);
	    loop_again.wait(lm, [&]() { return free_blocks != 0; });
	}
	
	free_blocks--;
    
	if (wave_hdr[current_block].dwFlags & WHDR_PREPARED) {
	    MMRESULT unprepare_result = waveOutUnprepareHeader(device, &wave_hdr[current_block], sizeof(WAVEHDR));
	
	    if (unprepare_result != MMSYSERR_NOERROR) {
		error("ERROR: Could not clear wave header\n");
	    }
	}

	for (DWORD i = 0; i < WINDIO_SAMPLES_SZ; ++i) {
	    short sample_freq = static_cast<short>((get_sound_frequency() * volume) * SHRT_MAX);

	    block[(current_block * WINDIO_SAMPLES_SZ) + i] = sample_freq;
	    global_time = (global_time + WINDIO_TIME_STEP);
	}
    
	MMRESULT prepare_result = waveOutPrepareHeader(device, &wave_hdr[current_block], sizeof(WAVEHDR));

	if (prepare_result != MMSYSERR_NOERROR) {
	    error("ERROR: Could not prepare wave header\n");
	}

	MMRESULT write_result = waveOutWrite(device, &wave_hdr[current_block], sizeof(WAVEHDR));
    
	if (write_result != MMSYSERR_NOERROR) {
	    error("ERROR: Could not send audio to output device\n");
	}

	current_block = (current_block + 1) % WINDIO_BLOCKS_SZ;
    }
}

#endif // WINDIO_IMPLEMENTATION
