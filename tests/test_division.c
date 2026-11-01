/* Host-only exact arithmetic verification. Production code remains no-libc. */
#include <stdio.h>
#include "../internal/br_main.c"

int main(void){
    u64 cases[]={
        0ull,1ull,999999ull,1000000ull,1000001ull,
        4294967295ull,4294967296ull,4294967297ull,
        0x7fffffffffffffffull,0x8000000000000000ull,
        0xffffffffffffffffull
    };
    u32 divisors[]={1u,2u,3u,10u,1000000u,0xffffffffu};
    u32 count=0u;
    for(u32 i=0u;i<sizeof(cases)/sizeof(cases[0]);i++){
        for(u32 j=0u;j<sizeof(divisors)/sizeof(divisors[0]);j++){
            u64 n=cases[i];
            u32 d=divisors[j];
            if(BR_DIV_U64_U32(n,d)!=n/d){
                fprintf(stderr,"DIV FAIL case=%u divisor=%u\n",i,d);
                return 1;
            }
            count++;
        }
    }
    u64 seed=0x0123456789abcdefull;
    for(u32 i=0u;i<1024u;i++){
        seed^=seed<<13u;
        seed^=seed>>7u;
        seed^=seed<<17u;
        u32 d=(u32)(seed>>32u)|1u;
        if(BR_DIV_U64_U32(seed,d)!=seed/d){
            fprintf(stderr,"DIV FAIL mutation=%u\n",i);
            return 1;
        }
        count++;
    }
    if(BR_DIV_U64_U32(5u,0u)!=0u)return 1;
    count++;
    fprintf(stderr,"PASS_SCOPED arithmetic no-libc algorithm: %u exact comparisons\n",count);
    return 0;
}
