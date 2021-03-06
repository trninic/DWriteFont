
#include "stdafx.h"
#include "D2DSetup.h"
#include <assert.h>
#include <stdio.h>
#include "comdef.h"
#include <iostream>
#include <d3d10_1.h>
#include <D2d1_1.h>
#include "d2d1effects.h";


#define SK_A32_SHIFT 24
#define SK_R32_SHIFT 16
#define SK_G32_SHIFT 8
#define SK_B32_SHIFT 0

/**
*  Pack the components into a SkPMColor, checking (in the debug version) that
*  the components are 0..255, and are already premultiplied (i.e. alpha >= color)
*/
static inline int SkPackARGB32(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
  return (a << SK_A32_SHIFT) | (r << SK_R32_SHIFT) |
      (g << SK_G32_SHIFT) | (b << SK_B32_SHIFT);
}

static
HMONITOR GetPrimaryMonitorHandle()
{
  const POINT ptZero = { 0, 0 };
  return MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
}

void D2DSetup::Init()
{
  InitD3D();

  HRESULT hr = CoCreateInstance(
                        CLSID_WICImagingFactory,
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        IID_IWICImagingFactory,
                        (LPVOID*)&mWICFactory
  );
  assert(hr == S_OK);
  mFactory->GetDesktopDpi(&mDpiX, &mDpiY);

  InitD2D();
}

void
D2DSetup::CleanWICResources() {
  mWICBitmap->Release();
  mWICFactory->Release();
}

void
D2DSetup::CreateHardwareRenderTarget()
{
  RECT clientRect;
  GetClientRect(mHWND, &clientRect);

  D2D1_SIZE_U size = D2D1::SizeU(
    clientRect.right - clientRect.left,
    clientRect.bottom - clientRect.top
  );

  D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties();
  properties.dpiX = mDpiX;
  properties.dpiY = mDpiY;

  D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProperties = D2D1::HwndRenderTargetProperties(mHWND, size);
  HRESULT hr = mFactory->CreateHwndRenderTarget(&properties, &hwndProperties,
                      &mRenderTarget);
  assert(hr == S_OK);
}

void
D2DSetup::CreateD3DDevice()
{
  // This flag adds support for surfaces with a different color channel ordering than the API default.
  // You need it for compatibility with Direct2D.
  UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;

  // This array defines the set of DirectX hardware feature levels this app  supports.
  // The ordering is important and you should  preserve it.
  // Don't forget to declare your app's minimum required feature level in its
  // description.  All apps are assumed to support 9.1 unless otherwise stated.
  D3D_FEATURE_LEVEL featureLevels[] =
  {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1
  };

  DXGI_SWAP_CHAIN_DESC sdc;
  memset(&sdc, 0, sizeof(sdc));
  sdc.BufferCount = 1;
  sdc.BufferDesc.Width = 2048;
  sdc.BufferDesc.Height = 2048;
  sdc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sdc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sdc.OutputWindow = mHWND;
  sdc.Windowed = true;
  sdc.SampleDesc.Count = 1;
  sdc.SampleDesc.Quality = 0;
  sdc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    0,
    creationFlags,
    featureLevels,
    ARRAYSIZE(featureLevels),
    D3D11_SDK_VERSION,
    &sdc,
    &mSwapChain,
    &mD3D_Device,
    nullptr,
    &mD3D_DeviceContext);

  if (hr != S_OK) {
    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::wcout << errMsg;
  }
  assert(hr == S_OK);
}

void
D2DSetup::CreateDXGIResources()
{
  // Create d2d things now
  mD3D_Device->QueryInterface(&mDxgiDevice);
  assert(mDxgiDevice);

  HRESULT hr = mDxgiDevice->GetAdapter(&mAdapter);
  assert(hr == S_OK);

  hr = mAdapter->GetParent(IID_PPV_ARGS(&mDxgiFactory));
  assert(hr == S_OK);
}

void
D2DSetup::ReleaseD3D()
{
  mDxgiDevice->Release();
  mAdapter->Release();
  mDxgiFactory->Release();

  mSwapChain->Release();
  mD3D_Device->Release();
  mD3D_DeviceContext->Release();
}

void
D2DSetup::CreateD2DDevices()
{
  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &mFactory);
  assert(hr == S_OK);

  hr = mFactory->CreateDevice(mDxgiDevice, &md2d_device);
  assert(hr == S_OK);

  //hr = md2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &mDC);
	hr = md2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &mDC);
  assert(hr == S_OK);
}

void
D2DSetup::CreateImageBrushes()
{
  printf("Creating image brushes\n");
  mDC->CreateSolidColorBrush(D2D1::ColorF(0x404040, 1.0f), &mDarkBlackBrush);
  mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &mBlackBrush);
  mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0f), &mWhiteBrush);
  mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.0f), &mTransparentBlackBrush);
  mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red, 1.0F), &mRedBrush);
}

void
D2DSetup::ReleaseBrushes()
{
  mDarkBlackBrush->Release();
  mBlackBrush->Release();
  mWhiteBrush->Release();
  mTransparentBlackBrush->Release();
  mRedBrush->Release();
}

void
D2DSetup::InitDWrite()
{
  HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&mDwriteFactory));
  assert(hr == S_OK);

  mFontSize = 13.0f;
  hr = mDwriteFactory->CreateTextFormat(L"Georgia", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, mFontSize, L"", &mTextFormat);
  assert(hr == S_OK);

  mTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  mTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

  // Now we can play with params
  mDC->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_DEFAULT);

  IDWriteRenderingParams* defaultParams;
  mDwriteFactory->CreateRenderingParams(&mDefaultParams);

  IDWriteRenderingParams* customParams;
  printf("Default param contrast is: %f, gamma: %f, render mode: %d, pixel geometry %d\n",
      mDefaultParams->GetEnhancedContrast(),
      mDefaultParams->GetGamma(),
      mDefaultParams->GetRenderingMode(),
      mDefaultParams->GetPixelGeometry());

  float contrast = mDefaultParams->GetEnhancedContrast();
  contrast = 1.0f;

  mDwriteFactory->CreateCustomRenderingParams(mDefaultParams->GetGamma(), contrast, mDefaultParams->GetClearTypeLevel(), mDefaultParams->GetPixelGeometry(), DWRITE_RENDERING_MODE_DEFAULT, &mCustomParams);

  DWRITE_PIXEL_GEOMETRY geometry = mDefaultParams->GetPixelGeometry();
  mDwriteFactory->CreateCustomRenderingParams(mDefaultParams->GetGamma(), contrast, mDefaultParams->GetClearTypeLevel(), mDefaultParams->GetPixelGeometry(), DWRITE_RENDERING_MODE_GDI_CLASSIC, &mGDIParams);

  float grayscale = 0.0f;
  hr = mDwriteFactory->CreateCustomRenderingParams(mDefaultParams->GetGamma(), contrast, grayscale, mDefaultParams->GetPixelGeometry(), DWRITE_RENDERING_MODE_DEFAULT, &mGrayscaleParams);
  assert(hr == S_OK);
}

