#include "pch.h"
#include "ClothSystem.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Solver.h>
#include <NvCloth/Callbacks.h>
#include <iostream>

// ========== CPU 멀티스레드 시뮬레이션 ==========
// NvCloth CPU Factory는 멀티스레드로 최적화되어 있어
// GPU↔CPU 데이터 전송 오버헤드 없이 실시간 성능을 보장합니다

// ========== Allocator 구현 ==========
// @brief NvCloth가 메모리를 할당할 때 사용하는 커스텀 Allocator입니다
// @note NvCloth는 SIMD 연산을 위해 16바이트 정렬된 메모리를 요구합니다
void* FClothAllocator::allocate(size_t size, const char* typeName, const char* filename, int line)
{
    // _aligned_malloc: Visual Studio의 16바이트 정렬 메모리 할당 함수
    // @param size: 할당할 메모리 크기 (바이트)
    // @param alignment: 정렬 크기 (16바이트)
    // @return 할당된 메모리 포인터 (실패 시 nullptr)
    return _aligned_malloc(size, 16);
}

void FClothAllocator::deallocate(void* ptr)
{
    // _aligned_free: 정렬된 메모리 해제 함수
    // @param ptr: 해제할 메모리 포인터 (allocate()에서 반환된 값)
    _aligned_free(ptr);
}

// ========== ErrorCallback 구현 ==========
void FClothErrorCallback::reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
{
    const char* errorType = "Unknown";
    switch (code)
    {
    case physx::PxErrorCode::eDEBUG_INFO:       errorType = "Info"; break;
    case physx::PxErrorCode::eDEBUG_WARNING:    errorType = "Warning"; break;
    case physx::PxErrorCode::eINVALID_PARAMETER:errorType = "Invalid Param"; break;
    case physx::PxErrorCode::eINVALID_OPERATION:errorType = "Invalid Op"; break;
    case physx::PxErrorCode::eOUT_OF_MEMORY:    errorType = "Out of Memory"; break;
    case physx::PxErrorCode::eINTERNAL_ERROR:   errorType = "Internal Error"; break;
    case physx::PxErrorCode::eABORT:            errorType = "Abort"; break;
    case physx::PxErrorCode::ePERF_WARNING:     errorType = "Perf Warning"; break;
    }
    std::cout << "[NvCloth " << errorType << "] " << message << " (" << file << ":" << line << ")" << std::endl;
}

// ========== AssertHandler 구현 ==========
void FClothAssertHandler::operator()(const char* exp, const char* file, int line, bool& ignore)
{
    std::cout << "[NvCloth Assert] " << exp << " at " << file << ":" << line << std::endl;
    // 디버그 시 브레이크 포인트
    __debugbreak();
}

// ========== FClothSystem 싱글톤 ==========
FClothSystem* FClothSystem::Instance = nullptr;

FClothSystem& FClothSystem::GetInstance()
{
    if (Instance == nullptr)
    {
        Instance = new FClothSystem();
    }
    return *Instance;
}

FClothSystem::~FClothSystem()
{
    Shutdown();
}

// @brief NvCloth 시스템을 초기화합니다
// @note 게임 시작 시 World::Initialize()에서 호출됩니다
//
// === NvCloth 초기화 순서 ===
// 1. InitializeNvCloth(): 라이브러리 전역 초기화
// 2. Factory 생성: Cloth/Fabric/Solver 객체를 만드는 팩토리
// 3. Solver 생성: 실제 시뮬레이션을 수행하는 엔진
void FClothSystem::Initialize()
{
    // ===== 1. InitializeNvCloth() 호출 =====
    // @brief NvCloth 라이브러리를 전역으로 초기화합니다 (최초 1회만 호출)
    // @param allocator: 메모리 할당자 (16바이트 정렬 필수)
    // @param errorCallback: 에러 발생 시 호출되는 콜백
    // @param assertHandler: Assert 발생 시 호출되는 핸들러
    // @param profiler: 성능 프로파일링용 (nullptr = 비활성화)
    // @note 이 함수는 프로그램당 1회만 호출되어야 하며, 여러 번 호출 시 크래시 발생 가능
    nv::cloth::InitializeNvCloth(
        &Allocator,
        &ErrorCallback,
        &AssertHandler,
        nullptr // Profiler는 현재 사용하지 않음
    );

    // ===== 2. CPU Factory 생성 =====
    // Factory: Cloth, Fabric, Solver 객체를 생성하는 팩토리 클래스
    // - CPU Factory: 멀티스레드로 최적화된 CPU 시뮬레이션
    // - GPU↔CPU 데이터 전송 오버헤드 없음
    // - 40,000 정점 기준 실시간 성능 보장
    // @return Factory 포인터 (실패 시 nullptr)

    std::cout << "[ClothSystem] ===== CPU Multi-threaded Cloth Simulation =====" << std::endl;
    std::cout << "[ClothSystem] Creating CPU Factory..." << std::endl;

    Factory = NvClothCreateFactoryCPU();
    if (!Factory)
    {
        std::cout << "[ClothSystem] FATAL ERROR: NvClothCreateFactoryCPU failed!" << std::endl;
        return;
    }

    std::cout << "[ClothSystem] ===== CPU Factory created successfully! =====" << std::endl;
    std::cout << "[ClothSystem] Cloth simulations will run on multi-threaded CPU." << std::endl;

    // ===== 3. Solver 생성 =====
    // Solver: 여러 Cloth를 관리하고 시뮬레이션을 수행하는 엔진
    //   - 매 프레임 Update()에서 simulate()를 호출하여 모든 Cloth 업데이트
    //   - 병렬 처리 지원 (멀티스레드로 여러 Cloth를 동시에 시뮬레이션 가능)
    // @return Solver 포인터 (실패 시 nullptr)
    Solver = Factory->createSolver();
    if (!Solver)
    {
        std::cout << "[ClothSystem] Failed to create Solver!" << std::endl;
        return;
    }

    std::cout << "[ClothSystem] Initialized successfully!" << std::endl;
}

