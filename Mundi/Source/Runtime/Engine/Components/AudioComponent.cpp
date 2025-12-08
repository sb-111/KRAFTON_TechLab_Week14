// Audio component that plays USound via FAudioDevice

#include "pch.h"
#include "AudioComponent.h"
#include "../GameFramework/FAudioDevice.h"
#include "../Audio/Sound.h"
#include "../../Core/Misc/PathUtils.h"
#include <xaudio2.h>
#include "ResourceManager.h"
#include "LuaBindHelpers.h"

//extern "C" void LuaBind_Anchor_UAudioComponent() {}

// IMPLEMENT_CLASS is now auto-generated in UAudioComponent.generated.cpp

UAudioComponent::UAudioComponent()
    : Volume(1.0f)
    , Pitch(1.0f)
    , bIsLooping(false)
    , bAutoPlay(true)
    , bIs2D(false)
    , bIsPlaying(false)
{
}

UAudioComponent::~UAudioComponent()
{
    if (bIsPlaying && SourceVoice)
    {
        Stop();
    }
}

void UAudioComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoPlay)
    {
        Play();
    }
}

void UAudioComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (bIsPlaying && SourceVoice)
    {
        // 2D 사운드는 공간 업데이트 불필요
        if (!bIs2D)
        {
            FVector CurrentLocation = GetWorldLocation();
            FAudioDevice::UpdateSoundPosition(SourceVoice, CurrentLocation, Volume);
        }

        if (!bIsLooping)
        {
            XAUDIO2_VOICE_STATE state{};
            SourceVoice->GetState(&state);
            if (state.BuffersQueued == 0)
            {
                Stop();
            }
        }
    }
}

void UAudioComponent::EndPlay()
{
    Super::EndPlay();
    Stop();
} 

void UAudioComponent::Play()
{
    // default to first valid slot
    for (uint32 i = 0; i < Sounds.Num(); ++i)
    {
        if (Sounds[i]) { PlaySlot(i); return; }
    }
}

void UAudioComponent::Stop()
{
    // 이미 멈췄거나 보이스가 없으면 리턴
    if (!bIsPlaying || SourceVoice == nullptr)
        return;

    // 1. 먼저 플래그를 꺼서 Tick이나 다른 로직에서의 접근을 차단
    bIsPlaying = false;

    // 2. 멤버 변수를 먼저 nullptr로 만들어서 이 함수 내에서 재진입이나
    //    다른 스레드/함수가 이 포인터에 접근하는 것을 방지 (Swap 방식)
    IXAudio2SourceVoice* VoiceToDestroy = SourceVoice;
    SourceVoice = nullptr;

    // 3. 로컬 포인터를 이용해 안전하게 정리 요청
    if (VoiceToDestroy)
    {
        FAudioDevice::StopSound(VoiceToDestroy);
    }
}

void UAudioComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UAudioComponent::SetVolume(float InVolume)
{
    Volume = InVolume;

    // 재생 중인 사운드가 있으면 즉시 볼륨 적용
    if (bIsPlaying && SourceVoice)
    {
        SourceVoice->SetVolume(Volume);
    }
}


void UAudioComponent::PlaySlot(uint32 SlotIndex)
{
    if (SlotIndex >= (uint32)Sounds.Num()) return;
    USound* Selected = Sounds[SlotIndex];
    if (!Selected) return;
    //if (bIsPlaying) return;

    if (bIs2D)
    {
        // 2D: 공간 처리 없이 양쪽 귀에 동일 볼륨
        SourceVoice = FAudioDevice::PlaySound2D(Selected, Volume, bIsLooping);
    }
    else
    {
        // 3D: 컴포넌트 위치 기반 공간 오디오
        FVector CurrentLocation = GetWorldLocation();
        SourceVoice = FAudioDevice::PlaySound3D(Selected, CurrentLocation, Volume, bIsLooping);
    }

    if (SourceVoice)
    {
        SourceVoice->SetFrequencyRatio(Pitch);
        bIsPlaying = true;
    }
}