void
D2DSetup::ReleaseDWrite()
{
  mTextFormat->Release();
  mDwriteFactory->Release();
  mDefaultParams->Release();
  mCustomParams->Release();
  mGDIParams->Release();
  mGrayscaleParams->Release();
}

void
D2DSetup::ReleaseD2D()
{
  mDC->Release();
  mFactory->Release();
  mTargetBitmap->Release();
  md2d_device->Release();
}

void
D2DSetup::InitD2D()
{
  CreateImageBrushes();
  InitDWrite();
  QueryPerformanceFrequency(&mFrequency);
}

void
D2DSetup::SetD2DToBackBuffer()
{
// Direct2D needs the dxgi version of the back buffer
  ID3D11Texture2D* pBackBuffer;
  // Get a pointer to the back buffer
  HRESULT hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
    (LPVOID*)&pBackBuffer);
  assert(hr == S_OK);

  hr = pBackBuffer->QueryInterface(__uuidof(IDXGISurface),
                              (LPVOID*)&mDxgiSurface);
  assert(hr == S_OK);

  // Setup D2D to hook into the back buffer, using our own bitmap properties fails woot?
  D2D1_BITMAP_PROPERTIES1 bitmapProperties;
  memset(&bitmapProperties, 0, sizeof(D2D1_BITMAP_PROPERTIES1));
  bitmapProperties.dpiX = 96;
  bitmapProperties.dpiY = 96;
  bitmapProperties.pixelFormat = D2D1_PIXEL_FORMAT{ DXGI_FORMAT_R8G8B8A8_UNORM,  D2D1_ALPHA_MODE_PREMULTIPLIED };
  bitmapProperties.bitmapOptions = D2D1_BITMAP_OPTIONS{ D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW };
  bitmapProperties.colorContext = nullptr;

  hr = mDC->CreateBitmapFromDxgiSurface(mDxgiSurface, nullptr, &mTargetBitmap);
  if (hr != S_OK) {
    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::wcout << errMsg;
  }
  assert(hr == S_OK);

  mDC->SetTarget(mTargetBitmap);
}

D2DSetup::~D2DSetup()
{
  ReleaseBrushes();
  ReleaseDWrite();
  ReleaseD2D();
  ReleaseD3D();
}

void
D2DSetup::InitD3D()
{
  CreateD3DDevice();
  CreateDXGIResources();
  CreateD2DDevices();
  SetD2DToBackBuffer();
}

void D2DSetup::PrintElapsedTime(LARGE_INTEGER aStart, LARGE_INTEGER aEnd, const char* aMsg)
{
  LARGE_INTEGER elapsedTime;
  elapsedTime.QuadPart = aEnd.QuadPart - aStart.QuadPart;

  // Convert to microseconds
  elapsedTime.QuadPart *= 1000000;
  elapsedTime.QuadPart /= mFrequency.QuadPart;
  printf("%s elapsed time is: %lld microseconds\n", aMsg, elapsedTime.QuadPart);
}

void D2DSetup::Clear()
{
  mRenderTarget->BeginDraw();
  mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
  mRenderTarget->EndDraw();
}

void D2DSetup::DrawText(int x, int y, WCHAR message[])
{
  const int length = wcslen(message);
  mRenderTarget->BeginDraw();

  mRenderTarget->DrawTextW(message,
    length,
    mTextFormat,
    D2D1::RectF((float) x, y - 17.0f, 1000.0f, 1000.0f),
    mBlackBrush);

  mRenderTarget->EndDraw();
}

void D2DSetup::PrintFonts(IDWriteFontCollection* aFontCollection)
{
  UINT32 familyCount = aFontCollection->GetFontFamilyCount();
  for (uint32_t i = 0; i < familyCount; i++) {
    IDWriteFontFamily* temp;
    aFontCollection->GetFontFamily(i, &temp);

    IDWriteLocalizedStrings* familyName;
    temp->GetFamilyNames(&familyName);

    UINT32 index = 0;
    BOOL exists;
    familyName->FindLocaleName(L"en-us", &index, &exists);

    UINT32 length = 0;
    familyName->GetStringLength(index, &length);

    wchar_t fontName[256];
    familyName->GetString(index, fontName, length + 1);
    //wprintf(L"Font name: %s\n", fontName);
  }
}

static int
Blend(float src, float dst, float alpha) {
  float floatAlpha = alpha / 255;
  //return dst + ((src - dst) * alpha >> 8);
  float result = (src * floatAlpha) + (dst * (1 - floatAlpha));
  return (int) result;
}

static inline int SkBlend32(int src, int dst, int alpha) {
  return dst + ((src - dst) * alpha >> 5);
}

