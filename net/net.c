/* net/net.c — Minimal TCP/IP network stack for MyOS
   RTL8139 NIC + ARP + IP + UDP + DNS + TCP + HTTP + DHCP     */
#include "net.h"
#include "string.h"
#include "stdint.h"

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
uint8_t  net_mac[6];
uint32_t net_my_ip   = 0x0A00020F;  /* 10.0.2.15  (fallback static) */
uint32_t net_gw_ip   = 0x0A000202;  /* 10.0.2.2   */
uint32_t net_dns_ip  = 0x0A000203;  /* 10.0.2.3   */
uint32_t net_netmask = 0xFFFFFF00;  /* 255.255.255.0 */
int      net_dhcp_ok = 0;
int      net_ok      = 0;

/* Aliases so existing code still compiles unchanged */
#define MY_IP  net_my_ip
#define GW_IP  net_gw_ip
#define DNS_IP net_dns_ip

static uint8_t  _rxbuf[8192+16] __attribute__((aligned(4)));
static uint8_t  _txbuf[4][1800] __attribute__((aligned(4)));
static int      _txcur;
static uint32_t _rxcur;
static uint8_t  _gwmac[6];
static const uint8_t _bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* ---- RTL8139 ---- */
static int rtl_find(void){
    for(uint8_t d=0;d<32;d++){
        uint32_t v=pci_rd(0,d,0,0);
        if((v&0xFFFF)==0x10EC&&(v>>16)==0x8139){
            pci_wr(0,d,0,4,pci_rd(0,d,0,4)|0x04);
            _io=(uint16_t)(pci_rd(0,d,0,0x10)&0xFFFC);
            return 1;
        }
    }
    return 0;
}
static void rtl_init(void){
    outb(_io+R_CFG1,0);
    outb(_io+R_CMD,0x10);
    while(inb(_io+R_CMD)&0x10)ndelay(1000);
    for(int i=0;i<6;i++)net_mac[i]=inb(_io+R_IDR0+i);
    outl(_io+R_RBST,(uint32_t)_rxbuf);
    outw(_io+R_IMR,0);
    outw(_io+R_ISR,0xFFFF);
    outl(_io+R_RCR,0xF|(1<<7)|(7<<8));
    outl(_io+R_TCR,(3<<24)|(6<<8));
    outb(_io+R_CMD,0x0C);
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
        if(inb(_io+R_CMD)&0x01){ndelay(10);continue;}
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
typedef struct __attribute__((packed)){uint8_t type,code;uint16_t cs,id,seq;}ICH;

/* DHCP/BOOTP packet (300 bytes fixed per RFC 1497) */
typedef struct __attribute__((packed)){
    uint8_t  op,htype,hlen,hops;
    uint32_t xid;
    uint16_t secs,flags;
    uint32_t ciaddr,yiaddr,siaddr,giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint8_t  options[64];
} DHCP_PKT;

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
    EH*e=(EH*)fr;memcpy(e->d,_bcast,6);memcpy(e->s,net_mac,6);e->t=htons(0x0806);
    AH*a=(AH*)(fr+14);
    a->ht=htons(1);a->pt=htons(0x0800);a->hl=6;a->pl=4;a->op=htons(1);
    memcpy(a->sha,net_mac,6);a->spa=htonl(MY_IP);
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
    uint8_t udp[8+512];
    UH*u=(UH*)udp;u->sp=htons(12345);u->dp=htons(53);
    u->ln=htons((uint16_t)(8+ql));u->cs=0;
    memcpy(udp+8,q,ql);
    u->cs=udp_cs(htonl(MY_IP),htonl(DNS_IP),udp,(uint16_t)(8+ql));
    uint8_t fr[600];
    EH*e=(EH*)fr;memcpy(e->d,_gwmac,6);memcpy(e->s,net_mac,6);e->t=htons(0x0800);
    IH*iph=(IH*)(fr+14);
    iph->ihl=0x45;iph->tos=0;iph->len=htons((uint16_t)(20+8+ql));
    static uint16_t id2=100;iph->id=htons(id2++);iph->fl=0;
    iph->ttl=64;iph->pr=17;iph->cs=0;iph->s=htonl(MY_IP);iph->d=htonl(DNS_IP);
    iph->cs=cksum(iph,20);
    memcpy(fr+34,udp,8+ql);
    uint8_t rb[1500];
    /* UDP peu fiable en polling : on retransmet la requete a chaque tentative */
    for(int attempt=0;attempt<8;attempt++){
        rtl_tx(fr,(uint16_t)(14+20+8+ql));
        for(int i=0;i<60000;i++){
            uint16_t l=rtl_rx(rb,5);
            if(l<42)continue;
            EH*re=(EH*)rb;if(ntohs(re->t)!=0x0800)continue;
            IH*ri=(IH*)(rb+14);if(ri->pr!=17)continue;
            UH*ru=(UH*)(rb+34);if(ntohs(ru->dp)!=12345)continue;
            uint8_t*dns=rb+42;int dlen=l-42;
            if(!(dns[2]&0x80))continue;
            int dpos=12;
            int qc=(dns[4]<<8)|dns[5];
            for(int j=0;j<qc&&dpos<dlen;j++){
                while(dpos<dlen&&dns[dpos]!=0){
                    if((dns[dpos]&0xC0)==0xC0){dpos+=2;break;}
                    dpos+=dns[dpos]+1;
                }
                if(dns[dpos]!=0){}else{dpos++;}
                dpos+=4;
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
    }
    return 0;
}

/* ---- TCP ---- */
static uint32_t _dst_ip;
static uint16_t _dst_port,_src_port;
static uint32_t _seq,_ackn;
static uint32_t _rcv_nxt;     /* prochain numero de sequence attendu (reassemblage) */
static uint8_t  _dst_mac[6];
static uint16_t _next_port=49152;

static void tcp_tx(uint8_t fl,const void*data,uint16_t dl){
    uint8_t fr[1500];
    EH*e=(EH*)fr;memcpy(e->d,_dst_mac,6);memcpy(e->s,net_mac,6);e->t=htons(0x0800);
    IH*ip=(IH*)(fr+14);
    uint16_t tl=(uint16_t)(20+dl);
    ip->ihl=0x45;ip->tos=0;ip->len=htons((uint16_t)(20+tl));
    static uint16_t tid=200;ip->id=htons(tid++);ip->fl=0;
    ip->ttl=64;ip->pr=6;ip->cs=0;ip->s=htonl(MY_IP);ip->d=htonl(_dst_ip);
    ip->cs=cksum(ip,20);
    TH*t=(TH*)(fr+34);
    t->sp=htons(_src_port);t->dp=htons(_dst_port);
    t->sq=htonl(_seq);t->ak=htonl(_ackn);
    /* fenetre reduite : le serveur n'envoie pas plus que ce que l'anneau RX
       de 8 Ko peut absorber pendant qu'on traite (evite les pertes en rafale) */
    t->off=0x50;t->fl=fl;t->wn=htons(2920);t->cs=0;t->ug=0;
    if(dl>0)memcpy(fr+54,data,dl);
    t->cs=tcp_cs(htonl(MY_IP),htonl(_dst_ip),t,tl);
    rtl_tx(fr,(uint16_t)(14+20+tl));
}
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
        uint16_t doff=(uint16_t)((th->off>>4)*4);
        uint16_t iplen=ntohs(ip->len);
        uint16_t dl=(uint16_t)(iplen-20-doff);
        if(dl>bsz)dl=bsz;
        uint32_t seg=ntohl(th->sq);
        *ofl=th->fl;
        /* SYN-ACK : initialise le numero de sequence attendu */
        if(th->fl&0x02){_rcv_nxt=seg+1;_ackn=_rcv_nxt;return 0;}
        if(dl>0){
            if(seg!=_rcv_nxt){        /* doublon / hors-ordre : ignore, re-ACK */
                _ackn=_rcv_nxt;continue;
            }
            if(data)memcpy(data,buf+34+doff,dl);
            _rcv_nxt=seg+dl;
            if(th->fl&0x01)_rcv_nxt++;   /* FIN accompagnant des donnees */
            _ackn=_rcv_nxt;
            return dl;
        }
        /* pas de donnees : FIN eventuel dans l'ordre */
        if(th->fl&0x01){if(seg==_rcv_nxt)_rcv_nxt++;_ackn=_rcv_nxt;}
        return 0;
    }
    return 0;
}
static int tcp_connect(uint32_t ip,uint16_t port){
    _dst_ip=ip;_dst_port=port;_src_port=_next_port++;
    _seq=0xABCD1234;_ackn=0;
    memcpy(_dst_mac,_gwmac,6);
    tcp_tx(0x02,0,0);
    _seq++;
    uint8_t fl;
    for(int i=0;i<2000000;i++){
        tcp_rx(&fl,0,0,1);
        if(fl&0x12){tcp_tx(0x10,0,0);return 1;}
        if(fl&0x04)return 0;
    }
    return 0;
}

