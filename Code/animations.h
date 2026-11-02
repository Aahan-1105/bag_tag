#pragma once
/*
  Animation registry.

  Each animN.h is the *unmodified* output of image2cpp (https://javl.github.io/image2cpp/)
  for one GIF's frames. Wrapping each in its own namespace means you can leave
  image2cpp's default identifier prefix ("epd_bitmap_") alone for every GIF.

  To change what plays: overwrite anim1.h / anim2.h / anim3.h with your own
  image2cpp output, and adjust frameMs below (image2cpp has no timing info).
  To add a 4th animation: add anim4.h, a namespace block, and one table row.
*/

namespace anim1 {
#include "anim1.h"
}
namespace anim2 {
#include "anim2.h"
}
namespace anim3 {
#include "anim3.h"
}

struct Anim {
  const unsigned char* const* frames;  // array of 1024-byte (128x64, 1bpp) frames
  int      count;
  uint16_t frameMs;                    // delay between frames
};

static const Anim ANIMS[] = {
  { anim1::epd_bitmap_allArray, anim1::epd_bitmap_allArray_LEN, 100 },
  { anim2::epd_bitmap_allArray, anim2::epd_bitmap_allArray_LEN, 100 },
  { anim3::epd_bitmap_allArray, anim3::epd_bitmap_allArray_LEN, 100 },
};
#define ANIM_COUNT (int)(sizeof(ANIMS) / sizeof(ANIMS[0]))
