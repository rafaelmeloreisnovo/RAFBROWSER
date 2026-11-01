/* Host-only adversarial fixtures. The shipped freestanding binary still uses no libc.
 * No DNS, TCP, TLS or arbitrary URL requests are made by this test. */
#include <stdio.h>
#include <string.h>
#include "../internal/br_dns.h"
#include "../internal/br_html.h"
#include "../internal/br_links.h"

static unsigned checks=0u, failures=0u;
#define CHECK(x) do { checks++; if(!(x)){ failures++; \
    fprintf(stderr,"FAIL fixture line %d: %s\n",__LINE__,#x); }} while(0)

static void urls(void) {
    BCtx c;
    CHECK(URL_PARSE("http://example.com/a",&c)==0);
    CHECK(strcmp(c.host,"example.com")==0);
    CHECK(strcmp(c.path,"/a")==0 && c.port==80u && c.use_tls==0u);
    CHECK(URL_PARSE("https://example.com/",&c)==0 && c.use_tls==1u && c.port==443u);
    CHECK(URL_PARSE("http://127.0.0.1:65535/test",&c)==0 && c.port==65535u);
    CHECK(URL_PARSE("http://x:65536/",&c)!=0);
    CHECK(URL_PARSE("http://x:0/",&c)!=0);
    CHECK(URL_PARSE("http://x:abc/",&c)!=0);
    CHECK(URL_PARSE("http://u@x/",&c)!=0);
    CHECK(URL_PARSE("http://x/\r\nX-Evil: yes",&c)!=0);
    char long_host[350],long_path[620];
    memcpy(long_host,"http://",7);
    memset(long_host+7,'x',300);
    memcpy(long_host+307,"/\0",2);
    CHECK(URL_PARSE(long_host,&c)!=0);
    memcpy(long_path,"http://a/",9);
    memset(long_path+9,'z',580);long_path[589]=0;
    CHECK(URL_PARSE(long_path,&c)!=0);
    CHECK(URL_PARSE("http://example.com?lang=pt#local",&c)==0);
    CHECK(strcmp(c.path,"/?lang=pt")==0);
    CHECK(URL_PARSE("http://example.com/a?q=1#section",&c)==0);
    CHECK(strcmp(c.path,"/a?q=1")==0);
    CHECK(URL_PARSE("http://example.com#section",&c)==0);
    CHECK(strcmp(c.path,"/")==0);
    CHECK(URL_PARSE("ftp://example.com/file",&c)!=0);
    CHECK(URL_PARSE("javascript:alert(1)",&c)!=0);
    CHECK(URL_PARSE("http://host..name/",&c)!=0);
    CHECK(URL_PARSE("http://host/path#fragment",&c)==0);
    CHECK(strcmp(c.path,"/path")==0);
    CHECK(URL_PARSE("",&c)!=0);
    CHECK(URL_PARSE((const char*)0,&c)!=0);
}

static void http(void) {
    u8 req[1024];
    u32 n=HTTP_BUILD_REQ("example.com","/a",req,sizeof(req));
    CHECK(n>0u && n<sizeof(req));
    CHECK(n>=15u && memcmp(req,"GET /a HTTP/1.1\r\n",17u)==0);
    CHECK(HTTP_BUILD_REQ("bad\r\nInjected:1","/",req,sizeof(req))==0u);
    CHECK(HTTP_BUILD_REQ("example.com","/\r\nX:y",req,sizeof(req))==0u);
    CHECK(HTTP_BUILD_REQ("example.com","/",req,5u)==0u);
    n=HTTP_BUILD_REQ_PORT("example.com","/a?q=1",8080u,req,sizeof(req));
    CHECK(n>0u && n<sizeof(req));
    req[n]=0u;
    CHECK(strstr((const char*)req,"Host: example.com:8080\r\n")!=0);
    CHECK(strstr((const char*)req,"GET /a?q=1 HTTP/1.1\r\n")!=0);
    CHECK(HTTP_BUILD_REQ_PORT("example.com","/",0u,req,sizeof(req))==0u);


    const char ok[]="HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nBODY";
    const char malformed[]="HTTP/1.1 200 OK\r\nContent-Length: 4x\r\n\r\nBODY";
    const char over[]="HTTP/1.1 200 OK\r\nContent-Length: 4294967296\r\n\r\n";
    const char duplicate[]="HTTP/1.1 200 OK\r\nContent-Length: 4\r\ncontent-length: 99\r\n\r\nBODY";
    const char incomplete[]="HTTP/1.1 200 OK\r\nContent-Length: 4\r\n";
    u32 len=0u;
    CHECK(HTTP_PARSE_STATUS((const u8*)ok,sizeof(ok)-1u)==200u);
    CHECK(HTTP_PARSE_STATUS((const u8*)"garbage 200 OK\r\n",16u)==0u);
    CHECK(HTTP_PARSE_STATUS((const u8*)"HTTP/9.9 200 OK\r\n",17u)==0u);
    CHECK(HTTP_CONTENT_LENGTH((const u8*)ok,sizeof(ok)-1u,&len)==1 && len==4u);
    CHECK(HTTP_CONTENT_LENGTH((const u8*)malformed,sizeof(malformed)-1u,&len)<0);
    CHECK(HTTP_CONTENT_LENGTH((const u8*)over,sizeof(over)-1u,&len)<0);
    CHECK(HTTP_CONTENT_LENGTH((const u8*)duplicate,sizeof(duplicate)-1u,&len)<0);
    CHECK(HTTP_CONTENT_LENGTH((const u8*)incomplete,sizeof(incomplete)-1u,&len)<0);
    CHECK(HTTP_HEADERS_END((const u8*)incomplete,sizeof(incomplete)-1u)==0u);

    const char chunked_ok[]="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    const char te_unsupported[]="HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\n";
    const char te_duplicate[]="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\ntransfer-encoding: chunked\r\n\r\n";
    const char header_fold[]="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n gzip\r\n\r\n";
    const char bad_name[]="HTTP/1.1 200 OK\r\nFoo Bar: yes\r\n\r\n";
    CHECK(HTTP_TRANSFER_ENCODING((const u8*)ok,sizeof(ok)-1u)==0);
    CHECK(HTTP_TRANSFER_ENCODING((const u8*)chunked_ok,sizeof(chunked_ok)-1u)==1);
    CHECK(HTTP_TRANSFER_ENCODING((const u8*)te_unsupported,sizeof(te_unsupported)-1u)<0);
    CHECK(HTTP_TRANSFER_ENCODING((const u8*)te_duplicate,sizeof(te_duplicate)-1u)<0);
    CHECK(HTTP_TRANSFER_ENCODING((const u8*)header_fold,sizeof(header_fold)-1u)<0);
    CHECK(HTTP_TRANSFER_ENCODING((const u8*)bad_name,sizeof(bad_name)-1u)<0);

    u8 chunks[]="4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n";
    u32 d=HTTP_DECHUNK(chunks,sizeof(chunks)-1u);
    CHECK(d==9u && memcmp(chunks,"Wikipedia",9u)==0);
    u8 bad[]="4\r\nWiki\r\n0\r\n";
    CHECK(HTTP_DECHUNK(bad,sizeof(bad)-1u)==0xFFFFFFFFu);
}


static void dns_packets(void){
    const u8 q[19]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,1,'a',0,0,1,0,1};
    u8 rsp[35]={0};
    memcpy(rsp,q,sizeof(q));
    rsp[2]=0x81;rsp[3]=0x80; /* QR+RD+RA, response code 0 */
    rsp[6]=0;rsp[7]=1;      /* one answer */
    rsp[19]=0xc0;rsp[20]=12; /* points to echoed question */
    rsp[22]=1;rsp[24]=1;   /* A, IN */
    rsp[30]=4;             /* RDLENGTH */
    rsp[31]=192;rsp[32]=0;rsp[33]=2;rsp[34]=123;
    u8 ip[4]={0};
    CHECK(DNS_PARSE_A_REPLY(rsp,sizeof(rsp),q,sizeof(q),ip)==0);
    CHECK(ip[0]==192&&ip[1]==0&&ip[2]==2&&ip[3]==123);
    rsp[0]^=1;
    CHECK(DNS_PARSE_A_REPLY(rsp,sizeof(rsp),q,sizeof(q),ip)!=0);
    rsp[0]^=1;
    rsp[13]='z';
    CHECK(DNS_PARSE_A_REPLY(rsp,sizeof(rsp),q,sizeof(q),ip)!=0);
    rsp[13]='a';
    rsp[20]=13;
    CHECK(DNS_PARSE_A_REPLY(rsp,sizeof(rsp),q,sizeof(q),ip)!=0);
    rsp[20]=12;
    rsp[2]=0x83;
    CHECK(DNS_PARSE_A_REPLY(rsp,sizeof(rsp),q,sizeof(q),ip)!=0);
    rsp[2]=0x81;
    CHECK(DNS_PARSE_A_REPLY(rsp,34u,q,sizeof(q),ip)!=0);
    CHECK(PARSE_IP("127.0.0.1",ip)==0);
    CHECK(PARSE_IP("1..2.3",ip)!=0);
    CHECK(PARSE_IP("1.2.3.",ip)!=0);
    CHECK(PARSE_IP("01.2.3.4",ip)!=0);
    CHECK(PARSE_IP("256.2.3.4",ip)!=0);
}


