#ifndef _WEIGHT_DISPLAY_INCLUDE_
#define _WEIGHT_DISPLAY_INCLUDE_

#include "Bitmap.h"

const int SEGMENT_COUNT = 7;

////////////////////////////////////////////////
// Bathroom scales lcd display
////////////////////////////////////////////////
class WeightDisplay : public Bitmap<bool, SEGMENT_COUNT, 4>
{
public:
  int decode_digit(int digit, bool blank_is_zero=false);
  bool is_current_weight();
  bool is_previous_weight();
  bool is_kilograms();
  bool try_get_weight_grams(int& grams);
  static int decode_7_segment(bool blank_is_zero, int bits);
};

#endif
