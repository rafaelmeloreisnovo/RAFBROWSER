/* br_main.c — Browser entry point
 * Fluxo: URL → DNS → TCP → (TLS) → HTTP → HTML → RENDER
 * [R35] Rollback: GM/GRS de arena em caso de erro
 * [R36] Failsafe: TTL 3 tentativas por fase
 * [R37] Flags lineares: abertos em sequência (FL_DNS→FL_CONNECT→...)
 * [R38] Inline ASM para seções críticas de timing
 */
#include "br_types.h"
#include "br_sys.h"
#include "br_tls.h"
#include "br_http.h"
#include "br_html.h"
#include "br_dns.h"
#include "br_links.h"
#include "br_license.h"
/* Explicit per-process endpoint override; no external resolver is consulted. */
static u8 _BR_FORCED_IP[4];
static u8 _BR_FORCED_IP_SET=0u;
#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif


/* ARMv7 has no native 64-bit unsigned divide instruction; using the usual
 * C '/' on u64 would import __aeabi_uldivmod from a runtime library.
 * This bounded, fixed-iteration unsigned long division requires no CRT.
 * d is nonzero at all callers and checked defensively. */
static u64 BR_DIV_U64_U32(u64 n,u32 d){
    if(d==0u)return 0u;
    u64 q=0u,r=0u;
    for(u32 i=64u;i!=0u;){
        --i;
        r=(r<<1u)|((n>>i)&1u);
        u64 mask=(u64)0-(u64)(r>=(u64)d);
        r-=((u64)d)&mask;
        q|=(mask&1u)<<i;
    }
    return q;
}

/* ── UI DE STATUS ──────────────────────────────────────────────────────── */
static void STATUS(u8 flags,const char*msg){
    PS("\033[1;36m[");
    if(FF_GET(flags,FL_DNS))   PS("DNS ");
    if(FF_GET(flags,FL_CONNECT))PS("TCP ");
    if(FF_GET(flags,FL_TLS_HS))PS("TLS ");
    if(FF_GET(flags,FL_HTTP_TX))PS("TX ");
    if(FF_GET(flags,FL_HTTP_RX))PS("RX ");
    if(FF_GET(flags,FL_HTML_RND))PS("HTML ");
    if(FF_GET(flags,FL_ERROR)) PS("ERR ");
    if(FF_GET(flags,FL_DONE))  PS("DONE ");
    PS("]\033[0m ");PS(msg);PS("\n");
}

