#pragma once

#include <windows.h>
#include <cmath>

#include "Object.h"
#include "Vector.h"
#include "ImGui/imgui.h"

// 게임패드 버튼 매핑
enum class EGamepadButton
{
    DPadUp = XINPUT_GAMEPAD_DPAD_UP,
    DPadDown = XINPUT_GAMEPAD_DPAD_DOWN,
    DPadLeft = XINPUT_GAMEPAD_DPAD_LEFT,
    DPadRight = XINPUT_GAMEPAD_DPAD_RIGHT,
    Start = XINPUT_GAMEPAD_START,
    Back = XINPUT_GAMEPAD_BACK,
    LeftThumb = XINPUT_GAMEPAD_LEFT_THUMB,
    RightThumb = XINPUT_GAMEPAD_RIGHT_THUMB,
    LeftShoulder = XINPUT_GAMEPAD_LEFT_SHOULDER,
    RightShoulder = XINPUT_GAMEPAD_RIGHT_SHOULDER,
    A = XINPUT_GAMEPAD_A,
    B = XINPUT_GAMEPAD_B,
    X = XINPUT_GAMEPAD_X,
    Y = XINPUT_GAMEPAD_Y
};

// 게임패드 상태 구조체
struct FGamepadState
{
    bool bConnected = false;
    float LeftStickX = 0.0f;
    float LeftStickY = 0.0f;
    float RightStickX = 0.0f;
    float RightStickY = 0.0f;
    float LeftTrigger = 0.0f;
    float RightTrigger = 0.0f;
    WORD Buttons = 0; // 비트마스크
};

// 마우스 버튼 상수
enum EMouseButton
{
    LeftButton = 0,
    RightButton = 1,
    MiddleButton = 2,
    XButton1 = 3,
    XButton2 = 4,
    MaxMouseButtons = 5
};

class UInputManager : public UObject
{
public:
    DECLARE_CLASS(UInputManager, UObject)

    // 생성자/소멸자 (싱글톤)
    UInputManager();
protected:
    ~UInputManager() override;

    // 복사 방지
    UInputManager(const UInputManager&) = delete;
    UInputManager& operator=(const UInputManager&) = delete;

public:
    // 싱글톤 접근자
    static UInputManager& GetInstance();

    // 생명주기
    void Initialize(HWND hWindow);
    void Update(); // 매 프레임 호출
    void ProcessMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 마우스 함수들
    FVector2D GetMousePosition() const { return MousePosition; }
    FVector2D GetMouseDelta() const { return MousePosition - PreviousMousePosition; }
    // 화면 크기 (픽셀) - 매 호출 시 동적 조회
    FVector2D GetScreenSize() const;

	void SetLastMousePosition(const FVector2D& Pos) { PreviousMousePosition = Pos; }

    bool IsMouseButtonDown(EMouseButton Button) const;
    bool IsMouseButtonPressed(EMouseButton Button) const; // 이번 프레임에 눌림
    bool IsMouseButtonReleased(EMouseButton Button) const; // 이번 프레임에 떼짐

    // 키보드 함수들
    bool IsKeyDown(int KeyCode) const;
    bool IsKeyPressed(int KeyCode) const; // 이번 프레임에 눌림
    bool IsKeyReleased(int KeyCode) const; // 이번 프레임에 떼짐
    void ResetAllKeyStates(); // 모든 키 상태 리셋 (포커스 잃을 때 호출)

    // 마우스 휠 함수들
    float GetMouseWheelDelta() const { return MouseWheelDelta; }
    // 디버그 로그 토글
    void SetDebugLoggingEnabled(bool bEnabled) { bEnableDebugLogging = bEnabled; }
    bool IsDebugLoggingEnabled() const { return bEnableDebugLogging; }

    bool GetIsGizmoDragging() const { return bIsGizmoDragging; }
    void SetIsGizmoDragging(bool bInGizmoDragging) { bIsGizmoDragging = bInGizmoDragging; }

