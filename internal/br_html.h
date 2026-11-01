#pragma once
/* br_html.h — HTML tokenizer/renderer text-mode (estilo Lynx/Links)
 * [R23] Máquina de estados: TEXT | TAG | ENTITY | COMMENT
 * [R24] Tags reconhecidas: p br h1..h6 li ul ol a title body head
 * [R25] Entities: &amp; &lt; &gt; &nbsp; &quot;
 * [R26] Output: ANSI colors para headings e links
 * [R27] Sem heap: buffer estático _RB[NET_BUF]
 * [R28] Branchless: estado via flip-flop de 2 bits
 */
#include "br_http.h"

/* Estados do tokenizer HTML */
#define HS_TEXT    0u  /* fora de tag */
#define HS_TAG     1u  /* dentro de <tag> */
#define HS_ENT     2u  /* dentro de &entity; */
#define HS_CMNT    3u  /* dentro de <!--comment--> */

static u32 HTML_RENDER(const u8*html,u32 n,u8*out,u32 cap){
    /* No writes at zero capacity; no dereference of missing source. */
    if(!out||cap==0u)return 0u;
    if(!html&&n!=0u){out[0]=0u;return 0u;}
    u8  st=HS_TEXT;          /* estado: flip-flop 2-bit */
    u32 oi=0;                /* índice de saída */
    u32 ci=0;                /* coluna atual (para wrap em 80) */
    u8  tc[32];u32 ti=0;     /* buffer de tag atual */
    u8  ec[8]; u32 ei=0;     /* buffer de entity atual */
    u8  in_head=0;           /* dentro de <head> */
    u8  col_h=0;             /* cor de heading ativa */
    u8  raw_skip=0;          /* 1=script 2=style; never render script source */
    u8  tag_done=0;          /* tag attributes are not tag-name bytes */

    /* Macro: emite byte no output sem overflow */
#define EM(c) do{u8 _em_byte=(u8)(c);if(oi<cap-1u){out[oi++]=_em_byte;}}while(0)
#define EMS(s) do{const char*_s=(s);while(*_s)EM(*_s++);}while(0)

    for(u32 i=0;i<n;i++){
        u8 c=html[i];
        /* HTML raw-text elements are not printable content. Case-insensitive
         * lookahead is bounded by n, without allocations or external engine. */
        if(raw_skip){
            if(c=='<'&&i+2u<n&&html[i+1u]=='/'){
                const char*name=(raw_skip==1u)?"script":"style";
                u32 len=(raw_skip==1u)?6u:5u;
                if(i+2u+len<n){
                    u32 k=0u;
                    while(k<len){
                        u8 v=html[i+2u+k];
                        if(v>='A'&&v<='Z')v=(u8)(v+32u);
                        if(v!=(u8)name[k])break;
                        k++;
                    }
                    if(k==len){
                        u32 j=i+2u+len;
                        while(j<n&&(html[j]==' '||html[j]=='\t'||html[j]=='\r'||html[j]=='\n'))j++;
                        if(j<n&&html[j]=='>'){
                            raw_skip=0u;st=HS_TEXT;i=j;
                        }
                    }
                }
            }
            continue;
        }
        /* Flip-flop de estado */
        switch(st){
        case HS_TEXT:
            if(c=='<'){st=HS_TAG;ti=0;tag_done=0u;MC0(tc,32u);break;}
            if(c=='&'){st=HS_ENT;ei=0;MC0(ec,8u);break;}
            if(in_head)break;
            /* Untrusted HTML must never inject terminal ESC/BEL/CSI controls. */
            if((c<0x20u&&c!='\t'&&c!='\n'&&c!='\r')||c==0x7Fu)break;
            /* wrap a 80 cols */
            if(c=='\n'||c=='\r')break;
            if(c==' '||c=='\t'){if(ci>0&&oi>0&&out[oi-1]!=' '){EM(' ');ci++;}break;}
            EM(c);ci++;
            if(ci>=78u){EM('\n');ci=0;}
            break;
        case HS_TAG:
            if(c=='>'){
                st=HS_TEXT;
                tc[ti<31u?ti:31u]=0;
                /* Compara tag — branchless usando memcmp */
                u8*t=tc; /* alias para tc */
                /* Pula '/' para closing tags */
                u8*tt=t+(t[0]=='/'?1u:0u);
                /* lowercase implícito: qualquer case */
                /* heading tags → ANSI bold/color */
                if(tt[0]=='h'&&tt[1]>='1'&&tt[1]<='6'&&tt[2]==0){
                    if(t[0]!='/'){col_h=tt[1]-'0';
                        if(oi>0&&out[oi-1]!='\n'){EM('\n');ci=0;}
                        EMS("\033[1;33m"); /* yellow bold */
                    } else {
                        EMS("\033[0m\n\n");ci=0;col_h=0;
                    }
                } else if(BR_MEMCMP(tt,"p",2u)==0){
                    if(t[0]!='/'){if(ci>0){EM('\n');ci=0;}EM('\n');}
                } else if(BR_MEMCMP(tt,"br",3u)==0||BR_MEMCMP(tt,"br/",4u)==0){
                    EM('\n');ci=0;
                } else if(BR_MEMCMP(tt,"li",3u)==0){
                    if(ci>0){EM('\n');ci=0;}EMS("  \xe2\x80\xa2 ");ci=4;
                } else if(BR_MEMCMP(tt,"a",2u)==0){
                    if(t[0]!='/') EMS("\033[0;36m"); /* cyan for links */
                    else EMS("\033[0m");
                } else if(BR_MEMCMP(tt,"title",6u)==0){
                    if(t[0]!='/'){EMS("\n\033[1;32m[TÍTULO] ");} else {EMS("\033[0m\n");}
                } else if(BR_MEMCMP(tt,"head",5u)==0){
                    in_head=(t[0]!='/');
                } else if(BR_MEMCMP(tt,"body",5u)==0){
                    in_head=0;
                } else if(BR_MEMCMP(tt,"script",7u)==0||
                          BR_MEMCMP(tt,"style",6u)==0){
                    if(t[0]!='/')raw_skip=(u8)((tt[1]=='c')?1u:2u);
                }
                ti=0;
            } else if(c=='!'&&ti==0){
                /* Pode ser comentário <!-- */
                st=HS_CMNT;
            } else {
                /* Keep attributes out of the tag name. */
                if(c==' '||c=='\t'||c=='\n'||c=='\r')tag_done=1u;
                else if(!tag_done&&ti<31u){
                    if(c>='A'&&c<='Z')tc[ti++]=(u8)(c+32u);
                    else tc[ti++]=c;
                }
            }
            break;
        case HS_ENT:
            if(c==';'){
                st=HS_TEXT;ec[ei<7u?ei:7u]=0;
                if(BR_MEMCMP(ec,"amp",4u)==0)EM('&');
                else if(BR_MEMCMP(ec,"lt",3u)==0)EM('<');
                else if(BR_MEMCMP(ec,"gt",3u)==0)EM('>');
                else if(BR_MEMCMP(ec,"quot",5u)==0)EM('"');
                else if(BR_MEMCMP(ec,"apos",5u)==0)EM('\'');
                else if(BR_MEMCMP(ec,"nbsp",5u)==0)EM(' ');
                else if(ec[0]=='#'){
                    u32 code=STR2U32(ec+1,ei-1u);
                    if(code>=0x20u&&code<0x7Fu)EM((u8)code);
                }
                ei=0;
            } else if(c=='<'||c=='\n'){
                /* entity mal formada: emite & e texto */
                st=HS_TEXT;EM('&');
                for(u32 j=0;j<ei;j++)EM(ec[j]);
                if(c!='<')EM(c); else {st=HS_TAG;ti=0;}
            } else {
                if(ei<7u)ec[ei++]=c;
            }
            break;
        case HS_CMNT:
            /* Simplificado: volta ao TEXT no próximo '>' */
            if(c=='>')st=HS_TEXT;
            break;
        }
    }
    /* Reseta cor */
    EMS("\033[0m\n");
    out[oi]=0;
#undef EM
#undef EMS
    return oi;
}

/* Plaintext MIME must not interpret any "<tag>" as markup. UTF-8 bytes are
 * passed through unchanged; C0/DEL terminal controls are suppressed. */
static u32 TEXT_RENDER(const u8*src,u32 n,u8*out,u32 cap){
    if(!out||!cap)return 0u;
    if(!src&&n){out[0]=0u;return 0u;}
    u32 oi=0u;
    for(u32 i=0u;i<n&&oi+1u<cap;i++){
        u8 c=src[i];
        if(c==0x7Fu||c==0x1Bu||c==0u)continue;
        if(c=='\r'){out[oi++]='\n';continue;}
        if(c<0x20u&&c!='\n'&&c!='\t')continue;
        out[oi++]=c;
    }
    out[oi]=0u;
    return oi;
}