// @brief 매 프레임 Cloth 시뮬레이션을 업데이트합니다
// @param DeltaTime: 프레임 시간 (초 단위)
// @note World::Tick()에서 호출되며, Solver에 등록된 모든 Cloth가 업데이트됩니다
//
// === Simulation Flow ===
// 1. beginSimulation(): 시뮬레이션 시작 (DeltaTime 전달)
// 2. simulateChunk(): 각 Chunk(작업 단위)를 시뮬레이션 (병렬 처리 가능)
// 3. endSimulation(): 시뮬레이션 종료 및 결과 적용
void FClothSystem::Update(float DeltaTime)
{
    if (!Solver) return;

    // ===== Solver::beginSimulation() 호출 =====
    // @brief 시뮬레이션을 시작하고 내부 상태를 준비합니다
    // @param dt: 시뮬레이션 시간 간격 (초 단위, 일반적으로 1/60 = 0.0167초)
    // @return 시뮬레이션을 수행할 Chunk가 있으면 true, 없으면 false
    // @note DeltaTime이 너무 크면 시뮬레이션이 불안정해질 수 있으므로 적절히 제한 필요
    if (Solver->beginSimulation(DeltaTime))
    {
        // ===== Chunk 기반 시뮬레이션 =====
        // Chunk: Solver가 작업을 분할한 단위 (병렬 처리를 위해)
        //   - 단일 스레드: for 루프로 순차 처리
        //   - 멀티 스레드: 각 Chunk를 별도 스레드에서 병렬 처리 가능
        int chunkCount = Solver->getSimulationChunkCount();
        for (int i = 0; i < chunkCount; i++)
        {
            // ===== Solver::simulateChunk() 호출 =====
            // @brief 특정 Chunk의 시뮬레이션을 수행합니다
            // @param chunkIdx: Chunk 인덱스 (0 ~ chunkCount-1)
            // @note 이 함수는 병렬로 호출 가능하므로 멀티스레드 최적화에 유용
            Solver->simulateChunk(i);
        }

        // ===== Solver::endSimulation() 호출 =====
        // @brief 시뮬레이션을 종료하고 결과를 각 Cloth에 적용합니다
        // @note 이 함수 호출 후 Cloth::getCurrentParticles()로 결과를 가져올 수 있음
        Solver->endSimulation();
    }
}

void FClothSystem::Shutdown()
{
    std::cout << "[ClothSystem] Shutting down..." << std::endl;

    // ===== 1. Solver 정리 =====
    if (Solver)
    {
        delete Solver;
        Solver = nullptr;
        std::cout << "[ClothSystem] Solver destroyed." << std::endl;
    }

    // ===== 2. Factory 정리 =====
    if (Factory)
    {
        NvClothDestroyFactory(Factory);
        Factory = nullptr;
        std::cout << "[ClothSystem] Factory destroyed." << std::endl;
    }

    std::cout << "[ClothSystem] Shutdown complete." << std::endl;
}

void FClothSystem::Destroy()
{
    if (Instance)
    {
        delete Instance;
        Instance = nullptr;
    }
}