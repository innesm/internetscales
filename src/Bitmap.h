
#ifndef _BITMAP_INCLUDE_
#define _BITMAP_INCLUDE_

//////////////////////////////////////////////////
// Generic pixel display
//////////////////////////////////////////////////
template<typename TPixel, int W, int H>
class Bitmap
{
  TPixel _pixels[W*H];

public:
  Bitmap()
  {
  }

  TPixel& pixel(int x, int y)
  {
    return _pixels[y * W + x];
  }
};

#endif
