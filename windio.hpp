#ifndef WINDIO_HPP
#define WINDIO_HPP

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <climits>

// Windows specific
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>

enum class Wave {
    SIN = 0,
};

void windioInitialize();
void windioUninitialize();
void windioGetDevsInfo();
void windioPlay(double frequency, Wave wave);

#endif // WINDIO_HPP

#ifdef WINDIO_IMPLEMENTATION

static constexpr double PI = 2.0 * acos(0.0);
static constexpr DWORD BLOCKS_SZ = 8;
static constexpr DWORD SAMPLES_SZ = 256;
static constexpr DWORD SAMPLE_RATE = 44100;
static constexpr double TIME_STEP = 1.0 / SAMPLE_RATE;

static DWORD FREE_BLOCKS = BLOCKS_SZ;
static UINT AV_DEVS = 0;
static size_t CURRENT_BLOCK = 0;
static double GLOBAL_TIME = 0.0;
static HWAVEOUT DEVICE;
static WAVEHDR* WAVE_HDR;
static short* BLOCK;

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void) hwo;
    (void) dwInstance;
    (void) dwParam1;
    (void) dwParam2;

    if (uMsg == WOM_DONE) {
	FREE_BLOCKS++;
    }
}

static void error(const char* err_msg)
{
    fprintf(stderr, err_msg);
    windioUninitialize();
    exit(1);
}

void windioInitialize()
{
    UINT devices = waveOutGetNumDevs();
    WAVEFORMATEX wave = {};
    
    if (devices == 0) {
	error("ERROR: No output devices were found!\n");
    }
    
    AV_DEVS = devices;

    wave.wFormatTag = WAVE_FORMAT_PCM;
    wave.nSamplesPerSec = SAMPLE_RATE;
    wave.nChannels = 1;
    wave.wBitsPerSample = sizeof(short) * 8;
    wave.nBlockAlign = (wave.nChannels * (wave.wBitsPerSample / 8));
    wave.nAvgBytesPerSec = (wave.nSamplesPerSec * wave.nBlockAlign);
    wave.cbSize = 0;
    
    MMRESULT open_result = waveOutOpen(&DEVICE, 0, &wave, reinterpret_cast<DWORD_PTR>(waveOutProc), 0, CALLBACK_FUNCTION);

    if (open_result != MMSYSERR_NOERROR) {
	error("ERROR: Default audio output device could not be properly opened!\n");
    }

    WAVE_HDR = new WAVEHDR[BLOCKS_SZ];

    if (WAVE_HDR == nullptr) {
	error("ERROR: Could not allocate enough memory for WAVEHDR\n");
    }

    BLOCK = new short[BLOCKS_SZ * SAMPLES_SZ];

    if (BLOCK == nullptr) {
	error("ERROR: Could not allocate enough memory for a block of samples\n");
    }
    
    memset(WAVE_HDR, 0, sizeof(WAVEHDR) * BLOCKS_SZ);
    memset(BLOCK, 0, sizeof(short) * BLOCKS_SZ * SAMPLES_SZ);

    // WAVE_HDR will be pointing to data from BLOCK
    for (DWORD i = 0; i < BLOCKS_SZ; ++i) {
	WAVE_HDR[i].dwBufferLength = SAMPLES_SZ * sizeof(short);
	WAVE_HDR[i].lpData = reinterpret_cast<LPSTR>(((BLOCK + (i * SAMPLES_SZ))));
    }
}

void windioUninitialize()
{
    if (BLOCK) {
	delete[] BLOCK;
    }

    if (WAVE_HDR) {
	delete[] WAVE_HDR;
    }
    
    waveOutClose(DEVICE);
}

void windioGetDevsInfo()
{
    WAVEOUTCAPS caps = {};
    
    printf("Devices Found:\n");
    for (UINT i = 0; i < AV_DEVS; ++i) {
	MMRESULT result = waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS));

	if (result != MMSYSERR_NOERROR) {
	    error("ERROR: There was a problem retrieving information from one of the available devices!\n");
	}
	
	printf("%s\n", caps.szPname);
    }
}

void windioPlay(double frequency, Wave wave)
{
    (void) wave;

    if (FREE_BLOCKS == 0) return;
    FREE_BLOCKS--;

    if (WAVE_HDR[CURRENT_BLOCK].dwFlags & WHDR_PREPARED) {
	MMRESULT unprepare_result = waveOutUnprepareHeader(DEVICE, &WAVE_HDR[CURRENT_BLOCK], sizeof(WAVEHDR));
	
	if (unprepare_result != MMSYSERR_NOERROR) {
	    error("ERROR: Could not unprepare wave header\n");
	}
    }

    for (DWORD i = 0; i < SAMPLES_SZ; ++i) {
	short sample_freq = sin(frequency * 2.0 * PI * GLOBAL_TIME) * SHRT_MAX;
	    
	BLOCK[(CURRENT_BLOCK * SAMPLES_SZ) + i] = sample_freq * 0.3;
	GLOBAL_TIME += TIME_STEP;
    }
    
    MMRESULT prepare_result = waveOutPrepareHeader(DEVICE, &WAVE_HDR[CURRENT_BLOCK], sizeof(WAVEHDR));

    if (prepare_result != MMSYSERR_NOERROR) {
	error("ERROR: Could not prepare wave header\n");
    }

    MMRESULT write_result = waveOutWrite(DEVICE, &WAVE_HDR[CURRENT_BLOCK], sizeof(WAVEHDR));
    
    if (write_result != MMSYSERR_NOERROR) {
	error("ERROR: Could not send audio to output device\n");
    }

    CURRENT_BLOCK = (CURRENT_BLOCK + 1) % BLOCKS_SZ;
}

#endif // WINDIO_IMPLEMENTATION
