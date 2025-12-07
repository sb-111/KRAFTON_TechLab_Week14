#pragma once
#include "ParticleModule.h"
#include "Distribution.h"
#include "UParticleModuleVelocityCone.generated.h"

UCLASS(DisplayName="속도 (Cone)", Description="지정된 축을 기준으로 원뿔(Cone) 범위 내에서 랜덤하게 속도를 부여하는 모듈입니다.")
class UParticleModuleVelocityCone : public UParticleModule
{
public:
    GENERATED_REFLECTION_BODY()
    
public:
    UParticleModuleVelocityCone();

    // 속도 크기 (Distribution)
    // 설명: 파티클이 얼마나 빠르게 날아갈지 결정
    UPROPERTY(EditAnywhere, Category="Velocity", meta=(ToolTip="파티클 생성 시 초기 속력(Speed)입니다."))
    FDistributionFloat Velocity;
    
    // 원뿔 각도 (Distribution)
    // 설명: 원뿔이 얼마나 넓게 벌어질지 결정 (0~180도)
    UPROPERTY(EditAnywhere, Category="Velocity", meta=(ToolTip="원뿔의 퍼짐 각도(Degree)입니다. 0도면 기준 방향으로 직선 발사되고, 값이 커질수록 넓게 퍼집니다."))
    FDistributionFloat Angle;

    // 기준 방향 (Vector)
    // 설명: 원뿔의 중심 축이 되는 방향
    UPROPERTY(EditAnywhere, Category="Velocity", meta=(ToolTip="원뿔의 중심 축(Axis)이 되는 방향 벡터입니다. (예: 0,0,1 은 위쪽)"))
    FVector Direction;
    
    // --- 인터페이스 ---
    virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
    virtual uint32 RequiredBytes(FParticleEmitterInstance* Owner) override;
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
