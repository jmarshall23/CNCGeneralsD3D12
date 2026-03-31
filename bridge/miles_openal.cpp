// miles_openal.cpp
//
// Wrapper implementation for a Miles-style header using OpenAL.
// Designed to satisfy linkage and provide practical WAV/PCM playback.
//
// Notes:
// - Supports static WAV/PCM samples and simple whole-file stream loading.
// - Uses OpenAL source/buffer/listener model.
// - Leaves MSS provider/filter/DirectSound-specific features as stubs.
// - Intended as a compatibility wrapper, not a byte-for-byte Miles replacement.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <map>
#include <new>

#include "mss.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <AL/al.h>
#include <AL/alc.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ------------------------------------------------------------
// Internal helpers/state
// ------------------------------------------------------------

static char g_lastError[512] = { 0 };

static void SetLastMilesError(const char* text)
{
  if (!text) {
    g_lastError[0] = '\0';
    return;
  }
#if defined(_MSC_VER)
  strncpy_s(g_lastError, text, _TRUNCATE);
#else
  strncpy(g_lastError, text, sizeof(g_lastError) - 1);
  g_lastError[sizeof(g_lastError) - 1] = '\0';
#endif
}

static const char* ALErrorString(ALenum err)
{
  switch (err) {
  case AL_NO_ERROR:          return "AL_NO_ERROR";
  case AL_INVALID_NAME:      return "AL_INVALID_NAME";
  case AL_INVALID_ENUM:      return "AL_INVALID_ENUM";
  case AL_INVALID_VALUE:     return "AL_INVALID_VALUE";
  case AL_INVALID_OPERATION: return "AL_INVALID_OPERATION";
  case AL_OUT_OF_MEMORY:     return "AL_OUT_OF_MEMORY";
  default:                   return "AL_UNKNOWN_ERROR";
  }
}

static bool CheckALError(const char* where)
{
  ALenum err = alGetError();
  if (err != AL_NO_ERROR) {
    char buf[512];
#if defined(_MSC_VER)
    sprintf_s(buf, "%s failed: %s", where, ALErrorString(err));
#else
    snprintf(buf, sizeof(buf), "%s failed: %s", where, ALErrorString(err));
#endif
    SetLastMilesError(buf);
    return false;
  }
  return true;
}

static ALenum ToALFormat(int channels, int bits)
{
  if (channels == 1 && bits == 8)  return AL_FORMAT_MONO8;
  if (channels == 1 && bits == 16) return AL_FORMAT_MONO16;
  if (channels == 2 && bits == 8)  return AL_FORMAT_STEREO8;
  if (channels == 2 && bits == 16) return AL_FORMAT_STEREO16;
  return 0;
}

static float MilesIntVolumeToFloat(int vol)
{
  if (vol < 0) vol = 0;
  if (vol > 127) vol = 127;
  return (float)vol / 127.0f;
}

static int FloatToMilesVolume(float v)
{
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  return (int)(v * 127.0f + 0.5f);
}

static float MilesPanToALPositionX(int pan)
{
  // Miles pan is typically centered at 64.
  // Map 0..127 to -1..+1
  float t = ((float)pan - 64.0f) / 63.0f;
  if (t < -1.0f) t = -1.0f;
  if (t > 1.0f) t = 1.0f;
  return t;
}

static int ALPositionXToMilesPan(float x)
{
  if (x < -1.0f) x = -1.0f;
  if (x > 1.0f) x = 1.0f;
  int pan = (int)(64.0f + x * 63.0f + 0.5f);
  if (pan < 0) pan = 0;
  if (pan > 127) pan = 127;
  return pan;
}

// ------------------------------------------------------------
// File callbacks
// ------------------------------------------------------------

static AIL_file_open_callback  g_fileOpenCB = NULL;
static AIL_file_close_callback g_fileCloseCB = NULL;
static AIL_file_seek_callback  g_fileSeekCB = NULL;
static AIL_file_read_callback  g_fileReadCB = NULL;

struct MemoryFile
{
  std::vector<unsigned char> data;
};

static bool ReadEntireFileDefault(const char* filename, std::vector<unsigned char>& out)
{
  if (!filename) {
    SetLastMilesError("ReadEntireFileDefault: filename is null");
    return false;
  }

  FILE* f = fopen(filename, "rb");
  if (!f) {
    SetLastMilesError("Failed to open file");
    return false;
  }

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (len <= 0) {
    fclose(f);
    SetLastMilesError("File is empty");
    return false;
  }

  out.resize((size_t)len);
  if (fread(out.data(), 1, (size_t)len, f) != (size_t)len) {
    fclose(f);
    out.clear();
    SetLastMilesError("Failed to read file");
    return false;
  }

  fclose(f);
  return true;
}

static bool ReadEntireFileWithCallbacks(const char* filename, std::vector<unsigned char>& out)
{
  if (!g_fileOpenCB || !g_fileCloseCB || !g_fileSeekCB || !g_fileReadCB) {
    return ReadEntireFileDefault(filename, out);
  }

  void* handle = NULL;
  if (!g_fileOpenCB(filename, &handle) || !handle) {
    SetLastMilesError("AIL file open callback failed");
    return false;
  }

  if (g_fileSeekCB(handle, 0, AIL_FILE_SEEK_END) < 0) {
    g_fileCloseCB(handle);
    SetLastMilesError("AIL file seek end callback failed");
    return false;
  }

  long size = g_fileSeekCB(handle, 0, AIL_FILE_SEEK_CURRENT);
  if (size <= 0) {
    g_fileCloseCB(handle);
    SetLastMilesError("AIL file size callback failed");
    return false;
  }

  if (g_fileSeekCB(handle, 0, AIL_FILE_SEEK_BEGIN) < 0) {
    g_fileCloseCB(handle);
    SetLastMilesError("AIL file seek begin callback failed");
    return false;
  }

  out.resize((size_t)size);
  unsigned long got = g_fileReadCB(handle, out.data(), (unsigned long)size);
  g_fileCloseCB(handle);

  if (got != (unsigned long)size) {
    out.clear();
    SetLastMilesError("AIL file read callback failed");
    return false;
  }

  return true;
}

