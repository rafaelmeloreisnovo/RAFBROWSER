#pragma once
/* br_entropy.h — entropy boundary for freestanding BrowserRaf
 *
 * Security contract:
 *   - Linux getrandom(2) is the only entropy source.
 *   - short reads and EINTR are handled explicitly.
 *   - any other error fails closed and clears the destination buffer.
 *   - there is no deterministic, timestamp, address or LFSR fallback.
 */
#include "br_sys.h"

#define BR_EINTR 4
#define BR_GRND_DEFAULT 0u

#if defined(__arm__)
#define BR_NR_GETRANDOM 384u
AI s32 BR_GETRANDOM(void*buf,u32 n,u32 flags){
    return _sc3(BR_NR_GETRANDOM,(u32)(usize)buf,n,flags);
}
#elif defined(__aarch64__)
#define BR_NR_GETRANDOM 278u
AI s32 BR_GETRANDOM(void*buf,u32 n,u32 flags){
    return (s32)_sc3(BR_NR_GETRANDOM,(u64)(usize)buf,(u64)n,(u64)flags);
}
#elif defined(__x86_64__)
#define BR_NR_GETRANDOM 318u
AI s32 BR_GETRANDOM(void*buf,u32 n,u32 flags){
    return (s32)_sc3(BR_NR_GETRANDOM,(u64)(usize)buf,(u64)n,(u64)flags);
}
#else
#error "BrowserRaf entropy boundary supports only ARM32, ARM64 and x86-64"
#endif

AI s32 BR_RANDOM_FILL(void*buf,u32 n){
    u8*p=(u8*)buf;
    u32 off=0u;

    if(!p&&n!=0u)return-1;

    while(off<n){
        s32 got=BR_GETRANDOM(p+off,n-off,BR_GRND_DEFAULT);
        if(got<0){
            if(got==-(s32)BR_EINTR)continue;
            MC0(p,n);
            return-1;
        }
        if(got==0||(u32)got>n-off){
            MC0(p,n);
            return-1;
        }
        off+=(u32)got;
    }
    return 0;
}
