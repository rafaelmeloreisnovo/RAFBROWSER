#pragma once
/* br_http.h — HTTP/1.1 request builder + response parser
 * [R17] Sem sprintf: string assembly byte a byte
 * [R18] HTTP/1.1: Host + Connection:close + User-Agent
 * [R19] Response parser: status line + headers + body
 * [R20] Content-Length: parse de decimal para u32
 * [R21] Chunked transfer: parse de hex chunks
 * [R22] Branchless header matching via CRC32C parcial
 */
#include "br_tls.h"

/* ── BUILDER DE REQUEST HTTP ────────────────────────────────────────────── */
static u32 HTTP_BUILD_REQ_PORT(const char*host,const char*path,u32 port,u8*buf,u32 cap){
    /* Host names restricted to DNS/IPv4 ASCII. URL_PARSE rejects unsupported
     * IPv6 authorities, controls and userinfo. Local nondefault ports MUST be
     * reflected in Host for HTTP/1.1 virtual hosting. */
    if(!host||!path||!buf||port==0u||port>65535u)return 0u;
    u32 hlen=0u,plen=0u;
    while(hlen<254u&&host[hlen])hlen++;
    while(plen<512u&&path[plen])plen++;
    if(!hlen||hlen>253u||!plen||plen>=512u||path[0]!='/')return 0u;
    for(u32 i=0u;i<hlen;i++){
        u8 c=(u8)host[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
            (c>='0'&&c<='9')||c=='-'||c=='.'))return 0u;
    }
    for(u32 i=0u;i<plen;i++){
        u8 c=(u8)path[i];
        if(c<=0x20u||c>=0x7Fu||c=='#'||c=='\\')return 0u;
    }
    u32 digits=0u;
    if(port!=80u){
        u32 v=port; do{digits++;v/=10u;}while(v);
        digits++; /* colon */
    }
    /* The constant portion is 194 bytes, verified from the wire literals. */
    if(cap<194u+hlen+plen+digits)return 0u;
    u8*p=buf;
#define BR_APP(s) do{const char*txt=(s);u32 nn=(u32)(sizeof(s)-1u);MC(p,txt,nn);p+=nn;}while(0)
    BR_APP("GET ");
    MC(p,path,plen);p+=plen;
    BR_APP(" HTTP/1.1\r\n");
    BR_APP("Host: ");
    MC(p,host,hlen);p+=hlen;
    if(port!=80u){
        *p++=':';
        char dec[5];u32 n=0u,v=port;
        do{dec[n++]=(char)('0'+(v%10u));v/=10u;}while(v);
        while(n)*p++=(u8)dec[--n];
    }
    BR_APP("\r\n");
    BR_APP("Connection: close\r\n");
    BR_APP("User-Agent: RAFAELIA-Browser/1.0 (ARM; freestanding; nolibc)\r\n");
    BR_APP("Accept: text/html,text/plain;q=0.9,*/*;q=0.8\r\n");
    BR_APP("Accept-Language: pt-BR,pt;q=0.9,en;q=0.8\r\n");
    BR_APP("\r\n");
#undef BR_APP
    return(u32)(p-buf);
}
static u32 HTTP_BUILD_REQ(const char*host,const char*path,u8*buf,u32 cap){
    return HTTP_BUILD_REQ_PORT(host,path,80u,buf,cap);
}

/* ── PARSE DE STATUS LINE ───────────────────────────────────────────────── */
/* "HTTP/1.1 200 OK\r\n" → retorna status code */
static u32 HTTP_PARSE_STATUS(const u8*buf,u32 n){
    /* Only the implemented HTTP/1.0 or 1.1 status-line syntax is accepted.
     * A prefix match such as HTTP/9.9 or HTTPevil is not a valid response. */
    if(!buf||n<14u)return 0u;
    if(BR_MEMCMP(buf,"HTTP/1.",7u)!=0)return 0u;
    if((buf[7]!='0'&&buf[7]!='1')||buf[8]!=' ')return 0u;
    u32 code=0u;
    for(u32 j=9u;j<12u;j++){
        if(buf[j]<'0'||buf[j]>'9')return 0u;
        code=code*10u+(u32)(buf[j]-'0');
    }
    if(code<100u||code>599u)return 0u;
    if(buf[12]=='\r'){
        if(buf[13]!='\n')return 0u;
    }else if(buf[12]!=' '){
        return 0u;
    }
    return code;
}

/* ── PARSE DE HEADER VALUE ───────────────────────────────────────────────── */
static u32 HTTP_HEADERS_END(const u8*buf,u32 n);