static bool ReadEntireFile(const char* filename, std::vector<unsigned char>& out)
{
  return ReadEntireFileWithCallbacks(filename, out);
}

// ------------------------------------------------------------
// WAV parsing
// ------------------------------------------------------------

#pragma pack(push, 1)
struct RIFFHeader
{
  char riff[4];
  unsigned int size;
  char wave[4];
};

struct ChunkHeader
{
  char id[4];
  unsigned int size;
};

struct WAVFmtPCM
{
  unsigned short formatTag;
  unsigned short channels;
  unsigned int sampleRate;
  unsigned int avgBytesPerSec;
  unsigned short blockAlign;
  unsigned short bitsPerSample;
};
#pragma pack(pop)

static bool ParseWAVInfo(const void* data, AILSOUNDINFO* info, const unsigned char** pcmPtr)
{
  if (!data || !info) {
    SetLastMilesError("ParseWAVInfo: invalid args");
    return false;
  }

  const unsigned char* bytes = (const unsigned char*)data;
  const RIFFHeader* riff = (const RIFFHeader*)bytes;

  if (memcmp(riff->riff, "RIFF", 4) != 0 || memcmp(riff->wave, "WAVE", 4) != 0) {
    SetLastMilesError("Not a RIFF/WAVE file");
    return false;
  }

  const unsigned char* p = bytes + sizeof(RIFFHeader);
  const unsigned char* end = bytes + 8 + riff->size;

  const WAVFmtPCM* fmt = NULL;
  const unsigned char* sampleData = NULL;
  unsigned int sampleDataSize = 0;

  while (p + sizeof(ChunkHeader) <= end) {
    const ChunkHeader* ch = (const ChunkHeader*)p;
    const unsigned char* cdata = p + sizeof(ChunkHeader);

    if (memcmp(ch->id, "fmt ", 4) == 0) {
      if (ch->size < sizeof(WAVFmtPCM)) {
        SetLastMilesError("Invalid WAV fmt chunk");
        return false;
      }
      fmt = (const WAVFmtPCM*)cdata;
    }
    else if (memcmp(ch->id, "data", 4) == 0) {
      sampleData = cdata;
      sampleDataSize = ch->size;
    }

    unsigned int adv = sizeof(ChunkHeader) + ch->size;
    if (ch->size & 1) adv++;
    p += adv;
  }

  if (!fmt || !sampleData) {
    SetLastMilesError("Missing WAV fmt/data chunk");
    return false;
  }

  if (fmt->formatTag != WAVE_FORMAT_PCM) {
    SetLastMilesError("Only PCM WAV is supported in this wrapper");
    return false;
  }

  memset(info, 0, sizeof(*info));
  info->format = fmt->formatTag;
  info->data_ptr = sampleData;
  info->data_len = sampleDataSize;
  info->rate = fmt->sampleRate;
  info->bits = fmt->bitsPerSample;
  info->channels = fmt->channels;
  info->block_size = fmt->blockAlign;
  info->samples = (fmt->blockAlign != 0) ? (sampleDataSize / fmt->blockAlign) : 0;
  info->initial_ptr = sampleData;

  if (pcmPtr) {
    *pcmPtr = sampleData;
  }

  return true;
}

// ------------------------------------------------------------
// OpenAL object wrappers
// ------------------------------------------------------------

struct MilesOpenALDriver
{
  _DIG_DRIVER config;
  ALCdevice* device;
  ALCcontext* context;

  MilesOpenALDriver()
    : device(NULL), context(NULL)
  {
  }
};

struct MilesOpenALBaseObject
{
  ALuint source;
  ALuint buffer;

  float gain;
  float pitch;
  float posX, posY, posZ;
  float velX, velY, velZ;
  float faceX, faceY, faceZ;
  float upX, upY, upZ;
  float minDist;
  float maxDist;
  float effectsLevel;
  float occlusion;
  int loopCount;
  unsigned int userData[8];
  bool is3D;

  std::vector<unsigned char> fileImage;

  MilesOpenALBaseObject()
    : source(0)
    , buffer(0)
    , gain(1.0f)
    , pitch(1.0f)
    , posX(0.0f), posY(0.0f), posZ(0.0f)
    , velX(0.0f), velY(0.0f), velZ(0.0f)
    , faceX(0.0f), faceY(0.0f), faceZ(-1.0f)
    , upX(0.0f), upY(1.0f), upZ(0.0f)
    , minDist(1.0f)
    , maxDist(1000000.0f)
    , effectsLevel(0.0f)
    , occlusion(0.0f)
    , loopCount(1)
    , is3D(false)
  {
    memset(userData, 0, sizeof(userData));
  }

  void ApplyCommon()
  {
    alSourcef(source, AL_GAIN, gain);
    alSourcef(source, AL_PITCH, pitch);
    alSource3f(source, AL_POSITION, posX, posY, posZ);
    alSource3f(source, AL_VELOCITY, velX, velY, velZ);
    alSourcef(source, AL_REFERENCE_DISTANCE, minDist);
    alSourcef(source, AL_MAX_DISTANCE, maxDist);
    alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
  }
};