/* ---- HTTP ---- */
static int http_get_raw(const char*host,const char*path,char*buf,int bsz){
    char req[512];int rl=0;
    #define A(s) {const char*_s=(s);while(*_s&&rl<500)req[rl++]=*_s++;}
    A("GET ") A(path) A(" HTTP/1.0\r\nHost: ") A(host) A("\r\nUser-Agent: MyBrowser/1.0\r\nConnection: close\r\n\r\n")
    #undef A
    req[rl]='\0';
    tcp_tx(0x18,(uint8_t*)req,(uint16_t)rl);
    _seq+=(uint32_t)rl;
    int total=0;uint8_t fl;
    uint8_t rb[1460];
    for(int i=0;i<200000&&total<bsz-1;i++){
        uint16_t dl=tcp_rx(&fl,rb,sizeof(rb),100);
        if(dl>0){
            tcp_tx(0x10,0,0);
            int sp=bsz-1-total;if(dl>sp)dl=(uint16_t)sp;
            memcpy(buf+total,rb,dl);total+=dl;
        }
        if(fl&0x01){tcp_tx(0x11,0,0);break;}
        if(fl&0x04)break;
    }
    buf[total]='\0';return total;
}

/* ---- DHCP ---- */
static int _dhcp_opt(const uint8_t*opts,int len,uint8_t code,uint8_t*out,int maxout){
    int i=4; /* skip magic cookie */
    while(i<len){
        if(opts[i]==255)break;
        if(opts[i]==0){i++;continue;}
        uint8_t c=opts[i],l=opts[i+1];
        if(c==code){int n=l<maxout?l:maxout;memcpy(out,opts+i+2,(size_t)n);return n;}
        i+=2+l;
    }
    return 0;
}

