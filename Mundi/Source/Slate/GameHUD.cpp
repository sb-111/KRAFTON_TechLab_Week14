#include "pch.h"

#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

#include "GameHUD.h"

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "windowscodecs")

static inline void SafeRelease(IUnknown* p) { if (p) p->Release(); }

UGameHUD& UGameHUD::Get()
{
    static UGameHUD Instance;
    return Instance;
}

void UGameHUD::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, IDXGISwapChain* InSwapChain)
{
    D3DDevice = InDevice;
    D3DContext = InContext;
    SwapChain = InSwapChain;
    bInitialized = (D3DDevice && D3DContext && SwapChain);
}

void UGameHUD::Shutdown()
{
    if (!bD2DInitialized)
    {
        D3DDevice = nullptr;
        D3DContext = nullptr;
        SwapChain = nullptr;
        bInitialized = false;
        return;
    }

    if (bFrameActive && D2dCtx)
    {
        D2dCtx->EndDraw();
        D2dCtx->SetTarget(nullptr);
        SafeRelease(FrameTargetBmp);
        SafeRelease(FrameSurface);
        FrameTargetBmp = nullptr;
        FrameSurface = nullptr;
        bFrameActive = false;
    }

    for (auto& pair : BitmapCache)
    {
        SafeRelease(BitmapCache[pair.first]);
    }
    BitmapCache.clear();

    for (auto& [size, format] : TextFormatCache)
    {
        SafeRelease(format);
    }
    TextFormatCache.clear();

    SafeRelease(CachedBrush);
    SafeRelease(Dwrite);
    
    // 순서 중요: Context -> Device -> Factory 순서가 안전합니다.
    SafeRelease(D2dCtx);
    SafeRelease(D2dDevice);
    SafeRelease(D2dFactory);
    SafeRelease(WICFactory);

    bD2DInitialized = false;

    // COM 정리
    CoUninitialize();

    D3DDevice = nullptr;
    D3DContext = nullptr;
    SwapChain = nullptr;
    bInitialized = false;
}

void UGameHUD::EnsureD2DInitialized()
{
    if (bD2DInitialized || !bInitialized || !D3DDevice)
        return;

    static bool bCOMInitialized = false;
    if (!bCOMInitialized)
    {
        if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        {
            return;
        }
        bCOMInitialized = true;
    }

    // D2D Factory 생성
    D2D1_FACTORY_OPTIONS Opts{};
#ifdef _DEBUG
    Opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &Opts, (void**)&D2dFactory)))
        return;

    // DXGI Device 쿼리
    IDXGIDevice* DxgiDevice = nullptr;
    if (FAILED(D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DxgiDevice)))
    {
        SafeRelease(D2dFactory);
        return;
    }

    // D2D Device 생성
    if (FAILED(D2dFactory->CreateDevice(DxgiDevice, &D2dDevice)))
    {
        SafeRelease(DxgiDevice);
        SafeRelease(D2dFactory);
        return;
    }
    SafeRelease(DxgiDevice);

    // D2D DeviceContext 생성
    if (FAILED(D2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2dCtx)))
    {
        SafeRelease(D2dDevice);
        SafeRelease(D2dFactory);
        return;
    }

    // DWrite Factory 생성
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&Dwrite)))
    {
        SafeRelease(D2dCtx);
        SafeRelease(D2dDevice);
        SafeRelease(D2dFactory);
        return;
    }

    // Brush 생성 (색상은 SetColor로 변경)
    if (FAILED(D2dCtx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &CachedBrush)))
    {
        SafeRelease(Dwrite);
        SafeRelease(D2dCtx);
        SafeRelease(D2dDevice);
        SafeRelease(D2dFactory);
        return;
    }

    // WIC Factory 생성 (이미지 로딩용)
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&WICFactory))))
    {
        // WIC 실패해도 텍스트 렌더링은 가능하도록 계속 진행
        WICFactory = nullptr;
    }

    bD2DInitialized = true;
}