IDWriteFontFace* D2DSetup::GetFontFace()
{
  static const WCHAR fontFamilyName[] = L"Georgia";

  IDWriteFontCollection* systemFonts;
  mDwriteFactory->GetSystemFontCollection(&systemFonts, TRUE);
  //PrintFonts(systemFonts);

  UINT32 fontIndex;
  BOOL exists;
  HRESULT hr = systemFonts->FindFamilyName(fontFamilyName, &fontIndex, &exists);
  assert(exists);

  IDWriteFontFamily* fontFamily;
  systemFonts->GetFontFamily(fontIndex, &fontFamily);

  IDWriteFont* actualFont;
  fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &actualFont);

  IDWriteLocalizedStrings* names;
  BOOL info_string_exists = false;
  hr = actualFont->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_FULL_NAME, &names, &info_string_exists);
  assert(hr == S_OK);

  int count = names->GetCount();

  uint32_t length;
  names->GetStringLength(0, &length);
  assert(hr == S_OK);

  /*
  length += 1; // Need for null terminating char
  WCHAR* buffer = new WCHAR[length];
  hr = names->GetString(0, buffer, length);
  assert(hr == S_OK);

  wprintf(L"String is: %s\n", buffer);

  printf("Stretch: %d, Style: %d, Weight: %d\n",
    actualFont->GetStretch(), actualFont->GetStyle(), actualFont->GetWeight());
    */

  IDWriteFontFace* fontFace;
  actualFont->CreateFontFace(&fontFace);

  return fontFace;
}

void D2DSetup::CreateGlyphRun(DWRITE_GLYPH_RUN& glyphRun, IDWriteFontFace* fontFace, WCHAR message[], float aScale)
{
  //static const WCHAR message[] = L"Hello World Glyph";
  const int length = wcslen(message);

  UINT16* glyphIndices = new UINT16[length];
  UINT32* codePoints = new UINT32[length];
  DWRITE_GLYPH_METRICS* glyphMetrics = new DWRITE_GLYPH_METRICS[length];
  FLOAT* advances = new FLOAT[length];
  float fontSize = mFontSize * aScale;

  for (int i = 0; i < length; i++) {
    codePoints[i] = message[i];
  }

  fontFace->GetGlyphIndicesW(codePoints, length, glyphIndices);
  fontFace->GetDesignGlyphMetrics(glyphIndices, length, glyphMetrics);

  DWRITE_FONT_METRICS fontMetrics;
  fontFace->GetMetrics(&fontMetrics);

  for (int i = 0; i < length; i++) {
    int advance = glyphMetrics[i].advanceWidth;
    float realAdvance = ((float) advance * fontSize) / fontMetrics.designUnitsPerEm;
    advances[i] = realAdvance;
  }

  DWRITE_GLYPH_OFFSET* offset = new DWRITE_GLYPH_OFFSET[length];
  for (int i = 0; i < length; i++) {
    offset[i].advanceOffset = 0;
    offset[i].ascenderOffset = 0;
  }

  glyphRun.glyphCount = length;
  glyphRun.glyphAdvances = advances;
  glyphRun.fontFace = fontFace;
  glyphRun.fontEmSize = fontSize;
  glyphRun.bidiLevel = 0;
  glyphRun.glyphIndices = glyphIndices;
  glyphRun.isSideways = FALSE;
  glyphRun.glyphOffsets = offset;
}

void D2DSetup::DrawTextWithD2D(DWRITE_GLYPH_RUN& glyphRun, int x, int y,
                               IDWriteRenderingParams* aParams, bool aClear,
                               D2D1_TEXT_ANTIALIAS_MODE aaMode)
{
  D2D1_POINT_2F origin;
  origin.x = (float)x;
  origin.y = (float)y;

  mRenderTarget->BeginDraw();
  mRenderTarget->SetTextRenderingParams(aParams);
  mRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
  mRenderTarget->SetTextAntialiasMode(aaMode);

  if (aClear) {
    mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
  }

  mRenderTarget->DrawGlyphRun(origin, &glyphRun, mBlackBrush);
  mRenderTarget->EndDraw();
}

static inline int SkUpscale31To32(int value) {
  return value + (value >> 4);
}

// Converts the given rgb 3x1 cleartype alpha mask to the required RGBA_UNOM as required by bitmaps
// Also blends to draw black text on white
BYTE* D2DSetup::ConvertToBGRA(BYTE* aRGB, int width, int height, bool useLUT, bool convert, bool useGDILUT)
{
  int size = width * height * 4;
  BYTE* bitmapImage = (BYTE*)malloc(size);
  memset(bitmapImage, 0x00, size);

  const uint8_t* tableR = this->fPreBlend.fR;
  const uint8_t* tableG = this->fPreBlend.fG;
  const uint8_t* tableB = this->fPreBlend.fB;

  if (useGDILUT) {
    tableR = this->fGdiPreBlend.fR;
    tableG = this->fGdiPreBlend.fG;
    tableB = this->fGdiPreBlend.fB;
  }

  printf("Final output\n\n");

  for (int y = 0; y < height; y++) {
    int sourceHeight = y * width * 3;  // expect 3 bytes per pixel
    int destHeight = y * width * 4;    // convert to 4 bytes per pixel to add alpha opaque channel

    for (int i = 0; i < width; i++) {
      int destIndex = destHeight + (4 * i);
      int srcIndex = sourceHeight + (i * 3);

      BYTE r = aRGB[srcIndex];
      BYTE g = aRGB[srcIndex + 1];
      BYTE b = aRGB[srcIndex + 2];

      BYTE orig_r = r;
      BYTE orig_g = g;
      BYTE orig_b = b;

      BYTE lut_r;
      BYTE lut_g;
      BYTE lut_b;

      BYTE short_r;
      BYTE short_g;
      BYTE short_b;

      if (useLUT) {
        lut_r = sk_apply_lut_if<true>(r, tableR);
        lut_g = sk_apply_lut_if<true>(g, tableG);
        lut_b = sk_apply_lut_if<true>(b, tableB);

        r = sk_apply_lut_if<true>(r, tableR);
        g = sk_apply_lut_if<true>(g, tableG);
        b = sk_apply_lut_if<true>(b, tableB);
      }

      if (convert) {
        // Taken from http://searchfox.org/mozilla-central/source/gfx/skia/skia/include/core/SkColorPriv.h#654
        short_r = r >> 3;
        short_g = g >> 3;
        short_b = b >> 3;

        r = short_r << 3;
        g = short_g << 3;
        b = short_b << 3;
      }

      r = Blend(0x00, 0xFF, r);
      g = Blend(0x00, 0xFF, g);
      b = Blend(0x00, 0xFF, b);

      // Assume bgr8
      bitmapImage[destIndex] = b;
      bitmapImage[destIndex + 1] = g;
      bitmapImage[destIndex + 2] = r;
      bitmapImage[destIndex + 3] = 0xFF;

      /*
      BYTE upscale_r = SkUpscale31To32(short_r);
      BYTE upscale_g = SkUpscale31To32(short_g);
      BYTE upscale_b = SkUpscale31To32(short_b);

      BYTE skia_blend_r = SkBlend32(0, 255, upscale_r);
      BYTE skia_blend_g = SkBlend32(0, 255, upscale_g);
      BYTE skia_blend_b = SkBlend32(0, 255, upscale_b);

      bitmapImage[destIndex] = skia_blend_r;
      bitmapImage[destIndex + 1] = skia_blend_g;
      bitmapImage[destIndex + 2] = skia_blend_b;
      bitmapImage[destIndex + 3] = 0xFF;

      /*
      printf("Orig (%u, %u, %u) => LUT (%u, %u, %u) => Short (%u, %u, %u) => Upscale (%u, %u, %u) => final (%u, %u, %u)\n",
        orig_r, orig_g, orig_b,
        lut_r, lut_g, lut_b,
        short_r, short_g, short_b,
        upscale_r, upscale_g, upscale_b,
        skia_blend_r, skia_blend_g, skia_blend_b);
        */
    }
    //printf("\n");

  }
  return bitmapImage;
}