struct MilesOpenALSample : public MilesOpenALBaseObject
{
  MilesOpenALSample()
  {
    is3D = false;
  }
};

struct MilesOpenAL3DSample : public MilesOpenALBaseObject
{
  AIL_3dsample_callback eosCallback;

  MilesOpenAL3DSample()
    : eosCallback(NULL)
  {
    is3D = true;
  }
};

struct MilesOpenALStream
{
  ALuint source;
  ALuint buffer;
  float gain;
  float pitch;
  int loopCount;
  int loopStart;
  int loopEnd;
  AIL_stream_callback callback;
  std::vector<unsigned char> fileData;
  AILSOUNDINFO info;

  MilesOpenALStream()
    : source(0)
    , buffer(0)
    , gain(1.0f)
    , pitch(1.0f)
    , loopCount(1)
    , loopStart(0)
    , loopEnd(0)
    , callback(NULL)
  {
    memset(&info, 0, sizeof(info));
  }
};

struct MilesOpenALProvider
{
  int speakerType;
  MilesOpenALProvider() : speakerType(AIL_3D_2_SPEAKER) {}
};

struct MilesOpenALListener
{
  float pos[3];
  float face[3];
  float up[3];
  unsigned int userData[8];

  MilesOpenALListener()
  {
    pos[0] = pos[1] = pos[2] = 0.0f;
    face[0] = 0.0f; face[1] = 0.0f; face[2] = -1.0f;
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    memset(userData, 0, sizeof(userData));
  }

  void Apply()
  {
    alListener3f(AL_POSITION, pos[0], pos[1], pos[2]);
    float ori[6] = { face[0], face[1], face[2], up[0], up[1], up[2] };
    alListenerfv(AL_ORIENTATION, ori);
  }
};

static MilesOpenALDriver* g_driver = NULL;
static MilesOpenALProvider* g_provider = NULL;
static MilesOpenALListener* g_listener = NULL;
static int g_lockDepth = 0;

// ------------------------------------------------------------
// Object creation/destruction
// ------------------------------------------------------------

static bool CreateSourceAndBuffer(ALuint* source, ALuint* buffer)
{
  if (!source || !buffer) return false;
  alGenSources(1, source);
  if (!CheckALError("alGenSources")) return false;
  alGenBuffers(1, buffer);
  if (!CheckALError("alGenBuffers")) {
    alDeleteSources(1, source);
    *source = 0;
    return false;
  }
  return true;
}

static void DestroySourceAndBuffer(ALuint* source, ALuint* buffer)
{
  if (source && *source) {
    alSourceStop(*source);
    alSourcei(*source, AL_BUFFER, 0);
    alDeleteSources(1, source);
    *source = 0;
  }
  if (buffer && *buffer) {
    alDeleteBuffers(1, buffer);
    *buffer = 0;
  }
}

// ------------------------------------------------------------
// Exported Miles-style API
// ------------------------------------------------------------

