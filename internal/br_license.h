#pragma once
#include "br_types.h"
#include "br_sys.h"
/* Read the repository's active license from disk; no duplicated license text. */
AI s32 BR_LICENSE_SHOW(void){
    static const char path[]="LICENSE.md";
    s32 fd=-1;
#if defined(__arm__)
    fd=_sc3(5u,(u32)(usize)path,0u,0u);
#elif defined(__aarch64__)
    fd=(s32)_sc3(56u,(u64)(usize)(s64)-100,(u64)(usize)path,0u);
#elif defined(__x86_64__)
    fd=(s32)_sc3(257u,(u64)(s64)-100,(u64)(usize)path,0u);
#endif
    if(fd<0){PS("LICENSE.md ausente; execute na raiz do repositorio.\n");return-1;}
    u8 b[512];s32 total=0;for(u32 i=0;i<16u;i++){s32 n=BR_READ((u32)fd,b,sizeof(b));if(n<=0)break;u32 off=0;while(off<(u32)n){s32 w=WR(1u,b+off,(u32)n-off);if(w<=0){CLOSE(fd);return-1;}off+=(u32)w;}total+=n;}
    CLOSE(fd);return total>0?0:-1;
}
