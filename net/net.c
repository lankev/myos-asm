/* net/net.c — Minimal TCP/IP network stack for MyOS
   RTL8139 NIC + ARP + IP + UDP + DNS + TCP + HTTP     */
#include "net.h"
#include <string.h>
#include <stdint.h>

/* ---- I/O helpers ---- */
static inline uint8_t  inb(uint16_t p){uint8_t  v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint16_t inw(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint32_t inl(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outb(uint16_t p,uint8_t  v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline void outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline void outl(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static void ndelay(uint32_t n){for(uint32_t i=0;i<n;i++)__asm__ volatile("nop");}

/* ---- Byte order ---- */
static inline uint16_t htons(uint16_t x){return (uint16_t)((x>>8)|(x<<8));}
#define ntohs htons
static inline uint32_t htonl(uint32_t x){return(x>>24)|((x>>8)&0xFF00)|((x<<8)&0xFF0000)|(x<<24);}
#define ntohl htonl

/* ---- PCI ---- */
static uint32_t pci_rd(uint8_t b,uint8_t d,uint8_t f,uint8_t r){
    outl(0xCF8,0x80000000|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(r&0xFC));
    return inl(0xCFC);
}
static void pci_wr(uint8_t b,uint8_t d,uint8_t f,uint8_t r,uint32_t v){
    outl(0xCF8,0x80000000|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(r&0xFC));
    outl(0xCFC,v);
}

/* ---- RTL8139 registers ---- */
#define R_IDR0   0x00
#define R_TSD0   0x10
#define R_TSAD0  0x20
#define R_RBST   0x30
#define R_CMD    0x37
#define R_CAPR   0x38
#define R_IMR    0x3C
#define R_ISR    0x3E
#define R_TCR    0x40
#define R_RCR    0x44
#define R_CFG1   0x52

static uint16_t _io;
static uint8_t  _mac[6];
static uint8_t  _rxbuf[8192+16] __attribute__((aligned(4)));
static uint8_t  _txbuf[4][1800] __attribute__((aligned(4)));
static int      _txcur;
static uint32_t _rxcur;
int net_ok=0;

/* Hardcoded QEMU SLIRP */
#define MY_IP  0x0A00020F  /* 10.0.2.15  */
#define GW_IP  0x0A000202  /* 10.0.2.2   */
#define DNS_IP 0x0A000203  /* 10.0.2.3   */
static uint8_t _gwmac[6];
static const uint8_t _bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* ---- RTL8139 ---- */
static int rtl_find(void){
    for(uint8_t d=0;d<32;d++){
        uint32_t v=pci_rd(0,d,0,0);
        if((v&0xFFFF)==0x10EC&&(v>>16)==0x8139){
            pci_wr(0,d,0,4,pci_rd(0,d,0,4)|0x04); /* bus master */
            _io=(uint16_t)(pci_rd(0,d,0,0x10)&0xFFFC);
            return 1;
        }
    }
    return 0;
}
static void rtl_init(void){
    outb(_io+R_CFG1,0);           /* power on */
    outb(_io+R_CMD,0x10);         /* reset */
    while(inb(_io+R_CMD)&0x10)ndelay(1000);
    for(int i=0;i<6;i++)_mac[i]=inb(_io+R_IDR0+i);
    outl(_io+R_RBST,(uint32_t)_rxbuf);
    outw(_io+R_IMR,0);
    outw(_io+R_ISR,0xFFFF);
    outl(_io+R_RCR,0xF|(1<<7)|(7<<8)); /* AB|AM|APM|AAP + WRAP + unlimited DMA */
    outl(_io+R_TCR,(3<<24)|(6<<8));
    outb(_io+R_CMD,0x0C);         /* enable TX+RX */
    _rxcur=0;_txcur=0;
}
static void rtl_tx(const void*p,uint16_t len){
    memcpy(_txbuf[_txcur],p,len);
    outl(_io+R_TSAD0+_txcur*4,(uint32_t)_txbuf[_txcur]);
    outl(_io+R_TSD0+_txcur*4,(uint32_t)len);
    uint32_t t=0;
    while(!(inl(_io+R_TSD0+_txcur*4)&0x8000)&&t<2000000)t++;
    _txcur=(_txcur+1)&3;
}
static uint16_t rtl_rx(void*buf,uint32_t iters){
    for(uint32_t t=0;t<iters;t++){
        if(inb(_io+R_CMD)&0x01){ndelay(10);continue;} /* BUFE: buffer empty */
        uint8_t*p=_rxbuf+_rxcur;
        uint16_t stat=*(uint16_t*)p;
        uint16_t plen=*(uint16_t*)(p+2);
        if(!(stat&1)||plen<14||plen>1514){
            _rxcur=(_rxcur+4+3)&~3U;
            if(_rxcur>=8192)_rxcur=0;
            outw(_io+R_CAPR,(uint16_t)(_rxcur-16));
            continue;
        }
        uint16_t dlen=(uint16_t)(plen-4);
        memcpy(buf,p+4,dlen);
        _rxcur=(_rxcur+4+plen+3)&~3U;
        if(_rxcur>=8192)_rxcur-=8192;
        outw(_io+R_CAPR,(uint16_t)(_rxcur-16));
        return dlen;
    }
    return 0;
}

/* ---- Protocol headers ---- */
typedef struct __attribute__((packed)){uint8_t d[6],s[6];uint16_t t;}EH;
typedef struct __attribute__((packed)){
    uint16_t ht,pt;uint8_t hl,pl;uint16_t op;
    uint8_t sha[6];uint32_t spa;uint8_t tha[6];uint32_t tpa;
}AH;
typedef struct __attribute__((packed)){
    uint8_t ihl,tos;uint16_t len,id,fl;uint8_t ttl,pr;uint16_t cs;uint32_t s,d;
}IH;
typedef struct __attribute__((packed)){uint16_t sp,dp,ln,cs;}UH;
typedef struct __attribute__((packed)){
    uint16_t sp,dp;uint32_t sq,ak;uint8_t off,fl;uint16_t wn,cs,ug;
}TH;

/* ---- Checksum ---- */
static uint16_t cksum(const void*b,int n){
    const uint16_t*p=(const uint16_t*)b;uint32_t s=0;
    while(n>1){s+=*p++;n-=2;}
    if(n)s+=*(uint8_t*)p;
    while(s>>16)s=(s&0xFFFF)+(s>>16);
    return (uint16_t)~s;
}
static uint16_t tcp_cs(uint32_t si,uint32_t di,const void*seg,uint16_t tl){
    uint8_t ph[12];*(uint32_t*)(ph)=si;*(uint32_t*)(ph+4)=di;
    ph[8]=0;ph[9]=6;*(uint16_t*)(ph+10)=htons(tl);
    uint8_t tmp[1500];memcpy(tmp,ph,12);memcpy(tmp+12,seg,tl);
    return cksum(tmp,12+tl);
}
static uint16_t udp_cs(uint32_t si,uint32_t di,const void*seg,uint16_t ul){
    uint8_t ph[12];*(uint32_t*)(ph)=si;*(uint32_t*)(ph+4)=di;
    ph[8]=0;ph[9]=17;*(uint16_t*)(ph+10)=htons(ul);
    uint8_t tmp[600];memcpy(tmp,ph,12);memcpy(tmp+12,seg,ul);
    return cksum(tmp,12+ul);
}

/* ---- ARP ---- */
static void arp_req(uint32_t tip){
    uint8_t fr[60];memset(fr,0,60);
    EH*e=(EH*)fr;memcpy(e->d,_bcast,6);memcpy(e->s,_mac,6);e->t=htons(0x0806);
    AH*a=(AH*)(fr+14);
    a->ht=htons(1);a->pt=htons(0x0800);a->hl=6;a->pl=4;a->op=htons(1);
    memcpy(a->sha,_mac,6);a->spa=htonl(MY_IP);
    memset(a->tha,0,6);a->tpa=htonl(tip);
    rtl_tx(fr,60);
}
static int arp_resolve(uint32_t ip,uint8_t*mac){
    arp_req(ip);
    uint8_t buf[1500];
    for(int i=0;i<500000;i++){
        uint16_t l=rtl_rx(buf,10);
        if(l<42)continue;
        EH*e=(EH*)buf;if(ntohs(e->t)!=0x0806)continue;
        AH*a=(AH*)(buf+14);
        if(ntohs(a->op)==2&&ntohl(a->spa)==ip){memcpy(mac,a->sha,6);return 1;}
    }
    return 0;
}

/* ---- DNS ---- */
static int dns_resolve(const char*host,uint32_t*ip){
    uint8_t q[512];memset(q,0,12);
    q[0]=0x13;q[1]=0x37;q[2]=0x01;q[5]=1;
    int pos=12;
    for(const char*p=host;;){
        const char*dot=p;while(*dot&&*dot!='.')dot++;
        int sl=(int)(dot-p);q[pos++]=(uint8_t)sl;
        for(int i=0;i<sl;i++)q[pos++]=(uint8_t)p[i];
        p=dot;if(!*p)break;p++;
    }
    q[pos++]=0;q[pos++]=0;q[pos++]=1;q[pos++]=0;q[pos++]=1;
    uint16_t ql=(uint16_t)pos;
    /* UDP packet */
    uint8_t udp[8+512];
    UH*u=(UH*)udp;u->sp=htons(12345);u->dp=htons(53);
    u->ln=htons((uint16_t)(8+ql));u->cs=0;
    memcpy(udp+8,q,ql);
    u->cs=udp_cs(htonl(MY_IP),htonl(DNS_IP),udp,(uint16_t)(8+ql));
    /* IP+Eth frame */
    uint8_t fr[600];
    EH*e=(EH*)fr;memcpy(e->d,_gwmac,6);memcpy(e->s,_mac,6);e->t=htons(0x0800);
    IH*iph=(IH*)(fr+14);
    iph->ihl=0x45;iph->tos=0;iph->len=htons((uint16_t)(20+8+ql));
    static uint16_t id2=100;iph->id=htons(id2++);iph->fl=0;
    iph->ttl=64;iph->pr=17;iph->cs=0;iph->s=htonl(MY_IP);iph->d=htonl(DNS_IP);
    iph->cs=cksum(iph,20);
    memcpy(fr+34,udp,8+ql);
    rtl_tx(fr,(uint16_t)(14+20+8+ql));
    /* Wait for reply */
    uint8_t rb[1500];
    for(int i=0;i<1000000;i++){
        uint16_t l=rtl_rx(rb,5);
        if(l<42)continue;
        EH*re=(EH*)rb;if(ntohs(re->t)!=0x0800)continue;
        IH*ri=(IH*)(rb+14);if(ri->pr!=17)continue;
        UH*ru=(UH*)(rb+34);if(ntohs(ru->dp)!=12345)continue;
        uint8_t*dns=rb+42;int dlen=l-42;
        if(!(dns[2]&0x80))continue; /* not a response */
        int dpos=12;
        int qc=(dns[4]<<8)|dns[5];
        for(int j=0;j<qc&&dpos<dlen;j++){
            while(dpos<dlen&&dns[dpos]!=0){
                if((dns[dpos]&0xC0)==0xC0){dpos+=2;break;}
                dpos+=dns[dpos]+1;
            }
            if(dns[dpos]!=0){}else{dpos++;} /* null label */
            dpos+=4; /* type+class */
        }
        int ac=(dns[6]<<8)|dns[7];
        for(int j=0;j<ac&&dpos+12<=dlen;j++){
            if((dns[dpos]&0xC0)==0xC0)dpos+=2;
            else{while(dpos<dlen&&dns[dpos])dpos+=dns[dpos]+1;dpos++;}
            uint16_t rt=(uint16_t)((dns[dpos]<<8)|dns[dpos+1]);
            uint16_t rdl=(uint16_t)((dns[dpos+8]<<8)|dns[dpos+9]);
            dpos+=10;
            if(rt==1&&rdl==4){
                *ip=((uint32_t)dns[dpos]<<24)|((uint32_t)dns[dpos+1]<<16)|
                    ((uint32_t)dns[dpos+2]<<8)|dns[dpos+3];
                return 1;
            }
            dpos+=rdl;
        }
    }
    return 0;
}

/* ---- TCP ---- */
static uint32_t _dst_ip;
static uint16_t _dst_port,_src_port;
static uint32_t _seq,_ackn;
static uint8_t  _dst_mac[6];
static uint16_t _next_port=49152;

static void tcp_tx(uint8_t fl,const void*data,uint16_t dl){
    uint8_t fr[1500];
    EH*e=(EH*)fr;memcpy(e->d,_dst_mac,6);memcpy(e->s,_mac,6);e->t=htons(0x0800);
    IH*ip=(IH*)(fr+14);
    uint16_t tl=(uint16_t)(20+dl);
    ip->ihl=0x45;ip->tos=0;ip->len=htons((uint16_t)(20+tl));
    static uint16_t tid=200;ip->id=htons(tid++);ip->fl=0;
    ip->ttl=64;ip->pr=6;ip->cs=0;ip->s=htonl(MY_IP);ip->d=htonl(_dst_ip);
    ip->cs=cksum(ip,20);
    TH*t=(TH*)(fr+34);
    t->sp=htons(_src_port);t->dp=htons(_dst_port);
    t->sq=htonl(_seq);t->ak=htonl(_ackn);
    t->off=0x50;t->fl=fl;t->wn=htons(8192);t->cs=0;t->ug=0;
    if(dl>0)memcpy(fr+54,data,dl);
    t->cs=tcp_cs(htonl(MY_IP),htonl(_dst_ip),t,tl);
    rtl_tx(fr,(uint16_t)(14+20+tl));
}

/* Returns data length; updates _ackn; sets *ofl to received flags */
static uint16_t tcp_rx(uint8_t*ofl,void*data,uint16_t bsz,uint32_t iters){
    uint8_t buf[1500];*ofl=0;
    for(uint32_t t=0;t<iters;t++){
        uint16_t l=rtl_rx(buf,5);
        if(l<54){ndelay(5);continue;}
        EH*e=(EH*)buf;if(ntohs(e->t)!=0x0800)continue;
        IH*ip=(IH*)(buf+14);if(ip->pr!=6)continue;
        if(ntohl(ip->s)!=_dst_ip)continue;
        TH*th=(TH*)(buf+34);
        if(ntohs(th->dp)!=_src_port||ntohs(th->sp)!=_dst_port)continue;
        *ofl=th->fl;
        uint16_t doff=(uint16_t)((th->off>>4)*4);
        uint16_t iplen=ntohs(ip->len);
        uint16_t dl=(uint16_t)(iplen-20-doff);
        if(dl>bsz)dl=bsz;
        if(dl>0&&data)memcpy(data,buf+34+doff,dl);
        uint32_t na=ntohl(th->sq)+dl;
        if(th->fl&0x02)na++; /* SYN */
        if(th->fl&0x01)na++; /* FIN */
        _ackn=na;
        return dl;
    }
    return 0;
}

static int tcp_connect(uint32_t ip,uint16_t port){
    _dst_ip=ip;_dst_port=port;_src_port=_next_port++;
    _seq=0xABCD1234;_ackn=0;
    memcpy(_dst_mac,_gwmac,6);
    tcp_tx(0x02,0,0); /* SYN */
    _seq++;
    uint8_t fl;
    for(int i=0;i<2000000;i++){
        tcp_rx(&fl,0,0,1);
        if(fl&0x12){tcp_tx(0x10,0,0);return 1;} /* SYN+ACK -> ACK */
        if(fl&0x04)return 0; /* RST */
    }
    return 0;
}

/* ---- HTTP ---- */
static int http_get_raw(const char*host,const char*path,char*buf,int bsz){
    /* Build GET request */
    char req[512];int rl=0;
    #define A(s) {const char*_s=(s);while(*_s&&rl<500)req[rl++]=*_s++;}
    A("GET ") A(path) A(" HTTP/1.0\r\nHost: ") A(host) A("\r\nUser-Agent: MyBrowser/1.0\r\nConnection: close\r\n\r\n")
    #undef A
    req[rl]='\0';
    tcp_tx(0x18,(uint8_t*)req,(uint16_t)rl); /* PSH+ACK */
    _seq+=(uint32_t)rl;
    int total=0;uint8_t fl;
    uint8_t rb[1460];
    for(int i=0;i<200000&&total<bsz-1;i++){
        uint16_t dl=tcp_rx(&fl,rb,sizeof(rb),100);
        if(dl>0){
            tcp_tx(0x10,0,0); /* ACK */
            int sp=bsz-1-total;if(dl>sp)dl=(uint16_t)sp;
            memcpy(buf+total,rb,dl);total+=dl;
        }
        if(fl&0x01){tcp_tx(0x11,0,0);break;} /* FIN */
        if(fl&0x04)break; /* RST */
    }
    buf[total]='\0';return total;
}

/* ---- Public API ---- */
int net_init(void){
    if(!rtl_find())return 0;
    rtl_init();
    if(!arp_resolve(GW_IP,_gwmac))return 0;
    net_ok=1;return 1;
}

int net_http_get(const char*url,char*buf,int bsz){
    if(!net_ok)return -1;
    /* Strip http:// */
    const char*p=url;
    if(p[0]=='h'&&p[4]==':'&&p[5]=='/'&&p[6]=='/')p+=7;
    else if(p[0]=='h'&&p[5]==':'&&p[6]=='/'&&p[7]=='/')p+=8; /* https:// - try anyway */
    /* Extract host */
    char host[128];int hl=0;
    while(*p&&*p!='/'&&hl<127)host[hl++]=*p++;
    host[hl]='\0';
    const char*path=(*p=='/')?p:"/";
    /* Resolve */
    uint32_t ip;
    if(!dns_resolve(host,&ip)){
        int n=0;const char*e="Erreur: echec DNS pour ";
        while(*e&&n<bsz-1)buf[n++]=*e++;
        for(int i=0;host[i]&&n<bsz-1;i++)buf[n++]=host[i];
        buf[n]='\0';return n;
    }
    /* Connect */
    if(!tcp_connect(ip,80)){
        const char*e="Erreur: connexion TCP echouee";
        int n=0;while(*e&&n<bsz-1)buf[n++]=*e++;
        buf[n]='\0';return n;
    }
    return http_get_raw(host,path,buf,bsz);
}
