#pragma once
#include "pch.h"

// ===== Ragdoll Statistics =====
// 래그돌 시스템의 성능 및 상태 통계

struct FRagdollStats
{
    // === 구조 통계 ===
    int32 ActiveRagdollCount = 0;       // 활성 래그돌 수
    int32 TotalBodyCount = 0;           // 총 바디 수
    int32 TotalConstraintCount = 0;     // 총 컨스트레인트 수
    int32 SphereShapeCount = 0;         // 구체 Shape 수
    int32 BoxShapeCount = 0;            // 박스 Shape 수
    int32 CapsuleShapeCount = 0;        // 캡슐 Shape 수

    // === 성능 통계 ===
    double SyncBodiesToBonesTimeMS = 0.0;   // 물리 → 본 동기화 시간 (ms)
    double TotalRagdollTimeMS = 0.0;        // 래그돌 관련 총 처리 시간 (ms)

    // === 메모리 통계 ===
    uint64 BodiesMemoryBytes = 0;           // FBodyInstance 메모리
    uint64 ConstraintsMemoryBytes = 0;      // FConstraintInstance 메모리

    void Reset()
    {
        ActiveRagdollCount = 0;
        TotalBodyCount = 0;
        TotalConstraintCount = 0;
        SphereShapeCount = 0;
        BoxShapeCount = 0;
        CapsuleShapeCount = 0;
        SyncBodiesToBonesTimeMS = 0.0;
        TotalRagdollTimeMS = 0.0;
        BodiesMemoryBytes = 0;
        ConstraintsMemoryBytes = 0;
    }

    int32 GetTotalShapeCount() const
    {
        return SphereShapeCount + BoxShapeCount + CapsuleShapeCount;
    }

    uint64 GetTotalMemoryBytes() const
    {
        return BodiesMemoryBytes + ConstraintsMemoryBytes;
    }

    double GetTotalMemoryKB() const
    {
        return static_cast<double>(GetTotalMemoryBytes()) / 1024.0;
    }
};

// ===== Ragdoll Stat Manager =====
// 래그돌 통계를 수집하고 관리하는 싱글톤

class FRagdollStatManager
{
public:
    static FRagdollStatManager& GetInstance()
    {
        static FRagdollStatManager Instance;
        return Instance;
    }

    // 프레임 시작 시 통계 리셋
    void ResetFrameStats()
    {
        CurrentStats.Reset();
    }

    // 통계 직접 업데이트
    void UpdateStats(const FRagdollStats& InStats)
    {
        CurrentStats = InStats;
    }

    // 현재 통계 반환
    const FRagdollStats& GetStats() const
    {
        return CurrentStats;
    }

    // === 헬퍼 함수들 ===

    // 래그돌 추가 (바디/컨스트레인트 수)
    void AddRagdoll(int32 BodyCount, int32 ConstraintCount)
    {
        CurrentStats.ActiveRagdollCount++;
        CurrentStats.TotalBodyCount += BodyCount;
        CurrentStats.TotalConstraintCount += ConstraintCount;
    }

    // Shape 수 추가
    void AddShapeCounts(int32 Spheres, int32 Boxes, int32 Capsules)
    {
        CurrentStats.SphereShapeCount += Spheres;
        CurrentStats.BoxShapeCount += Boxes;
        CurrentStats.CapsuleShapeCount += Capsules;
    }

    // 동기화 시간 추가
    void AddSyncTime(double TimeMS)
    {
        CurrentStats.SyncBodiesToBonesTimeMS += TimeMS;
        CurrentStats.TotalRagdollTimeMS += TimeMS;
    }

    // 메모리 사용량 추가
    void AddMemory(uint64 BodiesBytes, uint64 ConstraintsBytes)
    {
        CurrentStats.BodiesMemoryBytes += BodiesBytes;
        CurrentStats.ConstraintsMemoryBytes += ConstraintsBytes;
    }

private:
    FRagdollStatManager() = default;
    FRagdollStats CurrentStats;
};