static void _dhcp_tx(uint8_t msg_type,uint32_t req_ip,uint32_t srv_ip){
    DHCP_PKT pkt;
    memset(&pkt,0,sizeof(pkt));
    pkt.op=1;pkt.htype=1;pkt.hlen=6;
    pkt.xid=htonl(0xDEADBEEF);
    pkt.flags=htons(0x8000); /* broadcast flag */
    memcpy(pkt.chaddr,net_mac,6);
    /* DHCP magic cookie */
    pkt.options[0]=99;pkt.options[1]=130;pkt.options[2]=83;pkt.options[3]=99;
    int o=4;
    pkt.options[o++]=53;pkt.options[o++]=1;pkt.options[o++]=msg_type;
    if(req_ip){
        pkt.options[o++]=50;pkt.options[o++]=4;
        pkt.options[o++]=(uint8_t)(req_ip>>24);pkt.options[o++]=(uint8_t)(req_ip>>16);
        pkt.options[o++]=(uint8_t)(req_ip>>8); pkt.options[o++]=(uint8_t)(req_ip);
    }
    if(srv_ip){
        pkt.options[o++]=54;pkt.options[o++]=4;
        pkt.options[o++]=(uint8_t)(srv_ip>>24);pkt.options[o++]=(uint8_t)(srv_ip>>16);
        pkt.options[o++]=(uint8_t)(srv_ip>>8); pkt.options[o++]=(uint8_t)(srv_ip);
    }
    pkt.options[o++]=55;pkt.options[o++]=3;
    pkt.options[o++]=1; /* subnet mask */
    pkt.options[o++]=3; /* router */
    pkt.options[o++]=6; /* DNS */
    pkt.options[o]=255; /* end */
    /* Build frame: Eth(14) + IP(20) + UDP(8) + DHCP(300) = 342 */
    uint16_t udp_len=(uint16_t)(8+sizeof(DHCP_PKT));
    uint8_t fr[14+20+8+300];
    EH*e=(EH*)fr;
    memcpy(e->d,_bcast,6);memcpy(e->s,net_mac,6);e->t=htons(0x0800);
    IH*ip=(IH*)(fr+14);
    ip->ihl=0x45;ip->tos=0;ip->len=htons((uint16_t)(20+udp_len));
    static uint16_t dhcp_id=300;ip->id=htons(dhcp_id++);ip->fl=0;
    ip->ttl=64;ip->pr=17;ip->cs=0;
    ip->s=0;            /* 0.0.0.0 — no IP yet */
    ip->d=0xFFFFFFFF;   /* 255.255.255.255 broadcast */
    ip->cs=cksum(ip,20);
    UH*u=(UH*)(fr+34);
    u->sp=htons(68);u->dp=htons(67);u->ln=htons(udp_len);u->cs=0;
    memcpy(fr+42,&pkt,sizeof(DHCP_PKT));
    rtl_tx(fr,(uint16_t)(14+20+udp_len));
}

