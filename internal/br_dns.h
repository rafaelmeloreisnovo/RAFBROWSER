#pragma once
/* br_dns.h — URL parser + DNS resolve mínimo
 * [R29] Parse de URL: scheme://host[:port]/path
 * [R30] DNS raw UDP: porta 53, query A record
 * [R31] Fallback: aceita IP numérico diretamente
 * [R32] Failsafe: TTL 3 tentativas com rollback de estado
 */
#include "br_http.h"

/* Parse de URL → preenche BCtx */
static s32 URL_PARSE(const char*url,BCtx*ctx){
    /* Explicit supported schemes; never silently reinterpret ftp://, spaces,
     * fragments or truncated hostnames as different network destinations. */
    if(!url||!ctx)return-1;
    u32 n=0u;
    while(n<768u&&url[n])n++;
    if(!n||n==768u)return-1;
    MC0(ctx->host,256u);MC0(ctx->path,512u);
    ctx->port=80u;ctx->use_tls=0u;
    const char*p=url;
    if(n>=8u&&BR_MEMCMP(p,"https://",8u)==0){
        ctx->use_tls=1u;ctx->port=443u;p+=8;
    }else if(n>=7u&&BR_MEMCMP(p,"http://",7u)==0){
        p+=7;
    }else if(n>=2u&&BR_MEMCMP(p,"//",2u)==0){
        p+=2;
    }else{
        /* Bare host is accepted; unknown schemes and authority delimiters are not. */
        for(u32 i=0u;i<n;i++){
            if(url[i]==':'||url[i]=='\\')return-1;
            if(i+2u<n&&url[i]=='/'&&url[i+1u]=='/')return-1;
            if(url[i]=='/'||url[i]=='?'||url[i]=='#')break;
        }
    }
    u32 hi=0u;
    while(*p&&*p!='/'&&*p!=':'&&*p!='?'&&*p!='#'){
        u8 c=(u8)*p;
        u8 valid=(u8)((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
            (c>='0'&&c<='9')||c=='-'||c=='.');
        if(!valid||hi>=253u)return-1;
        ctx->host[hi++]=(char)c;p++;
    }
    if(!hi||ctx->host[0]=='.'||ctx->host[hi-1u]=='.')return-1;
    for(u32 j=1u;j<hi;j++){
        if(ctx->host[j]=='.'&&ctx->host[j-1u]=='.')return-1;
    }
    ctx->host[hi]=0;
    if(*p==':'){
        u32 port=0u,digits=0u;
        p++;
        while(*p>='0'&&*p<='9'){
            u32 v=(u32)(*p-'0');
            if(digits>=5u||port>6553u||(port==6553u&&v>5u))return-1;
            port=port*10u+v;digits++;p++;
        }
        if(!digits||!port||(*p&&*p!='/'&&*p!='?'&&*p!='#'))return-1;
        ctx->port=port;
    }
    u32 pi=0u;
    if(*p!='/')ctx->path[pi++]='/';
    while(*p&&*p!='#'){
        u8 c=(u8)*p++;
        if(c<0x21u||c>=0x7Fu||c=='\\'||pi>=511u)return-1;
        ctx->path[pi++]=(char)c;
    }
    if(!pi)ctx->path[pi++]='/';
    ctx->path[pi]=0;
    return 0;
}

/* Plain DNS over UDP is NOT DNSSEC and leaks resolver queries. The current
 * fixed resolver 8.8.8.8 remains an explicit privacy/portability limitation.
 * This implementation still validates origin via connected UDP, random TXID,
 * echoed question, response flags, and bounded A-record decoding. */
#define SOCK_DGRAM 2
#define AF_INET 2


/* This is a conservative *routing* filter, not DNSSEC, TLS, a certificate
 * validator, or a guarantee that an address belongs to the intended host.
 * Known non-global IPv4 blocks must never be accepted from public DNS. */
static u8 BR_IPV4_GLOBAL_CANDIDATE(const u8 ip[4]){
    if(!ip)return 0u;
    u8 a=ip[0],b=ip[1],c=ip[2];
    if(a==0u||a==10u||a==127u||a>=224u)return 0u;
    if(a==100u&&b>=64u&&b<=127u)return 0u; /* shared CGN */
    if(a==169u&&b==254u)return 0u; /* link local */
    if(a==172u&&b>=16u&&b<=31u)return 0u;
    if(a==192u&&(b==168u||b==0u||(b==88u&&c==99u)))return 0u;
    if(a==192u&&b==0u&&c==2u)return 0u;
    if(a==198u&&(b==18u||b==19u||(b==51u&&c==100u)))return 0u;
    if(a==203u&&b==0u&&c==113u)return 0u;
    return 1u;
}
/* Explicit local testing remains possible ONLY for the exact localhost
 * hostname. Other hostnames cannot be rebound to loopback/private targets
 * via --connect-ip. Numeric URL hosts already bypass DNS without this flag. */
static u8 BR_OVERRIDE_IP_ALLOWED(const char*host,const u8 ip[4]){
    if(!host||!ip)return 0u;
    if(BR_IPV4_GLOBAL_CANDIDATE(ip))return 1u;
    return (u8)(BR_MEMCMP(host,"localhost",10u)==0&&host[9]==0&&
                ip[0]==127u&&ip[1]==0u&&ip[2]==0u&&ip[3]==1u);
}

static s32 DNS_PARSE_A_REPLY(const u8*rsp,u32 n,const u8*query,u32 qlen,u8 ip[4]){
    if(!rsp||!query||!ip||qlen<17u||qlen>512u||n<qlen+12u)return-1;
    if(rsp[0]!=query[0]||rsp[1]!=query[1])return-1;
    u16 flags=(u16)(((u16)rsp[2]<<8u)|rsp[3]);
    u16 questions=(u16)(((u16)rsp[4]<<8u)|rsp[5]);
    u16 answers=(u16)(((u16)rsp[6]<<8u)|rsp[7]);
    if((flags&0x8000u)==0u||(flags&0x7800u)!=0u||
       (flags&0x0200u)!=0u||(flags&0x000Fu)!=0u||
       questions!=1u||answers==0u)return-1;
    /* Reject rewritten, substituted or compressed questions. */
    if(BR_MEMCMP(rsp+12u,query+12u,qlen-12u)!=0)return-1;
    u32 pos=qlen;
    for(u32 an=0u;an<(u32)answers;an++){
        if(pos>=n)return-1;
        if((rsp[pos]&0xC0u)==0xC0u){
            /* Only the already checked original question at offset 12. */
            if(n-pos<2u||rsp[pos]!=0xC0u||rsp[pos+1u]!=0x0Cu)return-1;
            pos+=2u;
        }else{
            u32 qname_len=qlen-12u-4u;
            if(qname_len>n-pos||BR_MEMCMP(rsp+pos,query+12u,qname_len)!=0)return-1;
            pos+=qname_len;
        }
        if(n-pos<10u)return-1;
        u16 type=(u16)(((u16)rsp[pos]<<8u)|rsp[pos+1u]);
        u16 klass=(u16)(((u16)rsp[pos+2u]<<8u)|rsp[pos+3u]);
        u16 rdlen=(u16)(((u16)rsp[pos+8u]<<8u)|rsp[pos+9u]);
        pos+=10u;
        if(rdlen>n-pos)return-1;
        if(type==1u&&klass==1u&&rdlen==4u){
            MC(ip,rsp+pos,4u);
            return 0;
        }
        pos+=(u32)rdlen;
    }
    return-1;
}

static s32 DNS_RESOLVE(const char*host,u8 ip[4]){
    if(!host||!ip)return-1;
    if(PARSE_IP(host,ip)==0)return 0;
    /* Dotted-decimal-looking but invalid strings must not leak to DNS. */
    u32 only_numeric=1u,host_len=0u;
    while(host_len<254u&&host[host_len]){
        u8 c=(u8)host[host_len++];
        if(!((c>='0'&&c<='9')||c=='.'))only_numeric=0u;
    }
    if(host_len==0u||host_len>=254u||only_numeric)return-1;

    u8 query[512],rsp[512],entropy[2];
    MC0(query,sizeof(query));
    if(BR_RANDOM_FILL(entropy,2u)!=0)return-1;
    query[0]=entropy[0];query[1]=entropy[1];
    query[2]=1u; /* RD */
    query[5]=1u; /* exactly one question */

    u32 pos=12u;
    const char*h=host;
    while(*h){
        const char*end=h;
        while(*end&&*end!='.')end++;
        u32 len=(u32)(end-h);
        if(len==0u||len>63u||pos+len+6u>sizeof(query))return-1;
        if(h[0]=='-'||h[len-1u]=='-')return-1;
        query[pos++]=(u8)len;
        for(u32 j=0u;j<len;j++){
            u8 c=(u8)h[j];
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                 (c>='0'&&c<='9')||c=='-'))return-1;
            if(c>='A'&&c<='Z')c=(u8)(c+32u);
            query[pos++]=c;
        }
        h=end;
        if(*h=='.')h++;
        if(*end=='.'&&*h==0)return-1;
    }
    query[pos++]=0u;
    query[pos++]=0u;query[pos++]=1u; /* QTYPE A */
    query[pos++]=0u;query[pos++]=1u; /* QCLASS IN */