IDWriteTextFormat* UGameHUD::GetTextFormat(float fontSize)
{
    int sizeKey = static_cast<int>(fontSize);

    auto it = TextFormatCache.find(sizeKey);
    if (it != TextFormatCache.end())
        return it->second;

    // 새 TextFormat 생성
    IDWriteTextFormat* newFormat = nullptr;
    if (FAILED(Dwrite->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"ko-kr",
        &newFormat)))
    {
        return nullptr;
    }

    newFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    newFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    TextFormatCache[sizeKey] = newFormat;
    return newFormat;
}

ID2D1Bitmap* UGameHUD::LoadBitmapFromFile(const FString& path)
{
    if (!WICFactory || !D2dCtx)
        return nullptr;

    // FString을 제대로 std::string으로 변환
    std::string pathKey;
    if constexpr (std::is_same_v<FString, std::string>)
    {
        pathKey = path;
    }
    else
    {
        // 일반적인 FString은 이렇게
        pathKey = std::string(path.begin(), path.end());
    }

    // 경로를 정규화 (대소문자, 슬래시 통일)
    std::ranges::transform(pathKey, pathKey.begin(), ::tolower);
    std::ranges::replace(pathKey, '\\', '/');

    // 캐시 확인
    auto it = BitmapCache.find(pathKey);
    if (it != BitmapCache.end())
    {
        if (it->second)  // nullptr 체크도 추가
            return it->second;
    }

    // UTF-8 -> Wide string 변환
    std::wstring WidePath = UTF8ToWide(path);

    // WIC Decoder 생성
    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(WICFactory->CreateDecoderFromFilename(
        WidePath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder)))
    {
        return nullptr;
    }

    // 프레임 가져오기
    IWICBitmapFrameDecode* frame = nullptr;
    if (FAILED(decoder->GetFrame(0, &frame)))
    {
        SafeRelease(decoder);
        return nullptr;
    }

    // 포맷 컨버터 생성
    IWICFormatConverter* converter = nullptr;
    if (FAILED(WICFactory->CreateFormatConverter(&converter)))
    {
        SafeRelease(frame);
        SafeRelease(decoder);
        return nullptr;
    }

    // BGRA 포맷으로 변환
    if (FAILED(converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeMedianCut)))
    {
        SafeRelease(converter);
        SafeRelease(frame);
        SafeRelease(decoder);
        return nullptr;
    }

    // D2D Bitmap 생성
    ID2D1Bitmap* bitmap = nullptr;
    if (FAILED(D2dCtx->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap)))
    {
        SafeRelease(converter);
        SafeRelease(frame);
        SafeRelease(decoder);
        return nullptr;
    }

    // 리소스 해제
    SafeRelease(converter);
    SafeRelease(frame);
    SafeRelease(decoder);

    // 캐시에 저장
    BitmapCache[pathKey] = bitmap;
    return bitmap;
}

void UGameHUD::SetScreenSize(float width, float height)
{
    ScreenWidth = width;
    ScreenHeight = height;
}

void UGameHUD::SetScreenOffset(float x, float y)
{
    ScreenOffsetX = x;
    ScreenOffsetY = y;
}

void UGameHUD::BeginFrame()
{
    if (!bVisible || !bInitialized || bFrameActive)
        return;

    EnsureD2DInitialized();
    if (!bD2DInitialized)
        return;

    // 백버퍼에서 DXGI Surface 가져오기
    if (FAILED(SwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&FrameSurface)))
        return;

    // D2D 렌더 타겟 비트맵 생성
    D2D1_BITMAP_PROPERTIES1 BmpProps = {};
    BmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    BmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    BmpProps.dpiX = 96.0f;
    BmpProps.dpiY = 96.0f;
    BmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    if (FAILED(D2dCtx->CreateBitmapFromDxgiSurface(FrameSurface, &BmpProps, &FrameTargetBmp)))
    {
        SafeRelease(FrameSurface);
        FrameSurface = nullptr;
        return;
    }

    D2dCtx->SetTarget(FrameTargetBmp);
    D2dCtx->BeginDraw();
    bFrameActive = true;
}

void UGameHUD::EndFrame()
{
    if (!bFrameActive)
        return;

    D2dCtx->EndDraw();
    D2dCtx->SetTarget(nullptr);

    SafeRelease(FrameTargetBmp);
    SafeRelease(FrameSurface);
    FrameTargetBmp = nullptr;
    FrameSurface = nullptr;
    bFrameActive = false;
}