int net_dhcp(void){
    _dhcp_tx(1,0,0); /* DHCPDISCOVER */
    uint8_t buf[600];
    uint32_t offer_ip=0,srv_ip=0;
    /* Wait for OFFER */
    for(int i=0;i<3000000;i++){
        uint16_t l=rtl_rx(buf,5);
        if(l<14+20+8+240)continue;
        EH*e=(EH*)buf;if(ntohs(e->t)!=0x0800)continue;
        IH*ip=(IH*)(buf+14);if(ip->pr!=17)continue;
        UH*u=(UH*)(buf+34);if(ntohs(u->dp)!=68)continue;
        DHCP_PKT*d=(DHCP_PKT*)(buf+42);
        if(d->op!=2)continue;
        if(d->xid!=htonl(0xDEADBEEF))continue;
        if(d->options[0]!=99||d->options[1]!=130)continue;
        uint8_t mtype=0;
        _dhcp_opt(d->options,64,53,&mtype,1);
        if(mtype!=2)continue; /* not OFFER */
        offer_ip=ntohl(d->yiaddr);
        uint8_t sv[4];
        if(_dhcp_opt(d->options,64,54,sv,4)==4)
            srv_ip=((uint32_t)sv[0]<<24)|((uint32_t)sv[1]<<16)|((uint32_t)sv[2]<<8)|sv[3];
        break;
    }
    if(!offer_ip)return 0;
    _dhcp_tx(3,offer_ip,srv_ip); /* DHCPREQUEST */
    /* Wait for ACK */
    for(int i=0;i<3000000;i++){
        uint16_t l=rtl_rx(buf,5);
        if(l<14+20+8+240)continue;
        EH*e=(EH*)buf;if(ntohs(e->t)!=0x0800)continue;
        IH*ip=(IH*)(buf+14);if(ip->pr!=17)continue;
        UH*u=(UH*)(buf+34);if(ntohs(u->dp)!=68)continue;
        DHCP_PKT*d=(DHCP_PKT*)(buf+42);
        if(d->op!=2)continue;
        if(d->xid!=htonl(0xDEADBEEF))continue;
        if(d->options[0]!=99||d->options[1]!=130)continue;
        uint8_t mtype=0;
        _dhcp_opt(d->options,64,53,&mtype,1);
        if(mtype!=5)continue; /* not ACK */
        net_my_ip=ntohl(d->yiaddr);
        uint8_t tmp[4];
        if(_dhcp_opt(d->options,64,1,tmp,4)==4)
            net_netmask=((uint32_t)tmp[0]<<24)|((uint32_t)tmp[1]<<16)|((uint32_t)tmp[2]<<8)|tmp[3];
        if(_dhcp_opt(d->options,64,3,tmp,4)==4)
            net_gw_ip=((uint32_t)tmp[0]<<24)|((uint32_t)tmp[1]<<16)|((uint32_t)tmp[2]<<8)|tmp[3];
        if(_dhcp_opt(d->options,64,6,tmp,4)==4)
            net_dns_ip=((uint32_t)tmp[0]<<24)|((uint32_t)tmp[1]<<16)|((uint32_t)tmp[2]<<8)|tmp[3];
        net_dhcp_ok=1;
        return 1;
    }
    return 0;
}

int net_reconnect(void){
    if(!_io)return 0;
    net_dhcp_ok=0;
    if(net_dhcp()&&arp_resolve(net_gw_ip,_gwmac)){net_ok=1;return 1;}
    net_ok=0;return 0;
}

/* ---- ICMP ping ---- */
int net_ping_gw(void){
    if(!net_ok)return 0;
    uint8_t fr[42];
    EH*e=(EH*)fr;memcpy(e->d,_gwmac,6);memcpy(e->s,net_mac,6);e->t=htons(0x0800);
    IH*ip=(IH*)(fr+14);
    ip->ihl=0x45;ip->tos=0;ip->len=htons(28);
    static uint16_t pid=400;ip->id=htons(pid++);ip->fl=0;
    ip->ttl=64;ip->pr=1;ip->cs=0;ip->s=htonl(net_my_ip);ip->d=htonl(net_gw_ip);
    ip->cs=cksum(ip,20);
    ICH*ic=(ICH*)(fr+34);
    ic->type=8;ic->code=0;ic->cs=0;ic->id=htons(0x1234);ic->seq=htons(1);
    ic->cs=cksum(ic,8);
    rtl_tx(fr,42);
    uint8_t buf[256];
    for(int i=0;i<1000000;i++){
        uint16_t l=rtl_rx(buf,5);
        if(l<42)continue;
        EH*re=(EH*)buf;if(ntohs(re->t)!=0x0800)continue;
        IH*ri=(IH*)(buf+14);if(ri->pr!=1)continue;
        if(ntohl(ri->s)!=net_gw_ip)continue;
        ICH*ric=(ICH*)(buf+34);
        if(ric->type==0&&ntohs(ric->id)==0x1234)return 1;
    }
    return 0;
}

