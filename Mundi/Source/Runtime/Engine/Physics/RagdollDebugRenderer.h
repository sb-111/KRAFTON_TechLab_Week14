#pragma once

#include "pch.h"
#include <PxPhysicsAPI.h>

using namespace physx;

class URenderer;
class USkeletalMeshComponent;
struct FBodyInstance;
struct FKAggregateGeom;
struct FKSphereElem;
struct FKBoxElem;
struct FKSphylElem;
struct FKConvexElem;

class UPhysicsAsset;

// ===== Ragdoll Debug Renderer =====
// Ragdoll Shape 및 Joint 디버그 시각화
class FRagdollDebugRenderer
{
public:
    // USkeletalMeshComponent 기반 래그돌 렌더링 (새 API)
    // non-const: 에디터 모드에서 Bodies lazy 초기화를 위해
    static void RenderSkeletalMeshRagdoll(
        URenderer* Renderer,
        USkeletalMeshComponent* SkelMeshComp,
        const FVector4& BoneColor = FVector4(0.0f, 1.0f, 0.0f, 1.0f),
        const FVector4& JointColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f)
    );

    // PhysicsAsset 기반 미리보기 렌더링 (Bodies 없이도 작동)
    // SkeletalMeshComponent의 본 트랜스폼을 사용하여 PhysicsAsset의 Shape들을 렌더링
    static void RenderPhysicsAssetPreview(
        URenderer* Renderer,
        USkeletalMeshComponent* SkelMeshComp,
        UPhysicsAsset* PhysAsset,
        const FVector4& BoneColor = FVector4(0.0f, 1.0f, 0.0f, 1.0f),
        const FVector4& JointColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f)
    );

    // FKAggregateGeom 기반 Shape 렌더링
    static void RenderAggGeom(
        URenderer* Renderer,
        const FKAggregateGeom& AggGeom,
        const PxTransform& WorldTransform,
        const FVector4& Color,
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors
    );

    // Sphere 렌더링 (와이어프레임 원)
    static void RenderSphere(
        const FKSphereElem& Sphere,
        const PxTransform& WorldTransform,
        const FVector4& Color,
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors
    );

    // Box 렌더링 (12개 엣지)
    static void RenderBox(
        const FKBoxElem& Box,
        const PxTransform& WorldTransform,
        const FVector4& Color,
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors
    );

    // Capsule 렌더링 (원기둥 + 반구)
    static void RenderCapsule(
        const FKSphylElem& Capsule,
        const PxTransform& WorldTransform,
        const FVector4& Color,
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors
    );

    // Convex 렌더링 (와이어프레임 삼각형)
    static void RenderConvex(
        const FKConvexElem& Convex,
        const PxTransform& WorldTransform,
        const FVector4& Color,
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors
    );

private:
    // 헬퍼: 원 렌더링 (세그먼트 수 지정)
    static void AddCircle(
        const FVector& Center,
        const FVector& Normal,
        float Radius,
        int32 NumSegments,
        const FVector4& Color,
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors
    );

    // 헬퍼: 반원 렌더링 (반구 표현용)
    // BulgeDir: 반원이 볼록하게 나갈 방향
    // PlaneAxis: 반원이 그려질 평면의 한 축
    static void AddSemicircle(
        const FVector& Center,
        const FVector& BulgeDir,
        const FVector& PlaneAxis,
        float Radius,
        int32 NumSegments,
        const FVector4& Color,
        TArray<FVector>& OutStartPoints,
        TArray<FVector>& OutEndPoints,
        TArray<FVector4>& OutColors
    );
};
