#pragma once
/* br_links.h — bounded, non-DOM anchor scanner and explicit text navigation.
 * 24 URLs maximum, no heap, no JS/credentials/forms/cookies.
 * Untrusted attributes are never emitted as terminal escape sequences. */
#include "br_dns.h"

#define BR_MAX_LINKS 24u
typedef struct {char href[512];} BRLink;
static BRLink _BR_LINKS[BR_MAX_LINKS];
static u32 _BR_LINK_COUNT=0u;

AI u8 BR_ASCII_LOWER(u8 c){return(c>='A'&&c<='Z')?(u8)(c+32u):c;}
static u8 BR_EQ_CI(const u8*buf,const char*lit,u32 n){
    for(u32 j=0u;j<n;j++)if(BR_ASCII_LOWER(buf[j])!=(u8)lit[j])return 0u;
    return 1u;
}
AI u8 BR_WS(u8 c){return(c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f');}

static u32 BR_LINK_SCAN(const u8*html,u32 n){
    _BR_LINK_COUNT=0u;
    if(!html)return 0u;
    u8 raw=0u; /* 1=script 2=style */
    for(u32 i=0u;i+2u<n&&_BR_LINK_COUNT<BR_MAX_LINKS;i++){
        if(html[i]!='<')continue;
        if(i+3u<n&&html[i+1u]=='!'&&html[i+2u]=='-'&&html[i+3u]=='-'){
            i+=4u;
            while(i+2u<n&&!(html[i]=='-'&&html[i+1u]=='-'&&html[i+2u]=='>'))i++;
            if(i+2u>=n)break;
            i+=2u;continue;
        }
        u32 j=i+1u;
        u8 closing=0u;
        if(html[j]=='/'){closing=1u;j++;}
        u32 name=j;
        while(j<n&&((html[j]>='a'&&html[j]<='z')||
                     (html[j]>='A'&&html[j]<='Z')))j++;
        u32 namelen=j-name;
        if(!namelen||j>=n||(!BR_WS(html[j])&&html[j]!='>'&&html[j]!='/'))continue;
        if(raw){
            if(closing&&((raw==1u&&namelen==6u&&BR_EQ_CI(html+name,"script",6u))||
                         (raw==2u&&namelen==5u&&BR_EQ_CI(html+name,"style",5u))))raw=0u;
            continue;
        }
        if(!closing&&namelen==6u&&BR_EQ_CI(html+name,"script",6u)){raw=1u;continue;}
        if(!closing&&namelen==5u&&BR_EQ_CI(html+name,"style",5u)){raw=2u;continue;}
        if(closing||namelen!=1u||BR_ASCII_LOWER(html[name])!='a')continue;
        /* Scan attributes up to >, only href is retained, never its contents
         * as HTML. Reject control bytes, invalid protocol and truncation. */
        while(j<n&&html[j]!='>'){
            while(j<n&&BR_WS(html[j]))j++;
            u32 attr=j;
            while(j<n&&((html[j]>='a'&&html[j]<='z')||
                        (html[j]>='A'&&html[j]<='Z')||
                        (html[j]>='0'&&html[j]<='9')||html[j]=='-'))j++;
            u32 attrlen=j-attr;
            if(!attrlen){if(j<n&&html[j]!='>')j++;continue;}
            while(j<n&&BR_WS(html[j]))j++;
            if(j>=n||html[j]!='=')continue;
            j++;
            while(j<n&&BR_WS(html[j]))j++;
            if(j>=n)break;
            u8 quote=0u;
            if(html[j]=='"'||html[j]=='\''){quote=html[j++];}
            u32 first=j;
            while(j<n&&((quote&&html[j]!=quote)||
                         (!quote&&!BR_WS(html[j])&&html[j]!='>')))j++;
            u32 len=j-first;
            if(quote&&j<n)j++;
            if(attrlen!=4u||!BR_EQ_CI(html+attr,"href",4u)||!len||len>=512u)continue;
            char *out=_BR_LINKS[_BR_LINK_COUNT].href;
            u32 dst=0u,valid=1u;
            for(u32 k=0u;k<len;k++){
                u8 c=html[first+k];
                if(c<0x21u||c>=0x7Fu||c=='\\'){valid=0u;break;}
                if(c=='&'&&len-k>=5u&&BR_MEMCMP(html+first+k,"&amp;",5u)==0){
                    c='&';k+=4u;
                }
                if(dst>=511u){valid=0u;break;}
                out[dst++]=(char)c;
            }
            if(!valid||!dst||out[0]=='#')continue;
            out[dst]=0;
            u32 has_colon=0u;
            for(u32 k=0u;k<dst;k++){if(out[k]==':')has_colon=1u;}
            if(has_colon&&!(dst>=7u&&BR_MEMCMP(out,"http://",7u)==0)&&
                !(dst>=8u&&BR_MEMCMP(out,"https://",8u)==0))continue;
            if(dst>=2u&&out[0]=='/'&&out[1]=='/')continue;
            _BR_LINK_COUNT++;
            break;
        }
    }
    return _BR_LINK_COUNT;
}

static s32 BR_LINK_FOLLOW(BCtx*ctx,u32 index){
    if(!ctx||index==0u||index>_BR_LINK_COUNT)return-1;
    const char *src=_BR_LINKS[index-1u].href;
    u32 n=SL(src);
    if(!n||n>=512u)return-1;
    if((n>=7u&&BR_MEMCMP(src,"http://",7u)==0)||
       (n>=8u&&BR_MEMCMP(src,"https://",8u)==0)){
        BCtx next;
        MC0(&next,sizeof(next));
        if(URL_PARSE(src,&next)!=0)return-1;
        MC(ctx->host,next.host,sizeof(ctx->host));
        MC(ctx->path,next.path,sizeof(ctx->path));
        ctx->port=next.port;ctx->use_tls=next.use_tls;
        return 0;
    }
    char path[512];u32 len=0u;
    if(src[0]=='/'){
        MC(path,src,n);len=n;
    }else{
        u32 base=0u;
        while(base<511u&&ctx->path[base]&&ctx->path[base]!='?')base++;
        if(src[0]=='?'){
            if(base+n>=512u)return-1;
            MC(path,ctx->path,base);len=base;
        }else{
            u32 dir=0u;
            for(u32 j=0u;j<base;j++)if(ctx->path[j]=='/')dir=j+1u;
            if(dir+n>=512u)return-1;
            MC(path,ctx->path,dir);len=dir;
        }
        MC(path+len,src,n);len+=n;
    }
    if(!len||path[0]!='/'||len>=512u)return-1;
    path[len]=0;
    /* Internal fragment is for page position, never the HTTP request. */
    for(u32 j=0u;j<len;j++){if(path[j]=='#'){path[j]=0;break;}}
    MC0(ctx->path,sizeof(ctx->path));MC(ctx->path,path,SL(path));
    return 0;
}