static void route_security(void){
    const u8 public_ip[4]={93u,184u,216u,34u};
    const u8 private_ip[4]={192u,168u,0u,1u};
    const u8 loopback[4]={127u,0u,0u,1u};
    const u8 cgn[4]={100u,64u,0u,1u};
    const u8 documentation[4]={203u,0u,113u,9u};
    const u8 multicast[4]={224u,1u,1u,1u};
    CHECK(BR_IPV4_GLOBAL_CANDIDATE(public_ip)==1u);
    CHECK(BR_IPV4_GLOBAL_CANDIDATE(private_ip)==0u);
    CHECK(BR_IPV4_GLOBAL_CANDIDATE(loopback)==0u);
    CHECK(BR_IPV4_GLOBAL_CANDIDATE(cgn)==0u);
    CHECK(BR_IPV4_GLOBAL_CANDIDATE(documentation)==0u);
    CHECK(BR_IPV4_GLOBAL_CANDIDATE(multicast)==0u);
    CHECK(BR_OVERRIDE_IP_ALLOWED("example.org",public_ip)==1u);
    CHECK(BR_OVERRIDE_IP_ALLOWED("example.org",loopback)==0u);
    CHECK(BR_OVERRIDE_IP_ALLOWED("localhost",loopback)==1u);
    CHECK(BR_OVERRIDE_IP_ALLOWED("localhost.evil",loopback)==0u);
}