/* ---- API publiques supplementaires (vraies trames sur le cable) ---- */
int net_arp(uint32_t ip,uint8_t*mac){
    if(!net_ok)return 0;
    return arp_resolve(ip,mac);
}
int net_dns(const char*host,uint32_t*ip){
    if(!net_ok)return 0;
    return dns_resolve(host,ip);
}
/* Ping ICMP generalise, TTL parametrable.
   Retour: 1=echo reply, 2=time-exceeded (hop intermediaire), 0=timeout.
   rtt_ticks: duree reelle en ticks PIT (18.2 Hz). hop_ip: source de la reponse. */
int net_ping(uint32_t dip,uint8_t ttl,uint32_t*rtt_ticks,uint32_t*hop_ip){
    if(!net_ok)return 0;
    uint8_t fr[42];
    EH*e=(EH*)fr;memcpy(e->d,_gwmac,6);memcpy(e->s,net_mac,6);e->t=htons(0x0800);
    IH*ih=(IH*)(fr+14);
    ih->ihl=0x45;ih->tos=0;ih->len=htons(28);
    static uint16_t pid=800;pid++;
    ih->id=htons(pid);ih->fl=0;
    ih->ttl=ttl;ih->pr=1;ih->cs=0;ih->s=htonl(net_my_ip);ih->d=htonl(dip);
    ih->cs=cksum(ih,20);
    ICH*ic=(ICH*)(fr+34);
    ic->type=8;ic->code=0;ic->cs=0;ic->id=htons(0x4D59);ic->seq=htons(pid);
    ic->cs=cksum(ic,8);
    volatile uint32_t* ticks=(volatile uint32_t*)0x046C;
    uint32_t t0=*ticks;
    rtl_tx(fr,42);
    uint8_t buf[600];
    for(int i=0;i<1500000;i++){
        uint16_t l=rtl_rx(buf,5);
        if(l<42)continue;
        EH*re=(EH*)buf;if(ntohs(re->t)!=0x0800)continue;
        IH*ri=(IH*)(buf+14);if(ri->pr!=1)continue;
        ICH*ric=(ICH*)(buf+34);
        if(ric->type==0&&ntohs(ric->id)==0x4D59&&ntohl(ri->s)==dip){
            if(rtt_ticks)*rtt_ticks=*ticks-t0;
            if(hop_ip)*hop_ip=ntohl(ri->s);
            return 1;
        }
        if(ric->type==11){   /* TTL expire en route */
            if(rtt_ticks)*rtt_ticks=*ticks-t0;
            if(hop_ip)*hop_ip=ntohl(ri->s);
            return 2;
        }
    }
    return 0;
}

/* ---- Public API ---- */
int net_init(void){
    if(!rtl_find())return 0;
    rtl_init();
    /* Try DHCP first; fall back to static defaults */
    if(!net_dhcp()){
        net_my_ip  =0x0A00020F;
        net_gw_ip  =0x0A000202;
        net_dns_ip =0x0A000203;
        net_netmask=0xFFFFFF00;
        net_dhcp_ok=0;
    }
    if(!arp_resolve(net_gw_ip,_gwmac))return 0;
    net_ok=1;return 1;
}

/* ============================================================
 * HTTPS : glue TLS <-> TCP. Fournit send/recv/rng a net/tls.c.
 * ============================================================ */
#include "tls.h"

/* RNG : RDTSC + xorshift. Suffisant pour completer le handshake ;
   PAS de qualite cryptographique (assume dans la demo). */