#if defined(__arm__)
    s32 fd=(s32)_sc3(281u,AF_INET,SOCK_DGRAM,0u);
#elif defined(__aarch64__)
    s32 fd=(s32)_sc3(198u,AF_INET,SOCK_DGRAM,0u);
#elif defined(__x86_64__)
    s32 fd=(s32)_sc3(41u,AF_INET,SOCK_DGRAM,0u);
#else
#error "DNS platform ABI missing"
#endif
    if(fd<0)return-1;
    s32 result=-1;
    SA4 sa;MC0(&sa,sizeof(sa));
    sa.fam=AF_INET;sa.port_be=HTON16(53u);
    sa.ip[0]=8u;sa.ip[1]=8u;sa.ip[2]=8u;sa.ip[3]=8u;
    /* UDP connect limits accepted traffic to this resolver/port. */
    if(CONNECT(fd,&sa)!=0)goto dns_done;
    if(SET_RECV_TIMEOUT(fd,1u)!=0)goto dns_done;
    s32 sent=SEND(fd,query,pos);
    if(sent!=(s32)pos)goto dns_done;
    s32 got=RECV(fd,rsp,sizeof(rsp));
    if(got>0)result=DNS_PARSE_A_REPLY(rsp,(u32)got,query,pos,ip);
dns_done:
    CLOSE(fd);
    return result;
}
