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

// 싱글톤 매니저
class FClothSystem
{
public:
    static FClothSystem& GetInstance();

    void Initialize();
    void Shutdown();
    void Update(float DeltaTime);   // Solver 시뮬레이션 실행

    static void Destroy();  // 싱글톤 정리

    nv::cloth::Factory* GetFactory() { return Factory; }
    nv::cloth::Solver* GetSolver() { return Solver; }

private:
    FClothSystem() = default;
    ~FClothSystem();

    FClothAllocator Allocator;
    FClothErrorCallback ErrorCallback;
    FClothAssertHandler AssertHandler;

    nv::cloth::Factory* Factory = nullptr;
    nv::cloth::Solver* Solver = nullptr;

    static FClothSystem* Instance;
};