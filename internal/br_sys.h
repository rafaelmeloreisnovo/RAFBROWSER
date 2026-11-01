#pragma once
/* br_sys.h — Syscalls ARM32/ARM64/x86-64: socket connect send recv
 * [R06] ARM32: socket=281 connect=283 sendto=290 recvfrom=292 close=6
 * [R07] ARM64: socket=198 connect=203 sendto=206 recvfrom=207 close=57
 * [R08] x86-64: socket=41 connect=42 sendto=44 recvfrom=45 close=3
 * [R09] Timestamp: clock_gettime CLOCK_MONOTONIC
 * [R10] Exit: exit_group ARM32=248 ARM64=94 x86-64=231
 */
#include "br_types.h"
/* ── AF_INET / SOCK_STREAM ─────────────────────────────────────────────── */
#define AF_INET  2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6
#define CLOCK_MONO  1
#define SOL_SOCKET  1
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21
#define MSG_NOSIGNAL 0x4000
typedef struct PK { usize tv_sec; usize tv_usec; } BRTimeVal;
typedef struct PK { u16 fam; u16 port_be; u8 ip[4]; u8 _z[8]; } SA4; /* sockaddr_in */

#if defined(__arm__)
typedef struct{s32 s,n;}TS32;
AI s32 _sc6(u32 r,u32 a,u32 b,u32 c,u32 d,u32 e,u32 f){
    register s32 r0 __asm__("r0")=(s32)a; register u32 r1 __asm__("r1")=b;
    register u32 r2 __asm__("r2")=c;     register u32 r3 __asm__("r3")=d;
    register u32 r4 __asm__("r4")=e;     register u32 r5 __asm__("r5")=f;
    register u32 r7 __asm__("r7")=r;
    __asm__ volatile("svc #0":"+r"(r0):"r"(r1),"r"(r2),"r"(r3),"r"(r4),"r"(r5),"r"(r7):"memory","cc");
    return r0;
}
AI s32 _sc3(u32 r,u32 a,u32 b,u32 c){return _sc6(r,a,b,c,0,0,0);}
AI s32 _sc2(u32 r,u32 a,u32 b){return _sc6(r,a,b,0,0,0,0);}
AI s32 _sc1(u32 r,u32 a){return _sc6(r,a,0,0,0,0,0);}
AI s32 SOCKET(void){return _sc3(281u,AF_INET,SOCK_STREAM,IPPROTO_TCP);}
AI s32 CONNECT(s32 fd,const SA4*a){return _sc3(283u,(u32)fd,(u32)(usize)a,(u32)sizeof(SA4));}
AI s32 SEND(s32 fd,const void*b,u32 n){return _sc6(290u,(u32)fd,(u32)(usize)b,(u32)n,MSG_NOSIGNAL,0,0);}
AI s32 RECV(s32 fd,void*b,u32 n){return _sc6(292u,(u32)fd,(u32)(usize)b,(u32)n,0,0,0);}
AI s32 CLOSE(s32 fd){return _sc1(6u,(u32)fd);}
AI s32 SETSOCKOPT(s32 fd,s32 lvl,s32 opt,const void*v,u32 n){return _sc6(294u,(u32)fd,(u32)lvl,(u32)opt,(u32)(usize)v,n,0);}
AI u64 NS(void){TS32 t={0,0};_sc2(263u,CLOCK_MONO,(u32)(usize)&t);return(u64)(u32)t.s*1000000000ULL+(u64)(u32)t.n;}
AI s32 WR(u32 f,const void*b,u32 n){return _sc3(4u,f,(u32)(usize)b,n);}
AI s32 BR_READ(u32 f,void*b,u32 n){return _sc3(3u,f,(u32)(usize)b,n);}
NR void EX(void){_sc1(248u,0u);__builtin_unreachable();}

