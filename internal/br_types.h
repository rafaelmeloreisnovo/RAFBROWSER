#pragma once
/* br_types.h — Tipos primitivos · registradores · flags · estados
 * [R01] ZERO stdlib ZERO heap ZERO float ZERO GC ZERO abstração
 * [R02] Flags lineares: abertos em sequência, fechados em sequência
 * [R03] Turing geométrica: estado × símbolo → estado × ação
 * [R04] Flip-flop: cada bit de estado é um flip-flop D
 * [R05] Branchless: toda operação via máscara de bit
 */
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed   int       s32;
typedef signed   long long s64;
#if defined(__aarch64__) || defined(__x86_64__)
typedef unsigned long long usize;
#else
typedef unsigned int       usize;
#endif
#define AI   __attribute__((always_inline)) static inline
#define NI   __attribute__((noinline))
#define NR   __attribute__((noreturn))
#define CLA  __attribute__((aligned(64)))
#define PK   __attribute__((packed))
/* ── FLAGS DE BROWSER (8 bits = 8 flip-flops) ──────────────────────────── */
/* Cada bit é um flip-flop D: SET=1 CLEAR=0 TOGGLE=XOR */
#define FL_IDLE      0x00u  /* 00000000: nenhum estado ativo            */
#define FL_DNS       0x01u  /* 00000001: resolvendo DNS                 */
#define FL_CONNECT   0x02u  /* 00000010: conectando TCP                 */
#define FL_TLS_HS    0x04u  /* 00000100: TLS handshake em progresso     */
#define FL_HTTP_TX   0x08u  /* 00001000: enviando requisição HTTP       */
#define FL_HTTP_RX   0x10u  /* 00010000: recebendo resposta HTTP        */
#define FL_HTML_RND  0x20u  /* 00100000: renderizando HTML              */
#define FL_ERROR     0x40u  /* 01000000: estado de erro (rollback)      */
#define FL_DONE      0x80u  /* 10000000: concluído                      */
/* Flip-flop ops: branchless, sem branch */
#define FF_SET(r,f)    ((r)|=(f))           /* SET bit               */
#define FF_CLR(r,f)    ((r)&=~(u8)(f))     /* CLEAR bit             */
#define FF_TOG(r,f)    ((r)^=(f))           /* TOGGLE bit            */
#define FF_GET(r,f)    (!!((r)&(f)))        /* GET bit               */
#define FF_NEXT(r,c,n) ((r)=(u8)(((r)&~(u8)(c))|(n))) /* transição  */
/* Transição validada de fase: só avança se máscara requerida estiver ativa */
AI u8 BR_PHASE_NEXT(u8*flags,u8 clear_mask,u8 set_mask,u8 required_mask){
    u8 f=*flags;
    if((f&required_mask)!=required_mask){
        *flags=(u8)((f&~(u8)clear_mask)|FL_ERROR);
        return 0u;
    }
    *flags=(u8)((f&~(u8)clear_mask)|set_mask);
    return 1u;
}
/* ── ESTADOS TLS 1.3 (máquina de Turing geométrica) ───────────────────── */
/* Hipercubo de estados: cada transição muda exatamente 1 bit            */
#define TLS_IDLE       0x00u  /* 000: inicial                          */
#define TLS_CLI_HELLO  0x01u  /* 001: ClientHello enviado              */
#define TLS_SRV_HELLO  0x03u  /* 011: ServerHello recebido             */
#define TLS_ENCRYPTED  0x07u  /* 111: modo criptografado ativo         */
#define TLS_APP_DATA   0x05u  /* 101: dados de aplicação               */
#define TLS_ERROR      0x04u  /* 100: erro no handshake                */
#define TLS_CLOSED     0x00u  /* 000: fechado                          */
/* ── ARENA 256KB sem malloc ──────────────────────────────────────────── */
#define AR_SZ (256u*1024u)
static u8  _AR[AR_SZ] CLA;
static u32 _AT=0, _AM=0;
AI void* GA(u32 n,u32 a){
    u32 m=a-1u,c=(_AT+m)&~m;
    if(c+n>AR_SZ)return(void*)0;
    void*p=_AR+c;_AT=c+n;return p;
}
AI void GR(void){_AT=0;}
AI void GM(void){_AM=_AT;}
AI void GRS(void){_AT=_AM;}
/* ── BUFFER ESTÁTICO 64KB para rede ────────────────────────────────────── */
#define NET_BUF 65536u
static u8 _NB[NET_BUF] CLA;  /* rx/tx buffer */
static u8 _RB[NET_BUF] CLA;  /* render buffer */
/* ── CONTEXTO DE BROWSER (sem nome de variável onde possível) ─────────── */
typedef struct PK CLA {
    /* Rede */
    s32 fd;         /* socket file descriptor                          */
    u32 port;       /* porta (80 ou 443)                               */
    u8  ip[4];      /* IPv4 do host                                    */
    /* Estado */
    u8  flags;      /* 8 flip-flops de estado do browser               */
    u8  tls;        /* estado TLS (máquina de Turing)                  */
    u8  http_ver;   /* 10=HTTP/1.0 11=HTTP/1.1                         */
    u8  use_tls;    /* 1=HTTPS 0=HTTP                                  */
    /* HTTP */
    u32 status;     /* HTTP status code                                */
    u32 content_len;/* Content-Length                                  */
    u32 rx_bytes;   /* bytes recebidos                                 */
    u32 tx_bytes;   /* bytes enviados                                  */
    /* Retry/Rollback TTL */
    u8  ttl;        /* tentativas restantes                            */
    u8  err;        /* código de erro                                  */
    u16 _pad;
    u64 t_start;    /* timestamp início                                */
    u64 t_ns;       /* latência total                                  */
    /* URL decomposto */
    char host[256]; /* hostname                                        */
    char path[512]; /* path                                            */
} BCtx;
static BCtx _BC;