extern "C" {

  int __stdcall AIL_startup(void)
  {
    SetLastMilesError("");
    return 1;
  }

  void __stdcall AIL_shutdown(void)
  {
    if (g_listener) {
      delete g_listener;
      g_listener = NULL;
    }

    if (g_provider) {
      delete g_provider;
      g_provider = NULL;
    }

    if (g_driver) {
      if (g_driver->context) {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(g_driver->context);
        g_driver->context = NULL;
      }
      if (g_driver->device) {
        alcCloseDevice(g_driver->device);
        g_driver->device = NULL;
      }
      delete g_driver;
      g_driver = NULL;
    }

    SetLastMilesError("");
  }

  int __stdcall AIL_quick_startup(
    int use_digital, int /*use_MIDI*/, unsigned int output_rate, int output_bits, int output_channels)
  {
    if (!use_digital) {
      SetLastMilesError("AIL_quick_startup: digital audio disabled");
      return 0;
    }

    if (g_driver) {
      return 1;
    }

    g_driver = new(std::nothrow) MilesOpenALDriver();
    if (!g_driver) {
      SetLastMilesError("Out of memory");
      return 0;
    }

    g_driver->device = alcOpenDevice(NULL);
    if (!g_driver->device) {
      delete g_driver;
      g_driver = NULL;
      SetLastMilesError("alcOpenDevice failed");
      return 0;
    }

    const ALCint attrs[] = {
        ALC_FREQUENCY, (ALCint)output_rate,
        0
    };

    g_driver->context = alcCreateContext(g_driver->device, attrs);
    if (!g_driver->context) {
      alcCloseDevice(g_driver->device);
      delete g_driver;
      g_driver = NULL;
      SetLastMilesError("alcCreateContext failed");
      return 0;
    }

    if (!alcMakeContextCurrent(g_driver->context)) {
      alcDestroyContext(g_driver->context);
      alcCloseDevice(g_driver->device);
      delete g_driver;
      g_driver = NULL;
      SetLastMilesError("alcMakeContextCurrent failed");
      return 0;
    }

    g_driver->config.outputRate = (int)output_rate;
    g_driver->config.outputBits = output_bits;
    g_driver->config.outputChannels = output_channels;

    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    CheckALError("alDistanceModel");

    if (!g_provider) g_provider = new(std::nothrow) MilesOpenALProvider();
    if (!g_listener) {
      g_listener = new(std::nothrow) MilesOpenALListener();
      if (g_listener) g_listener->Apply();
    }

    return 1;
  }

  void __stdcall AIL_quick_handles(HDIGDRIVER* pdig, HMDIDRIVER* pmdi, HDLSDEVICE* pdls)
  {
    if (pdig) *pdig = (HDIGDRIVER)g_driver;
    if (pmdi) *pmdi = NULL;
    if (pdls) *pdls = NULL;
  }

  int __stdcall AIL_waveOutOpen(HDIGDRIVER* driver, LPHWAVEOUT* waveout, int id, LPWAVEFORMAT format)
  {
    unsigned int rate = 44100;
    int bits = 16;
    int channels = 2;

    if (format) {
      rate = format->nSamplesPerSec;
      channels = format->nChannels;

      if (format->nChannels > 0) {
        unsigned int bytesPerSample = format->nBlockAlign / format->nChannels;
        if (bytesPerSample > 0) {
          bits = (int)(bytesPerSample * 8);
        }
      }
    }

    if (!AIL_quick_startup(1, 0, rate, bits, channels)) {
      return 0;
    }

    if (g_driver) {
      g_driver->config.deviceId = id;
      g_driver->config.outputRate = (int)rate;
      g_driver->config.outputBits = bits;
      g_driver->config.outputChannels = channels;
    }

    if (driver)  *driver = (HDIGDRIVER)g_driver;
    if (waveout) *waveout = NULL;
    return 1;
  }

  void __stdcall AIL_waveOutClose(HDIGDRIVER /*driver*/)
  {
    // Keep device alive until AIL_shutdown, like a process-wide wrapper.
  }

  HSAMPLE __stdcall AIL_allocate_sample_handle(HDIGDRIVER /*dig*/)
  {
    MilesOpenALSample* s = new(std::nothrow) MilesOpenALSample();
    if (!s) return NULL;

    if (!CreateSourceAndBuffer(&s->source, &s->buffer)) {
      delete s;
      return NULL;
    }

    s->ApplyCommon();
    return (HSAMPLE)s;
  }

  void __stdcall AIL_release_sample_handle(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;
    DestroySourceAndBuffer(&s->source, &s->buffer);
    delete s;
  }

  void __stdcall AIL_init_sample(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;

    alSourceStop(s->source);
    alSourcei(s->source, AL_BUFFER, 0);

    s->gain = 1.0f;
    s->pitch = 1.0f;
    s->posX = s->posY = s->posZ = 0.0f;
    s->velX = s->velY = s->velZ = 0.0f;
    s->loopCount = 1;
    s->fileImage.clear();

    s->ApplyCommon();
  }

  int __stdcall AIL_WAV_info(const void* data, AILSOUNDINFO* info)
  {
    return ParseWAVInfo(data, info, NULL) ? 1 : 0;
  }

  int __stdcall AIL_set_sample_file(HSAMPLE sample, const void* file_image, int /*block*/)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s || !file_image) return 0;

    AILSOUNDINFO info;
    const unsigned char* pcm = NULL;
    if (!ParseWAVInfo(file_image, &info, &pcm)) {
      return 0;
    }

    ALenum fmt = ToALFormat(info.channels, info.bits);
    if (!fmt) {
      SetLastMilesError("Unsupported PCM format");
      return 0;
    }

    alBufferData(s->buffer, fmt, info.data_ptr, (ALsizei)info.data_len, (ALsizei)info.rate);
    if (!CheckALError("alBufferData(sample)")) {
      return 0;
    }

    alSourcei(s->source, AL_BUFFER, (ALint)s->buffer);
    if (!CheckALError("alSourcei(AL_BUFFER)")) {
      return 0;
    }

    s->ApplyCommon();
    return 1;
  }

  int __stdcall AIL_set_named_sample_file(
    HSAMPLE sample, const char* /*file_name*/, const void* file_image, int /*file_size*/, int block)
  {
    return AIL_set_sample_file(sample, file_image, block);
  }

  void __stdcall AIL_start_sample(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;

    alSourcei(s->source, AL_LOOPING, (s->loopCount == 0 || s->loopCount > 1) ? AL_TRUE : AL_FALSE);
    alSourcePlay(s->source);
    CheckALError("alSourcePlay(sample)");
  }

  void __stdcall AIL_stop_sample(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;
    alSourcePause(s->source);
  }

  void __stdcall AIL_resume_sample(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;
    alSourcePlay(s->source);
  }

  void __stdcall AIL_end_sample(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;
    alSourceStop(s->source);
    alSourceRewind(s->source);
  }

  int __stdcall AIL_sample_volume(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    return s ? FloatToMilesVolume(s->gain) : 0;
  }

  void __stdcall AIL_set_sample_volume(HSAMPLE sample, int volume)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;
    s->gain = MilesIntVolumeToFloat(volume);
    alSourcef(s->source, AL_GAIN, s->gain);
  }

  int __stdcall AIL_sample_pan(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    return s ? ALPositionXToMilesPan(s->posX) : 64;
  }

  void __stdcall AIL_set_sample_pan(HSAMPLE sample, int pan)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;
    s->posX = MilesPanToALPositionX(pan);
    s->posY = 0.0f;
    s->posZ = -1.0f;
    alSourcei(s->source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(s->source, AL_POSITION, s->posX, s->posY, s->posZ);
  }

  void __stdcall AIL_sample_volume_pan(HSAMPLE sample, float* volume, float* pan)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) {
      if (volume) *volume = 0.0f;
      if (pan) *pan = 0.0f;
      return;
    }

    if (volume) *volume = s->gain;
    if (pan) *pan = s->posX;
  }

  void __stdcall AIL_set_sample_volume_pan(HSAMPLE sample, float volume, float pan)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;

    s->gain = volume;
    s->posX = pan;
    s->posY = 0.0f;
    s->posZ = -1.0f;

    alSourcef(s->source, AL_GAIN, s->gain);
    alSourcei(s->source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(s->source, AL_POSITION, s->posX, s->posY, s->posZ);
  }

  int __stdcall AIL_sample_playback_rate(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s || g_driver == NULL) return 0;
    return (int)(g_driver->config.outputRate * s->pitch + 0.5f);
  }

  void __stdcall AIL_set_sample_playback_rate(HSAMPLE sample, int playback_rate)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s || g_driver == NULL || g_driver->config.outputRate <= 0) return;
    s->pitch = (float)playback_rate / (float)g_driver->config.outputRate;
    if (s->pitch < 0.01f) s->pitch = 0.01f;
    alSourcef(s->source, AL_PITCH, s->pitch);
  }

  int __stdcall AIL_sample_loop_count(HSAMPLE sample)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    return s ? s->loopCount : 0;
  }

  void __stdcall AIL_set_sample_loop_count(HSAMPLE sample, int count)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;
    s->loopCount = count;
    alSourcei(s->source, AL_LOOPING, (count == 0 || count > 1) ? AL_TRUE : AL_FALSE);
  }

  void __stdcall AIL_sample_ms_position(HSAMPLE sample, long* total_ms, long* current_ms)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) {
      if (total_ms) *total_ms = 0;
      if (current_ms) *current_ms = 0;
      return;
    }

    ALint size = 0, bits = 16, channels = 2, freq = 44100, offset = 0;
    alGetBufferi(s->buffer, AL_SIZE, &size);
    alGetBufferi(s->buffer, AL_BITS, &bits);
    alGetBufferi(s->buffer, AL_CHANNELS, &channels);
    alGetBufferi(s->buffer, AL_FREQUENCY, &freq);
    alGetSourcei(s->source, AL_SAMPLE_OFFSET, &offset);

    int blockAlign = (channels * bits) / 8;
    long total = 0;
    long current = 0;

    if (blockAlign > 0 && freq > 0) {
      total = (long)((1000.0 * (double)(size / blockAlign)) / (double)freq);
      current = (long)((1000.0 * (double)offset) / (double)freq);
    }

    if (total_ms) *total_ms = total;
    if (current_ms) *current_ms = current;
  }

  void __stdcall AIL_set_sample_ms_position(HSAMPLE sample, int pos)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s) return;

    ALint freq = 44100;
    alGetBufferi(s->buffer, AL_FREQUENCY, &freq);

    int sampleOffset = (int)(((double)pos * (double)freq) / 1000.0);
    alSourcei(s->source, AL_SAMPLE_OFFSET, sampleOffset);
  }

  void __stdcall AIL_set_sample_user_data(HSAMPLE sample, unsigned int index, void* value)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s || index >= 8) return;
    s->userData[index] = (unsigned int)(uintptr_t)value;
  }

  void* __stdcall AIL_sample_user_data(HSAMPLE sample, unsigned int index)
  {
    MilesOpenALSample* s = (MilesOpenALSample*)sample;
    if (!s || index >= 8) return NULL;
    return (void*)(uintptr_t)s->userData[index];
  }

  H3DSAMPLE __stdcall AIL_allocate_3D_sample_handle(HPROVIDER /*lib*/)
  {
    MilesOpenAL3DSample* s = new(std::nothrow) MilesOpenAL3DSample();
    if (!s) return NULL;

    if (!CreateSourceAndBuffer(&s->source, &s->buffer)) {
      delete s;
      return NULL;
    }

    s->ApplyCommon();
    return (H3DSAMPLE)s;
  }

  void __stdcall AIL_release_3D_sample_handle(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    DestroySourceAndBuffer(&s->source, &s->buffer);
    delete s;
  }

  int __stdcall AIL_set_3D_sample_file(H3DSAMPLE sample, const void* file_image)
  {
    return AIL_set_sample_file((HSAMPLE)sample, file_image, 0);
  }