#elif defined(__aarch64__)
typedef struct{s64 s,n;}TS64;
AI s64 _sc6(u64 r,u64 a,u64 b,u64 c,u64 d,u64 e,u64 f){
    register u64 x8 __asm__("x8")=r;
    register s64 x0 __asm__("x0")=(s64)a; register u64 x1 __asm__("x1")=b;
    register u64 x2 __asm__("x2")=c;     register u64 x3 __asm__("x3")=d;
    register u64 x4 __asm__("x4")=e;     register u64 x5 __asm__("x5")=f;
    __asm__ volatile("svc #0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5):"memory","cc");
    return x0;
}
AI s64 _sc3(u64 r,u64 a,u64 b,u64 c){return _sc6(r,a,b,c,0,0,0);}
AI s64 _sc2(u64 r,u64 a,u64 b){return _sc6(r,a,b,0,0,0,0);}
AI s64 _sc1(u64 r,u64 a){return _sc6(r,a,0,0,0,0,0);}
AI s32 SOCKET(void){return(s32)_sc3(198u,AF_INET,SOCK_STREAM,IPPROTO_TCP);}
AI s32 CONNECT(s32 fd,const SA4*a){return(s32)_sc3(203u,(u64)fd,(u64)(usize)a,(u64)sizeof(SA4));}
AI s32 SEND(s32 fd,const void*b,u32 n){return(s32)_sc6(206u,(u64)fd,(u64)(usize)b,(u64)n,MSG_NOSIGNAL,0,0);}
AI s32 RECV(s32 fd,void*b,u32 n){return(s32)_sc6(207u,(u64)fd,(u64)(usize)b,(u64)n,0,0,0);}
AI s32 CLOSE(s32 fd){return(s32)_sc1(57u,(u64)fd);}
AI s32 SETSOCKOPT(s32 fd,s32 lvl,s32 opt,const void*v,u32 n){return(s32)_sc6(208u,(u64)fd,(u64)lvl,(u64)opt,(u64)(usize)v,(u64)n,0);}
AI u64 NS(void){TS64 t={0,0};_sc2(113u,CLOCK_MONO,(u64)(usize)&t);return(u64)t.s*1000000000ULL+(u64)t.n;}
AI s32 WR(u32 f,const void*b,u32 n){return(s32)_sc3(64u,(u64)f,(u64)(usize)b,(u64)n);}
AI s32 BR_READ(u32 f,void*b,u32 n){return(s32)_sc3(63u,(u64)f,(u64)(usize)b,(u64)n);}
NR void EX(void){_sc1(94u,0u);__builtin_unreachable();}

#elif defined(__x86_64__)
typedef struct{s64 s,n;}TS64;
AI s64 _sc6(u64 r,u64 a,u64 b,u64 c,u64 d,u64 e,u64 f){
    s64 x; register u64 r10 __asm__("r10")=d,r8 __asm__("r8")=e,r9 __asm__("r9")=f;
    __asm__ volatile("syscall":"=a"(x):"a"(r),"D"(a),"S"(b),"d"(c),"r"(r10),"r"(r8),"r"(r9):"rcx","r11","memory");
    return x;
}
AI s64 _sc3(u64 r,u64 a,u64 b,u64 c){return _sc6(r,a,b,c,0,0,0);}
AI s64 _sc2(u64 r,u64 a,u64 b){return _sc6(r,a,b,0,0,0,0);}
AI s64 _sc1(u64 r,u64 a){s64 x;__asm__ volatile("syscall":"=a"(x):"a"(r),"D"(a):"rcx","r11","memory");return x;}
AI s32 SOCKET(void){return(s32)_sc3(41u,AF_INET,SOCK_STREAM,IPPROTO_TCP);}
AI s32 CONNECT(s32 fd,const SA4*a){return(s32)_sc3(42u,(u64)fd,(u64)(usize)a,(u64)sizeof(SA4));}
AI s32 SEND(s32 fd,const void*b,u32 n){return(s32)_sc6(44u,(u64)fd,(u64)(usize)b,(u64)n,MSG_NOSIGNAL,0,0);}
AI s32 RECV(s32 fd,void*b,u32 n){return(s32)_sc6(45u,(u64)fd,(u64)(usize)b,(u64)n,0,0,0);}
AI s32 CLOSE(s32 fd){return(s32)_sc1(3u,(u64)fd);}
AI s32 SETSOCKOPT(s32 fd,s32 lvl,s32 opt,const void*v,u32 n){return(s32)_sc6(54u,(u64)fd,(u64)lvl,(u64)opt,(u64)(usize)v,(u64)n,0);}
AI u64 NS(void){TS64 t={0,0};_sc2(228u,CLOCK_MONO,(u64)(usize)&t);return(u64)t.s*1000000000ULL+(u64)t.n;}
AI s32 WR(u32 f,const void*b,u32 n){return(s32)_sc3(1u,(u64)f,(u64)(usize)b,(u64)n);}
AI s32 BR_READ(u32 f,void*b,u32 n){return(s32)_sc3(0u,(u64)f,(u64)(usize)b,(u64)n);}
NR void EX(void){_sc1(231u,0u);__builtin_unreachable();}
#endif

/* ── I/O SEM PRINTF ────────────────────────────────────────────────────── */
static void PS(const char*s){u32 n=0;while(s[n])n++;if(n)WR(1,s,n);}
static void PN(u64 v){char b[22];s32 i=21;b[i]='\n';i--;
    if(!v){b[i--]='0';}else{while(v){b[i--]='0'+(char)(v%10u);v/=10u;}}
    WR(1,b+i+1,(u32)(20u-i));}
static void PH(u32 v){static const char h[]="0123456789abcdef";
    char b[11];b[0]='0';b[1]='x';b[10]='\n';
    for(s32 i=9;i>=2;i--){b[i]=h[v&0xFu];v>>=4;}WR(1,b,11u);}

/* ── BRANCHLESS STRING OPS SEM LIBC ────────────────────────────────────── */
AI u32 SL(const char*s){u32 n=0;while(s[n])n++;return n;}
AI void MC(void*d,const void*s,u32 n){u8*dd=(u8*)d;const u8*ss=(const u8*)s;while(n--)dd[n]=ss[n];}
AI s32 MC0(void*d,u32 n){u8*dd=(u8*)d;while(n--)dd[n]=0;return 0;}
AI s32 BR_MEMCMP(const void*a,const void*b,u32 n){
    const u8*x=(const u8*)a;const u8*y=(const u8*)b;
    for(u32 i=0;i<n;i++){if(x[i]!=y[i])return (s32)x[i]-(s32)y[i];}
    return 0;
}
/* u32 → string decimal na stack */
AI u32 UTOA(u32 v,char*out){
    char t[10];s32 i=0;if(!v){out[0]='0';out[1]=0;return 1;}
    while(v){t[i++]='0'+(char)(v%10u);v/=10u;}
    u32 len=(u32)i;s32 j=0;while(i>0)out[j++]=t[--i];out[j]=0;return len;
}
/* Big-endian port */
AI u16 HTON16(u16 v){return(u16)((v>>8u)|(v<<8u));}
/* SO_RCVTIMEO protege recv/recvfrom; connect pode bloquear em socket blocking. */
/* TODO: CONNECT com timeout real via NONBLOCK + POLL. */
AI s32 SET_RECV_TIMEOUT(s32 fd,usize sec){
    BRTimeVal tv;
    tv.tv_sec=sec;
    tv.tv_usec=(usize)0u;
    return SETSOCKOPT(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,(u32)sizeof(tv));
}

AI s32 SET_SEND_TIMEOUT(s32 fd,usize sec){
    /* Kernel socket I/O send timeout also limits blocking connect on Linux;
     * runtime behavior on a particular Android kernel needs device evidence. */
    BRTimeVal tv;tv.tv_sec=sec;tv.tv_usec=(usize)0u;
    return SETSOCKOPT(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,(u32)sizeof(tv));
}
/* IP parse "a.b.c.d" → u8[4] */
AI s32 PARSE_IP(const char*s,u8*ip){
    if(!s||!ip)return-1;
    u32 index=0u,octet=0u,digits=0u,part=0u;
    while(s[index]&&index<16u){
        u8 c=(u8)s[index++];
        if(c>='0'&&c<='9'){
            if(digits>=3u||(digits!=0u&&part==0u))return-1;
            part=part*10u+(u32)(c-'0');
            if(part>255u)return-1;
            digits++;
        }else if(c=='.'){
            if(!digits||octet>=3u)return-1;
            ip[octet++]=(u8)part;
            part=0u;digits=0u;
        }else return-1;
    }
    if(index==16u||octet!=3u||digits==0u)return-1;
    ip[3]=(u8)part;
    return 0;
}
