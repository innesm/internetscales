
#include "WeightDisplay.h"

//Layout
//7x4 'pixels'
//cols:
// 0-1 = tens digit
// 2-3 = units digit (and decimal pt)
// 4-5 = tenths digit
// 6 = icons
//  row 0 = KG
//  row 1 = Lbs?
//  row 2 = "current wt"
//  row 3 = "previous wt"


int WeightDisplay::decode_digit(int digit, bool blank_is_zero)
{
  // each digit is displayed in two adjacent columns
  int s0 = digit * 2;
  int s1 = digit * 2 + 1;

  return decode_7_segment(blank_is_zero,
    (pixel(s0,3) << 6) +
    (pixel(s0,0) << 5) +
    (pixel(s1,1) << 4) +
    (pixel(s0,1) << 3) +
    (pixel(s0,2) << 2) +
    (pixel(s1,2) << 1) +
    pixel(s1,0));
}

bool WeightDisplay::is_current_weight()
{
  return pixel(6, 2);
}

bool WeightDisplay::is_previous_weight()
{
  return pixel(6, 3);
}

bool WeightDisplay::is_kilograms()
{
  return pixel(6, 0);
}

bool WeightDisplay::try_get_weight_grams(int& grams)
{
  grams = -1;

  //todo convert other units to grams
  if (!is_kilograms())
    return false;

  int tens = decode_digit(0, true);
  if (tens < 0)
    return false;
  grams = 10000 * tens;

  int units = decode_digit(1);
  if (units < 0)
    return false;
  grams += 1000 * units;

  int tenths = decode_digit(2);
  if (tenths < 0)
    return false;
  grams += 100 * tenths;

  return true;
}

int WeightDisplay::decode_7_segment(bool blank_is_zero, int bits)
{
    if (bits == 0) return blank_is_zero ? 0 : -1;
    if (bits == 0b1110111) return 0;
    if (bits == 0b0010010) return 1;
    if (bits == 0b1011101) return 2;
    if (bits == 0b1011011) return 3;
    if (bits == 0b0111010) return 4;
    if (bits == 0b1101011) return 5;
    if (bits == 0b1101111) return 6;
    if (bits == 0b0010011) return 7;
    if (bits == 0b1111111) return 8;
    if (bits == 0b1111011) return 9;
    return -2;
}