static void links(void){
    const char page[]="<a href='/next?q=1&amp;v=2'>GO</a>"
        "<!-- <a href='/comment'>hidden</a> -->"
        "<script><a href='/evil'>HIDDEN</a></script>"
        "<a href='javascript:alert(1)'>BAD</a>"
        "<a href='https://example.org/secure'>SECURE</a>";
    CHECK(BR_LINK_SCAN((const u8*)page,(u32)(sizeof(page)-1u))==2u);
    CHECK(strcmp(_BR_LINKS[0].href,"/next?q=1&v=2")==0);
    CHECK(strcmp(_BR_LINKS[1].href,"https://example.org/secure")==0);
    BCtx c;
    CHECK(URL_PARSE("http://example.com/base/start",&c)==0);
    CHECK(BR_LINK_FOLLOW(&c,1u)==0);
    CHECK(strcmp(c.path,"/next?q=1&v=2")==0);
    CHECK(c.use_tls==0u);
    CHECK(BR_LINK_FOLLOW(&c,2u)==0);
    CHECK(c.use_tls==1u); /* HTTPS will still fail closed before networking. */
    CHECK(BR_LINK_FOLLOW(&c,3u)!=0);
    const char rel[]="<a href='other.html'>relative</a><a href='?x=1'>query</a>";
    CHECK(BR_LINK_SCAN((const u8*)rel,sizeof(rel)-1u)==2u);
    CHECK(URL_PARSE("http://example.com/a/b.html",&c)==0);
    CHECK(BR_LINK_FOLLOW(&c,1u)==0);
    CHECK(strcmp(c.path,"/a/other.html")==0);
    CHECK(BR_LINK_FOLLOW(&c,2u)==0);
    CHECK(strcmp(c.path,"/a/other.html?x=1")==0);
}

