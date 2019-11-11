#include "scoped_gdiplus_initializer.h"
#include "ui/defs/RCGdiPlusDefs.h"
#pragma comment(lib, "gdiplus.lib")


ScopedGdiplusInitializer::ScopedGdiplusInitializer() : gdiplus_token_(0) {
  Gdiplus::GdiplusStartupInput input;
  Gdiplus::GdiplusStartup((ULONG_PTR*)&gdiplus_token_, &input, NULL);
}

ScopedGdiplusInitializer::~ScopedGdiplusInitializer() {
  if (gdiplus_token_ != 0) {
    Gdiplus::GdiplusShutdown(gdiplus_token_);
    gdiplus_token_ = 0;
  }
}
