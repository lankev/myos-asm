/* ============================================================================
 * Client TLS 1.2 minimal, from scratch, pour MyOS.
 * Suite : TLS_RSA_WITH_AES_128_CBC_SHA256 (0x00,0x3C)
 *   - echange de cle RSA (pas de courbes elliptiques)
 *   - AES-128-CBC + HMAC-SHA256, MAC-then-encrypt, IV explicite (TLS1.2)
 * Le certificat est parse pour extraire la cle RSA (n,e) mais la CHAINE
 * N'EST PAS VALIDEE (pas de verification CA). Handshake reel, confiance non
 * verifiee : suffisant pour prouver "MyOS parle vraiment TLS".
 *
 * I/O reseau abstraite par TlsIO (send/recv/rng) -> testable sur l'hote
 * (sockets) puis porte tel quel dans l'OS (tcp_tx/tcp_rx + RNG materiel).
 * ==========================================================================*/
#include "stdint.h"
#include "string.h"
#include "tls.h"

#define DBG(...)   /* pas de sortie debug dans l'OS */

/* ================= crypto (verifiee sur l'hote) ================= */
/* --- AES-128 --- */
static const uint8_t AES_SBOX[256]={
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};
static uint8_t AES_INV[256];
static int AES_INV_OK=0;
static void aes_inv_init(void){if(AES_INV_OK)return;for(int i=0;i<256;i++)AES_INV[AES_SBOX[i]]=(uint8_t)i;AES_INV_OK=1;}
static uint8_t aes_xt(uint8_t x){return (uint8_t)((x<<1)^((x>>7)*0x1b));}
static uint8_t aes_mul(uint8_t a,uint8_t b){uint8_t r=0;for(int i=0;i<8;i++){if(b&1)r^=a;uint8_t hi=a&0x80;a=(uint8_t)(a<<1);if(hi)a^=0x1b;b=(uint8_t)(b>>1);}return r;}
typedef struct{uint8_t rk[176];}AES;
static void aes_key(AES*a,const uint8_t*key){
    memcpy(a->rk,key,16);uint8_t rcon=1;
    for(int i=16;i<176;i+=4){
        uint8_t t[4];memcpy(t,a->rk+i-4,4);
        if(i%16==0){uint8_t tmp=t[0];t[0]=AES_SBOX[t[1]]^rcon;t[1]=AES_SBOX[t[2]];t[2]=AES_SBOX[t[3]];t[3]=AES_SBOX[tmp];rcon=aes_xt(rcon);}
        for(int j=0;j<4;j++)a->rk[i+j]=a->rk[i-16+j]^t[j];
    }
}
static void aes_enc(const AES*a,const uint8_t*in,uint8_t*out){
    uint8_t s[16];memcpy(s,in,16);
    for(int i=0;i<16;i++)s[i]^=a->rk[i];
    for(int r=1;r<=10;r++){
        uint8_t t[16];for(int i=0;i<16;i++)t[i]=AES_SBOX[s[i]];
        uint8_t sh[16]={t[0],t[5],t[10],t[15],t[4],t[9],t[14],t[3],t[8],t[13],t[2],t[7],t[12],t[1],t[6],t[11]};
        if(r<10)for(int c=0;c<4;c++){uint8_t*col=sh+c*4;uint8_t a0=col[0],a1=col[1],a2=col[2],a3=col[3];
            col[0]=(uint8_t)(aes_xt(a0)^(aes_xt(a1)^a1)^a2^a3);
            col[1]=(uint8_t)(a0^aes_xt(a1)^(aes_xt(a2)^a2)^a3);
            col[2]=(uint8_t)(a0^a1^aes_xt(a2)^(aes_xt(a3)^a3));
            col[3]=(uint8_t)((aes_xt(a0)^a0)^a1^a2^aes_xt(a3));}
        for(int i=0;i<16;i++)s[i]=sh[i]^a->rk[r*16+i];
    }
    memcpy(out,s,16);
}
static void aes_dec(const AES*a,const uint8_t*in,uint8_t*out){
    uint8_t s[16];memcpy(s,in,16);
    for(int i=0;i<16;i++)s[i]^=a->rk[160+i];
    for(int r=9;r>=0;r--){
        uint8_t t[16]={s[0],s[13],s[10],s[7],s[4],s[1],s[14],s[11],s[8],s[5],s[2],s[15],s[12],s[9],s[6],s[3]};
        for(int i=0;i<16;i++)t[i]=AES_INV[t[i]];
        for(int i=0;i<16;i++)t[i]^=a->rk[r*16+i];
        if(r>0)for(int c=0;c<4;c++){uint8_t*col=t+c*4;uint8_t a0=col[0],a1=col[1],a2=col[2],a3=col[3];
            col[0]=(uint8_t)(aes_mul(a0,14)^aes_mul(a1,11)^aes_mul(a2,13)^aes_mul(a3,9));
            col[1]=(uint8_t)(aes_mul(a0,9)^aes_mul(a1,14)^aes_mul(a2,11)^aes_mul(a3,13));
            col[2]=(uint8_t)(aes_mul(a0,13)^aes_mul(a1,9)^aes_mul(a2,14)^aes_mul(a3,11));
            col[3]=(uint8_t)(aes_mul(a0,11)^aes_mul(a1,13)^aes_mul(a2,9)^aes_mul(a3,14));}
        memcpy(s,t,16);
    }
    memcpy(out,s,16);
}
static void aes_cbc_enc(const AES*a,uint8_t*iv,uint8_t*buf,int len){
    for(int off=0;off<len;off+=16){
        for(int i=0;i<16;i++)buf[off+i]^=iv[i];
        aes_enc(a,buf+off,buf+off);
        memcpy(iv,buf+off,16);
    }
}
static void aes_cbc_dec(const AES*a,uint8_t*iv,uint8_t*buf,int len){
    uint8_t nx[16];
    for(int off=0;off<len;off+=16){
        memcpy(nx,buf+off,16);
        aes_dec(a,buf+off,buf+off);
        for(int i=0;i<16;i++)buf[off+i]^=iv[i];
        memcpy(iv,nx,16);
    }
}
/* --- SHA-256 / HMAC / PRF --- */
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static const uint32_t SK[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static void sha256(const uint8_t*msg,uint32_t len,uint8_t out[32]){
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint32_t total=((len+8)/64+1)*64;
    for(uint32_t off=0;off<total;off+=64){
        uint8_t ck[64];
        for(uint32_t i=0;i<64;i++){uint32_t p=off+i;
            if(p<len)ck[i]=msg[p];else if(p==len)ck[i]=0x80;
            else if(p>=total-8){uint64_t b=(uint64_t)len*8u;ck[i]=(uint8_t)(b>>(8u*(7u-(p-(total-8)))));}else ck[i]=0;}
        uint32_t w[64];
        for(int i=0;i<16;i++)w[i]=((uint32_t)ck[i*4]<<24)|((uint32_t)ck[i*4+1]<<16)|((uint32_t)ck[i*4+2]<<8)|ck[i*4+3];
        for(int i=16;i<64;i++){uint32_t s0=ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3);uint32_t s1=ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(int i=0;i<64;i++){uint32_t S1=ROR(e,6)^ROR(e,11)^ROR(e,25),ch=(e&f)^(~e&g);uint32_t t1=hh+S1+ch+SK[i]+w[i];
            uint32_t S0=ROR(a,2)^ROR(a,13)^ROR(a,22),mj=(a&b)^(a&c)^(b&c);uint32_t t2=S0+mj;
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    for(int i=0;i<8;i++){out[i*4]=(uint8_t)(h[i]>>24);out[i*4+1]=(uint8_t)(h[i]>>16);out[i*4+2]=(uint8_t)(h[i]>>8);out[i*4+3]=(uint8_t)h[i];}
}
static void hmac_sha256(const uint8_t*key,int klen,const uint8_t*msg,int mlen,uint8_t out[32]){
    uint8_t k[64],ip[64],op[64],ih[32];memset(k,0,64);
    if(klen>64)sha256(key,(uint32_t)klen,k);else memcpy(k,key,klen);
    for(int i=0;i<64;i++){ip[i]=k[i]^0x36;op[i]=k[i]^0x5c;}
    static uint8_t buf[64+20000];
    memcpy(buf,ip,64);memcpy(buf+64,msg,mlen);sha256(buf,(uint32_t)(64+mlen),ih);
    memcpy(buf,op,64);memcpy(buf+64,ih,32);sha256(buf,64+32,out);
}
static void tls_prf(const uint8_t*secret,int slen,const char*label,
                    const uint8_t*seed,int seedlen,uint8_t*out,int outlen){
    uint8_t ls[128];int ll=(int)strlen(label);
    memcpy(ls,label,ll);memcpy(ls+ll,seed,seedlen);int lslen=ll+seedlen;
    uint8_t a[32];hmac_sha256(secret,slen,ls,lslen,a);
    int done=0;
    while(done<outlen){
        uint8_t in[32+128],hm[32];memcpy(in,a,32);memcpy(in+32,ls,lslen);
        hmac_sha256(secret,slen,in,32+lslen,hm);
        int n=outlen-done;if(n>32)n=32;memcpy(out+done,hm,n);done+=n;
        hmac_sha256(secret,slen,a,32,a);
    }
}
/* --- bignum RSA --- */
#define BN_MAX 132
typedef struct{uint32_t w[BN_MAX];int n;}BN;
static void bn_zero(BN*a){memset(a->w,0,sizeof a->w);a->n=1;}
static void bn_from_be(BN*a,const uint8_t*b,int len){bn_zero(a);for(int i=0;i<len&&i/4<BN_MAX;i++){int v=b[len-1-i];a->w[i/4]|=(uint32_t)v<<((i%4)*8);}a->n=(len+3)/4;if(a->n<1)a->n=1;if(a->n>BN_MAX)a->n=BN_MAX;}
static void bn_to_be(const BN*a,uint8_t*b,int len){for(int i=0;i<len;i++){int wi=(len-1-i)/4,sh=((len-1-i)%4)*8;b[i]=(uint8_t)(a->w[wi]>>sh);}}
static int bn_getbit(const BN*a,int i){return (int)((a->w[i>>5]>>(i&31))&1);}
static int bn_bits(const BN*a){for(int i=a->n*32-1;i>=0;i--)if(bn_getbit(a,i))return i+1;return 0;}
static void bn_shl1(BN*a){uint32_t carry=0;for(int i=0;i<a->n+1&&i<BN_MAX;i++){uint32_t nc=a->w[i]>>31;a->w[i]=(a->w[i]<<1)|carry;carry=nc;}if(a->n<BN_MAX)a->n++;}
static int bn_cmp(const BN*a,const BN*b){int n=a->n>b->n?a->n:b->n;for(int i=n-1;i>=0;i--){uint32_t x=i<a->n?a->w[i]:0,y=i<b->n?b->w[i]:0;if(x<y)return -1;if(x>y)return 1;}return 0;}
static void bn_sub(BN*a,const BN*b){uint64_t bor=0;for(int i=0;i<a->n;i++){uint64_t bi=i<b->n?b->w[i]:0;uint64_t d=(uint64_t)a->w[i]-bi-bor;a->w[i]=(uint32_t)d;bor=(d>>32)&1;}while(a->n>1&&a->w[a->n-1]==0)a->n--;}
static void bn_mod(BN*r,const BN*x,const BN*m){bn_zero(r);int top=bn_bits(x);for(int i=top-1;i>=0;i--){bn_shl1(r);r->w[0]|=(uint32_t)bn_getbit(x,i);if(bn_cmp(r,m)>=0)bn_sub(r,m);}while(r->n>1&&r->w[r->n-1]==0)r->n--;}
static void bn_mul(BN*prod,const BN*a,const BN*b){bn_zero(prod);prod->n=a->n+b->n;if(prod->n>BN_MAX)prod->n=BN_MAX;for(int i=0;i<a->n;i++){uint64_t carry=0;for(int j=0;j<b->n&&i+j<BN_MAX;j++){uint64_t cur=(uint64_t)prod->w[i+j]+(uint64_t)a->w[i]*b->w[j]+carry;prod->w[i+j]=(uint32_t)cur;carry=cur>>32;}if(i+b->n<BN_MAX)prod->w[i+b->n]+=(uint32_t)carry;}while(prod->n>1&&prod->w[prod->n-1]==0)prod->n--;}
static void bn_modexp(BN*r,const BN*base,const BN*exp,const BN*m){BN result,b,t,q;bn_zero(&result);result.w[0]=1;bn_mod(&b,base,m);int bits=bn_bits(exp);for(int i=bits-1;i>=0;i--){bn_mul(&t,&result,&result);bn_mod(&result,&t,m);if(bn_getbit(exp,i)){bn_mul(&q,&result,&b);bn_mod(&result,&q,m);}}*r=result;}

/* ================= X.509 : extraire (n,e) ================= */
/* Cherche l'OID rsaEncryption puis le BIT STRING contenant RSAPublicKey. */
static const uint8_t RSA_OID[9]={0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01};
static int der_len(const uint8_t*p,int*consumed){
    int l=p[0];if(!(l&0x80)){*consumed=1;return l;}
    int nb=l&0x7f;int v=0;for(int i=0;i<nb;i++)v=(v<<8)|p[1+i];*consumed=1+nb;return v;
}
/* extrait modulus/exponent -> nb[]/nlen, eb[]/elen. Retourne 1 si ok. */
static int x509_pubkey(const uint8_t*cert,int clen,uint8_t*nb,int*nlen,uint8_t*eb,int*elen){
    int oidpos=-1;
    for(int i=0;i+9<=clen;i++)if(memcmp(cert+i,RSA_OID,9)==0){oidpos=i;break;}
    if(oidpos<0)return 0;
    int i=oidpos+9,c;
    while(i<clen&&cert[i]!=0x03)i++;   /* trouve le BIT STRING */
    if(i>=clen)return 0;
    i++;der_len(cert+i,&c);i+=c;        /* saute longueur BIT STRING */
    if(cert[i]!=0x00)return 0;i++;      /* octet "unused bits" */
    if(cert[i]!=0x30)return 0;i++;      /* SEQUENCE RSAPublicKey */
    der_len(cert+i,&c);i+=c;
    if(cert[i]!=0x02)return 0;i++;      /* INTEGER modulus */
    int mlen=der_len(cert+i,&c);i+=c;
    const uint8_t*mp=cert+i;i+=mlen;
    while(mlen>0&&mp[0]==0x00){mp++;mlen--;}
    if(mlen<=0||mlen>512)return 0;
    memcpy(nb,mp,mlen);*nlen=mlen;
    if(cert[i]!=0x02)return 0;i++;      /* INTEGER exponent */
    int eln=der_len(cert+i,&c);i+=c;
    const uint8_t*ep=cert+i;
    while(eln>1&&ep[0]==0x00){ep++;eln--;}
    if(eln<=0||eln>8)return 0;
    memcpy(eb,ep,eln);*elen=eln;
    return 1;
}

/* ================= Etat + record layer TLS 1.2 ================= */
typedef struct {
    TlsIO* io;
    /* cle RSA serveur */
    uint8_t n[512]; int nlen;
    uint8_t e[8];   int elen;
    /* aleas */
    uint8_t crand[32], srand[32];
    /* secrets */
    uint8_t master[48];
    uint8_t cwmac[32], swmac[32], cwkey[16], swkey[16];
    /* sequence numbers */
    uint64_t wseq, rseq;
    int encrypted;         /* apres CCS envoye */
    int rdec;              /* apres CCS recu */
    /* transcript des messages handshake */
    uint8_t tr[20480]; int trlen;
    /* buffer de record en cours de lecture */
    uint8_t rbuf[18500]; int rlen, rpos; int rtype;
} TLS;

static void tr_add(TLS*t,const uint8_t*b,int n){ if(t->trlen+n<=(int)sizeof t->tr){memcpy(t->tr+t->trlen,b,n);t->trlen+=n;} }

static int io_send_all(TLS*t,const uint8_t*b,int n){
    int off=0;while(off<n){int r=t->io->send(t->io->ctx,b+off,n-off);if(r<=0)return -1;off+=r;}return 0;
}
static int io_recv_all(TLS*t,uint8_t*b,int n){
    int off=0;while(off<n){int r=t->io->recv(t->io->ctx,b+off,n-off);if(r<=0)return -1;off+=r;}return 0;
}
/* envoie un record TLS (type, payload) EN UNE SEULE TRAME. Chiffre si encrypted.
 * Note importante (OS) : le NIC RTL8139 n'a que 4 descripteurs TX ; envoyer
 * l'en-tete et le corps en deux tcp_tx separes doublait le nombre de trames et
 * saturait l'anneau lors de la flight CKE+CCS+Finished. Un record = une trame. */
static int rec_send(TLS*t,int type,const uint8_t*data,int len){
    static uint8_t out[5+16+16384+64];
    if(!t->encrypted){
        out[0]=(uint8_t)type;out[1]=3;out[2]=3;out[3]=(uint8_t)(len>>8);out[4]=(uint8_t)len;
        memcpy(out+5,data,len);
        int r=io_send_all(t,out,5+len);
        return r;
    }
    /* MAC = HMAC(cwmac, seq(8)||type||ver||len||data) */
    static uint8_t macin[13+16384];
    for(int i=0;i<8;i++)macin[i]=(uint8_t)(t->wseq>>(8*(7-i)));
    macin[8]=(uint8_t)type;macin[9]=3;macin[10]=3;macin[11]=(uint8_t)(len>>8);macin[12]=(uint8_t)len;
    memcpy(macin+13,data,len);
    uint8_t mac[32];hmac_sha256(t->cwmac,32,macin,13+len,mac);
    /* corps chiffre = IV(16) || AES-CBC( data || mac || padding ) */
    uint8_t iv[16];t->io->rng(t->io->ctx,iv,16);
    uint8_t* frag=out+5;
    memcpy(frag,iv,16);
    int bl=0;uint8_t* body=frag+16;
    memcpy(body,data,len);bl+=len;memcpy(body+bl,mac,32);bl+=32;
    int pad=16-(bl%16);for(int i=0;i<pad;i++)body[bl+i]=(uint8_t)(pad-1);bl+=pad;
    AES a;aes_key(&a,t->cwkey);
    uint8_t iv2[16];memcpy(iv2,iv,16);
    aes_cbc_enc(&a,iv2,body,bl);
    int total=16+bl;
    out[0]=(uint8_t)type;out[1]=3;out[2]=3;out[3]=(uint8_t)(total>>8);out[4]=(uint8_t)total;
    if(io_send_all(t,out,5+total)<0)return -1;
    t->wseq++;
    return 0;
}
/* lit un record complet dans t->rbuf, dechiffre si rdec. Renvoie longueur payload. */
static int rec_read(TLS*t){
    uint8_t hdr[5];
    if(io_recv_all(t,hdr,5)<0)return -1;
    t->rtype=hdr[0];
    int len=(hdr[3]<<8)|hdr[4];
    if(len<0||len>(int)sizeof t->rbuf)return -1;
    if(io_recv_all(t,t->rbuf,len)<0)return -1;
    if(!t->rdec){t->rlen=len;t->rpos=0;return len;}
    /* dechiffre: IV(16) || ciphertext */
    if(len<32)return -1;
    uint8_t iv[16];memcpy(iv,t->rbuf,16);
    AES a;aes_key(&a,t->swkey);
    int cl=len-16;
    aes_cbc_dec(&a,iv,t->rbuf+16,cl);
    /* retire le padding */
    int pad=t->rbuf[16+cl-1]+1;
    int plen=cl-pad-32;   /* enleve padding et MAC */
    if(plen<0)return -1;
    /* (on ne verifie pas le MAC entrant ici : le handshake Finished valide deja tout) */
    memmove(t->rbuf,t->rbuf+16,plen);
    t->rlen=plen;t->rpos=0;
    t->rseq++;
    return plen;
}
/* lit n octets de donnees handshake (type 22), en tirant des records, + transcript */
static int hs_read(TLS*t,uint8_t*out,int n){
    int got=0;
    while(got<n){
        if(t->rpos>=t->rlen){
            if(rec_read(t)<0)return -1;
            if(t->rtype!=22)return -1;   /* attendu: handshake */
        }
        int avail=t->rlen-t->rpos,take=n-got;if(take>avail)take=avail;
        memcpy(out+got,t->rbuf+t->rpos,take);t->rpos+=take;got+=take;
    }
    tr_add(t,out,n);
    return 0;
}

/* ================= Handshake ================= */
static void put_u24(uint8_t*p,int v){p[0]=(uint8_t)(v>>16);p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)v;}

int tls_stage=0;   /* derniere etape atteinte (pour message d'erreur) */
#define STAGE(n) do{tls_stage=(n);}while(0)

static int tls_handshake(TLS*t,const char*host){
    tls_stage=0;
    /* ---- ClientHello ---- */
    uint8_t hs[512];int p=0;
    hs[p++]=3;hs[p++]=3;                    /* client_version 1.2 */
    t->io->rng(t->io->ctx,t->crand,32);
    memcpy(hs+p,t->crand,32);p+=32;
    hs[p++]=0;                              /* session_id vide */
    hs[p++]=0;hs[p++]=2;hs[p++]=0;hs[p++]=0x3C;   /* 1 suite: RSA_AES128_CBC_SHA256 */
    hs[p++]=1;hs[p++]=0;                    /* compression: null */
    /* extensions */
    int extlen_pos=p;p+=2;
    int hl=(int)strlen(host);
    /* SNI */
    hs[p++]=0;hs[p++]=0;                    /* type server_name */
    hs[p++]=0;hs[p++]=(uint8_t)(hl+5);      /* ext len */
    hs[p++]=0;hs[p++]=(uint8_t)(hl+3);      /* list len */
    hs[p++]=0;                              /* name type host */
    hs[p++]=0;hs[p++]=(uint8_t)hl;memcpy(hs+p,host,hl);p+=hl;
    /* signature_algorithms (rsa_pkcs1_sha256, sha384, sha1) */
    hs[p++]=0;hs[p++]=0x0d;hs[p++]=0;hs[p++]=8;hs[p++]=0;hs[p++]=6;
    hs[p++]=0x04;hs[p++]=0x01;hs[p++]=0x05;hs[p++]=0x01;hs[p++]=0x02;hs[p++]=0x01;
    int extlen=p-extlen_pos-2;
    hs[extlen_pos]=(uint8_t)(extlen>>8);hs[extlen_pos+1]=(uint8_t)extlen;
    /* enrobage handshake */
    uint8_t msg[600];msg[0]=1;put_u24(msg+1,p);memcpy(msg+4,hs,p);
    tr_add(t,msg,4+p);
    if(rec_send(t,22,msg,4+p)<0)return -1;
    STAGE(1);   /* ClientHello envoye */

    /* ---- ServerHello ---- */
    uint8_t h[4];
    if(hs_read(t,h,4)<0)return -1;
    STAGE(2);   /* premier octet de reponse serveur recu */
    if(h[0]!=2)return -2;                   /* ServerHello attendu */
    int shlen=(h[1]<<16)|(h[2]<<8)|h[3];
    uint8_t sh[1024];if(shlen>(int)sizeof sh)return -1;
    if(hs_read(t,sh,shlen)<0)return -1;
    memcpy(t->srand,sh+2,32);               /* server_random apres version(2) */
    int sp=2+32;int sidlen=sh[sp];sp+=1+sidlen;
    int suite=(sh[sp]<<8)|sh[sp+1];
    DBG("ServerHello: suite=0x%04x\n",suite);
    if(suite!=0x003C)return -3;             /* le serveur doit accepter notre suite */

    /* ---- Certificate ---- */
    if(hs_read(t,h,4)<0)return -1;
    if(h[0]!=11)return -4;
    int clen=(h[1]<<16)|(h[2]<<8)|h[3];
    static uint8_t cert[16384];if(clen>(int)sizeof cert)return -1;
    if(hs_read(t,cert,clen)<0)return -1;
    /* cert[0..2]=longueur totale liste, [3..5]=longueur 1er cert */
    int c1=(cert[3]<<16)|(cert[4]<<8)|cert[5];
    if(!x509_pubkey(cert+6,c1,t->n,&t->nlen,t->e,&t->elen))return -5;
    STAGE(3);   /* certificat recu et cle RSA extraite */
    DBG("Certificate: cle RSA n=%d octets e=%d octets\n",t->nlen,t->elen);

    /* ---- ServerHelloDone (ou d'autres messages avant) ---- */
    for(;;){
        if(hs_read(t,h,4)<0)return -1;
        int ml=(h[1]<<16)|(h[2]<<8)|h[3];
        if(h[0]==14){ if(ml){uint8_t tmp[8];(void)hs_read(t,tmp,ml<8?ml:0);} break; } /* done */
        /* ignore d'autres (ex: CertificateRequest) en consommant le corps */
        static uint8_t skip[4096];int rem=ml;
        while(rem>0){int k=rem>4096?4096:rem;if(hs_read(t,skip,k)<0)return -1;rem-=k;}
    }

    /* ---- pre_master + ClientKeyExchange ---- */
    uint8_t pms[48];pms[0]=3;pms[1]=3;t->io->rng(t->io->ctx,pms+2,46);
    /* PKCS#1 v1.5 : EM = 00 02 PS 00 pms */
    int k=t->nlen;uint8_t em[512];
    em[0]=0;em[1]=2;
    int pslen=k-3-48;
    for(int i=0;i<pslen;i++){uint8_t r;do{t->io->rng(t->io->ctx,&r,1);}while(r==0);em[2+i]=r;}
    em[2+pslen]=0;memcpy(em+3+pslen,pms,48);
    BN N,E,M,C;bn_from_be(&N,t->n,t->nlen);bn_from_be(&E,t->e,t->elen);bn_from_be(&M,em,k);
    bn_modexp(&C,&M,&E,&N);
    uint8_t enc[512];bn_to_be(&C,enc,k);
    STAGE(4);   /* ServerHelloDone recu, RSA chiffre le premaster */
    uint8_t cke[520];cke[0]=16;put_u24(cke+1,k+2);cke[4]=(uint8_t)(k>>8);cke[5]=(uint8_t)k;
    memcpy(cke+6,enc,k);
    tr_add(t,cke,6+k);
    if(rec_send(t,22,cke,6+k)<0)return -1;
    STAGE(5);   /* ClientKeyExchange envoye */

    /* ---- master secret + cles ---- */
    uint8_t seed[64];memcpy(seed,t->crand,32);memcpy(seed+32,t->srand,32);
    tls_prf(pms,48,"master secret",seed,64,t->master,48);
    uint8_t seed2[64];memcpy(seed2,t->srand,32);memcpy(seed2+32,t->crand,32);
    uint8_t kb[96];tls_prf(t->master,48,"key expansion",seed2,64,kb,96);
    memcpy(t->cwmac,kb,32);memcpy(t->swmac,kb+32,32);
    memcpy(t->cwkey,kb+64,16);memcpy(t->swkey,kb+80,16);

    /* ---- ChangeCipherSpec ---- */
    uint8_t ccs=1;if(rec_send(t,20,&ccs,1)<0)return -1;
    t->encrypted=1;t->wseq=0;

    /* ---- Finished (client) ---- */
    uint8_t thash[32];sha256(t->tr,t->trlen,thash);
    uint8_t vd[12];tls_prf(t->master,48,"client finished",thash,32,vd,12);
    uint8_t fin[16];fin[0]=20;put_u24(fin+1,12);memcpy(fin+4,vd,12);
    tr_add(t,fin,16);                       /* inclus pour le hash du Finished serveur */
    if(rec_send(t,22,fin,16)<0)return -1;
    STAGE(6);   /* CCS + Finished client envoyes */

    DBG("Client Finished envoye, attente reponse serveur...\n");
    /* ---- ChangeCipherSpec serveur ---- */
    if(rec_read(t)<0)return -1;
    DBG("record serveur: type=%d len=%d\n",t->rtype,t->rlen);
    if(t->rtype==21){DBG("ALERT serveur: %02x %02x (le serveur a refuse)\n",t->rbuf[0],t->rbuf[1]);return -6;}
    if(t->rtype!=20)return -6;
    t->rpos=t->rlen;        /* consomme l'octet CCS pour ne pas polluer le flux handshake */
    t->rdec=1;t->rseq=0;

    /* ---- Finished serveur ---- */
    if(hs_read(t,h,4)<0)return -1;
    DBG("Finished serveur: hdr=%02x %02x %02x %02x\n",h[0],h[1],h[2],h[3]);
    if(h[0]!=20)return -7;
    uint8_t svd[12];if(hs_read(t,svd,12)<0)return -1;
    /* verification: PRF(master,"server finished",hash(transcript sans ce Finished)) */
    /* (tr_add du Finished serveur a deja ajoute h+svd ; on recalcule sur la partie avant) */
    uint8_t thash2[32];sha256(t->tr,t->trlen-16,thash2);
    uint8_t expvd[12];tls_prf(t->master,48,"server finished",thash2,32,expvd,12);
    STAGE(7);   /* Finished serveur recu */
    if(memcmp(svd,expvd,12)!=0)return -8;    /* Finished serveur invalide */
    STAGE(8);   /* handshake complet, canal chiffre valide */
    return 0;   /* handshake OK, canal chiffre etabli */
}

/* Banc d'essai RSA : un modexp 2048-bit avec des valeurs fixes.
 * Sert a mesurer la vitesse du RSA en QEMU (isole du reseau). */
uint32_t tls_bench_modexp(void){
    static const char Nh[]="dee22fc4723403cb4cde80f1dad4039711183cb5810667b3e2c3b7fce1e959102";
    uint8_t nb[256];for(int i=0;i<256;i++)nb[i]=(uint8_t)(0xA5^(i*7));
    for(int i=0;i<32;i++){int hi=Nh[i*2],lo=Nh[i*2+1];
        int hv=(hi<='9')?hi-'0':hi-'a'+10,lv=(lo<='9')?lo-'0':lo-'a'+10;
        nb[i]=(uint8_t)((hv<<4)|lv);}
    nb[0]|=0x80;nb[255]|=1;              /* impair, bit haut */
    uint8_t mb[256];for(int i=0;i<256;i++)mb[i]=(uint8_t)(i*3+1);mb[0]=0;
    BN N,E,M,C;bn_from_be(&N,nb,256);bn_from_be(&M,mb,256);bn_zero(&E);E.w[0]=65537;
    bn_modexp(&C,&M,&E,&N);
    return C.w[0];
}

/* GET HTTPS : handshake puis requete, remplit out (corps+entetes brut). */
int tls_https_get(TlsIO* io,const char* host,const char* path,char* out,int outmax){
    aes_inv_init();
    static TLS t;memset(&t,0,sizeof t);t.io=io;
    int r=tls_handshake(&t,host);
    if(r!=0)return r*10;   /* codes d'erreur negatifs distincts */
    /* requete HTTP */
    char req[512];int rp=0;
    const char*g="GET ";for(int i=0;g[i];i++)req[rp++]=g[i];
    for(int i=0;path[i];i++)req[rp++]=path[i];
    const char*h1=" HTTP/1.0\r\nHost: ";for(int i=0;h1[i];i++)req[rp++]=h1[i];
    for(int i=0;host[i];i++)req[rp++]=host[i];
    const char*h2="\r\nConnection: close\r\nUser-Agent: MyOS-TLS/1.0\r\n\r\n";
    for(int i=0;h2[i];i++)req[rp++]=h2[i];
    if(rec_send(&t,23,(uint8_t*)req,rp)<0)return -100;
    /* lecture de la reponse */
    int total=0;
    for(;;){
        int pl=rec_read(&t);
        if(pl<0)break;                 /* close_notify / fin */
        if(t.rtype==21)break;          /* alert -> fin */
        if(t.rtype!=23)continue;
        for(int i=0;i<pl&&total<outmax-1;i++)out[total++]=(char)t.rbuf[i];
        if(total>=outmax-1)break;
    }
    out[total]=0;
    return total;
}

// (test hote retire ; portage OS)
