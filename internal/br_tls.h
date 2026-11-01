#pragma once
/* br_tls.h — TLS 1.3 state machine: ClientHello · handshake · record layer
 * [R11] Máquina de Turing geométrica: estado × símbolo → estado × output
 * [R12] TLS 1.3 record layer: type(1) + version(2) + length(2) + data
 * [R13] ClientHello: legacy_version=0x0303 + random(32) + session_id
 *        + cipher_suites + extensions (supported_versions TLS1.3=0x0304)
 * [R14] Cipher suites: TLS_AES_128_GCM_SHA256=0x1301
 *                      TLS_AES_256_GCM_SHA384=0x1302
 *                      TLS_CHACHA20_POLY1305_SHA256=0x1303
 * [R15] Flip-flop TLS: cada bit do estado TLS é um flip-flop D
 * [R16] Branchless transition: mask = -(cond), new = (a&mask)|(b&~mask)
 */
#include "br_entropy.h"

/* ── TLS 1.3 RECORD LAYER ───────────────────────────────────────────────── */
/* record = [type:1][0x03 0x03:2][len_hi:1][len_lo:1][data:len] */
#define TLS_RT_CHANGE_CS  0x14u  /* ChangeCipherSpec (legacy) */
#define TLS_RT_ALERT      0x15u  /* Alert              */
#define TLS_RT_HANDSHAKE  0x16u  /* Handshake          */
#define TLS_RT_APP_DATA   0x17u  /* Application Data   */
#define TLS_VER_10        0x0301u /* TLS 1.0 legacy */
#define TLS_VER_12        0x0303u /* TLS 1.2 (wire) */
#define TLS_VER_13        0x0304u /* TLS 1.3 */
/* Handshake types */
#define TLS_HT_CLIENT_HELLO   0x01u
#define TLS_HT_SERVER_HELLO   0x02u
#define TLS_HT_ENCRYPTED_EXT  0x08u
#define TLS_HT_CERTIFICATE    0x0Bu
#define TLS_HT_CERT_VERIFY    0x0Fu
#define TLS_HT_FINISHED       0x14u
#define TLS_ALERT_INTERNAL_ERROR 80u
/* Extension types */
#define EXT_SERVER_NAME       0x0000u
#define EXT_SUPPORTED_GROUPS  0x000Au
#define EXT_SIG_ALGS          0x000Du
#define EXT_SUPPORTED_VERS    0x002Bu
#define EXT_KEY_SHARE         0x0033u
#define EXT_SESSION_TICKET    0x0023u
/* Cipher suites */
#define CS_AES128_GCM_SHA256  0x1301u
#define CS_AES256_GCM_SHA384  0x1302u
#define CS_CHACHA20_SHA256    0x1303u

/* Contexto TLS */
typedef struct PK {
    u8  state;          /* estado da máquina de Turing TLS             */
    u8  flags;          /* flip-flops de status                        */
    u8  alert;          /* último alert recebido                       */
    u8  _p;
    u32 rx_seq;         /* sequence number RX                          */
    u32 tx_seq;         /* sequence number TX                          */
    u8  random[32];     /* client random from fail-closed getrandom    */
    u8  session[32];    /* session ID (zeros para TLS 1.3)             */
    u16 cipher;         /* cipher suite negociado                      */
    u16 _p2;
} TLSCtx;
static TLSCtx _TLS;

/* Inicialização de contexto sem fallback pseudo-aleatório.
 * A ausência de entropia do kernel bloqueia a criação do ClientHello. */
static s32 TLS_INIT(TLSCtx*t){
    MC0(t,sizeof(*t));
    t->state=TLS_IDLE;
    if(BR_RANDOM_FILL(t->random,(u32)sizeof(t->random))!=0){
        t->alert=TLS_ALERT_INTERNAL_ERROR;
        return-1;
    }
    return 0;
}