void UGameHUD::DrawTextInternal(const FString& text, float x, float y, float fontSize,
                                const FLinearColor& textColor, bool hasBg,
                                const FLinearColor& bgColor)
{
    if (!bFrameActive || !D2dCtx || !CachedBrush)
        return;

    IDWriteTextFormat* format = GetTextFormat(fontSize);
    if (!format)
        return;

    // 뷰포트 오프셋 적용
    x += ScreenOffsetX;
    y += ScreenOffsetY;

    // UTF-8 -> Wide string 변환
    std::wstring wideText(text.begin(), text.end());

    // 텍스트 레이아웃 생성하여 크기 측정
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(Dwrite->CreateTextLayout(
        wideText.c_str(),
        static_cast<UINT32>(wideText.length()),
        format,
        ScreenWidth,  // Max width
        ScreenHeight, // Max height
        &layout)))
    {
        return;
    }

    // 텍스트 크기 측정
    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);

    const float padding = 4.0f;
    D2D1_RECT_F rect = D2D1::RectF(
        x,
        y,
        x + metrics.width + padding * 2,
        y + metrics.height + padding * 2
    );

    // 배경 그리기
    if (hasBg)
    {
        CachedBrush->SetColor(D2D1::ColorF(bgColor.R, bgColor.G, bgColor.B, bgColor.A));
        D2dCtx->FillRectangle(rect, CachedBrush);
    }

    // 텍스트 그리기
    CachedBrush->SetColor(D2D1::ColorF(textColor.R, textColor.G, textColor.B, textColor.A));
    D2D1_RECT_F textRect = D2D1::RectF(
        x + padding,
        y + padding,
        rect.right,
        rect.bottom
    );
    D2dCtx->DrawTextW(
        wideText.c_str(),
        static_cast<UINT32>(wideText.length()),
        format,
        textRect,
        CachedBrush
    );

    SafeRelease(layout);
}

void UGameHUD::DrawGameText(const FString& text, float x, float y, float fontSize, const FLinearColor& color)
{
    DrawTextInternal(text, x, y, fontSize, color, false, FLinearColor());
}

void UGameHUD::DrawTextRel(const FString& text, float rx, float ry, float fontSize, const FLinearColor& color)
{
    float x = rx * ScreenWidth;
    float y = ry * ScreenHeight;
    DrawTextInternal(text, x, y, fontSize, color, false, FLinearColor());
}

void UGameHUD::DrawTextWithBg(const FString& text, float x, float y, float fontSize,
                              const FLinearColor& textColor, const FLinearColor& bgColor)
{
    DrawTextInternal(text, x, y, fontSize, textColor, true, bgColor);
}

void UGameHUD::DrawTextWithBgRel(const FString& text, float rx, float ry, float fontSize,
                                 const FLinearColor& textColor, const FLinearColor& bgColor)
{
    float x = rx * ScreenWidth;
    float y = ry * ScreenHeight;
    DrawTextInternal(text, x, y, fontSize, textColor, true, bgColor);
}

void UGameHUD::DrawImage(const FString& imagePath, float x, float y, float width, float height)
{
    if (!bFrameActive || !D2dCtx)
        return;

    ID2D1Bitmap* bitmap = LoadBitmapFromFile(imagePath);
    if (!bitmap)
        return;

    // 뷰포트 오프셋 적용
    x += ScreenOffsetX;
    y += ScreenOffsetY;

    D2D1_RECT_F destRect = D2D1::RectF(x, y, x + width, y + height);
    D2dCtx->DrawBitmap(bitmap, destRect);
}

void UGameHUD::DrawImageRel(const FString& imagePath, float rx, float ry, float rwidth, float rheight)
{
    float x = rx * ScreenWidth;
    float y = ry * ScreenHeight;
    float w = rwidth * ScreenWidth;
    float h = rheight * ScreenHeight;
    DrawImage(imagePath, x, y, w, h);
}

void UGameHUD::LoadGameImage(const FString& imagePath)
{
    // 이미지 프리로드 (캐시에 저장)
    LoadBitmapFromFile(imagePath);
}

void UGameHUD::SetVisible(bool visible)
{
    bVisible = visible;
}

bool UGameHUD::IsVisible() const
{
    return bVisible;
}
