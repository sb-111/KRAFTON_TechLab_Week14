#include "pch.h"
#include "PhysicsSystem.h"
#include "PhysicsScene.h"

// ===== 기본 필터 셰이더 =====
// PhysX Joint가 연결된 바디 간 충돌을 자동으로 비활성화함
// 따라서 커스텀 래그돌 필터 불필요 - 표준 방식 사용
static PxFilterFlags DefaultFilterShader(
    PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1,
    PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
    // 트리거 처리
    if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
    {
        pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    }

    // 기본 충돌 처리 (Joint가 인접 본 충돌을 자동 비활성화)
    pairFlags = PxPairFlag::eCONTACT_DEFAULT;
    return PxFilterFlag::eDEFAULT;
}

FPhysicsSystem* FPhysicsSystem::Instance = nullptr;
FPhysicsSystem& FPhysicsSystem::GetInstance()
{
	if (Instance == nullptr)
	{
		Instance = new FPhysicsSystem();
		Instance->Initialize();
	}

	return *Instance;
}

FPhysicsSystem::~FPhysicsSystem()
{
}

void FPhysicsSystem::Update(float DeltaTime)
{


}
void FPhysicsSystem::Initialize()
{

	// PX_PHYSICS_VERSION: 헤더파일이랑 라이브러리 버전 맞춤
	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, Allocator, ErrorCallback);
	if (!Foundation)
	{
		return;
	}

	Pvd = PxCreatePvd(*Foundation);
	// 127.0.0.1 : 로컬 호스트(내 컴퓨타), 5425: Pvd 기본 포트(바꾸려면 Pvd에서도 바꿔야함)
	PxPvdTransport* Transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
	// eDEBUG: 씬 지오메트리, 위치 회전 정보, 디버그 라인 띄움
	// ePROFILE: 물리 연산 시간 Stat
	// eMEMORY: Physx 메모리 상태 추적
	Pvd->connect(*Transport, PxPvdInstrumentationFlag::eALL);

	// PxTolerancesScale: 단위 설정, 기본값 1.0f가 1m -> 1.0f를 1cm로 만들려면 100을 넣으면 됨 -> 100.0f가 1m
	// TrackOutstandingAllocation: 메모리 누수 추적, Release시에 false로 하는게 나음
	// Pvd: 위에서 설정한 Pvd랑 연결
	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale(), true, Pvd);

	// 확장팩: Joint 물리 추가, Pvd에 조인트 디버그 렌더링 기능 추가
	PxInitExtensions(*Physics, Pvd);

	// Params: Scale, TargetPlatform(기본값: 현재 플랫폼. 플랫폼마다 최적화 방식이 다름) 등 
	Cooking = PxCreateCooking(PX_PHYSICS_VERSION, *Foundation, PxCookingParams(PxTolerancesScale()));

	Dispatcher = PxDefaultCpuDispatcherCreate(3);

	// 정지마찰, 운동마찰, 반발 계수(디폴트, 원하면 새로 생성해서 사용)
	Material = Physics->createMaterial(0.5f, 0.5f, 0.6f);

}

PxPhysics* FPhysicsSystem::GetPhysics()
{
	return Physics;
}

PxMaterial* FPhysicsSystem::GetDefaultMaterial()
{
	return Material;
}

static PxFilterFlags SimulationFilerShader(PxFilterObjectAttributes Attributes0, PxFilterData FilterData0,
	PxFilterObjectAttributes Attributes1, PxFilterData FilterData1,
	PxPairFlags& PairFlags, const void* ConstantBlock, PxU32 ConstantBlockSize)
{
	// 
	PairFlags = PxPairFlag::eCONTACT_DEFAULT;

	// 충돌 시작하면 onContact 호출
	// 여기서 추가한 플래그가 콜백할때 쓰이는 거임.
	// 충돌 후 떨어졌을 때도 알고 싶으면 eNOTIFY_TOUCH_LOST 추가
	PairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND
		| PxPairFlag::eNOTIFY_CONTACT_POINTS;
	

	return PxFilterFlag::eDEFAULT;
}

std::unique_ptr<FPhysicsScene> FPhysicsSystem::CreateScene()
{
	PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

	SceneDesc.cpuDispatcher = Dispatcher;
	// filterShader: 충돌 처리 로직, 일단 기본 필터 사용 수정 가능
	SceneDesc.filterShader = SimulationFilerShader;
	// Persistent contact manifold: 지속적인 접촉 시 떨림 완화
	// ENABLE_ACTIVE_ACTORS: 움직인 엑터만 명단 뽑아놓음
	SceneDesc.flags |= PxSceneFlag::eENABLE_PCM | PxSceneFlag::eENABLE_ACTIVE_ACTORS | PxSceneFlag::eENABLE_CCD;

	PxScene* NewScene = Physics->createScene(SceneDesc);

	PxPvdSceneClient* PvdClient = NewScene->getScenePvdClient();
	if (PvdClient)
	{
		// 필요한 디버그 정보만 Pvd에 보내는 역할
		// 조인트 보여줌
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		// 충돌부위 보여줌
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		// 레이캐스트 시 레이 보여줌
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}

	return std::make_unique<FPhysicsScene>(NewScene);
}

void FPhysicsSystem::Destroy()
{
	if (Instance)
	{
		Instance->Shutdown();

		delete Instance;
		Instance = nullptr;
	}
}
void FPhysicsSystem::Shutdown()
{
	PxCloseExtensions();
	if (Dispatcher) Dispatcher->release();
	if (Cooking) Cooking->release();
	if (Material) Material->release();
	if (Physics) Physics->release();
	if (Pvd) { PxPvdTransport* Transport = Pvd->getTransport();  Pvd->release(); Transport->release(); }
	if (Foundation) Foundation->release();

}
