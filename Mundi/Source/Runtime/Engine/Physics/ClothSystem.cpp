#include "pch.h"
#include "ClothSystem.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Solver.h>
#include <NvCloth/Callbacks.h>
#include <iostream>

// ========== Allocator 구현 ==========
void* FClothAllocator::allocate(size_t size, const char* typeName, const char* filename, int line)
{
    // NvCloth는 16바이트 정렬된 메모리 필요
    return _aligned_malloc(size, 16);
}

void FClothAllocator::deallocate(void* ptr)
{
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

void FClothSystem::Initialize()
{
    // 1. NvCloth 라이브러리 초기화
    nv::cloth::InitializeNvCloth(
        &Allocator,
        &ErrorCallback,
        &AssertHandler,
        nullptr // Profiler 생략
    );

    // 2. CPU Factory 생성
    Factory = NvClothCreateFactoryCPU();

    // 3. Solver 생성 (시뮬레이셔 실행기)
    Solver = Factory->createSolver();
}

void FClothSystem::Update(float DeltaTime)
{
    // 시뮬레이션 실행 (단일 스레드 버전)
    if (Solver->beginSimulation(DeltaTime))
    {
        for (int i = 0; i < Solver->getSimulationChunkCount(); i++)
        {
            Solver->simulateChunk(i);
        }
        Solver->endSimulation();
    }
}

void FClothSystem::Shutdown()
{
    // 역순 정리
    if (Solver)
    {
        delete Solver;
        Solver = nullptr;
    }
    if (Factory)
    {
        NvClothDestroyFactory(Factory);
        Factory = nullptr;
    }
}

void FClothSystem::Destroy()
{
    if (Instance)
    {
        delete Instance;
        Instance = nullptr;
    }
}