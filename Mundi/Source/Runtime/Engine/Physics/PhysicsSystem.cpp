#include "pch.h"
#include "PhysicsSystem.h"
#include "PhysicsScene.h"
#include "FKConvexElem.h"

#define MAX_PHYSX_THREADS 8

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
	if (!PxInitVehicleSDK(*Physics))
	{
		UE_LOG("비히클 SDK 초기화 실패! 망했다!");
	}
	// Up과 Forward 인자로 받음
	PxVehicleSetBasisVectors(PhysxConverter::ToPxVec3(FVector(0.0f, 0.0f, 1.0f)), PhysxConverter::ToPxVec3(FVector(1.0f, 0.0f, 0.0f)));

	PxVehicleSetUpdateMode(PxVehicleUpdateMode::eVELOCITY_CHANGE);

	// Params: Scale, TargetPlatform(기본값: 현재 플랫폼. 플랫폼마다 최적화 방식이 다름) 등 
	Cooking = PxCreateCooking(PX_PHYSICS_VERSION, *Foundation, PxCookingParams(PxTolerancesScale()));

	int LogicCores = std::thread::hardware_concurrency();

	int TargetThread = LogicCores - 2;

	int PhysxThreadCount = std::clamp(TargetThread, 1, MAX_PHYSX_THREADS);
	
	Dispatcher = PxDefaultCpuDispatcherCreate(PhysxThreadCount);

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

	if (PxFilterObjectIsTrigger(Attributes0) || PxFilterObjectIsTrigger(Attributes1))
	{
		// Trigger면 뚫고 지나가야 하니 반발력(Solve) 끄기
		PairFlags &= ~PxPairFlag::eSOLVE_CONTACT;
        
		// Trigger 진입/이탈 알림 켜기
		PairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
		PairFlags |= PxPairFlag::eNOTIFY_TOUCH_LOST;
        
		return PxFilterFlag::eDEFAULT;
	}
	
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

	auto Scene = std::make_unique<FPhysicsScene>(NewScene);
	Scene->InitVehicleSDK();
	return Scene;
}

PxConvexMesh* FPhysicsSystem::CreateConvexMesh(FKConvexElem* ConvexElement)
{
	TArray<physx::PxVec3> Points;
	Points.reserve(ConvexElement->VertexData.Num());
	for (const FVector& Vertex : ConvexElement->VertexData)
	{
		Points.Add(PhysxConverter::ToPxVec3(Vertex));
	}
	physx::PxConvexMeshDesc ConvexDesc;
	ConvexDesc.points.count = Points.Num();
	ConvexDesc.points.data = Points.GetData();
	ConvexDesc.points.stride = sizeof(physx::PxVec3);
	ConvexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

	if (Cooking)
	{
		physx::PxDefaultMemoryOutputStream WriteBuffer;
		physx::PxConvexMeshCookingResult::Enum Result;
		bool bSuccess = Cooking->cookConvexMesh(ConvexDesc, WriteBuffer, &Result);

		if (bSuccess)
		{
			// 쿠킹된 메시에서 버텍스/인덱스 추출
			physx::PxDefaultMemoryInputData ReadBuffer(WriteBuffer.getData(), WriteBuffer.getSize());
			physx::PxConvexMesh* ConvexMesh = Physics->createConvexMesh(ReadBuffer);
			return ConvexMesh;
		}
	}
	return nullptr;
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
	PxCloseVehicleSDK();
	if (Dispatcher) Dispatcher->release();
	if (Cooking) Cooking->release();
	if (Material) Material->release();
	if (Physics) Physics->release();
	if (Pvd) { PxPvdTransport* Transport = Pvd->getTransport();  Pvd->release(); Transport->release(); }
	if (Foundation) Foundation->release();

}