/* Compara memória ASCII sem libc, normalizando apenas A-Z -> a-z. */
static int BR_MEMCASECMP_ASCII(const u8*a,const char*b,u32 n){
    for(u32 i=0;i<n;i++){
        u8 ca=a[i];
        u8 cb=(u8)b[i];
        if(ca>='A'&&ca<='Z')ca=(u8)(ca+('a'-'A'));
        if(cb>='A'&&cb<='Z')cb=(u8)(cb+('a'-'A'));
        if(ca!=cb)return(int)ca-(int)cb;
    }
    return 0;
}

/* Procura "Key: value\r\n" em buf[0..n], retorna ponteiro para value */
static const u8* HTTP_FIND_HEADER(const u8*buf,u32 n,const char*key){
    u32 kl=SL(key);
    u32 hdr_end=HTTP_HEADERS_END(buf,n);
    for(u32 i=0;i+kl+2u<hdr_end;i++){
        u32 bol=(i==0u)||((i>=2u)&&buf[i-2u]=='\r'&&buf[i-1u]=='\n');
        if(bol&&BR_MEMCASECMP_ASCII(buf+i,key,kl)==0&&buf[i+kl]==':'){
            u32 j=i+kl+1u;
            while(j<hdr_end&&(buf[j]==' '||buf[j]=='\t'))j++;
            return buf+j;
        }
    }
    return(const u8*)0;
}

/* Parse decimal string → u32 */
static u32 STR2U32(const u8*s,u32 n){
    u32 v=0,i=0;
    while(i<n&&s[i]>='0'&&s[i]<='9'){v=v*10u+(u32)(s[i]-'0');i++;}
    return v;
}

/* Strict bounded Content-Length: 0 absent, 1 valid, -1 malformed.
 * Do not reuse STR2U32: HTML numeric entities need separate semantics. */
static s32 HTTP_CONTENT_LENGTH(const u8*buf,u32 n,u32*out){
    if(!buf||!out)return-1;
    u32 hend=HTTP_HEADERS_END(buf,n);
    if(!hend)return-1;
    /* Duplicate Content-Length is ambiguous even when values coincide.
     * Reject the entire response, instead of trusting the first header. */
    u32 occurrences=0u;
    for(u32 i=0u;i+15u<hend;i++){
        u32 bol=(i==0u)||((i>=2u)&&buf[i-2u]=='\r'&&buf[i-1u]=='\n');
        if(bol&&BR_MEMCASECMP_ASCII(buf+i,"Content-Length",14u)==0&&buf[i+14u]==':'){
            if(++occurrences>1u)return-1;
        }
    }
    const u8*value=HTTP_FIND_HEADER(buf,n,"Content-Length");
    if(!value)return 0;
    u32 off=(u32)(value-buf);
    u32 v=0u,digits=0u;
    for(u32 i=off;i<hend;i++){
        u8 c=buf[i];
        if(c=='\r'){
            if(!digits||i+1u>=hend||buf[i+1u]!='\n')return-1;
            *out=v;
            return 1;
        }
        if(c<'0'||c>'9')return-1;
        u32 digit=(u32)(c-'0');
        if(v>429496729u||(v==429496729u&&digit>5u))return-1;
        v=v*10u+digit;
        digits++;
    }
    return-1;
}

/* ── FIND END OF HEADERS ──────────────────────────────────────────────────── */
/* Retorna offset do início do body (após \r\n\r\n) */


/* Fail closed for unimplemented HTTP response transfer codings.
 * Only a single Transfer-Encoding: chunked is supported; any other coding,
 * repeated header, malformed value or folded header is rejected.
 * Return -1 invalid, 0 absent, 1 chunked. */
static s32 HTTP_TRANSFER_ENCODING(const u8*buf,u32 n){
    if(!buf)return-1;
    u32 hend=HTTP_HEADERS_END(buf,n);
    if(!hend)return-1;
    u32 pos=0u,first=1u,found=0u;
    while(pos+1u<hend){
        u32 end=pos;
        while(end+1u<hend&&!(buf[end]=='\r'&&buf[end+1u]=='\n')){
            u8 c=buf[end];
            if(c==0u||c==0x7Fu||c<0x20u)return-1;
            end++;
        }
        if(end+1u>=hend)return-1;
        if(end==pos){
            /* Empty line closes headers, must be the final CRLF. */
            return(end+2u==hend)?(s32)found:-1;
        }
        if(first){
            if(end<12u||HTTP_PARSE_STATUS(buf,end+2u)==0u)return-1;
            first=0u;
        }else{
            u32 colon=pos;
            while(colon<end&&buf[colon]!=':'){
                u8 c=buf[colon];
                if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                     (c>='0'&&c<='9')||c=='-'))return-1;
                colon++;
            }
            if(colon==pos||colon==end)return-1;
            if(colon-pos==17u&&BR_MEMCASECMP_ASCII(buf+pos,"Transfer-Encoding",17u)==0){
                if(found)return-1;
                u32 value=colon+1u;
                while(value<end&&(buf[value]==' '||buf[value]=='\t'))value++;
                u32 stop=end;
                while(stop>value&&(buf[stop-1u]==' '||buf[stop-1u]=='\t'))stop--;
                if(stop-value!=7u||BR_MEMCASECMP_ASCII(buf+value,"chunked",7u)!=0)return-1;
                found=1u;
            }
            for(u32 k=colon+1u;k<end;k++){
                u8 c=buf[k];
                if((c<0x20u&&c!='\t')||c==0x7Fu)return-1;
            }
        }
        pos=end+2u;
    }
    return-1;
}

