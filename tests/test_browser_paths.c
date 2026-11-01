#include <assert.h>
#include <string.h>
#include "../settings/br_settings.h"
#include "../browser/br_resource.h"
#include "../network/compact/br_frame.h"
#include "../network/binary/br_chunk.h"
#include "../updates/br_manifest.h"
int main(void){
 (void)_BC;(void)_RB;(void)_NB;
 BRSettings s;BR_SETTINGS_DEFAULT(&s);assert(s.timeout_seconds==3&&s.max_response_bytes==65535);
 const char cfg[]="timeout_seconds=9\nmax_response_bytes=4096\ninteractive=1\n";assert(BR_SETTINGS_PARSE(cfg,sizeof(cfg)-1,&s)==0&&s.timeout_seconds==9&&s.interactive==1);
 const char badcfg[]="timeout_seconds=0\n";assert(BR_SETTINGS_PARSE(badcfg,sizeof(badcfg)-1,&s)<0);
 assert(BR_RESOURCE_CLASSIFY("text/html",9)==BR_RES_HTML);assert(BR_RESOURCE_ROUTE(BR_RES_HTML)==BR_ROUTE_RENDER_TEXT);assert(BR_RESOURCE_ROUTE(BR_RES_JS)==BR_ROUTE_STORE_ONLY);assert(BR_RESOURCE_ROUTE(BR_RES_IMAGE)==BR_ROUTE_REJECT);
 u8 frame[1100],type=0,payload[]={1,2,3};u32 seq=0;const u8*view=0;u16 n=0;s32 z=BR_FRAME_ENCODE(frame,sizeof(frame),7,42,payload,3);assert(z==17);assert(BR_FRAME_DECODE(frame,(u32)z,&type,&seq,&view,&n)==0&&type==7&&seq==42&&n==3&&view[2]==3);frame[16]^=1;assert(BR_FRAME_DECODE(frame,(u32)z,0,0,0,0)<0);
 BRBinaryStream stream;BR_BINARY_INIT(&stream,5);assert(BR_BINARY_ACCEPT(&stream,0,3)==0);assert(BR_BINARY_ACCEPT(&stream,2,2)<0);assert(BR_BINARY_ACCEPT(&stream,1,2)==0&&!stream.active);
 const char manifest[]="version=2\nimage_bytes=3\nsha256=0000000000000000000000000000000000000000000000000000000000000000\nsignature=00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000\n";BRUpdateManifest m;assert(BR_UPDATE_PARSE(manifest,sizeof(manifest)-1,&m)==0&&m.version==2&&m.image_bytes==3);assert(BR_UPDATE_AUTHORIZED(&m)<0);
 return 0;
}
