// ────────────────────────────────────────────────────────────────────────────
// PlayerController.cpp
// 플레이어 입력 처리 Controller 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "InputComponent.h"
#include "InputManager.h"
#include "PlayerCameraManager.h"
#include "CameraComponent.h"
#include "World.h"

APlayerController::APlayerController()
	: InputManager(nullptr)
	, bInputEnabled(true)
	, bShowMouseCursor(true)
	, bIsMouseLocked(false)
	, MouseSensitivity(1.0f)
	, PlayerCameraManager(nullptr)
{
}

APlayerController::~APlayerController()
{
	// PlayerCameraManager는 World가 관리하므로 여기서 직접 삭제하지 않음
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::BeginPlay()
{
	Super::BeginPlay();

	// InputManager 참조 획득
	InputManager = &UInputManager::GetInstance();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->BeginPlay();
	}

	// 기존 게임모드, 컨트롤러, 인풋 컴포넌트, 폰 구조를 이해하기 어려워서 급한대로 하드코딩함(수정 필요)
	for (AActor* Actor : World->GetActors())
	{
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			Possess(Pawn);
			break;
		}
	}
}

void APlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 입력 처리
	if (bInputEnabled)
	{
		ProcessPlayerInput();
	}

	// 카메라 업데이트는 PlayerCameraManager의 Tick에서 자동으로 처리됨
}

void APlayerController::Possess(APawn* InPawn)
{
	Super::Possess(InPawn);
}

void APlayerController::UnPossess()
{
	Super::UnPossess();
}

void APlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		// Pawn의 InputComponent에 입력 바인딩 설정
		UInputComponent* InputComp = InPawn->GetInputComponent();
		if (InputComp)
		{
			InPawn->SetupPlayerInputComponent(InputComp);
		}

		// PlayerCameraManager 생성 (OnPossess가 BeginPlay보다 먼저 호출될 수 있음)
		if (!PlayerCameraManager && World && World->bPie)
		{
			PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		}

		// TODO: Pawn의 CameraComponent를 찾아서 PlayerCameraManager에 설정
		// Week11에서는 SetViewCamera(UCameraComponent*)를 사용
		// 2단계에서 Pawn 구현 후 활성화
	}
}

void APlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 처리
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::ProcessPlayerInput()
{
	if (!PossessedPawn)
	{
		return;
	}

	float ThrottleInput = 0.0f;
	float SteerInput = 0.0f;
	const float StickThreshold = 0.25f; 

	// Forward (W): 키보드 W OR 왼쪽 스틱 위로 OR 십자키 위
	if (InputManager->IsKeyDown('W') || 
		InputManager->GetGamepadLeftStickY() > StickThreshold || 
		InputManager->IsGamepadButtonDown(EGamepadButton::DPadUp))
	{
		ThrottleInput += 1.0f;
	}

	// Right (D): 키보드 D OR 왼쪽 스틱 오른쪽 OR 십자키 오른쪽
	if (InputManager->IsKeyDown('D') || 
		InputManager->GetGamepadLeftStickX() > StickThreshold || 
		InputManager->IsGamepadButtonDown(EGamepadButton::DPadRight))
	{
		SteerInput += 1.0f;
	}

	// Left (A): 키보드 A OR 왼쪽 스틱 왼쪽 OR 십자키 왼쪽
	if (InputManager->IsKeyDown('A') || 
		InputManager->GetGamepadLeftStickX() < -StickThreshold || 
		InputManager->IsGamepadButtonDown(EGamepadButton::DPadLeft))
	{
		SteerInput -= 1.0f;
	}

	// Backward (S): 키보드 S OR 왼쪽 스틱 아래로 OR 십자키 아래
	if (InputManager->IsKeyDown('S') || 
		InputManager->GetGamepadLeftStickY() < -StickThreshold || 
		InputManager->IsGamepadButtonDown(EGamepadButton::DPadDown))
	{
		ThrottleInput -= 1.0f;
	}

	PossessedPawn->ThrottleSteerInput(ThrottleInput, SteerInput);
	// Pawn의 InputComponent에 입력 전달
	UInputComponent* InputComp = PossessedPawn->GetInputComponent();
	if (InputComp)
	{
		InputComp->ProcessInput();
	}
}

void APlayerController::ShowMouseCursor()
{
	bShowMouseCursor = true;

	if (InputManager)
	{
		InputManager->SetCursorVisible(true);
	}
}

void APlayerController::HideMouseCursor()
{
	bShowMouseCursor = false;

	if (InputManager)
	{
		InputManager->SetCursorVisible(false);
	}
}

void APlayerController::LockMouseCursor()
{
	bIsMouseLocked = true;

	if (InputManager)
	{
		InputManager->LockCursor();
	}
}

void APlayerController::UnlockMouseCursor()
{
	bIsMouseLocked = false;

	if (InputManager)
	{
		InputManager->ReleaseCursor();
	}
}

UCameraComponent* APlayerController::GetCameraComponentForRendering() const
{
	if (PlayerCameraManager)
	{
		return PlayerCameraManager->GetViewCamera();
	}
	return nullptr;
}

// ────────────────────────────────────────────────────────────────────────────
// 복제
// ────────────────────────────────────────────────────────────────────────────

void APlayerController::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void APlayerController::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: 필요한 멤버 변수 직렬화 추가
}