static uint64_t _tls_rdtsc(void){uint32_t lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));return((uint64_t)hi<<32)|lo;}
static uint32_t _rng_state=0;
static void tls_rng(void*c,uint8_t*b,int n){
    (void)c;
    if(!_rng_state)_rng_state=(uint32_t)_tls_rdtsc()|1u;
    for(int i=0;i<n;i++){
        _rng_state^=(uint32_t)_tls_rdtsc();
        _rng_state^=_rng_state<<13;_rng_state^=_rng_state>>17;_rng_state^=_rng_state<<5;
        b[i]=(uint8_t)(_rng_state>>((i&3)*8));
    }
}
/* send : decoupe en segments <= MSS (tcp_tx a un buffer de trame de 1500 o).
   io_send_all (cote tls.c) boucle sur la valeur retournee. */
static int tls_send(void*c,const uint8_t*b,int n){
    (void)c;
    int chunk=n>1400?1400:n;
    tcp_tx(0x18,b,(uint16_t)chunk);   /* PSH|ACK */
    _seq+=(uint32_t)chunk;
    return chunk;
}
/* recv : un segment TCP a la fois, ACK, livraison a la demande.
   Deadline global (ticks PIT) pour borner le temps total du handshake. */
static uint8_t _tls_rx[2048];
static int _tls_rxlen=0,_tls_rxpos=0,_tls_fin=0;
static uint32_t _tls_deadline=0;
static int tls_recv(void*c,uint8_t*b,int n){
    (void)c;
    if(_tls_rxpos>=_tls_rxlen){
        if(_tls_fin)return 0;
        uint8_t fl;int tries=0;
        for(;;){
            if(*(volatile uint32_t*)0x046C>_tls_deadline)return -1;  /* deadline global */
            uint16_t dl=tcp_rx(&fl,_tls_rx,sizeof(_tls_rx),120);
            if(dl>0){
                tcp_tx(0x10,0,0);
                _tls_rxlen=dl;_tls_rxpos=0;
                if(fl&0x01)_tls_fin=1;
                break;
            }
            if(fl&0x01){tcp_tx(0x11,0,0);_tls_fin=1;return 0;}
            if(fl&0x04)return 0;
            if(++tries>500)return -1;
        }
    }
    int avail=_tls_rxlen-_tls_rxpos,take=n<avail?n:avail;
    memcpy(b,_tls_rx+_tls_rxpos,take);_tls_rxpos+=take;
    return take;
}

/* GET HTTPS reel : DNS + TCP:443 + TLS 1.2 from scratch */
int net_https_get(const char*url,char*buf,int bsz){
    if(!net_ok)return -1;
    const char*p=url;
    if(p[0]=='h'&&p[4]=='s'&&p[5]==':'&&p[6]=='/'&&p[7]=='/')p+=8;      /* https:// */
    else if(p[0]=='h'&&p[4]==':'&&p[5]=='/'&&p[6]=='/')p+=7;            /* http://  */
    char host[128];int hl=0;
    while(*p&&*p!='/'&&hl<127)host[hl++]=*p++;
    host[hl]='\0';
    const char*path=(*p=='/')?p:"/";
    uint32_t ip;
    if(!dns_resolve(host,&ip))return -2;
    _tls_rxlen=_tls_rxpos=_tls_fin=0;
    _tls_deadline=*(volatile uint32_t*)0x046C+110;   /* ~6s (PIT 18.2 Hz) */
    if(!tcp_connect(ip,443))return -3;
    TlsIO io={0,tls_send,tls_recv,tls_rng};
    return tls_https_get(&io,host,path,buf,bsz);
}

int net_http_get(const char*url,char*buf,int bsz){
    if(!net_ok)return -1;
    const char*p=url;
    if(p[0]=='h'&&p[4]==':'&&p[5]=='/'&&p[6]=='/')p+=7;
    else if(p[0]=='h'&&p[5]==':'&&p[6]=='/'&&p[7]=='/')p+=8;
    char host[128];int hl=0;
    while(*p&&*p!='/'&&hl<127)host[hl++]=*p++;
    host[hl]='\0';
    const char*path=(*p=='/')?p:"/";
    uint32_t ip;
    if(!dns_resolve(host,&ip)){
        int n=0;const char*e="Erreur: echec DNS pour ";
        while(*e&&n<bsz-1)buf[n++]=*e++;
        for(int i=0;host[i]&&n<bsz-1;i++)buf[n++]=host[i];
        buf[n]='\0';return n;
    }
    if(!tcp_connect(ip,80)){
        const char*e="Erreur: connexion TCP echouee";
        int n=0;while(*e&&n<bsz-1)buf[n++]=*e++;
        buf[n]='\0';return n;
    }
    return http_get_raw(host,path,buf,bsz);
}