static void html(void) {
    u8 out[256];
    u8 sentinel=0xA5u;
    const u8 esc[]={'a',0x1B,'[','3','1','m','b',0x07,0x7F};
    CHECK(HTML_RENDER(esc,sizeof(esc),out,sizeof(out))>0u);
    CHECK(strchr((const char*)out,'a')!=0 && strchr((const char*)out,'b')!=0);
    /* Non-trusted raw ESC/BEL/DEL never pass to stdout. Renderer may emit its
     * own fixed reset ANSI at the end; avoid falsely banning that sequence. */
    CHECK(strstr((const char*)out,"a\033[31m")==0);
    CHECK(strchr((const char*)out,0x07)==0);
    CHECK(HTML_RENDER((const u8*)"<script>EVIL</script>HELLO",26u,out,sizeof(out))>0u);
    CHECK(strstr((const char*)out,"EVIL")==0);
    CHECK(strstr((const char*)out,"HELLO")!=0);
    CHECK(HTML_RENDER((const u8*)"x",1u,&sentinel,0u)==0u && sentinel==0xA5u);
    CHECK(HTML_RENDER((const u8*)"x",1u,out,1u)==0u && out[0]==0u);
    CHECK(HTML_RENDER((const u8*)0,3u,out,sizeof(out))==0u && out[0]==0u);
}


/* Offline deterministic mutation corpus: bounded, seeded, reproducible.
 * Exercises raw truncated/untrusted bytes through the actual public parsers.
 * The CI sanitizer is test-only, never linked into the freestanding build. */
static u32 next_mutation(u32 *s) {
    u32 v=*s;
    v^=v<<13u;
    v^=v>>17u;
    v^=v<<5u;
    *s=v;
    return v;
}
static void deterministic_mutations(void) {
    u32 seed=0xBADC0DEu;
    for(u32 run=0u;run<4096u;run++){
        u8 raw[160],dechunk[160],rendered[256];
        u32 n=next_mutation(&seed)%129u;
        for(u32 j=0u;j<n;j++){
            u32 r=next_mutation(&seed);
            raw[j]=(u8)(r&0xFFu);
            dechunk[j]=raw[j];
        }
        raw[n]=0u;
        (void)HTTP_PARSE_STATUS(raw,n);
        (void)HTTP_HEADERS_END(raw,n);
        u32 content_len=0u;
        (void)HTTP_CONTENT_LENGTH(raw,n,&content_len);
        (void)HTTP_TRANSFER_ENCODING(raw,n);
        (void)BR_LINK_SCAN(raw,n);
        const u8 q[19]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,1,'a',0,0,1,0,1};
        u8 ip4[4]={0};
        (void)DNS_PARSE_A_REPLY(raw,n,q,sizeof(q),ip4);
        u32 decoded=HTTP_DECHUNK(dechunk,n);
        CHECK(decoded==0xFFFFFFFFu||decoded<=n);
        u32 len=HTML_RENDER(raw,n,rendered,(u32)sizeof(rendered));
        CHECK(len<sizeof(rendered) && rendered[len]==0u);
        BCtx c;
        s32 status=URL_PARSE((const char*)raw,&c);
        if(status==0) {
            CHECK(c.host[0]!=0u);
            CHECK(c.port>0u && c.port<=65535u);
            CHECK(c.path[0]=='/');
        }
    }
}

int main(void) {
    urls();http();dns_packets();route_security();html();links();deterministic_mutations();
    fprintf(stderr,"BrowserRaf negative fixtures: %u checks, %u failed\n",checks,failures);
    return failures?1:0;
}
