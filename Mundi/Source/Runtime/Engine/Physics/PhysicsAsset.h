#pragma once

#include "Object.h"
#include "BodySetup.h"
#include "UPhysicsAsset.generated.h"

// ===== Physics Asset =====
// Skeletal Mesh에 연결되는 물리 에셋
// 여러 개의 BodySetup(본별 충돌체)을 포함
UCLASS(DisplayName = "Physics Asset", Description = "스켈레탈 메시의 물리 설정을 담는 에셋")
class UPhysicsAsset : public UObject
{
    GENERATED_REFLECTION_BODY()
public:
    // 본별 물리 바디 설정들
    UPROPERTY(EditAnywhere, Category = "Bodies")
    TArray<UBodySetup*> SkeletalBodySetups;

    // TODO: Constraint 설정들 (FConstraintInstance)
    // UPROPERTY(EditAnywhere, Category = "Constraints")
    // TArray<FConstraintInstance> ConstraintSetup;

    UPhysicsAsset() = default;
    virtual ~UPhysicsAsset() = default;

    // 본 이름으로 BodySetup 찾기
    UBodySetup* FindBodySetup(const FName& BoneName) const
    {
        for (UBodySetup* Body : SkeletalBodySetups)
        {
            if (Body && Body->BoneName == BoneName)
            {
                return Body;
            }
        }
        return nullptr;
    }

    // BodySetup 개수
    int32 GetBodySetupCount() const
    {
        return SkeletalBodySetups.Num();
    }
};