/* ── CONSTRUTOR DE ClientHello ──────────────────────────────────────────── */
/* Gera ClientHello TLS 1.3 em buf[], retorna tamanho total do record
 * Estrutura (sem abstração, byte a byte):
 * [0x16][0x03][0x01][len_hi][len_lo]     ← TLS record header
 * [0x01][hs_hi][hs_mid][hs_lo]           ← Handshake header
 * [0x03][0x03]                           ← legacy_version
 * [32 bytes random]
 * [0x00]                                 ← session_id_length = 0
 * [0x00][0x06]                           ← cipher_suites_length = 6
 * [0x13][0x01][0x13][0x02][0x13][0x03]  ← 3 cipher suites TLS 1.3
 * [0x01][0x00]                           ← compression = null
 * extensions...                          */
static u32 TLS_BUILD_CLIENT_HELLO(TLSCtx*t,const char*host,u8*buf,u32 cap){
    /* Wire serialization ONLY. HTTPS/TLS authentication remains disabled. */
    if(!t||!host||!buf)return 0u;
    u32 hlen=0u;
    while(hlen<254u&&host[hlen])hlen++;
    if(!hlen||hlen>253u||cap<92u+hlen)return 0u;
    for(u32 i=0u;i<hlen;i++){
        u8 c=(u8)host[i];
        if(c<=0x20u||c>=0x7Fu)return 0u;
    }
    /* Usamos ponteiro p que avança — sem nomes de variável adicionais */
    u8*p=buf+5;   /* reserva 5 bytes para o record header */
    u8*hs=p;      /* handshake header começa aqui */
    *p++=0x01;    /* HandshakeType: client_hello */
    *p++=0;*p++=0;*p++=0; /* length placeholder (3 bytes) */
    /* legacy_version = TLS 1.2 (0x0303) */
    *p++=0x03;*p++=0x03;
    /* client_random: 32 bytes */
    MC(p,t->random,32u); p+=32;
    /* session_id: 0 bytes para TLS 1.3 */
    *p++=0x00;
    /* cipher_suites: 3 suites × 2 bytes + 2 bytes length = 8 bytes */
    *p++=0x00;*p++=0x06;
    *p++=0x13;*p++=0x01; /* TLS_AES_128_GCM_SHA256 */
    *p++=0x13;*p++=0x02; /* TLS_AES_256_GCM_SHA384 */
    *p++=0x13;*p++=0x03; /* TLS_CHACHA20_POLY1305_SHA256 */
    /* compression_methods: null only */
    *p++=0x01;*p++=0x00;
    /* extensions: calculamos comprimento depois */
    u8*ext_len_ptr=p; *p++=0;*p++=0;
    u8*ext_start=p;
    /* EXT: supported_versions (0x002B) — anuncia TLS 1.3 */
    *p++=0x00;*p++=0x2B; /* type */
    *p++=0x00;*p++=0x03; /* ext length = 3 */
    *p++=0x02;           /* versions list length = 2 */
    *p++=0x03;*p++=0x04; /* TLS 1.3 */
    /* EXT: server_name (0x0000) — SNI */
    *p++=0x00;*p++=0x00; /* type */
    u32 sni_ext_len=hlen+5u;
    *p++=(u8)(sni_ext_len>>8u);*p++=(u8)(sni_ext_len);
    u32 sni_list_len=hlen+3u;
    *p++=(u8)(sni_list_len>>8u);*p++=(u8)(sni_list_len);
    *p++=0x00; /* name_type: host_name */
    *p++=(u8)(hlen>>8u);*p++=(u8)(hlen);
    MC(p,(const u8*)host,hlen);p+=hlen;
    /* EXT: supported_groups (0x000A) — x25519 */
    *p++=0x00;*p++=0x0A;
    *p++=0x00;*p++=0x04; /* ext length */
    *p++=0x00;*p++=0x02; /* list length */
    *p++=0x00;*p++=0x1D; /* x25519 */
    /* EXT: signature_algorithms (0x000D) */
    *p++=0x00;*p++=0x0D;
    *p++=0x00;*p++=0x08;
    *p++=0x00;*p++=0x06;
    *p++=0x04;*p++=0x03; /* ecdsa_secp256r1_sha256 */
    *p++=0x08;*p++=0x07; /* ed25519 */
    *p++=0x04;*p++=0x01; /* rsa_pkcs1_sha256 */
    /* Preenche comprimentos */
    u32 ext_total=(u32)(p-ext_start);
    ext_len_ptr[0]=(u8)(ext_total>>8u);ext_len_ptr[1]=(u8)(ext_total);
    u32 hs_body=(u32)(p-hs-4u);
    hs[1]=(u8)(hs_body>>16u);hs[2]=(u8)(hs_body>>8u);hs[3]=(u8)(hs_body);
    u32 rec_body=(u32)(p-buf-5u);
    buf[0]=0x16;buf[1]=0x03;buf[2]=0x01;
    buf[3]=(u8)(rec_body>>8u);buf[4]=(u8)(rec_body);
    return(u32)(p-buf);
}

