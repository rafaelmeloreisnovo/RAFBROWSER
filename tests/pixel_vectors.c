/* Host-only verification of RAFAELIA PIXEL_DESC_V1. Source library uses
 * freestanding C only; printf/sanitizers are NOT part of target ELF. */
#include <stdio.h>
#include <string.h>
#include "../internal/br_pixel.h"
static u32 checks=0u,errors=0u;
#define C(x) do{checks++;if(!(x)){errors++;fprintf(stderr,"PIXEL_FAIL line=%d %s\n",__LINE__,#x);}}while(0)

static void color_vectors(void){
    BRPX_RGB colors[]={{0,0,0},{255,255,255},{255,0,0},
        {0,255,0},{0,0,255},{128,128,128},{1,0,0},{7,61,243},
        {23,54,72},{111,113,119},{255,2,250}};
    C(BRPX_LUMA(colors[0])==0u);
    C(BRPX_LUMA(colors[1])==255u);
    C(BRPX_LUMA(colors[2])==53u);
    C(BRPX_LUMA(colors[3])==182u);
    C(BRPX_LUMA(colors[4])==18u);
    C(BRPX_LUMA(colors[5])==128u);
    C(BRPX_PB(colors[2],128u)==0u);
    C(BRPX_PB(colors[3],128u)==1u);
    C(BRPX_PB(colors[5],128u)==1u);
    for(u32 i=0u;i<sizeof(colors)/sizeof(colors[0]);i++){
        BRPX_CMYK k=BRPX_TO_CMYK(colors[i]);
        BRPX_RGB b=BRPX_FROM_CMYK(k);
        C(b.r==colors[i].r && b.g==colors[i].g && b.b==colors[i].b);
    }
    /* Exhaustive 8-bit pair magnitudes, without using floating point. */
    for(u32 high=0u;high<256u;high++)for(u32 v=0u;v<=high;v++){
        BRPX_RGB a={(u8)high,(u8)v,(u8)0u};
        BRPX_CMYK k=BRPX_TO_CMYK(a);
        BRPX_RGB b=BRPX_FROM_CMYK(k);
        C(a.r==b.r&&a.g==b.g&&a.b==b.b);
    }
    u32 seed=0xC0FFEE42u;
    for(u32 i=0u;i<8192u;i++){
        seed^=seed<<13u;seed^=seed>>17u;seed^=seed<<5u;
        BRPX_RGB a={(u8)seed,(u8)(seed>>8u),(u8)(seed>>16u)};
        BRPX_CMYK k=BRPX_TO_CMYK(a);
        BRPX_RGB b=BRPX_FROM_CMYK(k);
        C(a.r==b.r&&a.g==b.g&&a.b==b.b);
        C(BRPX_PB(a,128u)==(u8)(BRPX_LUMA(a)>=128u));
    }
    /* No ICC gamut, press ink behavior, hue or historical motif is proven. */
}
static void geometric_vectors(void){
    const u8 src[]={
        0,1,2, 3,4,5, 6,7,8,
        9,10,11, 12,13,14, 15,16,17
    }; /* RGB, width=3 height=2, six distinct pixels */
    u8 a[18]={0},b[18]={0},c[18]={0},d[18]={0};
    C(BRPX_ROTATE(src,3u,2u,3u,1u,a,sizeof(a))==18);
    C(BRPX_ROTATE(a,2u,3u,3u,1u,b,sizeof(b))==18);
    C(BRPX_ROTATE(b,3u,2u,3u,1u,c,sizeof(c))==18);
    C(BRPX_ROTATE(c,2u,3u,3u,1u,d,sizeof(d))==18);
    C(memcmp(src,d,sizeof(src))==0); /* four quarter-turns identity */
    C(BRPX_ROTATE(src,3u,2u,3u,2u,b,sizeof(b))==18);
    C(BRPX_ROTATE(b,3u,2u,3u,2u,c,sizeof(c))==18);
    C(memcmp(src,c,sizeof(src))==0);
    C(BRPX_ROTATE(src,3u,2u,3u,1u,a,17u)==-1);
    C(BRPX_ROTATE(src,3u,2u,3u,4u,a,sizeof(a))==-1);
    C(BRPX_ROTATE(src,0u,2u,3u,0u,a,sizeof(a))==-1);
    C(BRPX_ROTATE(src,3u,2u,3u,0u,(u8*)src,sizeof(src))==-1);
    u8 overlapping[40]={0};memcpy(overlapping,src,sizeof(src));
    C(BRPX_ROTATE(overlapping,3u,2u,3u,1u,overlapping+1u,39u)==-1);
    u8 rgba[]={255,0,0,17, 0,255,0,34, 0,0,255,51, 255,255,255,68};
    u8 ro[16]={0},re[16]={0};
    C(BRPX_ROTATE(rgba,2u,2u,4u,1u,ro,sizeof(ro))==16);
    C(BRPX_ROTATE(ro,2u,2u,4u,3u,re,sizeof(re))==16);
    C(memcmp(rgba,re,sizeof(rgba))==0);
    const u8 mask[]={0,1,0,1,1,1,0,1,0};
    BRPX_Geo g;
    C(BRPX_GEOMETRY(mask,3u,3u,sizeof(mask),&g)==0);
    C(g.count==5u&&g.xmin==0u&&g.ymin==0u&&g.xmax==2u&&g.ymax==2u);
    C(g.cx_q8==256u&&g.cy_q8==256u);
    const u8 empty[]={0,0,0,0};
    C(BRPX_GEOMETRY(empty,2u,2u,sizeof(empty),&g)==1&&g.count==0u);
    const u8 malformed[]={0,2,0,0};
    C(BRPX_GEOMETRY(malformed,2u,2u,sizeof(malformed),&g)==-1);
    C(BRPX_GEOMETRY(mask,3u,3u,8u,&g)==-1);
}
static void tiles(void){
    /* 2x2 red/green/blue/white; explicit channel order and tiled metadata. */
    const u8 src[]={255,0,0, 0,255,0, 0,0,255, 255,255,255};
    BRPX_Tile a={0},b={0};
    C(BRPX_TILE16(src,2u,2u,sizeof(src),&a)==0);
    C(a.samples==4u);
    C(a.mean_rgb[0]==127u&&a.mean_rgb[1]==127u&&a.mean_rgb[2]==127u);
    C(a.mean_y==127u);
    C(a.key[0]==3u&&a.key[1]==3u&&a.key[2]==3u);
    C(BRPX_TILE16(src,2u,2u,sizeof(src),&b)==0);
    C(a.mean_y==b.mean_y&&a.var_y==b.var_y&&a.grad_l1==b.grad_l1&&
      memcmp(a.mean_rgb,b.mean_rgb,3u)==0&&memcmp(a.key,b.key,5u)==0);
    C(BRPX_TILE16(src,17u,1u,sizeof(src),&a)==-1);
    C(BRPX_TILE16(src,2u,2u,sizeof(src)-1u,&a)==-1);
    const u8 black[]={0u,0u,0u};
    C(BRPX_TILE16(black,1u,1u,3u,&a)==0);
    C(a.mean_y==0u&&a.var_y==0u&&a.grad_l1==0u);
    C(BRPX_ISQRT(0u)==0u&&BRPX_ISQRT(1u)==1u);
    C(BRPX_ISQRT(65535u)==255u&&BRPX_ISQRT(65536u)==256u);
}
int main(void){
    color_vectors();geometric_vectors();tiles();
    fprintf(stderr,"BrowserRaf PIXEL_DESC_V1: %u checks, %u failed\n",checks,errors);
    return errors?1:0;
}