BYTE* D2DSetup::BlitDirectly(BYTE* aBGR, int width, int height)
{
  int size = width * height * 4;
  BYTE* bitmapImage = (BYTE*)malloc(size);
  memset(bitmapImage, 0x00, size);

  for (int y = 0; y < height; y++) {
      int sourceHeight = y * width * 4;  // expect 4 bytes per pixel
      int destHeight = y * width * 4;    // convert to 4 bytes per pixel to add alpha opaque channel

      for (int i = 0; i < width; i++) {
          int srcIndex = sourceHeight + (4 * i);
          int destIndex = destHeight + (4 * i);

          // Assuming a BGR format
          BYTE b = aBGR[srcIndex];
          BYTE g = aBGR[srcIndex + 1];
          BYTE r = aBGR[srcIndex + 2];

          // Assume BGR8
          bitmapImage[destIndex] = b;
          bitmapImage[destIndex + 1] = g;
          bitmapImage[destIndex + 2] = r;
          bitmapImage[destIndex + 3] = 0xFF;
      }
  }

  return bitmapImage;
}

// This is what SKia does to convert Cleartype to Grayscale.
// Basically takes each channel, averages it, and gamma corrects with the G channel.
BYTE* D2DSetup::BlendSkiaGrayscale(BYTE* aBGR, int width, int height)
{
  int size = width * height * 4;
  BYTE* bitmapImage = (BYTE*)malloc(size);
  memset(bitmapImage, 0x00, size);

  const uint8_t* tableG = this->fPreBlend.fG;

  for (int y = 0; y < height; y++) {
    int sourceHeight = y * width * 3;  // expect 3 bytes per pixel
    int destHeight = y * width * 4;    // convert to 4 bytes per pixel to add alpha opaque channel

    for (int i = 0; i < width; i++) {
      int destIndex = destHeight + (4 * i);
      int srcIndex = sourceHeight + (i * 3);

      BYTE r = aBGR[srcIndex];
      BYTE g = aBGR[srcIndex + 1];
      BYTE b = aBGR[srcIndex + 2];

      // This is what Skia does
      int average = (r + g + b) / 3;
      BYTE pixel = sk_apply_lut_if<true>(r, tableG);
      pixel = Blend(0x00, 0xFF, pixel);

      bitmapImage[destIndex] = pixel;
      bitmapImage[destIndex + 1] = pixel;
      bitmapImage[destIndex + 2] = pixel;
      bitmapImage[destIndex + 3] = 0xFF;
    }
  }

  return bitmapImage;
}

void D2DSetup::CreateBitmap(ID2D1RenderTarget* aRenderTarget, ID2D1Bitmap** aOutBitmap,
                            int width, int height,
                            BYTE* aSource, uint32_t aStride)
{
  D2D1_BITMAP_PROPERTIES properties = { DXGI_FORMAT_B8G8R8A8_UNORM,  D2D1_ALPHA_MODE_PREMULTIPLIED };
  properties.dpiX = mDpiX;
  properties.dpiY = mDpiY;

  D2D1_SIZE_U bitmapSize = D2D1::SizeU(width, height);
  HRESULT hr;
  
  if (aSource) {
    hr = aRenderTarget->CreateBitmap(bitmapSize, aSource, aStride, properties, aOutBitmap);
  } else {
    hr = aRenderTarget->CreateBitmap(bitmapSize, properties, aOutBitmap);
  }

  assert(hr == S_OK);
}

void D2DSetup::DrawBitmap(BYTE* image, float width, float height, int x, int y, RECT bounds)
{
  ID2D1Bitmap* bitmap = nullptr;
  uint32_t stride = (uint32_t)width * 4;
  CreateBitmap(mRenderTarget, &bitmap, width, height, image, width * 4);

  D2D1_SIZE_F bitmapSize = bitmap->GetSize();

  // Finally draw the bitmap somewhere
  D2D1_RECT_F destRect;
  destRect.left = x;
  destRect.right = x + bitmapSize.width;
  destRect.top = y;
  destRect.bottom = y + bitmapSize.height;

  float opacity = 1.0;
  mRenderTarget->DrawBitmap(bitmap, &destRect, opacity);

  bitmap->Release();
}