#ifdef MILES_NOFLOAT
  int __stdcall AIL_3D_sample_volume(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    return s ? FloatToMilesVolume(s->gain) : 0;
  }

  void __stdcall AIL_set_3D_sample_volume(H3DSAMPLE sample, int volume)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    s->gain = MilesIntVolumeToFloat(volume);
    alSourcef(s->source, AL_GAIN, s->gain);
  }
#else
  float __stdcall AIL_3D_sample_volume(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    return s ? s->gain : 0.0f;
  }

  void __stdcall AIL_set_3D_sample_volume(H3DSAMPLE sample, float volume)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    s->gain = volume;
    alSourcef(s->source, AL_GAIN, s->gain);
  }
#endif

  void __stdcall AIL_start_3D_sample(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    alSourcei(s->source, AL_LOOPING, (s->loopCount == 0 || s->loopCount > 1) ? AL_TRUE : AL_FALSE);
    alSourcePlay(s->source);
  }

  void __stdcall AIL_stop_3D_sample(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    alSourcePause(s->source);
  }

  void __stdcall AIL_resume_3D_sample(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    alSourcePlay(s->source);
  }

  void __stdcall AIL_end_3D_sample(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    alSourceStop(s->source);
    alSourceRewind(s->source);
  }

  unsigned int __stdcall AIL_3D_sample_loop_count(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    return s ? (unsigned int)s->loopCount : 0;
  }

  void __stdcall AIL_set_3D_sample_loop_count(H3DSAMPLE sample, unsigned int count)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    s->loopCount = (int)count;
    alSourcei(s->source, AL_LOOPING, (count == 0 || count > 1) ? AL_TRUE : AL_FALSE);
  }

  int __stdcall AIL_3D_sample_length(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return 0;
    ALint size = 0;
    alGetBufferi(s->buffer, AL_SIZE, &size);
    return size;
  }

  unsigned int __stdcall AIL_3D_sample_offset(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return 0;
    ALint offset = 0;
    alGetSourcei(s->source, AL_BYTE_OFFSET, &offset);
    return (unsigned int)offset;
  }

  void __stdcall AIL_set_3D_sample_offset(H3DSAMPLE sample, unsigned int offset)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    alSourcei(s->source, AL_BYTE_OFFSET, (ALint)offset);
  }

  int __stdcall AIL_3D_sample_playback_rate(H3DSAMPLE sample)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s || !g_driver) return 0;
    return (int)(g_driver->config.outputRate * s->pitch + 0.5f);
  }

  void __stdcall AIL_set_3D_sample_playback_rate(H3DSAMPLE sample, int playback_rate)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s || !g_driver || g_driver->config.outputRate <= 0) return;
    s->pitch = (float)playback_rate / (float)g_driver->config.outputRate;
    if (s->pitch < 0.01f) s->pitch = 0.01f;
    alSourcef(s->source, AL_PITCH, s->pitch);
  }

  void __stdcall AIL_set_3D_sample_distances(H3DSAMPLE sample, float max_dist, float min_dist)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    s->maxDist = max_dist;
    s->minDist = min_dist;
    alSourcef(s->source, AL_MAX_DISTANCE, s->maxDist);
    alSourcef(s->source, AL_REFERENCE_DISTANCE, s->minDist);
  }

  void __stdcall AIL_set_3D_sample_effects_level(H3DSAMPLE sample, float effect_level)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    s->effectsLevel = effect_level;
    // Optional EFX send would go here.
  }

  void __stdcall AIL_set_3D_sample_occlusion(H3DSAMPLE sample, float occlusion)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    s->occlusion = occlusion;
    // Could be mapped to a low-pass filter if EFX is enabled.
  }

  void __stdcall AIL_set_3D_velocity_vector(H3DSAMPLE sample, float x, float y, float z)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return;
    s->velX = x; s->velY = y; s->velZ = z;
    alSource3f(s->source, AL_VELOCITY, x, y, z);
  }

  void __stdcall AIL_set_3D_position(H3DPOBJECT obj, float X, float Y, float Z)
  {
    if (!obj) return;

    if ((void*)obj == (void*)g_listener && g_listener) {
      g_listener->pos[0] = X;
      g_listener->pos[1] = Y;
      g_listener->pos[2] = Z;
      g_listener->Apply();
      return;
    }

    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)obj;
    s->posX = X; s->posY = Y; s->posZ = Z;
    alSource3f(s->source, AL_POSITION, X, Y, Z);
  }

  void __stdcall AIL_set_3D_orientation(
    H3DPOBJECT obj, float X_face, float Y_face, float Z_face, float X_up, float Y_up, float Z_up)
  {
    if (!obj) return;

    if ((void*)obj == (void*)g_listener && g_listener) {
      g_listener->face[0] = X_face;
      g_listener->face[1] = Y_face;
      g_listener->face[2] = Z_face;
      g_listener->up[0] = X_up;
      g_listener->up[1] = Y_up;
      g_listener->up[2] = Z_up;
      g_listener->Apply();
      return;
    }

    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)obj;
    s->faceX = X_face; s->faceY = Y_face; s->faceZ = Z_face;
    s->upX = X_up;   s->upY = Y_up;   s->upZ = Z_up;
  }

  void __stdcall AIL_set_3D_user_data(H3DPOBJECT obj, unsigned int index, void* value)
  {
    if (!obj || index >= 8) return;

    if ((void*)obj == (void*)g_listener && g_listener) {
      g_listener->userData[index] = (unsigned int)(uintptr_t)value;
      return;
    }

    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)obj;
    s->userData[index] = (unsigned int)(uintptr_t)value;
  }

  void* __stdcall AIL_3D_user_data(H3DSAMPLE sample, unsigned int index)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s || index >= 8) return NULL;
    return (void*)(uintptr_t)s->userData[index];
  }

  AIL_3dsample_callback __stdcall AIL_register_3D_EOS_callback(H3DSAMPLE sample, AIL_3dsample_callback EOS)
  {
    MilesOpenAL3DSample* s = (MilesOpenAL3DSample*)sample;
    if (!s) return NULL;
    AIL_3dsample_callback old = s->eosCallback;
    s->eosCallback = EOS;
    return old;
  }

  int __stdcall AIL_enumerate_3D_providers(HPROENUM* next, HPROVIDER* dest, char** name)
  {
    static char providerName[] = "OpenAL 3D Provider";

    if (!next || !dest || !name) return 0;
    if (*next != HPROENUM_FIRST) return 0;

    if (!g_provider) g_provider = new(std::nothrow) MilesOpenALProvider();

    *dest = (HPROVIDER)g_provider;
    *name = providerName;
    *next = 1;
    return 1;
  }

  M3DRESULT __stdcall AIL_open_3D_provider(HPROVIDER lib)
  {
    if (!lib) return -1;
    if (!g_provider) g_provider = (MilesOpenALProvider*)lib;
    return M3D_NOERR;
  }

  void __stdcall AIL_close_3D_provider(HPROVIDER /*lib*/)
  {
  }

  H3DPOBJECT __stdcall AIL_open_3D_listener(HPROVIDER /*lib*/)
  {
    if (!g_listener) {
      g_listener = new(std::nothrow) MilesOpenALListener();
      if (!g_listener) return NULL;
    }
    g_listener->Apply();
    return (H3DPOBJECT)g_listener;
  }

  void __stdcall AIL_close_3D_listener(H3DPOBJECT listener)
  {
    if ((void*)listener == (void*)g_listener) {
      delete g_listener;
      g_listener = NULL;
    }
  }

  void __stdcall AIL_set_3D_speaker_type(HPROVIDER lib, int speaker_type)
  {
    MilesOpenALProvider* p = (MilesOpenALProvider*)lib;
    if (!p) return;
    p->speakerType = speaker_type;
  }

  HSTREAM __stdcall AIL_open_stream(HDIGDRIVER /*dig*/, const char* filename, int /*stream_mem*/)
  {
    if (!filename) return NULL;

    MilesOpenALStream* st = new(std::nothrow) MilesOpenALStream();
    if (!st) return NULL;

    if (!CreateSourceAndBuffer(&st->source, &st->buffer)) {
      delete st;
      return NULL;
    }

    if (!ReadEntireFile(filename, st->fileData)) {
      DestroySourceAndBuffer(&st->source, &st->buffer);
      delete st;
      return NULL;
    }

    const unsigned char* pcm = NULL;
    if (!ParseWAVInfo(st->fileData.data(), &st->info, &pcm)) {
      DestroySourceAndBuffer(&st->source, &st->buffer);
      delete st;
      return NULL;
    }

    ALenum fmt = ToALFormat(st->info.channels, st->info.bits);
    if (!fmt) {
      DestroySourceAndBuffer(&st->source, &st->buffer);
      delete st;
      SetLastMilesError("Unsupported stream PCM format");
      return NULL;
    }

    alBufferData(st->buffer, fmt, st->info.data_ptr, (ALsizei)st->info.data_len, (ALsizei)st->info.rate);
    if (!CheckALError("alBufferData(stream)")) {
      DestroySourceAndBuffer(&st->source, &st->buffer);
      delete st;
      return NULL;
    }

    alSourcei(st->source, AL_BUFFER, (ALint)st->buffer);
    return (HSTREAM)st;
  }

  HSTREAM __stdcall AIL_open_stream_by_sample(HDIGDRIVER driver, HSAMPLE /*sample*/, const char* file_name, int mem)
  {
    return AIL_open_stream(driver, file_name, mem);
  }

  void __stdcall AIL_close_stream(HSTREAM stream)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return;
    DestroySourceAndBuffer(&st->source, &st->buffer);
    delete st;
  }

  void __stdcall AIL_start_stream(HSTREAM stream)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return;
    alSourcei(st->source, AL_LOOPING, (st->loopCount == 0 || st->loopCount > 1) ? AL_TRUE : AL_FALSE);
    alSourcePlay(st->source);
  }

  void __stdcall AIL_pause_stream(HSTREAM stream, int onoff)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return;
    if (onoff) alSourcePause(st->source);
    else alSourcePlay(st->source);
  }

  int __stdcall AIL_stream_volume(HSTREAM stream)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    return st ? FloatToMilesVolume(st->gain) : 0;
  }

  void __stdcall AIL_set_stream_volume(HSTREAM stream, int volume)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return;
    st->gain = MilesIntVolumeToFloat(volume);
    alSourcef(st->source, AL_GAIN, st->gain);
  }

  int __stdcall AIL_stream_pan(HSTREAM /*stream*/)
  {
    return 64;
  }

  void __stdcall AIL_set_stream_pan(HSTREAM /*stream*/, int /*pan*/)
  {
    // Not mapped in this simple wrapper.
  }

  void __stdcall AIL_set_stream_volume_pan(HSTREAM stream, float volume, float /*pan*/)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return;
    st->gain = volume;
    alSourcef(st->source, AL_GAIN, st->gain);
  }

  void __stdcall AIL_stream_volume_pan(HSTREAM stream, float* volume, float* pan)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (volume) *volume = st ? st->gain : 0.0f;
    if (pan) *pan = 0.0f;
  }

  int __stdcall AIL_stream_playback_rate(HSTREAM stream)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st || !g_driver) return 0;
    return (int)(g_driver->config.outputRate * st->pitch + 0.5f);
  }

  void __stdcall AIL_set_stream_playback_rate(HSTREAM stream, int rate)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st || !g_driver || g_driver->config.outputRate <= 0) return;
    st->pitch = (float)rate / (float)g_driver->config.outputRate;
    if (st->pitch < 0.01f) st->pitch = 0.01f;
    alSourcef(st->source, AL_PITCH, st->pitch);
  }

  int __stdcall AIL_stream_loop_count(HSTREAM stream)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    return st ? st->loopCount : 0;
  }

  void __stdcall AIL_set_stream_loop_count(HSTREAM stream, int count)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return;
    st->loopCount = count;
    alSourcei(st->source, AL_LOOPING, (count == 0 || count > 1) ? AL_TRUE : AL_FALSE);
  }

  void __stdcall AIL_set_stream_loop_block(HSTREAM stream, int loop_start, int loop_end)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return;
    st->loopStart = loop_start;
    st->loopEnd = loop_end;
    // Whole-file wrapper does not implement partial loop regions yet.
  }

  void __stdcall AIL_stream_ms_position(HSTREAM stream, S32* total_milliseconds, S32* current_milliseconds)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) {
      if (total_milliseconds) *total_milliseconds = 0;
      if (current_milliseconds) *current_milliseconds = 0;
      return;
    }

    ALint size = 0, bits = 16, channels = 2, freq = 44100, offset = 0;
    alGetBufferi(st->buffer, AL_SIZE, &size);
    alGetBufferi(st->buffer, AL_BITS, &bits);
    alGetBufferi(st->buffer, AL_CHANNELS, &channels);
    alGetBufferi(st->buffer, AL_FREQUENCY, &freq);
    alGetSourcei(st->source, AL_SAMPLE_OFFSET, &offset);

    int blockAlign = (channels * bits) / 8;
    S32 total = 0;
    S32 current = 0;

    if (blockAlign > 0 && freq > 0) {
      total = (S32)((1000.0 * (double)(size / blockAlign)) / (double)freq);
      current = (S32)((1000.0 * (double)offset) / (double)freq);
    }

    if (total_milliseconds) *total_milliseconds = total;
    if (current_milliseconds) *current_milliseconds = current;
  }

  void __stdcall AIL_set_stream_ms_position(HSTREAM stream, int pos)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return;

    ALint freq = 44100;
    alGetBufferi(st->buffer, AL_FREQUENCY, &freq);

    int sampleOffset = (int)(((double)pos * (double)freq) / 1000.0);
    alSourcei(st->source, AL_SAMPLE_OFFSET, sampleOffset);
  }

  AIL_stream_callback __stdcall AIL_register_stream_callback(HSTREAM stream, AIL_stream_callback callback)
  {
    MilesOpenALStream* st = (MilesOpenALStream*)stream;
    if (!st) return NULL;
    AIL_stream_callback old = st->callback;
    st->callback = callback;
    return old;
  }

  AIL_sample_callback __stdcall AIL_register_EOS_callback(HSAMPLE /*sample*/, AIL_sample_callback EOS)
  {
    // You can extend the sample struct to store this if the game uses it.
    return EOS;
  }

  void __stdcall AIL_set_file_callbacks(
    AIL_file_open_callback opencb, AIL_file_close_callback closecb,
    AIL_file_seek_callback seekcb, AIL_file_read_callback readcb)
  {
    g_fileOpenCB = opencb;
    g_fileCloseCB = closecb;
    g_fileSeekCB = seekcb;
    g_fileReadCB = readcb;
  }

  int __stdcall AIL_set_preference(unsigned int /*number*/, int value)
  {
    return value;
  }

  int __stdcall AIL_enumerate_filters(HPROENUM* next, HPROVIDER* dest, char** name)
  {
    static char filterName[] = "OpenAL Stub Filter";
    if (!next || !dest || !name) return 0;
    if (*next != HPROENUM_FIRST) return 0;
    *dest = NULL;
    *name = filterName;
    *next = 1;
    return 1;
  }

  HPROVIDER __stdcall AIL_set_sample_processor(HSAMPLE /*sample*/, SAMPLESTAGE /*pipeline_stage*/, HPROVIDER provider)
  {
    return provider;
  }

  void __stdcall AIL_set_filter_sample_preference(HSAMPLE /*sample*/, const char* /*name*/, const void* /*val*/)
  {
  }

  char* __stdcall AIL_last_error(void)
  {
    return g_lastError;
  }

  void __stdcall AIL_lock(void)
  {
    ++g_lockDepth;
  }

  void __stdcall AIL_unlock(void)
  {
    if (g_lockDepth > 0) --g_lockDepth;
  }

  void __stdcall AIL_stop_timer(HTIMER /*timer*/)
  {
  }

  void __stdcall AIL_release_timer_handle(HTIMER /*timer*/)
  {
  }

  unsigned long __stdcall AIL_get_timer_highest_delay(void)
  {
    return 1000;
  }

  int __stdcall AIL_decompress_ADPCM(const AILSOUNDINFO* /*info*/, void** outdata, unsigned long* outsize)
  {
    if (outdata) *outdata = NULL;
    if (outsize) *outsize = 0;
    SetLastMilesError("ADPCM decompression is not implemented in this wrapper");
    return 0;
  }

  void __stdcall AIL_get_DirectSound_info(
    HSAMPLE /*sample*/, AILLPDIRECTSOUND* lplpDS, AILLPDIRECTSOUNDBUFFER* lplpDSB)
  {
    if (lplpDS)  *lplpDS = NULL;
    if (lplpDSB) *lplpDSB = NULL;
  }

  void __stdcall AIL_mem_free_lock(void* ptr)
  {
    free(ptr);
  }

  HAUDIO __stdcall AIL_quick_load_and_play(const char* filename, unsigned int loop_count, int /*wait_request*/)
  {
    HSTREAM st = AIL_open_stream((HDIGDRIVER)g_driver, filename, 0);
    if (!st) return NULL;
    AIL_set_stream_loop_count(st, (int)loop_count);
    AIL_start_stream(st);
    return (HAUDIO)st;
  }

  void __stdcall AIL_quick_unload(HAUDIO audio)
  {
    AIL_close_stream((HSTREAM)audio);
  }

  void __stdcall AIL_quick_set_volume(HAUDIO audio, float volume, float /*extravol*/)
  {
    AIL_set_stream_volume_pan((HSTREAM)audio, volume, 0.0f);
  }

  char* __stdcall AIL_set_redist_directory(const char* dir)
  {
    return (char*)dir;
  }

  int MSS_auto_cleanup(void)
  {
    return 1;
  }

} // extern "C"
