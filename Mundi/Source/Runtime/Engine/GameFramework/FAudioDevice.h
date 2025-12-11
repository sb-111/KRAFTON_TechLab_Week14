#pragma once


#include <xaudio2.h>
#include <x3daudio.h>
#include <Vector.h>
#pragma comment(lib, "xaudio2.lib")
class USound;

// Minimal XAudio2 device bootstrap and 3D helpers
class FAudioDevice
{
public:
    static UINT32 GetOutputChannelCount();
    static bool Initialize();
    static void Shutdown();
    static void Update();

    static IXAudio2SourceVoice* PlaySound3D(USound* SoundToPlay, const FVector& EmitterPosition, float Volume = 1.0f, bool bIsLooping = false);
    static IXAudio2SourceVoice* PlaySound2D(USound* SoundToPlay, float Volume = 1.0f, bool bIsLooping = false);
    static void RegisterVoice(IXAudio2SourceVoice* pVoice);
    static void UnregisterVoice(IXAudio2SourceVoice* pVoice);
    static void StopSound(IXAudio2SourceVoice* pSourceVoice);
    static void StopAllSounds();

    static void SetListenerPosition(const FVector& Position, const FVector& ForwardVec, const FVector& UpVec);
    static void UpdateSoundPosition(IXAudio2SourceVoice* pSourceVoice, const FVector& EmitterPosition, float Volume = 1.0f);

    // Loads .wav files under GDataDir/Audio into resource manager
    static void Preload();

    // Master Volume control (affects all sounds)
    static void SetMasterVolume(float Volume);
    static float GetMasterVolume();

    // 최대 동시 재생 음성 수 설정
    static void SetMaxVoices(uint32 MaxVoices);
    static uint32 GetMaxVoices();
    static uint32 GetActiveVoiceCount();

private:
    // 음성 한도 초과 시 가장 오래된 음성 정리
    static void EnforceVoiceLimit();
    // 재생 완료된 음성 정리
    static void CleanupFinishedVoices();

    static IXAudio2*                pXAudio2;
    static IXAudio2MasteringVoice*  pMasteringVoice;
    static X3DAUDIO_HANDLE          X3DInstance;
    static X3DAUDIO_LISTENER        Listener;
    static DWORD                    dwChannelMask;
    static float                    MasterVolume;
    static uint32                   MaxActiveVoices;
    static std::vector<IXAudio2SourceVoice*> ActiveVoices;
};