    uint32 GetDraggingAxis() const { return DraggingAxis; }
    void SetDraggingAxis(uint32 Axis) { DraggingAxis = Axis; }

    // 커서 제어 함수
    void SetCursorVisible(bool bVisible);
    void LockCursor();
    void ReleaseCursor();
    bool IsCursorLocked() const { return bIsCursorLocked; }

    // 마우스 캡처 (뷰포트 밖에서도 마우스 이벤트 수신)
    void CaptureMouse();
    void ReleaseMouseCapture();

    // 게임패드 함수들 (ControllerIndex: 0~3)
    bool IsGamepadConnected(int ControllerIndex = 0) const;
    
    // 버튼 입력 (키보드와 동일한 패턴)
    bool IsGamepadButtonDown(EGamepadButton Button, int ControllerIndex = 0) const;
    bool IsGamepadButtonPressed(EGamepadButton Button, int ControllerIndex = 0) const; // 이번 프레임에 눌림
    bool IsGamepadButtonReleased(EGamepadButton Button, int ControllerIndex = 0) const; // 이번 프레임에 떼짐

    // 아날로그 입력 (반환값: -1.0 ~ 1.0 또는 0.0 ~ 1.0)
    float GetGamepadLeftStickX(int ControllerIndex = 0) const { return IsGamepadConnected(ControllerIndex) ? GamepadStates[ControllerIndex].LeftStickX : 0.0f; }
    float GetGamepadLeftStickY(int ControllerIndex = 0) const { return IsGamepadConnected(ControllerIndex) ? GamepadStates[ControllerIndex].LeftStickY : 0.0f; }
    float GetGamepadRightStickX(int ControllerIndex = 0) const { return IsGamepadConnected(ControllerIndex) ? GamepadStates[ControllerIndex].RightStickX : 0.0f; }
    float GetGamepadRightStickY(int ControllerIndex = 0) const { return IsGamepadConnected(ControllerIndex) ? GamepadStates[ControllerIndex].RightStickY : 0.0f; }
    float GetGamepadLeftTrigger(int ControllerIndex = 0) const { return IsGamepadConnected(ControllerIndex) ? GamepadStates[ControllerIndex].LeftTrigger : 0.0f; }
    float GetGamepadRightTrigger(int ControllerIndex = 0) const { return IsGamepadConnected(ControllerIndex) ? GamepadStates[ControllerIndex].RightTrigger : 0.0f; }
    // 진동 설정 (0.0 ~ 1.0)
    void SetGamepadVibration(float LeftMotor, float RightMotor, int ControllerIndex = 0);

private:
    // 내부 헬퍼 함수들
    void UpdateMousePosition(int X, int Y);
    void UpdateMouseButton(EMouseButton Button, bool bPressed);
    void UpdateKeyState(int KeyCode, bool bPressed);
    void UpdateGamepadStates();
    float ApplyDeadzone(float Value, float Deadzone);

    // 윈도우 핸들
    HWND WindowHandle;

    // 마우스 상태
    FVector2D MousePosition;
    FVector2D PreviousMousePosition;
    // 스크린/뷰포트 사이즈 (클라이언트 영역 픽셀)
    FVector2D ScreenSize;
    bool MouseButtons[MaxMouseButtons];
    bool PreviousMouseButtons[MaxMouseButtons];

    // 마우스 휠 상태
    float MouseWheelDelta;

    // 키보드 상태 (Virtual Key Code 기준)
    bool KeyStates[256];
    bool PreviousKeyStates[256];

    // 마스터 디버그 로그 온/오프
    bool bEnableDebugLogging = false;

    bool bIsGizmoDragging = false;
    uint32 DraggingAxis = 0;

    // 커서 잠금 상태
    bool bIsCursorLocked = false;
    FVector2D LockedCursorPosition; // 우클릭한 위치 (기준점)

    // 게임패드 상태 변수 (최대 4개 지원)
    static const int MaxControllers = 4;
    FGamepadState GamepadStates[MaxControllers];
    FGamepadState PreviousGamepadStates[MaxControllers];
};