void D2DSetup::DrawGrayscaleWithBitmap(DWRITE_GLYPH_RUN& glyphRun, int x, int y)
{
  mRenderTarget->BeginDraw();

  LARGE_INTEGER start;
  QueryPerformanceCounter(&start);

  RECT bounds;
  IDWriteGlyphRunAnalysis* analysis;
  GetGlyphBounds(glyphRun, bounds, &analysis);

  float width = bounds.right - bounds.left;
  float height = bounds.bottom - bounds.top;
  assert(width > 0);
  assert(height > 0);
  
  uint32_t bitmap_width = (uint32_t)width;
  uint32_t bitmap_height = (uint32_t)height;
  HRESULT hr = this->mWICFactory->CreateBitmap(bitmap_width,
      bitmap_height,
      // This has to be the PBGRA format, which means already pre multiplied.
      GUID_WICPixelFormat32bppPBGRA,
      WICBitmapCacheOnDemand,
      &mWICBitmap);
  assert (hr == S_OK);

  D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties();
  properties.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;

  // Gecko scales the font size and renders everything at 1.0 DPI instead.
  // We do the same here. Render the font onto a 1.0 DPI but scales font size
  // Then when we render back to the hardware render target, which is the proper
  // scaled display.
  properties.dpiX = 96;
  properties.dpiY = 96;

  // Known formats here - https://msdn.microsoft.com/en-us/library/windows/desktop/dd756766(v=vs.85).aspx
  properties.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);

  hr = mFactory->CreateWicBitmapRenderTarget(mWICBitmap, properties, &mBitmapRenderTarget);
  assert (hr == S_OK);

  D2D1_POINT_2F origin;
  origin.x = -bounds.left;
  origin.y = -bounds.top;
  mBitmapRenderTarget->BeginDraw();
  mBitmapRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

  mBitmapRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
  mBitmapRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
  mBitmapRenderTarget->SetTextRenderingParams(mGrayscaleParams);

  // Cannot use the same black brush created from a different render target
  ID2D1SolidColorBrush* blackBrush;
  hr = mBitmapRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &blackBrush);
  assert(hr == S_OK);

  mBitmapRenderTarget->DrawGlyphRun(origin, &glyphRun, blackBrush);

  hr = mBitmapRenderTarget->EndDraw();
  assert (hr == S_OK);

  // Now readback.
  IWICBitmapLock* readback;
  WICRect wicBounds;
  wicBounds.Height = bitmap_height;
  wicBounds.Width = bitmap_width;
  wicBounds.X = 0;
  wicBounds.Y = 0;

  hr = mWICBitmap->Lock(&wicBounds, WICBitmapLockRead, &readback);
  assert (hr == S_OK);

  UINT stride;
  hr = readback->GetStride(&stride);
  assert (hr == S_OK);
  assert (bitmap_width * 4 == stride);

  UINT buffer_size;
  BYTE* buffer_ptr;
  hr = readback->GetDataPointer(&buffer_size, &buffer_ptr);
  assert (hr == S_OK);

  UINT buffer_width;
  UINT buffer_height;
  hr = readback->GetSize(&buffer_width, &buffer_height);
  assert (hr == S_OK);
  assert (buffer_width * buffer_height * 4 == buffer_size);

  LARGE_INTEGER end;
  QueryPerformanceCounter(&end);
  PrintElapsedTime(start, end, "BitmapRenderTarget");

  // TODO: I think we have to do the LUT gamma correction here too.
  //BYTE* drawn_glyph = BlitDirectly(buffer_ptr, buffer_width, buffer_height);
  DrawBitmap(buffer_ptr, width, height, x, y, bounds);

  blackBrush->Release();
  readback->Release();
  mBitmapRenderTarget->Release();
  mWICBitmap->Release();

  //free(drawn_glyph);

  mRenderTarget->EndDraw();
}

void D2DSetup::DrawGrayscaleWithLUT(DWRITE_GLYPH_RUN& glyphRun, int x, int y) {
  mRenderTarget->BeginDraw();

  LARGE_INTEGER start;
  QueryPerformanceCounter(&start);

  RECT bounds;
  BYTE* bits = GetAlphaTexture(glyphRun, bounds,
                               DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL,
                               DWRITE_MEASURING_MODE_NATURAL);
  float width = bounds.right - bounds.left;
  float height = bounds.bottom - bounds.top;

  BYTE* bitmapImage = BlendSkiaGrayscale(bits, width, height);

  LARGE_INTEGER end;
  QueryPerformanceCounter(&end);
  PrintElapsedTime(start, end, "Grayscale LUT");

  DrawBitmap(bitmapImage, width, height, x, y, bounds);

  free(bits);
  free(bitmapImage);
  mRenderTarget->EndDraw();
}

void D2DSetup::GetGlyphBounds(DWRITE_GLYPH_RUN& aRun, RECT& aOutBounds,
                              IDWriteGlyphRunAnalysis** aOutAnalysis,
                              DWRITE_RENDERING_MODE aRenderMode,
                              DWRITE_MEASURING_MODE aMeasureMode)
{
  // Surprisingly, we don't have to account for the dpi here in the glyph run analysis,
  // we do that in the bitmap and render target instead.
  HRESULT hr = mDwriteFactory->CreateGlyphRunAnalysis(&aRun, 1.0f, nullptr, aRenderMode,
                                                      aMeasureMode, 0.0f, 0.0f, aOutAnalysis);
  assert(hr == S_OK);

  hr = (*aOutAnalysis)->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &aOutBounds);
  assert(hr == S_OK);
}

BYTE* D2DSetup::GetAlphaTexture(DWRITE_GLYPH_RUN& aRun, RECT& aOutBounds,
                                DWRITE_RENDERING_MODE aRenderMode,
                                DWRITE_MEASURING_MODE aMeasureMode)
{
  IDWriteGlyphRunAnalysis* analysis;
  GetGlyphBounds(aRun, aOutBounds, &analysis, aRenderMode, aMeasureMode);

  float scale = GetScaleFactor();

  long width = aOutBounds.right - aOutBounds.left;
  long height = aOutBounds.bottom - aOutBounds.top;

  int bufferSize = width * height * 3;
  BYTE* image = (BYTE*)malloc(bufferSize);
  memset(image, 0xFF, bufferSize);

  // DWRITE_TEXTURE_CLEARTYPE uses RGB, but we use BGR everywhere else.
  HRESULT hr = analysis->CreateAlphaTexture(DWRITE_TEXTURE_TYPE::DWRITE_TEXTURE_CLEARTYPE_3x1, &aOutBounds, image, bufferSize);
  assert(hr == S_OK);

  analysis->Release();
  return image;
}

