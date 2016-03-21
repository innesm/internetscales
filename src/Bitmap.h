
#ifndef _BITMAP_INCLUDE_
#define _BITMAP_INCLUDE_

//////////////////////////////////////////////////
// Generic pixel display
//////////////////////////////////////////////////
template<typename TPixel, size_t W, size_t H>
class Bitmap
{
  TPixel _pixels[W*H];

public:
  Bitmap()
  {
  }

  TPixel& pixel(size_t x, size_t y)
  {
    return _pixels[y * W + x];
  }
};

#endif
