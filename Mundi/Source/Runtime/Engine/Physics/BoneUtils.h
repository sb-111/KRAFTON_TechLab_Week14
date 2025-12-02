#pragma once

#include <algorithm>
#include <cctype>

// ===== 본 이름 기반 유틸리티 함수들 =====
// 래그돌 바디 자동 생성 시 본 타입 감지에 사용

namespace BoneUtils
{
    // 대소문자 무시 문자열 포함 검사
    inline bool ContainsIgnoreCase(const FString& Str, const FString& Pattern)
    {
        FString LowerStr = Str;
        FString LowerPattern = Pattern;
        std::transform(LowerStr.begin(), LowerStr.end(), LowerStr.begin(),
            [](unsigned char c) { return std::tolower(c); });
        std::transform(LowerPattern.begin(), LowerPattern.end(), LowerPattern.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return LowerStr.find(LowerPattern) != FString::npos;
    }

    // 손가락/발가락/말단 본 필터링 (캡슐 생성 스킵)
    inline bool ShouldSkipBone(const FString& BoneName)
    {
        // 손가락 관련
        if (ContainsIgnoreCase(BoneName, "thumb")) return true;
        if (ContainsIgnoreCase(BoneName, "index")) return true;
        if (ContainsIgnoreCase(BoneName, "middle")) return true;
        if (ContainsIgnoreCase(BoneName, "ring")) return true;
        if (ContainsIgnoreCase(BoneName, "pinky")) return true;
        if (ContainsIgnoreCase(BoneName, "finger")) return true;
        if (ContainsIgnoreCase(BoneName, "digit")) return true;

        // 발가락 관련
        if (ContainsIgnoreCase(BoneName, "toe")) return true;

        // 말단 본 마커
        if (ContainsIgnoreCase(BoneName, "_end")) return true;
        if (ContainsIgnoreCase(BoneName, ".end")) return true;
        if (ContainsIgnoreCase(BoneName, "_tip")) return true;
        if (ContainsIgnoreCase(BoneName, "nub")) return true;

        // IK 본 관련
        if (ContainsIgnoreCase(BoneName, "ik_")) return true;
        if (ContainsIgnoreCase(BoneName, "_ik")) return true;
        if (ContainsIgnoreCase(BoneName, "ikfoot")) return true;
        if (ContainsIgnoreCase(BoneName, "ikhand")) return true;
        if (ContainsIgnoreCase(BoneName, "pole")) return true;      // IK pole target
        if (ContainsIgnoreCase(BoneName, "vb ")) return true;       // Unreal virtual bone

        return false;
    }

    // Hand 본 감지
    inline bool IsHandBone(const FString& BoneName)
    {
        if (ContainsIgnoreCase(BoneName, "hand"))
        {
            if (ShouldSkipBone(BoneName)) return false;
            return true;
        }
        return false;
    }

    // Head 본 감지
    inline bool IsHeadBone(const FString& BoneName)
    {
        if (ContainsIgnoreCase(BoneName, "head"))
        {
            if (ShouldSkipBone(BoneName)) return false;
            return true;
        }
        return false;
    }

    // Spine/Pelvis/Chest 본 감지 (가로 캡슐용)
    inline bool IsSpineOrPelvisBone(const FString& BoneName)
    {
        if (ContainsIgnoreCase(BoneName, "spine")) return true;
        if (ContainsIgnoreCase(BoneName, "pelvis")) return true;
        if (ContainsIgnoreCase(BoneName, "hips")) return true;
        if (ContainsIgnoreCase(BoneName, "chest")) return true;
        return false;
    }

    // Neck 본 감지
    inline bool IsNeckBone(const FString& BoneName)
    {
        return ContainsIgnoreCase(BoneName, "neck");
    }

    // Shoulder/Clavicle 본 감지
    inline bool IsShoulderBone(const FString& BoneName)
    {
        if (ContainsIgnoreCase(BoneName, "shoulder")) return true;
        if (ContainsIgnoreCase(BoneName, "clavicle")) return true;
        return false;
    }

    // Foot 본 감지
    inline bool IsFootBone(const FString& BoneName)
    {
        if (ContainsIgnoreCase(BoneName, "foot")) return true;
        if (ContainsIgnoreCase(BoneName, "ankle")) return true;
        return false;
    }

    // BindPose 매트릭스에서 위치 추출
    inline FVector GetBonePosition(const FMatrix& BindPose)
    {
        return FVector(BindPose.M[3][0], BindPose.M[3][1], BindPose.M[3][2]);
    }

    // 방향 벡터를 매트릭스의 회전 부분으로 변환
    inline FVector TransformDirection(const FMatrix& M, const FVector& Dir)
    {
        return FVector(
            Dir.X * M.M[0][0] + Dir.Y * M.M[1][0] + Dir.Z * M.M[2][0],
            Dir.X * M.M[0][1] + Dir.Y * M.M[1][1] + Dir.Z * M.M[2][1],
            Dir.X * M.M[0][2] + Dir.Y * M.M[1][2] + Dir.Z * M.M[2][2]
        );
    }

    // 점(위치)을 매트릭스로 변환 (회전 + 이동 포함)
    inline FVector TransformPoint(const FMatrix& M, const FVector& Point)
    {
        return FVector(
            Point.X * M.M[0][0] + Point.Y * M.M[1][0] + Point.Z * M.M[2][0] + M.M[3][0],
            Point.X * M.M[0][1] + Point.Y * M.M[1][1] + Point.Z * M.M[2][1] + M.M[3][1],
            Point.X * M.M[0][2] + Point.Y * M.M[1][2] + Point.Z * M.M[2][2] + M.M[3][2]
        );
    }

    // ===== 본 방향 기반 캡슐 회전 계산 =====
    // 본의 로컬 방향에서 캡슐이 해당 방향을 향하도록 회전값 계산
    // 렌더링: FinalRotation = UserRotation * BaseRotation (BaseRotation 먼저 적용!)
    // 반환값: Capsule.Rotation에 저장할 오일러 각도 (ZYX 순서, 도 단위)
    inline FVector CalculateCapsuleRotationFromBoneDir(const FVector& BoneLocalDir)
    {
        // 유효하지 않은 방향이면 기본값 반환
        if (BoneLocalDir.SizeSquared() < KINDA_SMALL_NUMBER)
        {
            return FVector(0.0f, 0.0f, 0.0f);
        }

        // BaseRotation(-90,0,0) 적용 후 캡슐의 실제 축 계산
        // FinalRotation = UserRotation * BaseRotation 이므로,
        // BaseRotation이 (0,1,0)에 먼저 적용된 결과가 DefaultAxis
        FQuat BaseRotation = FQuat::MakeFromEulerZYX(FVector(-90.0f, 0.0f, 0.0f));
        FVector PostBaseAxis = BaseRotation.RotateVector(FVector(0.0f, 1.0f, 0.0f));

        FVector NormalizedDir = BoneLocalDir.GetNormalized();

        // PostBaseAxis에서 BoneLocalDir로 회전하는 쿼터니언 계산
        FQuat Rotation = FQuat::FindBetweenVectors(PostBaseAxis, NormalizedDir);

        // 쿼터니언 → 오일러 각도 (ZYX 순서, 도 단위)
        return Rotation.ToEulerZYXDeg();
    }

    // ===== 본 방향 기반 박스 회전 계산 =====
    // 본의 로컬 방향에서 박스가 해당 방향을 향하도록 회전값 계산
    // 박스의 기본 길이 축 = Z축 (0, 0, 1)
    // 반환값: Box.Rotation에 저장할 오일러 각도 (ZYX 순서, 도 단위)
    inline FVector CalculateBoxRotationFromBoneDir(const FVector& BoneLocalDir)
    {
        // 유효하지 않은 방향이면 기본값 반환
        if (BoneLocalDir.SizeSquared() < KINDA_SMALL_NUMBER)
        {
            return FVector(0.0f, 0.0f, 0.0f);
        }

        // 박스의 기본 길이 축 = Z축
        FVector DefaultAxis(0.0f, 0.0f, 1.0f);

        FVector NormalizedDir = BoneLocalDir.GetNormalized();

        // DefaultAxis에서 BoneLocalDir로 회전하는 쿼터니언 계산
        FQuat Rotation = FQuat::FindBetweenVectors(DefaultAxis, NormalizedDir);

        // 쿼터니언 → 오일러 각도 (ZYX 순서, 도 단위)
        return Rotation.ToEulerZYXDeg();
    }

    // ===== 본 타입별 파라미터 상수 =====
    constexpr float MinBoneSize = 0.03f;
    constexpr float MaxBoneSize = 0.50f;
    constexpr float RadiusRatio = 0.25f;
    constexpr float LengthRatio = 0.8f;

    constexpr float HandBoneSize = 0.15f;
    constexpr float HandRadiusRatio = 0.4f;

    constexpr float HeadBoneSize = 0.1f;       // Head 본 고정 크기
    constexpr float HeadRadiusRatio = 0.8f;
    constexpr float HeadCenterOffset = 0.1f;  // 머리 위치를 더 위로

    constexpr float FootRadiusRatio = 0.4f;

    constexpr float SpineRadiusRatio = 0.5f;
    constexpr float SpineLengthRatio = 1.2f;

    constexpr float NeckRadiusRatio = 0.7f;
    constexpr float NeckLengthRatio = 1.2f;

    constexpr float ShoulderRadiusRatio = 0.3f;
}
