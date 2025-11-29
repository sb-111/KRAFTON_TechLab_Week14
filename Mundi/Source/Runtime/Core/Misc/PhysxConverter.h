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
	inline physx::PxVec3 ToPxVec3(const FVector& InVector)
	{
		return physx::PxVec3(InVector.X, InVector.Y, InVector.Z);
	}

	inline physx::PxQuat ToPxQuat(const FQuat& InQuat)
	{
		return physx::PxQuat(InQuat.X, InQuat.Y, InQuat.Z, InQuat.W);
	}

	inline physx::PxTransform ToPxTransform(const FTransform& InTransform)
	{
		return physx::PxTransform(ToPxVec3(InTransform.Translation), ToPxQuat(InTransform.Rotation));
	}

	inline FVector ToFVector(const physx::PxVec3& InVector)
	{
		return FVector(InVector.x, InVector.y, InVector.z);
	}

	inline FQuat ToFQuat(const physx::PxQuat& InQuat)
	{
		return FQuat(InQuat.x, InQuat.y, InQuat.z, InQuat.w);
	}

	inline FTransform ToFTransform(const physx::PxTransform& InTransform)
	{
		return FTransform(ToFVector(InTransform.p), ToFQuat(InTransform.q), FVector(1,1,1));
	}
}