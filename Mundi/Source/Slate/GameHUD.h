#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <unordered_map>
#include "Color.h"
#include "UEContainer.h"

// Forward declarations for D2D/WIC interfaces
struct ID2D1Factory1;
struct ID2D1Device;
struct ID2D1DeviceContext;
struct ID2D1SolidColorBrush;
struct ID2D1Bitmap;
struct IDWriteFactory;
struct IDWriteTextFormat;
struct IWICImagingFactory;

/**
 * @class UGameHUD
 * @brief D2D 기반 게임 HUD 렌더링 시스템
 *
 * Lua에서 사용 가능한 유연한 HUD API를 제공:
 * - 텍스트 렌더링 (폰트 크기, 색상, 배경 지원)
 * - 이미지 렌더링 (PNG 로드 및 캐싱)
 * - 절대 좌표 및 상대 좌표 (0~1) 지원
 *
 * 사용 예 (Lua):
 *   HUD:DrawText("Score: 100", 10, 10, 24, Color(1,1,1,1))
 *   HUD:DrawTextRel("Ammo", 0.5, 0.02, 28, Color(1,1,0,1))
 *   HUD:DrawImage("Data/Textures/icon.png", 100, 100, 32, 32)
 */
class UGameHUD
{
public:
    static UGameHUD& Get();

    /**
     * @brief D2D 리소스 초기화
     * @param device D3D11 디바이스
     * @param context D3D11 디바이스 컨텍스트
     * @param swapChain 스왑 체인
     */
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain);

    /** @brief 리소스 해제 */
    void Shutdown();

    /** @brief 프레임 렌더링 시작 (Present 전에 호출) */
    void BeginFrame();

    /** @brief 프레임 렌더링 종료 */
    void EndFrame();

    /** @brief 화면 크기 설정 (상대 좌표 계산용) */
    void SetScreenSize(float width, float height);

    /** @brief 화면 오프셋 설정 (뷰포트 위치) */
    void SetScreenOffset(float x, float y);

    // ===== Lua에서 호출 가능한 API =====

    /**
     * @brief 텍스트 렌더링 (절대 좌표)
     * @param text 표시할 텍스트
     * @param x X 좌표 (픽셀)
     * @param y Y 좌표 (픽셀)
     * @param fontSize 폰트 크기
     * @param color 텍스트 색상
     */
    void DrawGameText(const FString& text, float x, float y, float fontSize, const FLinearColor& color);

    /**
     * @brief 텍스트 렌더링 (상대 좌표)
     * @param text 표시할 텍스트
     * @param rx X 좌표 (0~1, 화면 비율)
     * @param ry Y 좌표 (0~1, 화면 비율)
     * @param fontSize 폰트 크기
     * @param color 텍스트 색상
     */
    void DrawTextRel(const FString& text, float rx, float ry, float fontSize, const FLinearColor& color);

    /**
     * @brief 배경이 있는 텍스트 렌더링 (절대 좌표)
     * @param text 표시할 텍스트
     * @param x X 좌표 (픽셀)
     * @param y Y 좌표 (픽셀)
     * @param fontSize 폰트 크기
     * @param textColor 텍스트 색상
     * @param bgColor 배경 색상
     */
    void DrawTextWithBg(const FString& text, float x, float y, float fontSize,
                        const FLinearColor& textColor, const FLinearColor& bgColor);

    /**
     * @brief 배경이 있는 텍스트 렌더링 (상대 좌표)
     */
    void DrawTextWithBgRel(const FString& text, float rx, float ry, float fontSize,
                           const FLinearColor& textColor, const FLinearColor& bgColor);

    /**
     * @brief 이미지 렌더링 (절대 좌표)
     * @param imagePath 이미지 경로
     * @param x X 좌표 (픽셀)
     * @param y Y 좌표 (픽셀)
     * @param width 너비 (픽셀)
     * @param height 높이 (픽셀)
     */
    void DrawImage(const FString& imagePath, float x, float y, float width, float height);

    /**
     * @brief 이미지 렌더링 (상대 좌표)
     * @param imagePath 이미지 경로
     * @param rx X 좌표 (0~1)
     * @param ry Y 좌표 (0~1)
     * @param rwidth 너비 (0~1)
     * @param rheight 높이 (0~1)
     */
    void DrawImageRel(const FString& imagePath, float rx, float ry, float rwidth, float rheight);

    /**
     * @brief 이미지 프리로드 (게임 시작 시 호출 권장)
     * @param imagePath 이미지 경로
     */
    void LoadGameImage(const FString& imagePath);

    /** @brief HUD 표시/숨김 설정 */
    void SetVisible(bool visible);

    /** @brief HUD 표시 여부 반환 */
    bool IsVisible() const;

    /** @brief 화면 너비 반환 (Lua에서 사용) */
    float GetScreenWidth() const { return ScreenWidth; }

    /** @brief 화면 높이 반환 (Lua에서 사용) */
    float GetScreenHeight() const { return ScreenHeight; }

    /** @brief 화면 X 오프셋 반환 (Lua에서 사용) */
    float GetScreenOffsetX() const { return ScreenOffsetX; }

    /** @brief 화면 Y 오프셋 반환 (Lua에서 사용) */
    float GetScreenOffsetY() const { return ScreenOffsetY; }

private:
    UGameHUD() = default;
    ~UGameHUD() = default;
    UGameHUD(const UGameHUD&) = delete;
    UGameHUD& operator=(const UGameHUD&) = delete;

    /** @brief D2D 리소스 지연 초기화 */
    void EnsureD2DInitialized();

    /** @brief 특정 폰트 크기의 TextFormat 가져오기 (캐싱) */
    IDWriteTextFormat* GetTextFormat(float fontSize);

    /** @brief WIC를 사용해 이미지 로드 */
    ID2D1Bitmap* LoadBitmapFromFile(const FString& path);

    /** @brief 텍스트 렌더링 내부 구현 */
    void DrawTextInternal(const FString& text, float x, float y, float fontSize,
                          const FLinearColor& textColor, bool hasBg = false,
                          const FLinearColor& bgColor = FLinearColor());

private:
    bool bInitialized = false;
    bool bD2DInitialized = false;
    bool bVisible = true;
    bool bFrameActive = false;  // BeginFrame/EndFrame 사이인지

    float ScreenWidth = 1280.0f;
    float ScreenHeight = 720.0f;
    float ScreenOffsetX = 0.0f;
    float ScreenOffsetY = 0.0f;

    // D3D11 리소스 (외부에서 전달받음)
    ID3D11Device* D3DDevice = nullptr;
    ID3D11DeviceContext* D3DContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    // D2D 리소스 (자체 생성)
    ID2D1Factory1* D2dFactory = nullptr;
    ID2D1Device* D2dDevice = nullptr;
    ID2D1DeviceContext* D2dCtx = nullptr;
    IDWriteFactory* Dwrite = nullptr;
    ID2D1SolidColorBrush* CachedBrush = nullptr;

    // WIC Factory (이미지 로딩용)
    IWICImagingFactory* WICFactory = nullptr;

    // 폰트 크기별 TextFormat 캐시
    std::unordered_map<int, IDWriteTextFormat*> TextFormatCache;

    // 이미지 경로별 Bitmap 캐시
    std::unordered_map<std::string, ID2D1Bitmap*> BitmapCache;

    // 프레임별 리소스 (매 프레임 생성/해제)
    struct ID2D1Bitmap1* FrameTargetBmp = nullptr;
    struct IDXGISurface* FrameSurface = nullptr;
};
