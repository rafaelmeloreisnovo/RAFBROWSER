#pragma once
#include "../../internal/br_types.h"
/* Wire layout: magic[2]='R','F', version, type, seq_be32, payload_len_be16, crc32c_be32, payload. */
#define BR_FRAME_HEADER 14u
#define BR_FRAME_MAX_PAYLOAD 1024u
AI u32 BR_CRC32C(const u8*p,u32 n){u32 c=0xffffffffu;for(u32 i=0;i<n;i++){c^=p[i];for(u32 b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0x82f63b78u);}return ~c;}
AI void BR_PUT16(u8*p,u16 v){p[0]=(u8)(v>>8);p[1]=(u8)v;} AI u16 BR_GET16(const u8*p){return(u16)(((u16)p[0]<<8)|p[1]);}
AI void BR_PUT32(u8*p,u32 v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;} AI u32 BR_GET32(const u8*p){return((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
AI s32 BR_FRAME_ENCODE(u8*out,u32 cap,u8 type,u32 seq,const u8*payload,u16 n){if(!out||n>BR_FRAME_MAX_PAYLOAD||cap<BR_FRAME_HEADER+(u32)n||(!payload&&n))return-1;out[0]='R';out[1]='F';out[2]=1;out[3]=type;BR_PUT32(out+4,seq);BR_PUT16(out+8,n);if(n)for(u32 i=0;i<n;i++)out[BR_FRAME_HEADER+i]=payload[i];BR_PUT32(out+10,BR_CRC32C(out+BR_FRAME_HEADER,n));return(s32)(BR_FRAME_HEADER+n);}
AI s32 BR_FRAME_DECODE(const u8*in,u32 n,u8*type,u32*seq,const u8**payload,u16*plen){if(!in||n<BR_FRAME_HEADER||in[0]!='R'||in[1]!='F'||in[2]!=1)return-1;u16 z=BR_GET16(in+8);if(z>BR_FRAME_MAX_PAYLOAD||n!=BR_FRAME_HEADER+(u32)z||BR_GET32(in+10)!=BR_CRC32C(in+BR_FRAME_HEADER,z))return-1;if(type)*type=in[3];if(seq)*seq=BR_GET32(in+4);if(payload)*payload=in+BR_FRAME_HEADER;if(plen)*plen=z;return 0;}