void D2DSetup::DrawWithBitmap(DWRITE_GLYPH_RUN& glyphRun, int x, int y, bool useLUT, bool convert,
                DWRITE_RENDERING_MODE aRenderMode, DWRITE_MEASURING_MODE aMeasureMode,
                bool aClear, bool useGDILUT)
{
  mRenderTarget->BeginDraw();

  if (aClear) {
    mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
  }

  RECT bounds;
  BYTE* bits = GetAlphaTexture(glyphRun, bounds, aRenderMode, aMeasureMode);
  long width = bounds.right - bounds.left;
  long height = bounds.bottom - bounds.top;

  BYTE* bitmapImage = ConvertToBGRA(bits, width, height, useLUT, convert, useGDILUT);
  DrawBitmap(bitmapImage, width, height, x, y, bounds);

  free(bitmapImage);
  free(bits);
  mRenderTarget->EndDraw();
}

void D2DSetup::AlternateText(int count) {
  IDWriteFontFace* fontFace = GetFontFace();
  int x = 100; int y = 100;
  switch (count % 2) {
  case 0:
  {
    WCHAR d2dMessage[] = L"The Donald Trump GDI";
    DWRITE_GLYPH_RUN d2dGlyphRun;
    CreateGlyphRun(d2dGlyphRun, fontFace, d2dMessage);
    DrawTextWithD2D(d2dGlyphRun, x, y, mGDIParams, true);
    break;
  }
  case 1:
  {
    /*
    WCHAR d2dMessage[] = L"Donald Trump Sucks Custom";
    DWRITE_GLYPH_RUN d2dGlyphRun;
    CreateGlyphRunAnalysis(d2dGlyphRun, fontFace, d2dMessage);
    DrawTextWithD2D(d2dGlyphRun, x, y, mCustomParams, true);
    break;
    */
    WCHAR d2dLutChop[] = L"The Donald Trump GDI LUT";
    DWRITE_GLYPH_RUN d2dLutChopRun;
    CreateGlyphRun(d2dLutChopRun, fontFace, d2dLutChop);
    DrawWithBitmap(d2dLutChopRun, x, y, true, true,
            DWRITE_RENDERING_MODE_GDI_CLASSIC,
            DWRITE_MEASURING_MODE_NATURAL,
            true, true);
    break;
  }
  } // end switch
}

void D2DSetup::DrawWithMask()
{
  IDWriteFontFace* fontFace = GetFontFace();
  const int x = 100;
  const int y = 100;

  DWRITE_RENDERING_MODE recommendedMode = DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL;
  HRESULT hr = fontFace->GetRecommendedRenderingMode(mFontSize,
    1.0f,
    DWRITE_MEASURING_MODE_NATURAL, // We use this in gecko
    mGrayscaleParams,
    &recommendedMode);
  assert(hr == S_OK);
  printf("Recommended mode is: %d\n", recommendedMode);
  float scale = GetScaleFactor();

  /*
  WCHAR d2dMessage[] = L"T";
  DWRITE_GLYPH_RUN d2dGlyphRun;
  CreateGlyphRunAnalysis(d2dGlyphRun, fontFace, d2dMessage);
  DrawTextWithD2D(d2dGlyphRun, x, y, mDefaultParams);
  */

  WCHAR d2dMessage[] = L"The Donald Trump D2D";
  DWRITE_GLYPH_RUN d2dGlyphRun;
  CreateGlyphRun(d2dGlyphRun, fontFace, d2dMessage);

  LARGE_INTEGER start;
  LARGE_INTEGER end;
  QueryPerformanceCounter(&start);
  DrawTextWithD2D(d2dGlyphRun, x, y, mCustomParams);
  QueryPerformanceCounter(&end);
  PrintElapsedTime(start, end, "D2D Render Text");

  WCHAR bitmapMessage[] = L"The Donald Trump Bitmap";
  DWRITE_GLYPH_RUN bitmapGlyphRun;
  // We have to scale when we draw with bitmaps but not with d2d. D2D handles the scale for us automatically.
  //CreateGlyphRun(bitmapGlyphRun, fontFace, bitmapMessage, scale);
  //DrawWithBitmap(bitmapGlyphRun, x, y + 20, true, true);
  //DrawGrayscaleWithBitmap(bitmapGlyphRun, x, y + 40);
  //DrawGrayscaleWithLUT(bitmapGlyphRun, x, y + 20);

  WCHAR sym[] = L" T";
  DWRITE_GLYPH_RUN symRun;
  CreateGlyphRun(symRun, fontFace, sym);
  DrawWithBitmap(symRun, x, y + 20, true, true, DWRITE_RENDERING_MODE_GDI_CLASSIC);

  /*
  WCHAR gdi[] = L"The Donald Trump Sucks LUT";
  DWRITE_GLYPH_RUN gdiRun;
  CreateGlyphRunAnalysis(gdiRun, fontFace, gdi);
  DrawWithBitmap(gdiRun, x, y + 60, true, true);
  */
}

void
D2DSetup::Present()
{
  mSwapChain->Present(0, 0);
}

void
D2DSetup::PushLayer(ID2D1Image* aMaskImage)
{
  D2D1_LAYER_PARAMETERS layerParams;
  layerParams.contentBounds = D2D1::InfiniteRect();
  layerParams.geometricMask = nullptr;
  layerParams.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
  layerParams.maskTransform = D2D1::IdentityMatrix();
  layerParams.opacity = 1.0;
  layerParams.layerOptions = D2D1_LAYER_OPTIONS_NONE;

  D2D1_SIZE_F size = mTargetBitmap->GetSize();
  D2D1_RECT_F destRect;
  destRect.left = 0;
  destRect.bottom = 0;
  destRect.right = size.width;
  destRect.top = size.height;

  D2D1_IMAGE_BRUSH_PROPERTIES properties;
  properties.sourceRectangle = destRect;
  properties.extendModeX = D2D1_EXTEND_MODE_CLAMP;
  properties.extendModeY = D2D1_EXTEND_MODE_CLAMP;
  properties.interpolationMode = D2D1_INTERPOLATION_MODE_LINEAR;

  ID2D1ImageBrush* brush;
  HRESULT hr = mDC->CreateImageBrush(aMaskImage, properties, &brush);
  assert(hr == S_OK);
  layerParams.opacityBrush = brush;

  mDC->PushLayer(layerParams, nullptr);
}

