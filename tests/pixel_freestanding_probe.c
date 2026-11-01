/* No runtime support or libc: compile-only exact machine-code ABI exercise.
 * Does not claim device/board execution or physical pixel decoding. */
#include "../internal/br_pixel.h"
static volatile u32 observable;
s32 brpx_abi_probe(const u8*sample,u8*out){
    BRPX_Tile t={0};
    BRPX_Geo g={0};
    BRPX_RGB r={17u,61u,239u};
    BRPX_CMYK c=BRPX_TO_CMYK(r);
    BRPX_RGB back=BRPX_FROM_CMYK(c);
    s32 a=BRPX_TILE16(sample,1u,1u,3u,&t);
    s32 b=BRPX_ROTATE(sample,1u,1u,3u,1u,out,3u);
    u8 mask[1]={1u};
    s32 d=BRPX_GEOMETRY(mask,1u,1u,1u,&g);
    observable=(u32)BRPX_LUMA(back)+(u32)t.mean_y+g.count+(u32)BRPX_PB(r,128u);
    return a+b+d;
}
