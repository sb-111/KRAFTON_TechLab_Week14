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
    Stop();
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
            FAudioDevice::UpdateSoundPosition(SourceVoice, CurrentLocation);
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
    if (!bIsPlaying)
        return;

    if (SourceVoice)
    {
        FAudioDevice::StopSound(SourceVoice);
        SourceVoice = nullptr;
    }
    bIsPlaying = false;
}

void UAudioComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
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
