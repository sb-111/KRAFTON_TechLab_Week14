#pragma once

using namespace physx;
class FPhysicsScene;

#define VEHICLE_FILTER 0x00000001

class FPhysicsSystem
{
public:
	static FPhysicsSystem& GetInstance();

	void Initialize();

    void Update(float DeltaTime);

    PxPhysics* GetPhysics();

    std::unique_ptr<FPhysicsScene> CreateScene();

    PxMaterial* GetDefaultMaterial();

    static void Destroy();

	void Shutdown();

    FPhysicsSystem() = default;

private:

    // 메모리 관리
    PxDefaultAllocator Allocator;

    // 에러 리포터
    PxDefaultErrorCallback ErrorCallback;

    // OS마다 메모리 관리 방법, 에러 출력 방법, 계산 방법(SIMD같은) 등이 다름, 그걸 래핑하는 역할
    PxFoundation* Foundation = nullptr;

    // Physx visual debugger, 시뮬레이션 결과를 Pvd로 네트워킹해줌
    PxPvd* Pvd = nullptr;         

    // D3D의 Device같은 역할, 물리 객체 팩토리
    PxPhysics* Physics = nullptr;

    // 복잡한 메쉬의 정점 데이터를 받아서 Physx가 계산하기 쉬운 형태로 변환(Bvh쓴다고 함)
    PxCooking* Cooking = nullptr;     

    // Physx에 스레드 배분해줌
    PxDefaultCpuDispatcher* Dispatcher = nullptr;     

    // 표면 성질 정의(마찰, 반발계수)
    PxMaterial* Material = nullptr;    

    static FPhysicsSystem* Instance;

	~FPhysicsSystem();
	FPhysicsSystem(const FPhysicsSystem&) = delete;
	FPhysicsSystem& operator=(const FPhysicsSystem&) = delete;
}; 