#pragma once
#include "../compact/br_frame.h"
typedef struct {u32 next_sequence;u32 total_bytes;u32 expected_bytes;u8 active;} BRBinaryStream;
AI void BR_BINARY_INIT(BRBinaryStream*s,u32 expected){s->next_sequence=0;s->total_bytes=0;s->expected_bytes=expected;s->active=1;}
AI s32 BR_BINARY_ACCEPT(BRBinaryStream*s,u32 sequence,u32 chunk_bytes){if(!s||!s->active||sequence!=s->next_sequence||chunk_bytes==0u||chunk_bytes>BR_FRAME_MAX_PAYLOAD||chunk_bytes>s->expected_bytes-s->total_bytes)return-1;s->total_bytes+=chunk_bytes;s->next_sequence++;if(s->total_bytes==s->expected_bytes)s->active=0;return 0;}
