#include "pch.h"
#include "ParticleModuleVelocityCone.h"
#include "ParticleEmitterInstance.h"
#include "ParticleSystemComponent.h"
#include "ParticleModuleVelocity.h" // FParticleVelocityPayload 정의 사용을 위해

IMPLEMENT_CLASS(UParticleModuleVelocityCone)

UParticleModuleVelocityCone::UParticleModuleVelocityCone()
{
    bSpawnModule = true;
    bUpdateModule = false;

    // 기본값 설정 (필요에 따라 조정)
    Direction = FVector(0, 0, 1);
}

// 언리얼 엔진 호환: 페이로드 크기 반환 (Velocity 모듈과 동일한 페이로드 사용 가정)
uint32 UParticleModuleVelocityCone::RequiredBytes(FParticleEmitterInstance* Owner)
{
    return sizeof(FParticleVelocityPayload);
}

void UParticleModuleVelocityCone::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
    if (!ParticleBase || !Owner || !Owner->Component)
    {
        return;
    }

    // 1. 값 추출 (Distribution + RandomStream 사용)
    // RandomStream에서 0.0~1.0 사이의 난수를 얻는 함수가 GetFraction()이라고 가정합니다.
    // 만약 엔진 API가 다르다면 Owner->GetRandomFloat() 등으로 대체하세요.
    float RandomVal1 = Owner->RandomStream.GetFraction(); 
    float RandomVal2 = Owner->RandomStream.GetFraction();
    float RandomVal3 = Owner->RandomStream.GetFraction();

    float CurrentSpeed = Velocity.GetValue(SpawnTime, Owner->RandomStream, Owner->Component);
    float CurrentAngleDeg = Angle.GetValue(SpawnTime, Owner->RandomStream, Owner->Component);

    // 2. Cone 수학 계산 (Local Space)
    CurrentAngleDeg = FMath::Clamp(CurrentAngleDeg, 0.0f, 180.0f);
    
    // 기준 방향 정규화
    FVector Dir = Direction.GetSafeNormal();
    if (Dir.IsZero()) Dir = FVector(0, 0, 1);

    float ConeHalfAngleRad = DegreesToRadians(CurrentAngleDeg);
    float CosAngle = cos(ConeHalfAngleRad);

    // 구면 균등 분포를 위한 Z값 계산 (CosAngle ~ 1.0)
    float Z = FMath::Lerp(CosAngle, 1.0f, RandomVal1);
    
    // 0 ~ 360도 회전
    float Pi = RandomVal2 * 2.0f * PI;
    
    // 반지름 계산
    float R = FMath::Sqrt(1.0f - Z * Z);

    // 로컬 공간(Z-Up 기준)에서의 랜덤 방향 벡터
    FVector ConeDir;
    ConeDir.X = R * cos(Pi);
    ConeDir.Y = R * sin(Pi);
    ConeDir.Z = Z;

    // 3. 회전 적용 (Z-Up -> 설정된 Direction)
    FQuat Rot = FQuat::FindBetweenVectors(FVector(0, 0, 1), Dir);
    FVector LocalVelocity = Rot.RotateVector(ConeDir) * CurrentSpeed;

    // 4. 월드 공간 변환 (Transform)
    const FTransform& ComponentTransform = Owner->Component->GetWorldTransform();
    // 방향 벡터이므로 TransformVectorNoScale 혹은 TransformVector 사용
    FVector WorldVelocity = ComponentTransform.TransformVector(LocalVelocity);

    // 5. 파티클에 적용
    // 기존 속도에 더할지(Add), 덮어쓸지(Set)는 기획 의도에 따름. 보통 Cone은 초기 발사라 += 사용.
    ParticleBase->Velocity += WorldVelocity;
    ParticleBase->BaseVelocity += WorldVelocity;

    // 6. 페이로드 처리 (새 엔진 규격 준수)
    // Offset 처리를 위해 PARTICLE_ELEMENT 매크로 사용
    uint32 CurrentOffset = Offset;
    PARTICLE_ELEMENT(FParticleVelocityPayload, VelPayload);

    // 페이로드 데이터 채우기
    VelPayload.InitialVelocity = ParticleBase->BaseVelocity;
    VelPayload.VelocityMagnitude = ParticleBase->BaseVelocity.Size();
}

void UParticleModuleVelocityCone::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    UParticleModule::Serialize(bInIsLoading, InOutHandle);

    JSON TempJson;
    if (bInIsLoading)
    {
        // Velocity 로드
        if (FJsonSerializer::ReadObject(InOutHandle, "Velocity", TempJson))
            Velocity.Serialize(true, TempJson);
        
        // Angle 로드
        if (FJsonSerializer::ReadObject(InOutHandle, "Angle", TempJson))
            Angle.Serialize(true, TempJson);

        // Direction 로드
        FJsonSerializer::ReadVector(InOutHandle, "Direction", Direction);
    }
    else
    {
        // Velocity 저장
        TempJson = JSON::Make(JSON::Class::Object);
        Velocity.Serialize(false, TempJson);
        InOutHandle["Velocity"] = TempJson;

        // Angle 저장
        TempJson = JSON::Make(JSON::Class::Object);
        Angle.Serialize(false, TempJson);
        InOutHandle["Angle"] = TempJson;

        // Direction 저장
        InOutHandle["Direction"] = FJsonSerializer::VectorToJson(Direction);
    }
}