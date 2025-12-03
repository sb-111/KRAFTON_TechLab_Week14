#pragma once
#include <foundation/PxAllocatorCallback.h>
#include <foundation/PxErrorCallback.h>
#include <NvCloth/Callbacks.h>

namespace nv
{
    namespace cloth
    {
        class Factory;
        class Solver;
    }
}

// NvCloth용 Allocator (16바이트 정렬 필수)
class FClothAllocator : public physx::PxAllocatorCallback
{
public:
    void* allocate(size_t size, const char* typeName, const char* filename, int line) override;
    void deallocate(void* ptr) override;
};

// NvCloth용 Error Callback
class FClothErrorCallback : public physx::PxErrorCallback
{
public:
    void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override;
};

// NvCloth용 Assert Handler
class FClothAssertHandler : public nv::cloth::PxAssertHandler
{
public:
    void operator()(const char* exp, const char* file, int line, bool& ignore) override;
};

// ========================================================================================================
// FClothSystem - NvCloth CPU 시뮬레이션 매니저 (싱글톤)
// ========================================================================================================
//
// === CPU 멀티스레드 시뮬레이션 ===
// - CPU Factory 사용 (멀티스레드 최적화)
// - GPU↔CPU 데이터 전송 오버헤드 없음
// - 40,000 정점 기준 실시간 성능 보장
//
// === 초기화 순서 ===
// 1. InitializeNvCloth(): 라이브러리 전역 초기화
// 2. CPU Factory 생성 (NvClothCreateFactoryCPU)
// 3. Solver 생성 (멀티스레드 CPU 시뮬레이션)
//
// === 시뮬레이션 루프 ===
// 1. Update(DeltaTime) 호출 (매 프레임)
// 2. Solver::beginSimulation()
// 3. Solver::simulateChunk() (멀티스레드 병렬 처리)
// 4. Solver::endSimulation()
//
// ========================================================================================================
class FClothSystem
{
public:
    static FClothSystem& GetInstance();

    void Initialize();   // CPU Factory 생성
    void Shutdown();     // 리소스 정리
    void Update(float DeltaTime);   // CPU 멀티스레드 시뮬레이션 실행

    static void Destroy();  // 싱글톤 정리

    nv::cloth::Factory* GetFactory() { return Factory; }
    nv::cloth::Solver* GetSolver() { return Solver; }

private:
    FClothSystem() = default;
    ~FClothSystem();

    FClothAllocator Allocator;
    FClothErrorCallback ErrorCallback;
    FClothAssertHandler AssertHandler;

    nv::cloth::Factory* Factory = nullptr;   // CPU Factory
    nv::cloth::Solver* Solver = nullptr;     // CPU Solver

    static FClothSystem* Instance;
};