static void HEADER_LINE(void){
    PS("\033[1;34m");
    PS("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    PS("\033[0m");
}


static s32 SEND_ALL(s32 fd,const void*buf,u32 n){
    const u8*p=(const u8*)buf;
    u32 off=0;
    while(off<n){
        s32 w=SEND(fd,p+off,n-off);
        if(w<=0)return-1;
        off+=(u32)w;
    }
    return(s32)off;
}


/* ── FETCH HTTP ─────────────────────────────────────────────────────────── */
static s32 DO_FETCH_R(BCtx*ctx,u32 redirects){
    ctx->flags=FL_IDLE;
    ctx->fd=-1; /* no owned socket until SOCKET succeeds; never close fd 0 */
    _BR_LINK_COUNT=0u;
    u64 t0=NS();
    s32 rc=-1;

    /* HTTPS must be refused BEFORE any DNS query, socket or TCP connection.
     * ClientHello serialization alone is not a validated TLS stack. */
    if(ctx->use_tls){
        ctx->tls=TLS_UPGRADE_MASK(); /* roadmap bits, never a capability */
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"HTTPS não suportado neste build");
        goto done;
    }

    /* ── FASE 1: DNS resolve ─────────────────────────────────────────── */
    if(!BR_PHASE_NEXT(&ctx->flags,0u,FL_DNS,0u)){
        STATUS(ctx->flags,"Estado invalido: entrada DNS");
        goto done;
    }
    STATUS(ctx->flags,"Resolvendo DNS...");
    u8 ttl=3;
    s32 dns_ok=-1;
    if(redirects>0u){
        /* Same-origin redirects retain the exact address used at hop zero.
         * Re-resolving would allow a DNS rebinding between HTTP requests. */
        dns_ok=0;
        PS("  [ROUTE] IP do primeiro salto preservado\n");
    }else if(_BR_FORCED_IP_SET){
        MC(ctx->ip,_BR_FORCED_IP,4u);
        dns_ok=0;
        PS("  [ROUTE] IP explicito; nenhuma consulta DNS\n");
    }else{
        while(ttl--){
            dns_ok=DNS_RESOLVE(ctx->host,ctx->ip);
            if(dns_ok==0)break;
            PS("  [RETRY DNS]\n");
        }
        /* Numeric IP URLs are explicit destinations (including local labs).
         * Hostname DNS must not silently select a private/reserved address. */
        u8 literal[4];
        if(dns_ok==0&&PARSE_IP(ctx->host,literal)!=0&&
           !BR_IPV4_GLOBAL_CANDIDATE(ctx->ip)){
            dns_ok=-1;
            STATUS(ctx->flags,"DNS retornou endereco especial/bloqueado");
        }
    }
    if(dns_ok!=0){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"DNS falhou ou rota bloqueada");
        goto done;
    }
    PS("  IP: ");PN(ctx->ip[0]);
    /* Exibe IP */
    {char ipstr[20];u32 i=0;
     for(u32 o=0;o<4u;o++){
         if(o)ipstr[i++]='.';
         UTOA(ctx->ip[o],ipstr+i);
         i+=SL(ipstr+i);
     }
     ipstr[i]=0;PS("  ");PS(ipstr);PS("\n");}
    if(!BR_PHASE_NEXT(&ctx->flags,FL_DNS,FL_CONNECT,FL_DNS)){
        STATUS(ctx->flags,"Estado invalido: DNS->CONNECT");
        goto done;
    }

    /* ── FASE 2: TCP connect ─────────────────────────────────────────── */
    STATUS(ctx->flags,"Conectando TCP...");
    GM(); /* checkpoint arena antes de alocar recursos de rede */

    SA4 sa;MC0(&sa,sizeof(sa));
    sa.fam=(u16)AF_INET;
    sa.port_be=HTON16((u16)ctx->port);
    MC(sa.ip,ctx->ip,4u);

    s32 conn_ok=-1;
    ttl=3;
    while(ttl--){
        ctx->fd=SOCKET();
        if(ctx->fd<0){
            PS("  [RETRY CONNECT socket fail]\n");
            continue;
        }
        if(SET_RECV_TIMEOUT(ctx->fd,(usize)3u)!=0||
           SET_SEND_TIMEOUT(ctx->fd,(usize)3u)!=0){
            FF_SET(ctx->flags,FL_ERROR);
            STATUS(ctx->flags,"TCP falhou (SO_RCVTIMEO/SO_SNDTIMEO)");
            goto done;
        }
        if(CONNECT(ctx->fd,&sa)==0){
            conn_ok=0;
            break;
        }

        CLOSE(ctx->fd);
        ctx->fd=-1;
        PS("  [RETRY CONNECT]\n");
    }
    if(conn_ok!=0){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"TCP falhou");
        PS("  Falha TCP\n");
        goto done;
    }

    /* HTTPS rejection happens at function entry before any network activity. */

    /* ── FASE 4: HTTP request ────────────────────────────────────────── */
    /* DNS -> CONNECT happened, but HTTP_TX must be entered before
     * BR_PHASE_NEXT(HTTP_TX -> HTTP_RX). Preserve strict phase gating. */
    if(!BR_PHASE_NEXT(&ctx->flags,FL_CONNECT,FL_HTTP_TX,FL_CONNECT)){
        STATUS(ctx->flags,"Estado invalido: CONNECT->HTTP_TX");
        goto done;
    }
    STATUS(ctx->flags,"Enviando request HTTP...");
    u32 reqlen=HTTP_BUILD_REQ_PORT(ctx->host,ctx->path,ctx->port,_NB,NET_BUF);
    if(!reqlen){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"HTTP request rejeitada: comprimento ou entrada invalida");
        goto done;
    }
    PS("  Request (");PN(reqlen);PS("B):\n");
    /* Do not echo HTTP path/query/headers to stderr: potential PII. */
    s32 sent=SEND_ALL(ctx->fd,_NB,reqlen);
    if(sent<0||(u32)sent!=reqlen){
        FF_SET(ctx->flags,FL_ERROR);
        goto done;
    }
    ctx->tx_bytes+=(u32)sent;
    if(!BR_PHASE_NEXT(&ctx->flags,FL_HTTP_TX,FL_HTTP_RX,FL_HTTP_TX)){
        STATUS(ctx->flags,"Estado invalido: HTTP_TX->HTTP_RX");
        goto done;
    }

    /* ── FASE 5: HTTP response ───────────────────────────────────────── */
    STATUS(ctx->flags,"Recebendo response...");
    u32 total=0;
    /* Acumula response em _NB */
    {
        u32 max=NET_BUF-1u;
        u8 eof=0u,io_error=0u,framed_complete=0u;
        while(total<max){
            s32 r=RECV(ctx->fd,(void*)(_NB+total),max-total);
            if(r<0){io_error=1u;break;}
            if(r==0){eof=1u;break;}
            total+=(u32)r;
            /* HTTP/1.1: a valid Content-Length defines the body boundary.
             * A server may keep the connection open despite our request to
             * close it. Do not hang or report truncation after exact body. */
            u32 header_off=HTTP_HEADERS_END(_NB,total);
            if(header_off){
                s32 te=HTTP_TRANSFER_ENCODING(_NB,header_off);
                u32 expected=0u;
                s32 cl=HTTP_CONTENT_LENGTH(_NB,header_off,&expected);
                if(te<0||cl<0||(te==1&&cl==1)){
                    io_error=1u;break;
                }
                if(te==0&&cl==1){
                    if(header_off>max||expected>max-header_off||
                       total-header_off>expected){
                        io_error=1u;break;
                    }
                    if(total-header_off==expected){
                        framed_complete=1u;break;
                    }
                }
            }
        }
        _NB[total]=0;
        /* For close-delimited or chunked body retain conservative EOF rule;
         * Content-Length responses may finish at exact advertised bytes. */
        if(io_error||(!eof&&!framed_complete)||total==0u){
            FF_SET(ctx->flags,FL_ERROR);
            STATUS(ctx->flags,"HTTP response incompleta, framing invalido ou erro de I/O");
            goto done;
        }
    }
    ctx->rx_bytes=total;

    CLOSE(ctx->fd);
    ctx->fd=-1;

    /* Parse status */
    ctx->status=HTTP_PARSE_STATUS(_NB,total);
    if(ctx->status==0){
        FF_SET(ctx->flags,FL_ERROR);
        PS("  Status HTTP invalido: ");PN(ctx->status);
        goto done;
    }
    PS("  Status HTTP: ");PN(ctx->status);
    /* Same-origin, path-absolute redirects only; at most four hops. Unknown
     * schemes, cross-origin, scheme-relative and relative-location redirects
     * fail closed until their separate origin policy is implemented. */
    if(ctx->status==301u||ctx->status==302u||ctx->status==303u||
       ctx->status==307u||ctx->status==308u){
        u32 hend=HTTP_HEADERS_END(_NB,total);
        if(redirects>=4u||!hend||HTTP_TRANSFER_ENCODING(_NB,hend)<0){
            FF_SET(ctx->flags,FL_ERROR);
            STATUS(ctx->flags,"Redirecionamento inseguro, malformado ou excessivo");
            goto done;
        }
        u32 cl_tmp=0u;
        if(HTTP_CONTENT_LENGTH(_NB,total,&cl_tmp)<0)goto done;
        const u8*loc=HTTP_FIND_HEADER(_NB,total,"Location");
        if(!loc){
            FF_SET(ctx->flags,FL_ERROR);
            STATUS(ctx->flags,"Redirecionamento sem Location");
            goto done;
        }
        u32 off=(u32)(loc-_NB),len=0u;
        /* Only a single origin-relative absolute path. Reject //host. */
        if(off>=hend||loc[0]!='/'||loc[1]=='/')goto done;
        while(off+len<hend&&loc[len]!='\r'&&loc[len]!='#'){
            u8 c=loc[len];
            if(c<0x21u||c>=0x7Fu||c=='\\'||len>=511u)goto done;
            len++;
        }
        if(len==0u||off+len>=hend)goto done;
        ctx->path[0]=0u;
        MC(ctx->path,loc,len);
        ctx->path[len]=0;
        PS("  [REDIRECT] mesmo host, caminho: ");PS(ctx->path);PS("\n");
        rc=DO_FETCH_R(ctx,redirects+1u);
        goto done;
    }
    if(ctx->status<200u||ctx->status>=300u){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"HTTP status não navegável");
        goto done;
    }

    /* Content-Length: distinguish absent, malformed and fully parsed. */
    s32 cl_state=HTTP_CONTENT_LENGTH(_NB,total,&ctx->content_len);
    if(cl_state<0){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"Content-Length invalido");
        goto done;
    }
    PS("  Content-Length: ");PN(ctx->content_len);

    /* ── FASE 6: Render HTML ─────────────────────────────────────────── */
    if(!BR_PHASE_NEXT(&ctx->flags,FL_HTTP_RX,FL_HTML_RND,FL_HTTP_RX)){
        STATUS(ctx->flags,"Estado invalido: HTTP_RX->HTML_RND");
        goto done;
    }
    u32 body_off=HTTP_HEADERS_END(_NB,total);
    if(!body_off){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"HTTP headers incompletos");
        goto done;
    }
    u32 body_len=total-body_off;
    s32 mime=HTTP_CONTENT_TYPE(_NB,body_off);
    if(mime<0){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"Content-Type binário ou não suportado");
        goto done;
    }
    s32 te_state=HTTP_TRANSFER_ENCODING(_NB,body_off);
    if(te_state<0){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"Transfer-Encoding ou headers inválidos");
        goto done;
    }
    u8 is_chunked=(u8)(te_state==1);
    /* Concurrent chunked framing and Content-Length is ambiguous:
     * do not guess which one controls the response body. */
    if(is_chunked&&cl_state==1){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"Framing HTTP ambiguo");
        goto done;
    }
    if(is_chunked){
        u32 dec=HTTP_DECHUNK(_NB+body_off,body_len);
        if(dec==0xFFFFFFFFu){
            FF_SET(ctx->flags,FL_ERROR);
            PS("  Chunked decode falhou\n");
            goto done;
        }
        body_len=dec;
    }else if(cl_state==1&&body_len!=ctx->content_len){
        FF_SET(ctx->flags,FL_ERROR);
        STATUS(ctx->flags,"Content-Length divergente do body recebido");
        goto done;
    }
    STATUS(ctx->flags,"Renderizando HTML...");
    PS("  Body: ");PN(body_len);PS("B\n");

    u32 rlen=0u,link_count=0u;
    if(mime==2){
        rlen=TEXT_RENDER(_NB+body_off,body_len,_RB,NET_BUF);
    }else{
        rlen=HTML_RENDER(_NB+body_off,body_len,_RB,NET_BUF);
        link_count=BR_LINK_SCAN(_NB+body_off,body_len);
    }
    if(!BR_PHASE_NEXT(&ctx->flags,FL_HTML_RND,FL_DONE,FL_HTML_RND)){
        STATUS(ctx->flags,"Estado invalido: HTML_RND->DONE");
        goto done;
    }

    /* ── OUTPUT RENDERIZADO ───────────────────────────────────────────── */
    HEADER_LINE();
    PS("\033[1;32m");PS(ctx->host);PS(ctx->path);PS("\033[0m\n");
    HEADER_LINE();
    WR(1,_RB,rlen);
    if(link_count){
        PS("\nLinks disponíveis (use --interactive para navegar):\n");
        for(u32 k=0u;k<link_count;k++){
            char number[12];UTOA(k+1u,number);
            PS("  [");PS(number);PS("] ");PS(_BR_LINKS[k].href);PS("\n");
        }
    }
    HEADER_LINE();

    rc=0;
