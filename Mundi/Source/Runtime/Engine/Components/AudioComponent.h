#pragma once
#include "SceneComponent.h"
#include "../Audio/Sound.h"
#include "UAudioComponent.generated.h"

struct IXAudio2SourceVoice;

UCLASS(DisplayName="오디오 컴포넌트", Description="사운드를 재생하는 컴포넌트입니다")
class UAudioComponent : public USceneComponent
{
    GENERATED_REFLECTION_BODY()

    UAudioComponent();
    virtual ~UAudioComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;
    virtual void EndPlay() override;

    UFUNCTION(LuaBind, DisplayName="Play", Tooltip="Play the audio")
    void Play();

    UFUNCTION(LuaBind, DisplayName="Stop", Tooltip="Stop the audio")
    void Stop();

    UFUNCTION(LuaBind, DisplayName="PlayOneShot", Tooltip="Play audio from specific slot index")
    void PlaySlot(uint32 SlotIndex);

    // Convenience: set first slot
    UFUNCTION(LuaBind, DisplayName="SetSound", Tooltip="Set sound for the first slot")
    void SetSound(USound* NewSound) { if (Sounds.IsEmpty()) Sounds.Add(NewSound); else Sounds[0] = NewSound; }

    // Looping 설정
    UFUNCTION(LuaBind, DisplayName="SetLooping", Tooltip="Set looping on/off")
    void SetLooping(bool bLoop) { bIsLooping = bLoop; }

    UFUNCTION(LuaBind, DisplayName="GetLooping", Tooltip="Get looping state")
    bool GetLooping() const { return bIsLooping; }

    // AutoPlay 설정
    UFUNCTION(LuaBind, DisplayName="SetAutoPlay", Tooltip="Set auto play on/off")
    void SetAutoPlay(bool bAuto) { bAutoPlay = bAuto; }

    // 재생 상태 확인
    UFUNCTION(LuaBind, DisplayName="IsPlaying", Tooltip="Check if audio is playing")
    bool IsPlaying() const { return bIsPlaying; }

    // Volume 설정
    UFUNCTION(LuaBind, DisplayName="SetVolume", Tooltip="Set volume 0.0 to 1.0")
    void SetVolume(float InVolume) { Volume = InVolume; }

    UFUNCTION(LuaBind, DisplayName="GetVolume", Tooltip="Get volume")
    float GetVolume() const { return Volume; }

    // 2D/3D 설정
    UFUNCTION(LuaBind, DisplayName="SetIs2D", Tooltip="Set 2D mode on/off")
    void SetIs2D(bool bMode) { bIs2D = bMode; }

    UFUNCTION(LuaBind, DisplayName="GetIs2D", Tooltip="Get 2D mode state")
    bool GetIs2D() const { return bIs2D; }

public:
    // Multiple sounds accessible by index
    UPROPERTY(EditAnywhere, Category="Sound", Tooltip="Array of sound assets to play")
    TArray<USound*> Sounds;

    UPROPERTY(EditAnywhere, Category="Audio", Tooltip="Volume (0..1)")
    float Volume;

    UPROPERTY(EditAnywhere, Category="Audio", Tooltip="Pitch (frequency ratio)")
    float Pitch;

    UPROPERTY(EditAnywhere, Category="Audio", Tooltip="Loop playback")
    bool  bIsLooping;

    UPROPERTY(EditAnywhere, Category="Audio", Tooltip="Auto play on BeginPlay")
    bool  bAutoPlay;

    UPROPERTY(EditAnywhere, Category="Audio", Tooltip="Play as 2D sound without spatial positioning")
    bool  bIs2D;

    // Duplication
    virtual void DuplicateSubObjects() override;

protected:
    bool bIsPlaying;
    IXAudio2SourceVoice* SourceVoice = nullptr;
};