/* ── PARSE DE RECORD TLS (resposta do servidor) ─────────────────────────── */
typedef struct PK { u8 type; u16 version; u16 length; } TLSRec;
AI s32 TLS_PARSE_RECORD(const u8*buf,u32 n,TLSRec*r){
    if(n<5u)return-1;
    r->type=buf[0];
    r->version=(u16)(((u16)buf[1]<<8u)|buf[2]);
    r->length=(u16)(((u16)buf[3]<<8u)|buf[4]);
    return 0;
}

/* Máquina de Turing TLS: transição de estado
 * Entrada: estado atual + tipo de mensagem recebida
 * Saída: novo estado (via flip-flop geométrico)
 * BRANCHLESS: usa máscaras e XOR em vez de if/else */

/* ── MÓDULO PRÉ-CODED PARA UPGRADE TLS 1.3 ───────────────────────────── */
#define TLS_UP_X25519        0x01u
#define TLS_UP_HKDF          0x02u
#define TLS_UP_TRANSCRIPT    0x04u
#define TLS_UP_AEAD          0x08u
#define TLS_UP_FINISHED      0x10u
#define TLS_UP_RECORD_CRYPT  0x20u
#define TLS_UP_ALL (TLS_UP_X25519|TLS_UP_HKDF|TLS_UP_TRANSCRIPT|TLS_UP_AEAD|TLS_UP_FINISHED|TLS_UP_RECORD_CRYPT)

AI u8 TLS_UPGRADE_MASK(void){
    /* Máscara de roadmap para implementação futura sem fallback inseguro */
    return (u8)TLS_UP_ALL;
}

AI u8 TLS_TRANSITION(u8 cur,u8 msg_type){
    /* Tabela de transição codificada em bits:
     * IDLE     + CH_SENT  → CLI_HELLO (bit0 flip)
     * CLI_HELLO + SH_RECV → SRV_HELLO (bit1 flip)
     * SRV_HELLO + ENC     → ENCRYPTED (bit2 flip)
     * ENCRYPTED + APP     → APP_DATA  */
    u32 is_idle =(u32)(cur==TLS_IDLE      );
    u32 is_cli  =(u32)(cur==TLS_CLI_HELLO );
    u32 is_srv  =(u32)(cur==TLS_SRV_HELLO );
    u32 is_enc  =(u32)(cur==TLS_ENCRYPTED );
    /* Branchless: máscara negativa */
    u8 n0=(u8)(TLS_CLI_HELLO & -(is_idle &(msg_type==TLS_HT_CLIENT_HELLO)));
    u8 n1=(u8)(TLS_SRV_HELLO & -(is_cli  &(msg_type==TLS_HT_SERVER_HELLO)));
    u8 n2=(u8)(TLS_ENCRYPTED & -(is_srv  &(msg_type==TLS_HT_FINISHED    )));
    u8 n3=(u8)(TLS_APP_DATA  & -(is_enc  &(msg_type==TLS_RT_APP_DATA    )));
    u8 nxt=n0|n1|n2|n3;
    /* Se nenhuma transição aconteceu E havia um estado: mantém */
    u32 no_trans=(u32)(nxt==0u)&(u32)(cur!=TLS_IDLE);
    return(u8)(nxt|(cur&-(no_trans)));
}