/* Return -1 unsupported/duplicate Content-Type, 0 absent (legacy HTML
 * subset), 1 text/html, 2 text/plain. Never render binary types in terminal. */
static s32 HTTP_CONTENT_TYPE(const u8*buf,u32 n){
    if(!buf)return-1;
    u32 hend=HTTP_HEADERS_END(buf,n);
    if(!hend)return-1;
    const u8*val=HTTP_FIND_HEADER(buf,n,"Content-Type");
    u32 found=0u;
    for(u32 pos=0u;pos+13u<hend;pos++){
        u32 bol=(pos==0u)||(pos>=2u&&buf[pos-2u]=='\r'&&buf[pos-1u]=='\n');
        if(bol&&BR_MEMCASECMP_ASCII(buf+pos,"Content-Type",12u)==0&&
           buf[pos+12u]==':'){
            if(++found>1u)return-1;
        }
    }
    if(!val)return 0;
    u32 pos=(u32)(val-buf),end=pos;
    while(end<hend&&buf[end]!='\r'&&buf[end]!=';'&&
          buf[end]!=' '&&buf[end]!='\t')end++;
    u32 len=end-pos;
    if(len==9u&&BR_MEMCASECMP_ASCII(buf+pos,"text/html",9u)==0)return 1;
    if(len==10u&&BR_MEMCASECMP_ASCII(buf+pos,"text/plain",10u)==0)return 2;
    return-1;
}

/* ── DECHUNK IN-PLACE ───────────────────────────────────────────────────── */
/* Retorna tamanho do body dechunkado, ou 0xFFFFFFFF em erro */
static u32 HTTP_DECHUNK(u8*buf,u32 n){
    u32 r=0u,w=0u;
    while(r<n){
        u32 sz=0u;
        u32 got_digit=0u;
        while(r<n){
            u8 c=buf[r++];
            if(c=='\r')break;
            if(c==';'){
                while(r<n&&buf[r]!='\r')r++;
                break;
            }
            u32 v;
            if(c>='0'&&c<='9')v=(u32)(c-'0');
            else if(c>='a'&&c<='f')v=10u+(u32)(c-'a');
            else if(c>='A'&&c<='F')v=10u+(u32)(c-'A');
            else return 0xFFFFFFFFu;
            got_digit=1u;
            if(sz>0x0FFFFFFFu)return 0xFFFFFFFFu;
            sz=(sz<<4u)|v;
        }
        if(!got_digit)return 0xFFFFFFFFu;
        if(r>=n||buf[r]!='\n')return 0xFFFFFFFFu;
        r++;

        if(sz==0u){
            if(r+1u>=n||buf[r]!='\r'||buf[r+1u]!='\n')return 0xFFFFFFFFu;
            /* Only an explicit terminal CRLF is accepted here. Trailer
             * support is deliberately NOT implied by this subset parser. */
            r+=2u;
            if(r!=n)return 0xFFFFFFFFu;
            return w;
        }

        if(sz>n-r)return 0xFFFFFFFFu; /* subtraction avoids unsigned wrap */
        /* In-place dechunk writes below source (w<=r). MC copies backwards,
         * which corrupts overlapping ranges when dest<src. Move forwards. */
        for(u32 i=0u;i<sz;i++)buf[w+i]=buf[r+i];
        w+=sz;
        r+=sz;

        if(r+1u>=n||buf[r]!='\r'||buf[r+1u]!='\n')return 0xFFFFFFFFu;
        r+=2u;
    }
    return 0xFFFFFFFFu;
}
static u32 HTTP_HEADERS_END(const u8*buf,u32 n){
    for(u32 i=0;i+3u<n;i++){
        if(buf[i]=='\r'&&buf[i+1]=='\n'&&buf[i+2]=='\r'&&buf[i+3]=='\n')
            return i+4u;
    }
    return 0u; /* incomplete header: reject, not a zero-length page */
}
