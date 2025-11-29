#pragma once

//physics
UENUM(DisplayName = "충돌 옵션")
enum class ECollisionEnabled : uint8
{
	None,
	QueryOnly,  // 레이캐스트 처리
	PhysicsOnly, // 물리처리만 함
	PhysicsAndQuery
};

UENUM(DisplayName = "모빌리티")
enum class EMobilityType : uint8
{
	Static,
	Movable
};

namespace PhysxConverter
{
	// 왼손 zUp xForward -> 오른손 yUp zForward
	inline physx::PxVec3 ToPxVec3(const FVector& InVector)
	{
		return physx::PxVec3(InVector.Y, InVector.Z, -InVector.X);
	}

	// 쿼터니언은 축에 더해 회전 방향도 달라지므로 허수부 부호 반전
	inline physx::PxQuat ToPxQuat(const FQuat& InQuat)
	{
		return physx::PxQuat(-InQuat.Y, -InQuat.Z, InQuat.X, InQuat.W);
	}

	inline physx::PxTransform ToPxTransform(const FTransform& InTransform)
	{
		return physx::PxTransform(ToPxVec3(InTransform.Translation), ToPxQuat(InTransform.Rotation));
	}

	// 오른손 yUp zBack -> 왼손 zUp xForward  
	inline FVector ToFVector(const physx::PxVec3& InVector)
	{
		return FVector(-InVector.z, InVector.x, InVector.y);
	}

	inline FQuat ToFQuat(const physx::PxQuat& InQuat)
	{
		return FQuat(InQuat.z, -InQuat.x, -InQuat.y, InQuat.w);
	}

	inline FTransform ToFTransform(const physx::PxTransform& InTransform)
	{
		return FTransform(ToFVector(InTransform.p), ToFQuat(InTransform.q), FVector(1,1,1));
	}
}