void
D2DSetup::PopLayer()
{
  mDC->PopLayer();
}

void
D2DSetup::PrintTargetBitmap(D2D1_SIZE_U aBitmapSize)
{
  // Do a readback
  ID2D1Bitmap1* softwareBitmap;
  D2D1_BITMAP_PROPERTIES1 properties;
  properties.colorContext = nullptr;
  mTargetBitmap->GetDpi(&properties.dpiX, &properties.dpiY);
  properties.pixelFormat = mTargetBitmap->GetPixelFormat();
  properties.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW |
                             D2D1_BITMAP_OPTIONS_CPU_READ;;

  HRESULT hr = mDC->CreateBitmap(aBitmapSize, nullptr, 0, properties, &softwareBitmap);
  assert(hr == S_OK);

  hr = softwareBitmap->CopyFromBitmap(nullptr, mTargetBitmap, nullptr);
  assert(hr == S_OK);

  // Print out the first row
  D2D1_MAPPED_RECT map;
  hr = softwareBitmap->Map(D2D1_MAP_OPTIONS_READ, &map);
  assert(hr == S_OK);

  uint32_t stride = map.pitch;
  uint32_t last_row = stride * (aBitmapSize.height - 2);
  uint8_t* data = map.bits;

  for (uint32_t i = 0; i < stride; i += 4) {
    printf("(%u, %u, %u, %u) ",
            data[last_row + i],
            data[last_row + i+1],
            data[last_row + i+2],
            data[last_row + i+3]);
  }

  softwareBitmap->Unmap();
  softwareBitmap->Release();
}

void
D2DSetup::PrintAlphaBitmap(ID2D1Bitmap1* aAlphaBitmap)
{
  D2D1_MAPPED_RECT map;
  HRESULT hr = aAlphaBitmap->Map(D2D1_MAP_OPTIONS_READ, &map);
  assert(hr == S_OK);

  D2D1_SIZE_F size = aAlphaBitmap->GetSize();
  uint32_t stride = map.pitch;
  uint32_t last_row = stride * (size.height - 2);
  uint8_t* data = map.bits;

  for (uint32_t row = 0; row < size.height; row++) {
    for (uint32_t column = 0; column < stride; column++) {
      uint32_t i = (row * stride) + column;
      //printf("%u ", data[last_row + i]);
      assert(data[i] == 0);
    }
  }

  for (uint32_t c = 0; c < stride; c++) {
    printf("%u ", data[c]);
  }

  aAlphaBitmap->Unmap();
}

void
D2DSetup::PrintAlphaBitmapWithCreateReadback(ID2D1Bitmap1* aAlphaBitmap)
{
  // Do a readback
  ID2D1Bitmap1* softwareBitmap;
  D2D1_BITMAP_PROPERTIES1 properties;
  properties.colorContext = nullptr;
  aAlphaBitmap->GetDpi(&properties.dpiX, &properties.dpiY);
  properties.pixelFormat = aAlphaBitmap->GetPixelFormat();
  properties.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW |
                             D2D1_BITMAP_OPTIONS_CPU_READ;

  D2D1_SIZE_F size = aAlphaBitmap->GetSize();
  D2D1_SIZE_U usize;
  usize.width = (uint32_t)size.width;
  usize.height = (uint32_t)size.height;

  HRESULT hr = mDC->CreateBitmap(usize, nullptr, 0, properties, &softwareBitmap);
  assert(hr == S_OK);

  hr = softwareBitmap->CopyFromBitmap(nullptr, aAlphaBitmap, nullptr);
  assert(hr == S_OK);

  PrintAlphaBitmap(softwareBitmap);

  softwareBitmap->Release();
}