done:
    if(ctx->fd>=0){
        CLOSE(ctx->fd);
        ctx->fd=-1;
    }
    FF_CLR(ctx->flags,FL_DNS|FL_CONNECT|FL_TLS_HS|FL_HTTP_TX|FL_HTTP_RX|FL_HTML_RND);
    if(rc==0){
        FF_CLR(ctx->flags,FL_ERROR);
    }else{
        FF_CLR(ctx->flags,FL_DONE);
        FF_SET(ctx->flags,FL_ERROR);
        GRS();
    }
    ctx->t_ns=NS()-t0;
    return rc;
}

static s32 DO_FETCH(BCtx*ctx){return DO_FETCH_R(ctx,0u);}

/* ── ENTRY POINT ─────────────────────────────────────────────────────────── */
/* URLs de teste — passados via argvX no _start como posição fixa da stack */
/* Em Termux sem argc/argv: URL hardcoded ou lido de /proc/self/cmdline     */
static const char DEFAULT_URL[]="http://example.com/";

void browser_main(void){
    GR(); /* reset arena */

    /* ASCII art logo */
    PS("\033[1;36m");
    PS("╔══════════════════════════════════════════════════════════════╗\n");
    PS("║  \033[1;33m██████╗ ██████╗  ██████╗ ██╗    ██╗███████╗███████╗██████╗\033[1;36m  ║\n");
    PS("║  \033[1;32m RAFAELIA BROWSER · HTTP/1.1 · HTTPS FAIL-CLOSED         \033[1;36m  ║\n");
    PS("║  \033[0;37m ARM32/ARM64/x86-64 · nolibc · nomalloc · inline ASM    \033[1;36m  ║\n");
    PS("║  \033[0;35m Turing Geométrica · Flip-Flop · Branchless · F*=23.158 \033[1;36m  ║\n");
    PS("╚══════════════════════════════════════════════════════════════╝\n");
    PS("\033[0m\n");

    /* Tenta ler URL de /proc/self/cmdline (argumento 1) */
    const char*url=DEFAULT_URL;
    /* Lê cmdline para obter argumento */
    static u8 _CMD[512];
    MC0(_CMD,512u);
#if defined(__arm__)
    s32 cfd=(s32)_sc3(5u,(u32)(usize)"/proc/self/cmdline",0,0);
    if(cfd>=0){_sc3(3u,(u32)cfd,(u32)(usize)_CMD,511u);_sc1(6u,(u32)cfd);}
#elif defined(__aarch64__)
    s32 cfd=(s32)_sc3(56u,(u64)(usize)AT_FDCWD,(u64)(usize)"/proc/self/cmdline",0u);
    if(cfd>=0){_sc3(63u,(u64)cfd,(u64)(usize)_CMD,511u);_sc1(57u,(u64)cfd);}
#elif defined(__x86_64__)
    s32 cfd=(s32)_sc3(2u,(u64)(usize)"/proc/self/cmdline",0,0);
    if(cfd>=0){_sc3(0u,(u64)cfd,(u64)(usize)_CMD,511u);_sc1(3u,(u64)cfd);}
#endif
    /* Bounded cmdline: reject unknown, duplicate and truncated options.
     * --connect-ip=IP pins an endpoint without contacting DNS. It does NOT
     * authenticate HTTP; HTTPS remains disabled before any network I/O. */
    u8 interactive=0u,bad_option=0u;
    {u32 ci=0u;
     while(ci<511u&&_CMD[ci])ci++;
     if(ci>=511u)bad_option=1u;
     else if(++ci<511u&&_CMD[ci]){
         u32 first_end=ci;while(first_end<511u&&_CMD[first_end])first_end++;
         if(first_end-ci==9u&&BR_MEMCMP(_CMD+ci,"--license",9u)==0){
             if(BR_LICENSE_SHOW()!=0)PS("[ERRO] nao foi possivel ler LICENSE.md\n");
             EX();
         }
         url=(const char*)(_CMD+ci);
         while(ci<511u&&_CMD[ci])ci++;
         if(ci>=511u)bad_option=1u;
         else{
             ci++;
             while(ci<511u&&_CMD[ci]){
                 const char*arg=(const char*)(_CMD+ci);
                 u32 al=0u;
                 while(ci+al<511u&&_CMD[ci+al])al++;
                 if(ci+al>=511u){bad_option=1u;break;}
                 if(al==13u&&BR_MEMCMP(arg,"--interactive",13u)==0){
                     if(interactive)bad_option=1u;
                     interactive=1u;
                 }else if(al>13u&&BR_MEMCMP(arg,"--connect-ip=",13u)==0){
                     if(_BR_FORCED_IP_SET||PARSE_IP(arg+13u,_BR_FORCED_IP)!=0)
                         bad_option=1u;
                     else _BR_FORCED_IP_SET=1u;
                 }else bad_option=1u;
                 ci+=al+1u;
             }
         }
     }}
    if(bad_option||(_BR_FORCED_IP_SET&&interactive)){
        PS("[ERRO] opcao invalida ou --connect-ip com --interactive\n");
        EX();
    }

    PS("\033[1;37mURL: \033[0;36m");PS(url);PS("\033[0m\n\n");

    /* Inicializa contexto */
    MC0(&_BC,sizeof(_BC));
    if(URL_PARSE(url,&_BC)!=0){
        PS("\033[1;31m[ERRO] URL inválida\033[0m\n");
        EX();
    }
    if(_BR_FORCED_IP_SET){
        u8 numeric_host[4];
        if(_BC.use_tls||PARSE_IP(_BC.host,numeric_host)==0||
           !BR_OVERRIDE_IP_ALLOWED(_BC.host,_BR_FORCED_IP)){
            PS("[ERRO] rota direta recusada (host/IP/TLS)\n");
            EX();
        }
        PS("[AVISO] HTTP direto por IP: DNS ignorado, conteudo nao autenticado\n");
    }

    PS("Host: ");PS(_BC.host);PS("\n");
    PS("Port: ");PN(_BC.port);
    PS("Path: ");PS(_BC.path);PS("\n");
    PS("TLS:  ");PS(_BC.use_tls?"SOLICITADO (FAIL-CLOSED)":"NAO");PS("\n\n");

    /* Executa fetch */
    s32 res=DO_FETCH(&_BC);

    /* Relatório final */
    HEADER_LINE();
    PS("\033[1;37mRELATÓRIO:\033[0m\n");
    PS("  Status:   ");PN(_BC.status);
    PS("  TX bytes: ");PN(_BC.tx_bytes);
    PS("  RX bytes: ");PN(_BC.rx_bytes);
    PS("  Latência: ");PN(BR_DIV_U64_U32(_BC.t_ns,1000000u));PS("ms\n");
    PS("  Flags:    ");PH(_BC.flags);
    PS("  TLS roadmap: ");PH(_BC.tls);
    HEADER_LINE();

    if(res==0){PS("\033[1;32m[OK] Fetch completo\033[0m\n");}
    else{
        PS("\033[1;31m[ERRO] Fetch falhou\033[0m\n");
        PS("  Métricas disponíveis:\n");
        PS("    status=");PN(_BC.status);
        PS("    tx=");PN(_BC.tx_bytes);
        PS("    rx=");PN(_BC.rx_bytes);
        PS("    t_ms=");PN(BR_DIV_U64_U32(_BC.t_ns,1000000u));PS("\n");
        PS("    flags finais=");PH(_BC.flags);
    }

    if(interactive){
        /* Opt-in, bounded, terminal-only navigation; no implicit remote
         * navigation or credential submission. "b" re-fetches prior page. */
        static BCtx history[8];
        u32 history_count=0u;
        for(u32 turn=0u;turn<128u;turn++){
            PS("\nBrowserRaf [1..24 link | b voltar | q sair] > ");
            char input[32];MC0(input,sizeof(input));
            s32 bytes=BR_READ(0u,input,sizeof(input)-1u);
            if(bytes<=0)break;
            if(input[0]=='q'||input[0]=='Q')break;
            if(input[0]=='b'||input[0]=='B'){
                if(history_count){
                    history_count--;
                    MC(&_BC,&history[history_count],sizeof(_BC));
                    (void)DO_FETCH(&_BC);
                }else PS("Histórico vazio\n");
                continue;
            }
            u32 chosen=0u,good=0u;
            for(s32 k=0;k<bytes;k++){
                u8 c=(u8)input[k];
                if(c=='\n'||c=='\r')break;
                if(c<'0'||c>'9'){good=0u;break;}
                chosen=chosen*10u+(u32)(c-'0');
                if(chosen>BR_MAX_LINKS){good=0u;break;}
                good=1u;
            }
            if(!good||chosen==0u||chosen>_BR_LINK_COUNT){
                PS("Seleção inválida ou link indisponível\n");continue;
            }
            BCtx next;
            MC(&next,&_BC,sizeof(_BC));
            if(BR_LINK_FOLLOW(&next,chosen)!=0){
                PS("Link inválido ou protocolo não suportado\n");continue;
            }
            if(history_count==8u){
                for(u32 k=1u;k<8u;k++)MC(&history[k-1u],&history[k],sizeof(BCtx));
                history_count=7u;
            }
            MC(&history[history_count++],&_BC,sizeof(_BC));
            MC(&_BC,&next,sizeof(_BC));
            (void)DO_FETCH(&_BC);
        }
    }

    EX();
}
