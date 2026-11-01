/* Test-only C standard library; not shipped into freestanding pixel core. */
#include <stdio.h>
#include "../internal/br_pixel.h"
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL:%d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned checks;
int main(void){
    BRPixRGB black={0,0,0},white={255,255,255},red={255,0,0},green={0,255,0},blue={0,0,255};
    CK(brpix_pb601(black)==0);CK(brpix_pb601(white)==255);
    CK(brpix_pb601(red)==76);CK(brpix_pb601(green)==149);CK(brpix_pb601(blue)==29);
    CK(brpix_pb709(red)==54);CK(brpix_pb709(green)==182);CK(brpix_pb709(blue)==18);
    BRPixCMYK c=brpix_cmyk(black);CK(c.c==0&&c.m==0&&c.y==0&&c.k==100);
    c=brpix_cmyk(white);CK(c.c==0&&c.m==0&&c.y==0&&c.k==0);
    c=brpix_cmyk(red);CK(c.c==0&&c.m==100&&c.y==100&&c.k==0);
    brpix_u8 rgb[12]={255,0,0,0,255,0,0,0,255,0,0,0};
    BRPixStats s;
    CK(brpix_scan_rgb24(rgb,sizeof(rgb),2,2,6,1,20,&s)==0);
    CK(s.count==4&&s.rs==255&&s.gs==255&&s.bs==255);
    CK(s.selected_count==1&&s.sx==0&&s.sy==1&&s.sxx==0&&s.syy==1);
    CK(brpix_scan_rgb24(rgb,sizeof(rgb)-1,2,2,6,1,20,&s)!=0);
    CK(brpix_scan_rgb24(rgb,sizeof(rgb),2,2,2,1,20,&s)!=0);
    CK(brpix_scan_rgb24(rgb,sizeof(rgb),2,2,6,0,20,&s)!=0);
    CK(brpix_scan_rgb24(0,sizeof(rgb),2,2,6,1,20,&s)!=0);
    CK(brpix_pb601((BRPixRGB){255,0,0})==brpix_pb601((BRPixRGB){255,0,1}));
    for(brpix_u32 n=0;n<=25500;n+=37){
        for(brpix_u32 d=1;d<=255;d+=17){
            CK(brpix_udiv(n,d)==n/d);
        }
    }
    /* One-bit input change can be invisible after BOTH lossy projections. */
    BRPixRGB a={197u,215u,20u},b={196u,215u,20u};
    CK(brpix_pb601(a)==187u&&brpix_pb601(b)==187u);
    BRPixCMYK ac=brpix_cmyk(a),bc=brpix_cmyk(b);
    CK(ac.c==bc.c&&ac.m==bc.m&&ac.y==bc.y&&ac.k==bc.k);
    CK(brpix_udiv(0xffffffffu,0xffffffffu)==1u);
    CK(brpix_udiv(0xffffffffu,0x80000000u)==1u);
    /* Reversible 2x2 RGB pixel-coordinate rotations, exact byte preservation. */
    const brpix_u8 square[24]={
        1,2,3, 4,5,6,
        7,8,9, 10,11,12,
        0,0,0, 0,0,0, 0,0,0, 0,0,0
    };
    const brpix_u8 rotated_expected[4][12]={
        {1,2,3, 4,5,6, 7,8,9, 10,11,12},
        {7,8,9, 1,2,3, 10,11,12, 4,5,6},
        {10,11,12, 7,8,9, 4,5,6, 1,2,3},
        {4,5,6, 10,11,12, 1,2,3, 7,8,9}
    };
    brpix_u8 rotated[12];
    for(brpix_u32 turn=0;turn<4;turn++){
        CK(BRPX_ROTATE(square,2,2,3,turn,rotated,sizeof(rotated))==12);
        for(brpix_u32 i=0;i<12;i++)CK(rotated[i]==rotated_expected[turn][i]);
    }
    CK(BRPX_ROTATE(square,2,2,3,1,rotated,11)!=12);
    CK(BRPX_ROTATE(square,2,2,3,1,(brpix_u8*)square+3,12)<0);
    CK(BRPX_ROTATE(square,2,2,3,4,rotated,sizeof(rotated))<0);
    fprintf(stderr,"PASS_PIXEL %u checks/no libc in kernel\n",checks);
    return 0;
}
