#pragma once
/* Bounded browser configuration parser. No heap, locale, or libc calls. */
#include "../internal/br_types.h"
#define BR_CFG_MAX_LINE 96u
#define BR_CFG_MAX_BYTES 2048u
typedef struct { u32 timeout_seconds; u32 max_response_bytes; u8 interactive; u8 allow_http; } BRSettings;
AI u8 BR_CFG_EQ(const char*a,u32 n,const char*b){u32 i=0;while(b[i]&&i<n&&a[i]==b[i])i++;return (u8)(i==n&&!b[i]);}
AI s32 BR_CFG_DEC(const char*p,u32 n,u32*out){if(!n)return-1;u32 v=0;for(u32 i=0;i<n;i++){if(p[i]<'0'||p[i]>'9')return-1;u32 d=(u32)(p[i]-'0');if(v>(0xffffffffu-d)/10u)return-1;v=v*10u+d;}*out=v;return 0;}
AI void BR_SETTINGS_DEFAULT(BRSettings*s){s->timeout_seconds=3u;s->max_response_bytes=65535u;s->interactive=0u;s->allow_http=1u;}
AI s32 BR_SETTINGS_PARSE(const char*p,u32 n,BRSettings*s){if(!p||!s||n>BR_CFG_MAX_BYTES)return-1;BRSettings t;BR_SETTINGS_DEFAULT(&t);u8 seen=0;u32 pos=0;while(pos<n){u32 end=pos;while(end<n&&p[end]!='\n')end++;u32 len=end-pos;if(len&&p[pos+len-1]=='\r')len--;if(len>BR_CFG_MAX_LINE)return-1;if(len&&p[pos]!='#'){u32 eq=0;while(eq<len&&p[pos+eq]!='=')eq++;if(eq==0||eq==len)return-1;const char*k=p+pos,*v=p+pos+eq+1;u32 vn=len-eq-1,val=0;if(BR_CFG_EQ(k,eq,"timeout_seconds")){if((seen&1u)||BR_CFG_DEC(v,vn,&val)||val<1u||val>30u)return-1;seen|=1u;t.timeout_seconds=val;}else if(BR_CFG_EQ(k,eq,"max_response_bytes")){if((seen&2u)||BR_CFG_DEC(v,vn,&val)||val<1024u||val>65535u)return-1;seen|=2u;t.max_response_bytes=val;}else if(BR_CFG_EQ(k,eq,"interactive")){if((seen&4u)||vn!=1u||(v[0]!='0'&&v[0]!='1'))return-1;seen|=4u;t.interactive=(u8)(v[0]-'0');}else return-1;}pos=end<n?end+1u:end;}*s=t;return 0;}