void
D2DSetup::DrawLuminanceEffect()
{
  // Read the image from disk
  IWICBitmapDecoder *pDecoder = NULL;
  IWICBitmapFrameDecode *pSource = NULL;
  IWICStream *pStream = NULL;
  IWICFormatConverter *pConverter = NULL;
  IWICBitmapScaler *pScaler = NULL;
  PCWSTR uri = L"C:\\firefox.png";

  HRESULT hr = mWICFactory->CreateDecoderFromFilename(
    uri,
    NULL,
    GENERIC_READ,
    WICDecodeMetadataCacheOnLoad,
    &pDecoder
  );

  if (hr != S_OK) {
    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::wcout << errMsg;
  }

  // Read the frame
  hr = pDecoder->GetFrame(0, &pSource);
  assert(hr == S_OK);

  // convert it to someting d2d can use
  hr = mWICFactory->CreateFormatConverter(&pConverter);
  assert(hr == S_OK);

  // Convert the image format to 32bppPBGRA
  // (DXGI_FORMAT_B8G8R8A8_UNORM + D2D1_ALPHA_MODE_PREMULTIPLIED).
  hr = pConverter->Initialize(
    pSource,
    GUID_WICPixelFormat32bppPBGRA,
    WICBitmapDitherTypeNone,
    NULL,
    0.f,
    WICBitmapPaletteTypeMedianCut
  );
  assert(hr == S_OK);

  // Create a Direct2D bitmap from the WIC bitmap.
  ID2D1Bitmap* imageBitmap;
  hr = mDC->CreateBitmapFromWicBitmap(pConverter, &imageBitmap);
  assert(hr == S_OK);

  D2D1_SIZE_F size = mDC->GetSize();
  D2D1_RECT_F destRect;
  destRect.left = 0;
  destRect.bottom = 0;
  destRect.right = size.width;
  destRect.top = size.height;

  D2D1_SIZE_U bitmapSize;
  bitmapSize.width = size.width;
  bitmapSize.height = size.height;

  // draw our image bitmap first
  mDC->BeginDraw();
  //mDC->DrawImage(imageBitmap);
  mDC->Clear();
  hr = mDC->EndDraw();
  assert(hr == S_OK);
  mDC->Flush();

  //Present();

  printf("Printing RGB values before hand\n");
  PrintTargetBitmap(bitmapSize);

  // Creates a read only bitmap
  ID2D1Bitmap1* tmpBitmap;
  D2D1_BITMAP_PROPERTIES1 properties;
  properties.colorContext = nullptr;
  mTargetBitmap->GetDpi(&properties.dpiX, &properties.dpiY);
  properties.pixelFormat = mTargetBitmap->GetPixelFormat();
  properties.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
  hr = mDC->CreateBitmap(bitmapSize, nullptr, 0, properties, &tmpBitmap);

  assert(hr == S_OK);
  hr = tmpBitmap->CopyFromBitmap(nullptr, mTargetBitmap, nullptr);
  assert(hr == S_OK);

  ID2D1Effect* luminanceEffect;
  hr = mDC->CreateEffect(CLSID_D2D1LuminanceToAlpha, &luminanceEffect);
  assert(hr == S_OK);
  luminanceEffect->SetInput(0, tmpBitmap);

  ID2D1Image* luminanceOutput;
  luminanceEffect->GetOutput(&luminanceOutput);

  // Alpha only bitmap that we draw the effect into
  D2D1_PIXEL_FORMAT alphaFormat;
  alphaFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  alphaFormat.format = DXGI_FORMAT_A8_UNORM;

  ID2D1Bitmap1* alphaBitmap;
  properties.pixelFormat = alphaFormat;
  properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
  hr = mDC->CreateBitmap(bitmapSize, nullptr, 0, properties, &alphaBitmap);
  assert(hr == S_OK);

  mDC->SetTarget(alphaBitmap);
  mDC->BeginDraw();
  mDC->DrawImage(luminanceEffect);
  hr = mDC->EndDraw();
  assert(hr == S_OK);
  mDC->Flush();
  Present();

  printf("\n\n\n\n\n");
  printf("Alpha luminance bitmap\n");
  PrintAlphaBitmapWithCreateReadback(alphaBitmap);

  mDC->SetTarget(mTargetBitmap);

  /*
  // So we can use it as an input;
  ID2D1Bitmap1* alphaInputBitmap;
  properties.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
  hr = mDC->CreateBitmap(bitmapSize, nullptr, 0, properties, &alphaInputBitmap);
  assert(hr == S_OK);
  hr = alphaInputBitmap->CopyFromBitmap(nullptr, alphaBitmap, nullptr);
  assert(hr == S_OK);

  ID2D1RectangleGeometry* rectGeo;
  hr = mFactory->CreateRectangleGeometry(destRect, &rectGeo);
  assert(hr == S_OK);

  // Use the alpha bitmap as a mask to paint the image.
  ID2D1BitmapBrush* imageBitmapBrush;
  D2D1_BITMAP_BRUSH_PROPERTIES bitmapBrushProperties;
  bitmapBrushProperties.extendModeX = D2D1_EXTEND_MODE_CLAMP;
  bitmapBrushProperties.extendModeY = D2D1_EXTEND_MODE_CLAMP;

  hr = mDC->CreateBitmapBrush(alphaBitmap, &imageBitmapBrush);
  if (hr != S_OK) {
    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::wcout << errMsg;
  }
  assert(hr == S_OK);

  mDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
  mDC->BeginDraw();
  mDC->Clear();

  PushLayer(luminanceOutput);
  mDC->FillRectangle(destRect, mWhiteBrush);
  PopLayer();

  hr = mDC->EndDraw();
  if (hr != S_OK) {
    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::wcout << errMsg;
  }
  assert(hr == S_OK);

  mDC->Flush();
  Present();
  */

  // Readback the bitmap
  /*
  ID2D1Bitmap1* alphaReadback;
  properties.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
  hr = mDC->CreateBitmap(bitmapSize, nullptr, 0, properties, &alphaReadback);
  assert(hr == S_OK);
  hr = alphaReadback->CopyFromBitmap(nullptr, alphaBitmap, nullptr);
  assert(hr == S_OK);

  PrintAlphaBitmap(alphaReadback);
  */

  pConverter->Release();
  pSource->Release();
  pDecoder->Release();
  imageBitmap->Release();

  luminanceEffect->Release();
  tmpBitmap->Release();
  alphaBitmap->Release();
}

SkMaskGamma::PreBlend D2DSetup::CreateLUT()
{
  const float contrast = 1.0;
  const float paintGamma = 1.8f;
  const float deviceGamma = 1.8f;
  SkMaskGamma* gamma = new SkMaskGamma(contrast, paintGamma, deviceGamma);

  // Gecko is always setting the preblend to black background.
  SkColor blackLuminanceColor = SkColorSetARGBInline(255, 0, 0, 0);
  SkColor whiteLuminance = SkColorSetARGBInline(255, 255, 255, 255);
  SkColor mozillaColor = SkColorSetARGBInline(255, 0x40, 0x40, 0x40);
  return gamma->preBlend(blackLuminanceColor);
}

// See http://searchfox.org/mozilla-central/source/gfx/skia/skia/src/ports/SkFontHost_win.cpp#1080
SkMaskGamma::PreBlend D2DSetup::CreateGdiLUT()
{
  UINT level = 0;
  if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGCONTRAST, 0, &level, 0) || !level) {
    // can't get the data, so use a default
    level = 1400;
  }

  const float contrast = 1.0;
  //const float paintGamma = (float) level / 1000.0f;
  //const float deviceGamma = (float)level / 1000.0f;

  const float paintGamma = 2.3f;
  const float deviceGamma = 2.3f;
  SkMaskGamma* gamma = new SkMaskGamma(contrast, paintGamma, deviceGamma);

  // Gecko is always setting the preblend to black background.
  SkColor blackLuminanceColor = SkColorSetARGBInline(255, 0, 0, 0);
  SkColor whiteLuminance = SkColorSetARGBInline(255, 255, 255, 255);
  SkColor mozillaColor = SkColorSetARGBInline(255, 0x40, 0x40, 0x40);
  return gamma->preBlend(blackLuminanceColor);
}