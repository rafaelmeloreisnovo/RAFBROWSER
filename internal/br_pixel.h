#pragma once
#include "br_types.h"
/* BrowserRaf S7 RGB24 pixel-space kernel. No libc, malloc, syscalls or floats.
 * Caller supplies DECODED RGB24 data, length, dimensions, row stride.
 * PNG/JPEG/ICC/raster-to-vector recovery are intentionally out of scope.
 * Colorspaces are arithmetic projections, NOT device-calibrated print proofs.
 */
#ifndef BR_TYPES_H_UNRELATED
/* Standalone header: may include br_types.h only in BrowserRaf build. */
typedef unsigned char brpix_u8;
typedef unsigned int brpix_u32;
typedef unsigned long long brpix_u64;
#endif

typedef struct { brpix_u8 r,g,b; } BRPixRGB;
typedef struct { brpix_u8 c,m,y,k; } BRPixCMYK;
typedef struct {
    brpix_u64 count,rs,gs,bs,gray601_sum,gray709_sum;
    brpix_u64 selected_count,sx,sy,sxx,syy,sxy;
    brpix_u64 edges_4;
} BRPixStats;

static inline brpix_u32 brpix_udiv(brpix_u32 n, brpix_u32 d){
    if(!d)return 0u;
    brpix_u32 r=0u,q=0u;
    for(brpix_u32 i=32u;i>0u;){
        --i;r=(r<<1u)|((n>>i)&1u);
        brpix_u32 m=0u-(brpix_u32)(r>=d);
        r-=d&m;q|=(m&1u)<<i;
    }
    return q;
}
/* Historical RAFAELIA incorporating source: BT.601-like luma floor. */
static inline brpix_u8 brpix_pb601(BRPixRGB p){
    return (brpix_u8)brpix_udiv(299u*p.r+587u*p.g+114u*p.b,1000u);
}
/* BT.709 code-value grayscale rounded; distinct from radiometric CIE luminance. */
static inline brpix_u8 brpix_pb709(BRPixRGB p){
    return (brpix_u8)brpix_udiv(2126u*p.r+7152u*p.g+722u*p.b+5000u,10000u);
}
/* Arithmetic CMYK 0..100; ICC profile/ink GCR/black generation UNMEASURED. */
static inline BRPixCMYK brpix_cmyk(BRPixRGB p){
    brpix_u32 max=(p.r>p.g?p.r:p.g);
    if((brpix_u32)p.b>max)max=p.b;
    BRPixCMYK x={0,0,0,100};
    if(!max)return x;
    x.c=(brpix_u8)brpix_udiv((max-p.r)*100u,max);
    x.m=(brpix_u8)brpix_udiv((max-p.g)*100u,max);
    x.y=(brpix_u8)brpix_udiv((max-p.b)*100u,max);
    x.k=(brpix_u8)brpix_udiv((255u-max)*100u,255u);
    return x;
}
static inline BRPixRGB brpix_at(const brpix_u8*buf,brpix_u64 off){
    BRPixRGB p={buf[off],buf[off+1u],buf[off+2u]};return p;
}
static inline brpix_u32 brpix_absdiff(brpix_u32 a,brpix_u32 b){
    return a>b?a-b:b-a;
}
/* Input packed RGB/RGBA, stride=w*channels, no external references.
 * Reversible raster transforms 0/90/180/270 clockwise. Geometry changes
 * coordinate map but does not resample, re-color or premultiply alpha. */
static inline s32 BRPX_ROTATE(const u8*src,u32 w,u32 h,u32 channels,u32 turns,
                      u8*dst,u32 capacity){
    if(!src||!dst||src==dst||!w||!h||w>256u||h>256u||
       (channels!=3u&&channels!=4u)||turns>3u)return-1;
    u32 pixels=w*h,bytes=pixels*channels;
    if(pixels>65536u||capacity<bytes)return-1;
    /* Reject ALL overlapping source/destination spans, not just src==dst.
     * Integer-address subtraction is bounded by the target flat ABI. */
    usize sa=(usize)src,da=(usize)dst;
    if((da>=sa&&da-sa<(usize)bytes)||(sa>da&&sa-da<(usize)bytes))
        return-1;
    u32 dw=(turns&1u)?h:w;
    for(u32 y=0u;y<h;y++)for(u32 x=0u;x<w;x++){
        u32 ox=x,oy=y;
        if(turns==1u){ox=h-1u-y;oy=x;}
        else if(turns==2u){ox=w-1u-x;oy=h-1u-y;}
        else if(turns==3u){ox=y;oy=w-1u-x;}
        u32 from=(y*w+x)*channels,to=(oy*dw+ox)*channels;
        for(u32 c=0u;c<channels;c++)dst[to+c]=src[from+c];
    }
    return (s32)bytes;
}
/* mode 1 blue, 2 orange; raw spatial moments are NOT source-space vectors.
 * Valid for dimensions in [1,4096], stride and total byte-size validated. */
static int brpix_scan_rgb24(const brpix_u8*data,brpix_u64 bytes,brpix_u32 w,
                           brpix_u32 h,brpix_u32 stride,brpix_u8 mode,
                           brpix_u8 edge_threshold,BRPixStats*o){
    if(!data||!o||!w||!h||w>4096u||h>4096u||
       stride<w*3u||mode<1u||mode>2u||
       bytes<(brpix_u64)(h-1u)*stride+(brpix_u64)w*3u)return-1;
    BRPixStats zero={0};*o=zero;
    for(brpix_u32 y=0u;y<h;y++){
        for(brpix_u32 x=0u;x<w;x++){
            brpix_u64 off=(brpix_u64)y*stride+(brpix_u64)x*3u;
            BRPixRGB p=brpix_at(data,off);
            brpix_u32 gy=brpix_pb601(p);
            o->count++;o->rs+=p.r;o->gs+=p.g;o->bs+=p.b;
            o->gray601_sum+=gy;o->gray709_sum+=brpix_pb709(p);
            brpix_u32 sel=(mode==1u)?
              (p.b>(brpix_u32)p.r*5u/4u&&p.b>(brpix_u32)p.g*107u/100u&&p.b>75u):
              (p.r>(brpix_u32)p.g*5u/4u&&p.g>(brpix_u32)p.b*14u/10u&&p.r>85u&&p.g>30u);
            if(sel){o->selected_count++;o->sx+=x;o->sy+=y;
                    o->sxx+=(brpix_u64)x*x;o->syy+=(brpix_u64)y*y;
                    o->sxy+=(brpix_u64)x*y;}
            if(x+1u<w){
                BRPixRGB p2=brpix_at(data,off+3u);
                if(brpix_absdiff(gy,brpix_pb601(p2))>=edge_threshold)o->edges_4++;
            }
            if(y+1u<h){
                BRPixRGB p2=brpix_at(data,off+stride);
                if(brpix_absdiff(gy,brpix_pb601(p2))>=edge_threshold)o->edges_4++;
            }
        }
    }
    return 0;
}
