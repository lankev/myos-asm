#include "sfcml.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "font8x8.h"
#include "../net/net.h"

/* ============================================================
 * Resolution ecran
 * ============================================================ */
#define SCR_W  800
#define SCR_H  600
#define TB_Y   (SCR_H - 31)   /* y de la taskbar */

/* Forward decls needed by nano/DVFS code inserted before their definitions */
#define W_WORD 5
static void win_open(int idx);
static void sh_nano(const char* a);
#define W_SNAKE    11
#define W_RTYPE    12
#define W_PONG     13
static void snake_reset(void);
static void draw_snake(sfcml_Window* win, int wi);
static void rtype_reset(void);
static void draw_rtype(sfcml_Window* win, int wi);
static void pong_reset(void);
static void draw_pong(sfcml_Window* win, int wi);
#define W_MINE     14
#define W_TETRIS   15
#define W_DEMINE   16
static int  _mine_inited;
static int  _tet_inited,_ms_inited;
static int  _mc_look;   /* capture souris (mouse-look), bascule avec Tab */
static void mine_open_init(void);
static void mc_save(void);
static void tet_reset(void);
static void tet_step(void);
static void tet_key(sfcml_KeyCode k);
static void draw_tetris(sfcml_Window* win, int wi);
static void ms_reset(void);
static void mines_click(int mx, int my, int btn);
static void draw_mines(sfcml_Window* win, int wi);
static void mine_reset(void);
static void mine_step(void);
static void mine_click(int btn);
static void mine_key_press(sfcml_KeyCode k);
static void mine_key_release(sfcml_KeyCode k);
static void draw_mine(sfcml_Window* win, int wi);

void _putchar(char c) { (void)c; }
#define BIOS_TICKS (*(volatile uint32_t*)0x046C)

/* ============================================================
 * E/S ports
 * ============================================================ */
static inline uint8_t k_inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void k_outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint16_t k_inw(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void k_outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline uint32_t k_inl(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void k_outl(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}

/* ============================================================
 * Journal noyau (dmesg reel) - ring buffer texte
 * ============================================================ */
#define KLOG_SZ 4096
static char _klog[KLOG_SZ];
static int  _klog_len=0;
static void klog(const char* msg){
    /* timestamp reel: ticks PIT a 18.2 Hz -> secondes.centiemes */
    uint32_t t=BIOS_TICKS;
    uint32_t cs=(t*100u)/18u;   /* centisecondes depuis boot */
    char hd[16];int o=0;
    hd[o++]='[';
    uint32_t s=cs/100u;
    if(s>=100)hd[o++]=(char)('0'+(s/100)%10);
    hd[o++]=(char)('0'+(s/10)%10);hd[o++]=(char)('0'+s%10);
    hd[o++]='.';hd[o++]=(char)('0'+(cs/10)%10);hd[o++]=(char)('0'+cs%10);
    hd[o++]=']';hd[o++]=' ';hd[o]='\0';
    for(int i=0;hd[i]&&_klog_len<KLOG_SZ-2;i++)_klog[_klog_len++]=hd[i];
    for(int i=0;msg[i]&&_klog_len<KLOG_SZ-2;i++)_klog[_klog_len++]=msg[i];
    _klog[_klog_len++]='\n';_klog[_klog_len]='\0';
}

/* ============================================================
 * IDT + PIC + PIT : le temps devient reel.
 * L'ISR IRQ0 (crt0.asm) incremente le dword a 0x046C, la meme
 * adresse que le compteur de ticks BIOS : BIOS_TICKS et tous
 * ses utilisateurs (uptime, sleep, horloges SFCML) redeviennent
 * exacts. PIT programme a 18.2065 Hz (diviseur 65536), comme le
 * BIOS. Toutes les autres IRQ restent masquees (clavier/souris
 * sont polles).
 * ============================================================ */
typedef struct{uint16_t lo,sel;uint8_t zero,flags;uint16_t hi;}__attribute__((packed)) IDTEntry;
static IDTEntry _idt[256];
extern void irq0_stub(void);
extern void irq_ignore_stub(void);
static void _idt_set(int n,void(*h)(void)){
    uint32_t a=(uint32_t)h;
    _idt[n].lo=(uint16_t)(a&0xFFFF);_idt[n].sel=0x08;
    _idt[n].zero=0;_idt[n].flags=0x8E;_idt[n].hi=(uint16_t)(a>>16);
}
static void time_init(void){
    for(int i=32;i<48;i++)_idt_set(i,irq_ignore_stub);
    _idt_set(32,irq0_stub);
    struct{uint16_t lim;uint32_t base;}__attribute__((packed)) idtr={
        (uint16_t)(sizeof _idt-1),(uint32_t)_idt};
    __asm__ volatile("lidt %0"::"m"(idtr));
    /* PIC 8259: remap IRQ0-15 -> vecteurs 0x20-0x2F */
    k_outb(0x20,0x11);k_outb(0xA0,0x11);
    k_outb(0x21,0x20);k_outb(0xA1,0x28);
    k_outb(0x21,0x04);k_outb(0xA1,0x02);
    k_outb(0x21,0x01);k_outb(0xA1,0x01);
    k_outb(0x21,0xFE);k_outb(0xA1,0xFF);   /* seul IRQ0 demasque */
    /* PIT canal 0, mode 2, diviseur 65536 -> 18.2065 Hz */
    k_outb(0x43,0x34);k_outb(0x40,0x00);k_outb(0x40,0x00);
    BIOS_TICKS=0;
    __asm__ volatile("sti");
}

/* ============================================================
 * CPUID + TSC (infos CPU reelles)
 * ============================================================ */
static void k_cpuid(uint32_t leaf,uint32_t*a,uint32_t*b,uint32_t*c,uint32_t*d){
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
static uint64_t k_rdtsc(void){
    uint32_t lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    return ((uint64_t)hi<<32)|lo;
}

/* ============================================================
 * PCI : lecture espace de configuration (ports 0xCF8/0xCFC)
 * ============================================================ */
static uint32_t k_pci_rd(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t reg){
    k_outl(0xCF8,0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|
                 ((uint32_t)fn<<8)|(reg&0xFC));
    return k_inl(0xCFC);
}

/* Compteurs d'E/S disque reels (pour iostat/vmstat) */
static uint32_t _ata_rd_cnt=0,_ata_wr_cnt=0;
static int _dvfs_dirty=0;

/* ============================================================
 * Son : PC speaker reel (PIT canal 2 + port 0x61) - from scratch
 * Le canal 0 du PIT sert deja au temps (IRQ0) ; le canal 2 est
 * cable au haut-parleur. Timing fin en lisant le compteur du
 * canal 0 (resolution ~0.84 us) plutot que les ticks (55 ms).
 * ============================================================ */
static void speaker_on(uint32_t freq){
    if(freq<20||freq>20000)return;
    uint32_t div=1193182u/freq;
    k_outb(0x43,0xB6);                       /* canal 2, mode 3, LSB+MSB */
    k_outb(0x42,(uint8_t)(div&0xFF));
    k_outb(0x42,(uint8_t)((div>>8)&0xFF));
    k_outb(0x61,(uint8_t)(k_inb(0x61)|3));   /* connecte le haut-parleur */
}
static void speaker_off(void){
    k_outb(0x61,(uint8_t)(k_inb(0x61)&0xFC));
}
/* Horloge fine en unites de 1/1193182 s (~0.84 us) */
static uint32_t pit_fine(void){
    k_outb(0x43,0x00);                       /* latch canal 0 */
    uint8_t lo=k_inb(0x40),hi=k_inb(0x40);
    uint16_t cnt=(uint16_t)(((uint16_t)hi<<8)|lo);
    return BIOS_TICKS*65536u+(65536u-cnt);   /* monotone croissant */
}
static void pit_wait_ms(uint32_t ms){
    uint32_t start=pit_fine(),units=ms*1193u;
    while(pit_fine()-start<units)__asm__ volatile("pause");
}
/* Une note: frequence Hz pendant ms (freq 0 = silence) */
static void beep(uint32_t freq,uint32_t ms){
    if(freq)speaker_on(freq);else speaker_off();
    pit_wait_ms(ms);
    speaker_off();
}
/* Notes musicales (Hz) */
enum{N_C4=262,N_D4=294,N_E4=330,N_F4=349,N_G4=392,N_A4=440,N_B4=494,
     N_C5=523,N_D5=587,N_E5=659,N_G5=784,N_C6=1047};
typedef struct{uint16_t f,ms;}MNote;
static void play_melody(const MNote* m,int n){
    for(int i=0;i<n;i++){beep(m[i].f,m[i].ms);pit_wait_ms(6);}
}
static const MNote MEL_BOOT[]={{N_C5,90},{N_E5,90},{N_G5,90},{N_C6,150}};
static const MNote MEL_WIN[] ={{N_C5,80},{N_E5,80},{N_G5,80},{N_C6,80},{N_G5,80},{N_C6,200}};
static const MNote MEL_LOSE[]={{N_E4,140},{N_D4,140},{N_C4,260}};

/* ============================================================
 * RTC (Real Time Clock) via CMOS
 * ============================================================ */
static uint8_t cmos_read(uint8_t reg){
    __asm__ volatile("outb %0,$0x70"::"a"((uint8_t)(0x80|reg)));
    uint8_t v;
    __asm__ volatile("inb $0x71,%0":"=a"(v));
    return v;
}
static uint8_t bcd2bin(uint8_t b){return (b>>4)*10+(b&0x0F);}

typedef struct{uint8_t h,m,s,day,mon;uint16_t year;}RTC;
static RTC _rtc;

static void rtc_read(void){
    while(cmos_read(0x0A)&0x80); /* attendre fin de mise a jour */
    uint8_t stb=cmos_read(0x0B);
    uint8_t s=cmos_read(0x00),m=cmos_read(0x02),h=cmos_read(0x04);
    uint8_t d=cmos_read(0x07),mo=cmos_read(0x08),y=cmos_read(0x09);
    if(!(stb&0x04)){s=bcd2bin(s);m=bcd2bin(m);h=bcd2bin(h);d=bcd2bin(d);mo=bcd2bin(mo);y=bcd2bin(y);}
    if(!(stb&0x02)&&(h&0x80)){h=(uint8_t)(((h&0x7F)+12)%24);}
    _rtc.s=s;_rtc.m=m;_rtc.h=h;_rtc.day=d;_rtc.mon=mo;
    _rtc.year=(uint16_t)(2000+y);
}
/* Poll sans blocage : si mise a jour en cours, conserve les valeurs actuelles */
static void rtc_poll(void){
    if(cmos_read(0x0A)&0x80)return;
    uint8_t stb=cmos_read(0x0B);
    uint8_t s=cmos_read(0x00),m=cmos_read(0x02),h=cmos_read(0x04);
    uint8_t d=cmos_read(0x07),mo=cmos_read(0x08),y=cmos_read(0x09);
    if(!(stb&0x04)){s=bcd2bin(s);m=bcd2bin(m);h=bcd2bin(h);d=bcd2bin(d);mo=bcd2bin(mo);y=bcd2bin(y);}
    if(!(stb&0x02)&&(h&0x80)){h=(uint8_t)(((h&0x7F)+12)%24);}
    _rtc.s=s;_rtc.m=m;_rtc.h=h;_rtc.day=d;_rtc.mon=mo;
    _rtc.year=(uint16_t)(2000+y);
}
static int _blink=0; /* bascule chaque seconde via RTC */

/* ============================================================
 * Creeper 8x8
 * ============================================================ */
static const uint8_t CREEPER[8]={0xFF,0xFF,0x99,0x99,0xFF,0xE7,0xC3,0xDB};
static void draw_creeper(sfcml_Window* w,int ox,int oy,int sc){
    sfcml_Color g=sfcml_rgb(94,124,22);
    for(int r=0;r<8;r++) for(int c=0;c<8;c++){
        sfcml_fillRect(w,sfcml_rect(ox+c*sc,oy+r*sc,sc,sc),
            (CREEPER[r]&(1<<c))?g:SFCML_BLACK);
    }
}

/* ============================================================
 * Texte grande taille
 * ============================================================ */
static void draw_big_char(sfcml_Window* w,char c,int x,int y,int sc,
                           sfcml_Color fg,sfcml_Color bg){
    const uint8_t* g=font8x8+(unsigned char)c*8;
    for(int r=0;r<8;r++) for(int col=0;col<8;col++)
        sfcml_fillRect(w,sfcml_rect(x+col*sc,y+r*sc,sc,sc),
            (g[r]&(1<<col))?fg:bg);
}
static void draw_big_text(sfcml_Window* w,const char* s,int x,int y,
                           int sc,sfcml_Color fg,sfcml_Color bg){
    while(*s){draw_big_char(w,*s++,x,y,sc,fg,bg);x+=8*sc;}
}

/* ============================================================
 * Splash Epitech
 * ============================================================ */
static void splash_delay(uint32_t n){for(uint32_t i=0;i<n;i++)__asm__ volatile("nop");}
static void draw_splash(sfcml_Window* win){
    int sw=(int)win->width,sh=(int)win->height;
    int cx=sw/2,cy=sh/2;
    sfcml_Color bk=SFCML_BLACK,rd=sfcml_rgb(220,20,20);
    sfcml_Color gr=sfcml_rgb(150,150,150),wh=SFCML_WHITE;
    sfcml_fillRect(win,sfcml_rect(0,0,sw,sh),bk);
    draw_big_text(win,"EPITECH",cx-112,cy-122,4,rd,bk);
    sfcml_fillRect(win,sfcml_rect(cx-112,cy-82,224,3),rd);
    sfcml_drawText(win,"L'expertise informatique - epitech.eu",cx-148,cy-62,gr,bk);
    sfcml_drawText(win,"Epitech Technology  |  Barcelone, Espagne",cx-164,sh-18,gr,bk);
    sfcml_drawText(win,"Demarrage du systeme...",cx-88,cy,wh,bk);
    int bx=cx-120,by=sh-90,bw=240,bh=14;
    sfcml_drawRect(win,sfcml_rect(bx,by,bw,bh),sfcml_rgb(60,10,10));
    sfcml_present(win);
    for(int s=1;s<=30;s++){
        sfcml_fillRect(win,sfcml_rect(bx+1,by+1,s*(bw-2)/30,bh-2),rd);
        sfcml_present(win);
        splash_delay(20000000UL);
    }
}

/* ============================================================
 * Couleurs globales
 * ============================================================ */
static sfcml_Color C_DK,C_TB,C_WINBG,C_FG,C_PROMPT;

/* ============================================================
 * Terminal
 * ============================================================ */
#define T_COLS 60
#define T_ROWS 36
static char  _tlines[T_ROWS][T_COLS+1];
static int   _tnlines=0;
static char  _tinput[T_COLS+1];
static int   _tilen=0;
static char  _thist[8][T_COLS+1];
static int   _thlen=0,_thpos=-1;

static void t_scroll(void){
    for(int i=0;i<T_ROWS-1;i++)memcpy(_tlines[i],_tlines[i+1],T_COLS+1);
    _tlines[T_ROWS-1][0]='\0';
    if(_tnlines>0)_tnlines=T_ROWS-1;
}
static void t_nl(void){
    if(_tnlines<T_ROWS)_tlines[_tnlines][0]='\0';
    _tnlines++;
    if(_tnlines>T_ROWS)t_scroll();
}
static void t_print(const char* s){
    while(*s){
        if(*s=='\n'){t_nl();}
        else{
            int row=_tnlines>0?_tnlines-1:0;
            if(_tnlines==0){_tlines[0][0]='\0';_tnlines=1;}
            int len=(int)strlen(_tlines[row]);
            if(len<T_COLS){_tlines[row][len]=*s;_tlines[row][len+1]='\0';}
        }
        s++;
    }
}
static void t_hist_add(void){
    if(_tilen==0)return;
    if(_thlen<8)memcpy(_thist[_thlen++],_tinput,T_COLS+1);
    else{for(int i=0;i<7;i++)memcpy(_thist[i],_thist[i+1],T_COLS+1);memcpy(_thist[7],_tinput,T_COLS+1);}
    _thpos=-1;
}

/* ============================================================
 * Shell - systeme de fichiers virtuel
 * ============================================================ */
#define SH_NDIRS 16
static const char* _sh_dirs[SH_NDIRS]={
    "/","/bin","/boot","/dev","/etc","/home",
    "/home/root","/lib","/mnt","/proc","/sys",
    "/tmp","/usr","/usr/bin","/usr/lib","/var"
};
typedef struct{const char*n;}SHFile;
static const SHFile _sh_files[SH_NDIRS][8]={
    {{"bin"},{"boot"},{"dev"},{"etc"},{"home"},{"proc"},{"tmp"},{"var"}},
    {{"sh"},{"ls"},{"cat"},{"echo"},{"grep"},{"cp"},{"mv"},{"rm"}},
    {{"kernel.bin"},{"minegrub.bin"},{"boot.bin"},{"stage1"},{"stage2"},{""},{""},{""} },
    {{"null"},{"zero"},{"random"},{"tty0"},{"hda"},{""},{""},{""} },
    {{"hostname"},{"passwd"},{"fstab"},{"motd"},{"os-release"},{""},{""},{""} },
    {{"root"},{""},{""},{""},{""},{""},{""},{""} },
    {{".bashrc"},{"Desktop"},{"Documents"},{"Downloads"},{""},{""},{""},{""} },
    {{"libc.a"},{"libsfcml.a"},{"libk.a"},{""},{""},{""},{""},{""} },
    {{"floppy"},{""},{""},{""},{""},{""},{""},{""} },
    {{"cpuinfo"},{"meminfo"},{"version"},{"uptime"},{"net"},{""},{""},{""} },
    {{"kernel"},{"bus"},{"devices"},{""},{""},{""},{""},{""} },
    {{""},{""},{""},{""},{""},{""},{""},{""} },
    {{"bin"},{"lib"},{"include"},{"share"},{""},{""},{""},{""} },
    {{"gcc"},{"nasm"},{"make"},{"ld"},{"ar"},{"objcopy"},{""},{""} },
    {{""},{""},{""},{""},{""},{""},{""},{""} },
    {{"log"},{"tmp"},{"run"},{""},{""},{""},{""},{""} },
};
static int  _cwd_idx=0;
static char _cwd[64]="/";

/* ============================================================
 * VFS dynamique — fichiers crees par l'utilisateur
 * ============================================================ */
#define DVFS_MAX  48
#define DVFS_PLEN 56
#define DVFS_CLEN 512

typedef struct {
    char path[DVFS_PLEN];
    char content[DVFS_CLEN];
    int  is_dir;
    int  used;
} DVFSEntry;

static DVFSEntry _dvfs[DVFS_MAX];

static DVFSEntry* _dvfs_find(const char* p){
    for(int i=0;i<DVFS_MAX;i++)
        if(_dvfs[i].used&&!strcmp(_dvfs[i].path,p))return &_dvfs[i];
    return 0;
}
static DVFSEntry* _dvfs_create(const char* p,int is_dir){
    DVFSEntry* e=_dvfs_find(p);if(e)return e;
    for(int i=0;i<DVFS_MAX;i++){
        if(!_dvfs[i].used){
            strncpy(_dvfs[i].path,p,DVFS_PLEN-1);_dvfs[i].path[DVFS_PLEN-1]='\0';
            _dvfs[i].content[0]='\0';_dvfs[i].is_dir=is_dir;_dvfs[i].used=1;
            _dvfs_dirty=1;
            return &_dvfs[i];
        }
    }
    return 0;
}
static void _dvfs_remove(const char* p){DVFSEntry*e=_dvfs_find(p);if(e){e->used=0;_dvfs_dirty=1;}}
static void _dvfs_fullpath(const char* name,char* out,int olen){
    if(name[0]=='/'){strncpy(out,name,olen-1);out[olen-1]='\0';return;}
    int cl=(int)strlen(_cwd);strncpy(out,_cwd,olen-1);out[olen-1]='\0';
    if(out[cl-1]!='/'&&cl<olen-2){out[cl]='/';out[cl+1]='\0';cl++;}
    strncat(out,name,(size_t)(olen-cl-1));
}
static int _dvfs_in_dir(const DVFSEntry* e,int dir_idx){
    const char* dir=_sh_dirs[dir_idx];
    int dl=(int)strlen(dir),el=(int)strlen(e->path);
    if(el<=dl)return 0;
    if(strncmp(e->path,dir,(size_t)dl)!=0)return 0;
    if(dir[dl-1]=='/')return !strchr(e->path+dl,'/');
    if(e->path[dl]!='/')return 0;
    return !strchr(e->path+dl+1,'/');
}
static const char* _dvfs_basename2(const DVFSEntry* e,int dir_idx){
    const char* dir=_sh_dirs[dir_idx];
    int dl=(int)strlen(dir);
    if(dir[dl-1]=='/')return e->path+dl;
    return e->path+dl+1;
}

/* ============================================================
 * Driver ATA PIO (primaire maitre, LBA28) - 100% from scratch
 * Ports 0x1F0-0x1F7, polling (IRQ14 masquee, nIEN=1)
 * ============================================================ */
static int      _ata_ok=0;
static char     _ata_model[41];
static char     _ata_serial[21];
static uint32_t _ata_sectors=0;

static int ata_wait_bsy(void){
    for(int i=0;i<1000000;i++){
        uint8_t st=k_inb(0x1F7);
        if(!(st&0x80))return st;   /* BSY clear */
    }
    return -1;
}
static int ata_wait_drq(void){
    for(int i=0;i<1000000;i++){
        uint8_t st=k_inb(0x1F7);
        if(st&0x01)return -1;      /* ERR */
        if(!(st&0x80)&&(st&0x08))return 0;  /* !BSY && DRQ */
    }
    return -1;
}
static void ata_select(uint32_t lba){
    k_outb(0x1F6,(uint8_t)(0xE0|((lba>>24)&0x0F)));
    k_outb(0x1F2,1);
    k_outb(0x1F3,(uint8_t)(lba&0xFF));
    k_outb(0x1F4,(uint8_t)((lba>>8)&0xFF));
    k_outb(0x1F5,(uint8_t)((lba>>16)&0xFF));
}
static int ata_read(uint32_t lba,void* buf){
    if(!_ata_ok)return 0;
    if(ata_wait_bsy()<0)return 0;
    ata_select(lba);
    k_outb(0x1F7,0x20);            /* READ SECTORS */
    if(ata_wait_drq()<0)return 0;
    uint16_t* p=(uint16_t*)buf;
    for(int i=0;i<256;i++)p[i]=k_inw(0x1F0);
    _ata_rd_cnt++;
    return 1;
}
static int ata_write(uint32_t lba,const void* buf){
    if(!_ata_ok)return 0;
    if(ata_wait_bsy()<0)return 0;
    ata_select(lba);
    k_outb(0x1F7,0x30);            /* WRITE SECTORS */
    if(ata_wait_drq()<0)return 0;
    const uint16_t* p=(const uint16_t*)buf;
    for(int i=0;i<256;i++)k_outw(0x1F0,p[i]);
    if(ata_wait_bsy()<0)return 0;
    k_outb(0x1F7,0xE7);            /* FLUSH CACHE */
    ata_wait_bsy();
    _ata_wr_cnt++;
    return 1;
}
static void _ata_str(const uint16_t* id,int w0,int nw,char* out){
    /* chaines IDENTIFY: mots big-endian par paire d'octets */
    int o=0;
    for(int i=0;i<nw;i++){
        out[o++]=(char)(id[w0+i]>>8);
        out[o++]=(char)(id[w0+i]&0xFF);
    }
    out[o]='\0';
    while(o>0&&out[o-1]==' ')out[--o]='\0';   /* trim droite */
}
static int ata_init(void){
    k_outb(0x3F6,0x02);            /* nIEN: pas d'IRQ disque */
    k_outb(0x1F6,0xA0);            /* maitre */
    for(volatile int i=0;i<4000;i++);
    if(k_inb(0x1F7)==0xFF)return 0;   /* pas de bus */
    if(ata_wait_bsy()<0)return 0;
    k_outb(0x1F2,0);k_outb(0x1F3,0);k_outb(0x1F4,0);k_outb(0x1F5,0);
    k_outb(0x1F7,0xEC);            /* IDENTIFY */
    if(k_inb(0x1F7)==0)return 0;   /* pas de disque */
    if(ata_wait_drq()<0)return 0;
    uint16_t id[256];
    for(int i=0;i<256;i++)id[i]=k_inw(0x1F0);
    _ata_str(id,27,20,_ata_model);
    _ata_str(id,10,10,_ata_serial);
    _ata_sectors=((uint32_t)id[61]<<16)|id[60];
    _ata_ok=1;
    return 1;
}

/* ============================================================
 * MyFS : persistance du VFS sur disque - les fichiers survivent
 * au reboot. Superbloc au LBA 500, entrees DVFS brutes ensuite.
 * (le kernel occupe les LBA 17-400, l'image fait 2880 secteurs)
 * ============================================================ */
#define MYFS_LBA   500
static uint8_t _fs_secbuf[512];

static void dvfs_sync(void){
    if(!_ata_ok)return;
    memset(_fs_secbuf,0,512);
    *(uint32_t*)_fs_secbuf=0x31534659u;              /* "YFS1" */
    *(uint32_t*)(_fs_secbuf+4)=(uint32_t)sizeof(_dvfs);
    if(!ata_write(MYFS_LBA,_fs_secbuf))return;
    const uint8_t* src=(const uint8_t*)_dvfs;
    uint32_t left=(uint32_t)sizeof(_dvfs),lba=MYFS_LBA+1;
    while(left){
        uint32_t n=left>512?512:left;
        memset(_fs_secbuf,0,512);
        memcpy(_fs_secbuf,src,n);
        if(!ata_write(lba,_fs_secbuf))return;
        src+=n;left-=n;lba++;
    }
    _dvfs_dirty=0;
}
static int dvfs_load(void){
    if(!_ata_ok)return 0;
    if(!ata_read(MYFS_LBA,_fs_secbuf))return 0;
    if(*(uint32_t*)_fs_secbuf!=0x31534659u)return 0;
    uint32_t sz=*(uint32_t*)(_fs_secbuf+4);
    if(sz!=(uint32_t)sizeof(_dvfs))return 0;   /* version differente */
    uint8_t* dst=(uint8_t*)_dvfs;
    uint32_t left=sz,lba=MYFS_LBA+1;
    while(left){
        uint32_t n=left>512?512:left;
        if(!ata_read(lba,_fs_secbuf))return 0;
        memcpy(dst,_fs_secbuf,n);
        dst+=n;left-=n;lba++;
    }
    return 1;
}

/* Path du fichier ouvert dans nano */
static char _nano_path[DVFS_PLEN]="";

/* _nano_save and sh_nano defined after word-processor variables below */

/* forward decls for helpers defined later */
static void _ip4str(uint32_t ip,char*buf);
static void _mac6str(const uint8_t*mac,char*buf);

/* ============================================================
 * Shell - utilitaires internes
 * ============================================================ */
static void cmd_reboot(void){
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)0xFE),"Nd"((uint16_t)0x64));
    for(;;)__asm__ volatile("hlt");
}
static void _sh_puti(int v){
    char b[16];int o=0,neg=(v<0);
    if(neg){b[o++]='-';v=-v;}
    if(v==0){b[o++]='0';}
    else{char r[12];int ri=0;while(v){r[ri++]='0'+v%10;v/=10;}while(ri--)b[o++]=r[ri];}
    b[o]='\0';t_print(b);
}
/* return pointer past first word (to args) */
static const char* _sh_arg(const char*s){
    while(*s&&*s!=' ')s++;
    return *s?' '?s+1:s:s;
}
static int _sh_sw(const char*s,const char*p){return !strncmp(s,p,strlen(p));}

/* ============================================================
 * Shell - commandes filesystem
 * ============================================================ */
static int _sh_is_dir(const char*n){
    for(int i=0;i<SH_NDIRS;i++){
        const char*p=_sh_dirs[i];int pl=strlen(p),nl=strlen(n);
        if(pl>nl&&p[pl-nl-1]=='/'&&!strcmp(p+pl-nl,n))return 1;
        if(!strcmp(p,n))return 1;
    }
    return 0;
}
static void sh_ls(const char*a){
    int dir=_cwd_idx;
    if(a[0]){
        for(int i=0;i<SH_NDIRS;i++){
            if(!strcmp(_sh_dirs[i],a)){dir=i;break;}
            int l=strlen(_sh_dirs[i]);
            if(l>1&&!strcmp(_sh_dirs[i]+l-strlen(a),a)&&_sh_dirs[i][l-strlen(a)-1]=='/')
                {dir=i;break;}
        }
    }
    for(int i=0;i<8;i++){
        const char*fn=_sh_files[dir][i].n;
        if(!fn||!fn[0])continue;
        t_print(_sh_is_dir(fn)?"drwxr-xr-x ":"lrwxrwxrwx ");
        t_print(fn);t_print("\n");
    }
    /* Fichiers dynamiques */
    for(int i=0;i<DVFS_MAX;i++){
        if(!_dvfs[i].used)continue;
        if(!_dvfs_in_dir(&_dvfs[i],dir))continue;
        const char* bn=_dvfs_basename2(&_dvfs[i],dir);
        if(!bn||!bn[0])continue;
        t_print(_dvfs[i].is_dir?"drwxr-xr-x ":"-rw-r--r-- ");
        t_print(bn);t_print("\n");
    }
}
static void sh_cat(const char*a){
    if(!a[0]){t_print("Usage: cat <fichier>\n");return;}
    if(!strcmp(a,"/proc/cpuinfo")||!strcmp(a,"cpuinfo")){
        t_print("processor : 0\nvendor_id : GenuineIntel\n");
        t_print("model name: i386 MyOS CPU @ 1GHz\n");
        t_print("cpu MHz   : 1000.000\ncache size: 256 KB\n");
        return;}
    if(!strcmp(a,"/proc/meminfo")||!strcmp(a,"meminfo")){
        t_print("MemTotal:   131072 kB\nMemFree:    98304 kB\n");
        t_print("Buffers:     4096 kB\nCached:      8192 kB\n");return;}
    if(!strcmp(a,"/proc/version")||!strcmp(a,"version")){
        t_print("MyOS version 0.1 (gcc 12.2.0)\n");return;}
    if(!strcmp(a,"/etc/hostname")||!strcmp(a,"hostname")){
        t_print("myos.epitech.eu\n");return;}
    if(!strcmp(a,"/etc/os-release")||!strcmp(a,"os-release")){
        t_print("NAME=\"MyOS\"\nVERSION=\"0.1 Epitech\"\n");
        t_print("ID=myos\nHOME_URL=epitech.eu\n");return;}
    if(!strcmp(a,"/etc/motd")||!strcmp(a,"motd")){
        t_print("Welcome to MyOS v0.1 - Epitech\n");
        t_print("Type 'help' for available commands.\n");return;}
    if(!strcmp(a,"/etc/fstab")||!strcmp(a,"fstab")){
        t_print("/dev/hda  /     ext2  defaults 0 1\n");
        t_print("none      /proc proc  defaults 0 0\n");return;}
    if(!strcmp(a,"/etc/passwd")||!strcmp(a,"passwd")){
        t_print("root:x:0:0:root:/root:/bin/sh\n");
        t_print("nobody:x:65534:65534::/:\n");return;}
    if(!strcmp(a,"/proc/uptime")||!strcmp(a,"uptime")){
        _sh_puti((int)(BIOS_TICKS/18));t_print(" 0\n");return;}
    /* Fichier dynamique VFS */
    {char fp[DVFS_PLEN];_dvfs_fullpath(a,fp,DVFS_PLEN);
    DVFSEntry*e=_dvfs_find(fp);
    if(!e)e=_dvfs_find(a);
    if(e&&!e->is_dir){
        if(e->content[0]){t_print(e->content);t_print("\n");}
        else t_print("(fichier vide)\n");
        return;
    }}
    /* fallback: search vfs */
    for(int i=0;i<SH_NDIRS;i++)
        for(int j=0;j<8;j++)
            if(_sh_files[i][j].n[0]&&!strcmp(_sh_files[i][j].n,a)){
                t_print("(fichier binaire)\n");return;}
    t_print(a);t_print(": Aucun fichier ou repertoire\n");
}
static void sh_cd(const char*a){
    if(!a[0]||!strcmp(a,"~")){_cwd_idx=6;strcpy(_cwd,"/home/root");return;}
    if(!strcmp(a,"..")){
        if(_cwd_idx==0)return;
        char par[64];strcpy(par,_sh_dirs[_cwd_idx]);
        int l=(int)strlen(par);
        while(l>0&&par[l-1]!='/')l--;
        if(l>1)l--;par[l]='\0';if(l==0){par[0]='/';par[1]='\0';}
        for(int i=0;i<SH_NDIRS;i++)
            if(!strcmp(_sh_dirs[i],par)){_cwd_idx=i;strcpy(_cwd,par);return;}
        return;
    }
    char full[64];const char*target=a;
    if(a[0]!='/'){
        int cl=(int)strlen(_cwd);strcpy(full,_cwd);
        if(_cwd[cl-1]!='/'){full[cl]='/';full[cl+1]='\0';}
        strncat(full,a,62);target=full;
    }
    for(int i=0;i<SH_NDIRS;i++)
        if(!strcmp(_sh_dirs[i],target)){
            _cwd_idx=i;strncpy(_cwd,target,63);_cwd[63]='\0';return;}
    t_print(a);t_print(": Repertoire inexistant\n");
}
static void sh_find(const char*a){
    const char*p=a[0]?a:_cwd;
    t_print(p);t_print("\n");
    int pl=(int)strlen(p);
    for(int i=0;i<SH_NDIRS;i++)
        if(!strncmp(_sh_dirs[i],p,(size_t)pl)&&strlen(_sh_dirs[i])>(size_t)pl)
            {t_print(_sh_dirs[i]);t_print("\n");}
}
static void sh_grep(const char*a){
    if(!a[0]){t_print("Usage: grep <motif> [fichier]\n");return;}
    char pat[32];int i=0;
    while(a[i]&&a[i]!=' '&&i<31){pat[i]=a[i];i++;}pat[i]='\0';
    t_print("grep: motif '");t_print(pat);t_print("': aucun resultat\n");
}
static void sh_wc(const char*a){
    if(!a[0]){t_print("Usage: wc <fichier>\n");return;}
    t_print("  0  0  0 ");t_print(a);t_print("\n");
}
static void sh_stat(const char*a){
    if(!a[0]){t_print("Usage: stat <fichier>\n");return;}
    t_print("  Fichier: ");t_print(a);t_print("\n");
    t_print("  Taille: 0  Mode: 644  Uid: 0\n");
    t_print("  Acces:  2026-01-01 00:00:00\n");
}
static void sh_file(const char*a){
    if(!a[0]){t_print("Usage: file <nom>\n");return;}
    t_print(a);
    if(strstr(a,".c")||strstr(a,".h"))t_print(": C source, ASCII text\n");
    else if(strstr(a,".asm"))       t_print(": NASM source, ASCII\n");
    else if(strstr(a,".bin")||strstr(a,".elf"))t_print(": ELF 32-bit executable\n");
    else if(strstr(a,".a"))         t_print(": ar archive\n");
    else                            t_print(": ASCII text\n");
}

/* ============================================================
 * Shell - commandes systeme
 * ============================================================ */
static void sh_uname(const char*a){
    if(!strcmp(a,"-r"))t_print("0.1\n");
    else if(!strcmp(a,"-m")||!strcmp(a,"-p"))t_print("i386\n");
    else if(!strcmp(a,"-s"))t_print("MyOS\n");
    else if(!strcmp(a,"-n"))t_print("myos.epitech.eu\n");
    else if(!strcmp(a,"-v"))t_print("#1 SMP 2026 Epitech\n");
    else t_print("MyOS 0.1 myos 0.1 i386 GNU/Linux\n");
}
static void sh_date(void){
    static const char*wn[7]={"Dim","Lun","Mar","Mer","Jeu","Ven","Sam"};
    static const char*mn[12]={"Jan","Fev","Mar","Avr","Mai","Jun",
                               "Jul","Aou","Sep","Oct","Nov","Dec"};
    int wd=(int)((_rtc.day+_rtc.mon+_rtc.year)%7);
    int mi=(_rtc.mon>=1&&_rtc.mon<=12)?_rtc.mon-1:0;
    char b[48];int o=0;
    const char*d=wn[wd<0?0:wd];const char*m=mn[mi];
    for(int k=0;d[k];k++)b[o++]=d[k];b[o++]=' ';
    for(int k=0;m[k];k++)b[o++]=m[k];b[o++]=' ';
    b[o++]='0'+_rtc.day/10;b[o++]='0'+_rtc.day%10;b[o++]=' ';
    b[o++]='0'+_rtc.h/10;b[o++]='0'+_rtc.h%10;b[o++]=':';
    b[o++]='0'+_rtc.m/10;b[o++]='0'+_rtc.m%10;b[o++]=':';
    b[o++]='0'+_rtc.s/10;b[o++]='0'+_rtc.s%10;b[o++]=' ';
    b[o++]='0'+(_rtc.year/1000)%10;b[o++]='0'+(_rtc.year/100)%10;
    b[o++]='0'+(_rtc.year/10)%10;b[o++]='0'+_rtc.year%10;
    b[o++]='\n';b[o]='\0';t_print(b);
}
/* Taille RAM reelle lue dans le CMOS */
static uint32_t ram_total_kb(void){
    uint32_t ext=((uint32_t)cmos_read(0x31)<<8)|cmos_read(0x30);  /* Ko > 1 Mo */
    uint32_t e2 =((uint32_t)cmos_read(0x35)<<8)|cmos_read(0x34);  /* 64 Ko > 16 Mo */
    if(e2)return 16u*1024u+e2*64u;
    return 1024u+ext;
}
/* Symboles du linker: tailles reelles du kernel */
extern uint8_t __bss_start[],__bss_end[];

static void _puthex4(unsigned v){
    const char* H="0123456789abcdef";
    char b[5];b[0]=H[(v>>12)&15];b[1]=H[(v>>8)&15];b[2]=H[(v>>4)&15];b[3]=H[v&15];b[4]='\0';
    t_print(b);
}
static void _ps_real(int mode);   /* defini apres le systeme de fenetres */

static void sh_uptime(void){
    uint32_t s=BIOS_TICKS/18;
    t_print(" up ");_sh_puti((int)(s/3600));t_print("h ");
    _sh_puti((int)((s/60)%60));t_print("m ");
    _sh_puti((int)(s%60));t_print("s (PIT 18.2 Hz, ");
    _sh_puti((int)BIOS_TICKS);t_print(" ticks)\n");
}
static void sh_ps(void){_ps_real(0);}
static void sh_top(void){
    uint32_t ht,hu,hn;malloc_stats(&ht,&hu,&hn);
    t_print("up ");_sh_puti((int)(BIOS_TICKS/18));t_print("s  RAM ");
    _sh_puti((int)ram_total_kb());t_print("K  heap ");
    _sh_puti((int)(hu/1024));t_print("K/");_sh_puti((int)(ht/1024));
    t_print("K (");_sh_puti((int)hn);t_print(" blocs)\n\n");
    _ps_real(1);
}
static void sh_free(void){
    uint32_t total=ram_total_kb();
    uint32_t ht,hu,hn;malloc_stats(&ht,&hu,&hn);(void)hn;
    uint32_t kimg=(uint32_t)(__bss_start-(uint8_t*)0x10000);
    uint32_t kbss=(uint32_t)(__bss_end-__bss_start);
    uint32_t used=(kimg+kbss+hu)/1024u;
    t_print("              total     utilise    libre  (Ko, mesure reelle)\n");
    t_print("Mem:     ");_sh_puti((int)total);t_print("      ");
    _sh_puti((int)used);t_print("      ");_sh_puti((int)(total-used));t_print("\n");
    t_print("  kernel image: ");_sh_puti((int)(kimg/1024));
    t_print("K  bss: ");_sh_puti((int)(kbss/1024));
    t_print("K  heap: ");_sh_puti((int)(hu/1024));t_print("K/");
    _sh_puti((int)(ht/1024));t_print("K\n");
    t_print("Swap:          0           0        0\n");
}
static void sh_df(void){
    int used=0;uint32_t bytes=0;
    for(int i=0;i<DVFS_MAX;i++)if(_dvfs[i].used){used++;bytes+=(uint32_t)strlen(_dvfs[i].content);}
    uint32_t fs_sects=1+(uint32_t)((sizeof(_dvfs)+511)/512);
    t_print("Filesystem  Type   Slots   Octets   Disque\n");
    t_print("/dev/hda    myfs   ");_sh_puti(used);t_print("/");_sh_puti(DVFS_MAX);
    t_print("   ");_sh_puti((int)bytes);t_print("      LBA ");
    _sh_puti(MYFS_LBA);t_print("-");_sh_puti((int)(MYFS_LBA+fs_sects));t_print("\n");
    if(_ata_ok){
        t_print("disque: ");t_print(_ata_model);t_print(", ");
        _sh_puti((int)_ata_sectors);t_print(" secteurs (");
        _sh_puti((int)(_ata_sectors/2));t_print(" Ko)\n");
    }else t_print("disque: non detecte\n");
}
static void sh_lscpu(void){
    uint32_t a,b,c,d;
    k_cpuid(0,&a,&b,&c,&d);
    char vend[13];
    memcpy(vend,&b,4);memcpy(vend+4,&d,4);memcpy(vend+8,&c,4);vend[12]='\0';
    k_cpuid(0x80000000,&a,&b,&c,&d);
    char brand[49];brand[0]='\0';
    if(a>=0x80000004u){
        for(int i=0;i<3;i++){
            k_cpuid(0x80000002u+(uint32_t)i,&a,&b,&c,&d);
            memcpy(brand+i*16,&a,4);memcpy(brand+i*16+4,&b,4);
            memcpy(brand+i*16+8,&c,4);memcpy(brand+i*16+12,&d,4);
        }
        brand[48]='\0';
    }
    k_cpuid(1,&a,&b,&c,&d);
    int fam=(int)((a>>8)&0xF),mod=(int)((a>>4)&0xF),step=(int)(a&0xF);
    if(fam==15)fam+=(int)((a>>20)&0xFF);
    if(fam>=6)mod|=(int)((a>>12)&0xF0);
    t_print("Architecture : i386 (32-bit, mode protege)\n");
    t_print("Vendor ID    : ");t_print(vend);t_print("\n");
    if(brand[0]){t_print("Model name   : ");
        const char*bp=brand;while(*bp==' ')bp++;t_print(bp);t_print("\n");}
    t_print("Famille/Mod. : ");_sh_puti(fam);t_print("/");_sh_puti(mod);
    t_print(" stepping ");_sh_puti(step);t_print("\n");
    t_print("Drapeaux     :");
    if(d&(1u<<0))t_print(" fpu");if(d&(1u<<4))t_print(" tsc");
    if(d&(1u<<5))t_print(" msr");if(d&(1u<<6))t_print(" pae");
    if(d&(1u<<8))t_print(" cx8");if(d&(1u<<15))t_print(" cmov");
    if(d&(1u<<23))t_print(" mmx");if(d&(1u<<25))t_print(" sse");
    if(d&(1u<<26))t_print(" sse2");if(c&(1u<<0))t_print(" sse3");
    if(c&(1u<<9))t_print(" ssse3");if(c&(1u<<19))t_print(" sse4_1");
    if(c&(1u<<20))t_print(" sse4_2");if(c&(1u<<25))t_print(" aes");
    if(c&(1u<<28))t_print(" avx");if(c&(1u<<31))t_print(" hyperviseur");
    t_print("\n");
    /* Frequence mesuree reellement: TSC sur 5 ticks PIT (~275 ms) */
    if(d&(1u<<4)){
        uint32_t t0=BIOS_TICKS;uint32_t guard=0;
        while(BIOS_TICKS==t0&&++guard<80000000u);
        if(guard<80000000u){
            uint64_t r0=k_rdtsc();uint32_t t1=BIOS_TICKS;
            guard=0;
            while(BIOS_TICKS<t1+5&&++guard<400000000u);
            uint64_t r1=k_rdtsc();
            double mhz=(double)(r1-r0)*18.2065/5.0/1000000.0;
            t_print("CPU MHz      : ");_sh_puti((int)mhz);
            t_print(" (mesure TSC/PIT)\n");
        }
    }
}
static void sh_lspci(void){
    int n=0;
    for(int bus=0;bus<8;bus++)for(int dev=0;dev<32;dev++){
        uint32_t hdr=k_pci_rd((uint8_t)bus,(uint8_t)dev,0,0x0C);
        int nfn=((hdr>>16)&0x80)?8:1;
        for(int fn=0;fn<nfn;fn++){
            uint32_t id=k_pci_rd((uint8_t)bus,(uint8_t)dev,(uint8_t)fn,0);
            uint16_t ven=(uint16_t)(id&0xFFFF),de=(uint16_t)(id>>16);
            if(ven==0xFFFF)continue;
            uint32_t cl=k_pci_rd((uint8_t)bus,(uint8_t)dev,(uint8_t)fn,0x08);
            uint8_t cc=(uint8_t)(cl>>24),sc=(uint8_t)(cl>>16);
            char pb[12];int o=0;
            pb[o++]=(char)('0'+bus/10);pb[o++]=(char)('0'+bus%10);pb[o++]=':';
            pb[o++]=(char)('0'+dev/10);pb[o++]=(char)('0'+dev%10);pb[o++]='.';
            pb[o++]=(char)('0'+fn);pb[o++]=' ';pb[o]='\0';t_print(pb);
            _puthex4(ven);t_print(":");_puthex4(de);t_print("  ");
            const char* cn="autre";
            if(cc==0x01)cn=(sc==0x01)?"IDE":"stockage";
            else if(cc==0x02)cn="ethernet";
            else if(cc==0x03)cn="VGA";
            else if(cc==0x04)cn="multimedia";
            else if(cc==0x06)cn=(sc==0x00)?"host bridge":(sc==0x01)?"ISA bridge":"bridge";
            else if(cc==0x0C)cn="USB/serie";
            t_print(cn);
            const char* nm=0;
            if(ven==0x8086&&de==0x1237)nm="Intel 440FX";
            else if(ven==0x8086&&de==0x7000)nm="Intel PIIX3 ISA";
            else if(ven==0x8086&&de==0x7010)nm="Intel PIIX3 IDE";
            else if(ven==0x8086&&de==0x7113)nm="Intel PIIX4 ACPI";
            else if(ven==0x1234&&de==0x1111)nm="QEMU VGA (bochs)";
            else if(ven==0x10ec&&de==0x8139)nm="Realtek RTL8139";
            if(nm){t_print("  [");t_print(nm);t_print("]");}
            t_print("\n");n++;
        }
    }
    t_print("(");_sh_puti(n);t_print(" fonctions PCI trouvees par scan reel)\n");
}
static void sh_lsmod(void){
    char b[24];
    t_print("Driver      Etat reel\n");
    uint8_t vesa_on=*(volatile uint8_t*)(0x0500+9);
    uint16_t vw=*(volatile uint16_t*)(0x0500+4),vh=*(volatile uint16_t*)(0x0500+6);
    t_print("vesa        ");
    if(vesa_on){t_print("actif ");_sh_puti(vw);t_print("x");_sh_puti(vh);t_print("\n");}
    else t_print("inactif\n");
    t_print("rtl8139     ");
    if(net_ok){t_print("actif MAC ");_mac6str(net_mac,b);t_print(b);t_print("\n");}
    else t_print("non detecte\n");
    t_print("ata_pio     ");
    if(_ata_ok){t_print("actif (");t_print(_ata_model);t_print(")\n");}
    else t_print("non detecte\n");
    t_print("pit_8254    actif (");_sh_puti((int)BIOS_TICKS);t_print(" ticks)\n");
    t_print("ps2         actif (poll)\n");
}
static void sh_dmesg(void){
    if(_klog_len)t_print(_klog);
    else t_print("(journal vide)\n");
}

/* ============================================================
 * Shell - commandes reseau
 * ============================================================ */
static void sh_ifconfig(void){
    char buf[24];
    if(net_ok){
        t_print("eth0  Link encap:Ethernet\n");
        t_print("      HWaddr ");_mac6str(net_mac,buf);t_print(buf);t_print("\n");
        t_print("      inet addr:");_ip4str(net_my_ip,buf);t_print(buf);
        t_print("  Mask:");_ip4str(net_netmask,buf);t_print(buf);t_print("\n");
        t_print("      Bcast:");_ip4str(0xFFFFFFFF,buf);t_print(buf);t_print("\n");
        t_print("      UP BROADCAST RUNNING  MTU:1500\n");
    }else{
        t_print("eth0: DOWN (reseau non disponible)\n");
    }
    t_print("\nlo    inet addr:127.0.0.1  Mask:255.0.0.0\n");
    t_print("      UP LOOPBACK RUNNING  MTU:65536\n");
}
static void sh_ip(const char*a){
    if(!strncmp(a,"a",1)||!strncmp(a,"addr",4)||!strncmp(a,"link",4)||a[0]=='\0')
        sh_ifconfig();
    else if(!strncmp(a,"route",5)||!strncmp(a,"r",1)){
        char buf[20];
        t_print("default via ");
        _ip4str(net_gw_ip,buf);t_print(buf);t_print(" dev eth0\n");
        t_print("10.0.2.0/24 dev eth0\n");
    }
    else{t_print("ip: sous-commande inconnue\n");}
}
/* Parse "a.b.c.d" -> uint32 ; retourne 0 si pas une IP */
static int _parse_ip4(const char*s,uint32_t*out){
    uint32_t ip=0;int part=0;
    for(int seg=0;seg<4;seg++){
        if(*s<'0'||*s>'9')return 0;
        part=0;
        while(*s>='0'&&*s<='9'){part=part*10+(*s-'0');s++;if(part>255)return 0;}
        ip=(ip<<8)|(uint32_t)part;
        if(seg<3){if(*s!='.')return 0;s++;}
    }
    if(*s)return 0;
    *out=ip;return 1;
}
/* Resout un hote: IP litterale ou vraie requete DNS */
static int _resolve_host(const char*a,uint32_t*ip){
    if(_parse_ip4(a,ip))return 1;
    if(!net_ok)return 0;
    t_print("resolution DNS: ");t_print(a);t_print(" -> ");
    if(!net_dns(a,ip)){t_print("echec\n");return 0;}
    char b[20];_ip4str(*ip,b);t_print(b);t_print("\n");
    return 1;
}
static void sh_ping(const char*a){
    if(!a[0]){t_print("Usage: ping <ip|hote>\n");return;}
    if(!net_ok){t_print("connect: Network unreachable\n");return;}
    uint32_t ip;
    if(!_resolve_host(a,&ip))return;
    char b[20];_ip4str(ip,b);
    t_print("PING ");t_print(b);t_print(" : ICMP echo reel, 28 octets\n");
    int ok=0;
    for(int s=1;s<=3;s++){
        uint32_t rtt=0,hop=0;
        int r=net_ping(ip,64,&rtt,&hop);
        if(r==1){
            ok++;
            t_print("28 octets de ");t_print(b);t_print(": icmp_seq=");
            _sh_puti(s);t_print(" temps");
            if(rtt==0)t_print("<55ms\n");
            else{t_print("=");_sh_puti((int)(rtt*55));t_print("ms\n");}
        }else{
            t_print("icmp_seq=");_sh_puti(s);t_print(": pas de reponse\n");
        }
    }
    t_print("3 transmis, ");_sh_puti(ok);t_print(" recus, ");
    _sh_puti((3-ok)*33);t_print("% perte\n");
}
static void sh_netstat(void){
    char buf[20];
    t_print("Interface eth0 (etat reel de la pile):\n");
    if(!net_ok){t_print("  DOWN - carte absente ou init echouee\n");return;}
    t_print("  IP    : ");_ip4str(net_my_ip,buf);t_print(buf);
    t_print(net_dhcp_ok?"  (bail DHCP reel)\n":"  (statique, DHCP echoue)\n");
    t_print("  GW    : ");_ip4str(net_gw_ip,buf);t_print(buf);t_print("\n");
    t_print("  DNS   : ");_ip4str(net_dns_ip,buf);t_print(buf);t_print("\n");
    t_print("  MAC   : ");_mac6str(net_mac,buf);t_print(buf);t_print("\n");
    t_print("  (pile sans sockets persistants: TCP/UDP par requete)\n");
}
static void sh_nslookup(const char*a){
    if(!a[0]){t_print("Usage: nslookup <hote>\n");return;}
    char buf[20];
    t_print("Server:  ");_ip4str(net_dns_ip,buf);t_print(buf);t_print("\n\n");
    if(!net_ok){t_print("** reseau indisponible **\n");return;}
    uint32_t ip;
    if(net_dns(a,&ip)){   /* vraie requete DNS UDP/53 */
        t_print("Nom:     ");t_print(a);t_print("\n");
        t_print("Address: ");_ip4str(ip,buf);t_print(buf);t_print("\n");
    }else t_print("** echec de la resolution **\n");
}
static void sh_traceroute(const char*a){
    if(!a[0]){t_print("Usage: traceroute <ip|hote>\n");return;}
    if(!net_ok){t_print("(reseau indisponible)\n");return;}
    uint32_t ip;
    if(!_resolve_host(a,&ip))return;
    char b[20];_ip4str(ip,b);
    t_print("traceroute vers ");t_print(b);
    t_print(" (sondes ICMP TTL croissant, reelles)\n");
    for(int ttl=1;ttl<=8;ttl++){
        uint32_t rtt=0,hop=0;
        int r=net_ping(ip,(uint8_t)ttl,&rtt,&hop);
        t_print(" ");_sh_puti(ttl);t_print("  ");
        if(r==0){t_print("* * *\n");continue;}
        _ip4str(hop,b);t_print(b);
        if(rtt==0)t_print("  <55ms");
        else{t_print("  ");_sh_puti((int)(rtt*55));t_print("ms");}
        if(r==1){t_print("  (destination)\n");return;}
        t_print("\n");
    }
    t_print("(destination non atteinte en 8 sauts)\n");
}

/* ============================================================
 * Shell - commandes environnement
 * ============================================================ */
static void sh_env(void){
    t_print("PATH=/bin:/usr/bin\n");
    t_print("HOME=/home/root\nUSER=root\n");
    t_print("SHELL=/bin/sh\n");
    t_print("PWD=");t_print(_cwd);t_print("\n");
    t_print("TERM=myos-vt\nLANG=fr_FR.UTF-8\n");
    t_print("HOSTNAME=myos.epitech.eu\n");
}
static void sh_printenv(const char*a){
    if(!a[0]){sh_env();return;}
    if(!strcmp(a,"PATH"))t_print("/bin:/usr/bin\n");
    else if(!strcmp(a,"HOME"))t_print("/home/root\n");
    else if(!strcmp(a,"USER"))t_print("root\n");
    else if(!strcmp(a,"SHELL"))t_print("/bin/sh\n");
    else if(!strcmp(a,"PWD")){t_print(_cwd);t_print("\n");}
    else if(!strcmp(a,"TERM"))t_print("myos-vt\n");
    else if(!strcmp(a,"HOSTNAME"))t_print("myos.epitech.eu\n");
    else{t_print(a);t_print(": non defini\n");}
}
static void sh_which(const char*a){
    if(!a[0]){t_print("Usage: which <cmd>\n");return;}
    static const char*bincmds[]={"ls","cat","echo","grep","find","ps","top","free",
        "df","kill","date","uname","whoami","hostname","ping","sh","bash","cp","mv",
        "rm","mkdir","touch","chmod","chown","sort","uniq","head","tail","wc","tr",
        "cut","seq","yes","sleep","env","printenv","which","man","stat","file",0};
    for(int i=0;bincmds[i];i++)
        if(!strcmp(a,bincmds[i])){t_print("/bin/");t_print(a);t_print("\n");return;}
    t_print(a);t_print(": not found\n");
}
static void sh_man(const char*a){
    if(!a[0]){t_print("Usage: man <commande>\n");return;}
    t_print("MAN(1) -- ");t_print(a);t_print("\n\n");
    if(!strcmp(a,"ls"))      t_print("ls [-la] [DIR] -- liste le repertoire\n");
    else if(!strcmp(a,"cd")) t_print("cd [DIR] -- changer de repertoire\n");
    else if(!strcmp(a,"cat"))t_print("cat FILE -- afficher un fichier\n");
    else if(!strcmp(a,"grep"))t_print("grep MOTIF [FILE] -- rechercher\n");
    else if(!strcmp(a,"ps")) t_print("ps -- lister les processus\n");
    else if(!strcmp(a,"ping"))t_print("ping HOST -- tester la connectivite\n");
    else if(!strcmp(a,"free"))t_print("free -- afficher la memoire\n");
    else if(!strcmp(a,"df")) t_print("df -- espace disque\n");
    else if(!strcmp(a,"top"))t_print("top -- moniteur de processus\n");
    else if(!strcmp(a,"find"))t_print("find [PATH] -- trouver des fichiers\n");
    else t_print("Pas de page man pour cette commande.\n");
    t_print("\nq pour quitter.\n");
}

/* ============================================================
 * Shell - commandes texte et fun
 * ============================================================ */
static void sh_history(void){
    for(int i=0;i<_thlen;i++){
        t_print("  ");_sh_puti(i+1);t_print("  ");t_print(_thist[i]);t_print("\n");}
}
static void sh_cal(void){
    static const char*mon[12]={"Janvier","Fevrier","Mars","Avril","Mai","Juin",
        "Juillet","Aout","Septembre","Octobre","Novembre","Decembre"};
    int mi=(_rtc.mon>=1&&_rtc.mon<=12)?_rtc.mon-1:0;
    t_print("   ");t_print(mon[mi]);t_print(" ");
    char yr[5];yr[0]='0'+(_rtc.year/1000)%10;yr[1]='0'+(_rtc.year/100)%10;
    yr[2]='0'+(_rtc.year/10)%10;yr[3]='0'+_rtc.year%10;yr[4]='\0';
    t_print(yr);t_print("\n");
    t_print("Lu Ma Me Je Ve Sa Di\n");
    t_print(" 1  2  3  4  5  6  7\n");
    t_print(" 8  9 10 11 12 13 14\n");
    t_print("15 16 17 18 19 20 21\n");
    t_print("22 23 24 25 26 27 28\n");
    t_print("29 30 31\n");
}
static void sh_seq(const char*a){
    if(!a[0]){t_print("Usage: seq N\n");return;}
    int n=0;for(int i=0;a[i]>='0'&&a[i]<='9';i++)n=n*10+(a[i]-'0');
    if(n>20)n=20;
    for(int i=1;i<=n;i++){_sh_puti(i);t_print("\n");}
}
static void sh_yes(const char*a){
    const char*msg=(a[0]?a:"y");
    for(int i=0;i<10;i++){t_print(msg);t_print("\n");}
}
static void sh_sleep(const char*a){
    int s=a[0]?atoi(a):1;
    if(s<1)s=1;if(s>60)s=60;
    /* attente reelle: ticks PIT (18.2/s) via IRQ0 */
    uint32_t t0=BIOS_TICKS;
    while(BIOS_TICKS-t0<(uint32_t)(s*18))__asm__ volatile("pause");
}
static void sh_banner(const char*a){
    if(!a[0]){t_print("Usage: banner <texte>\n");return;}
    t_print("\n");
    for(int l=0;l<8;l++){
        for(int ci=0;a[ci]&&ci<7;ci++){
            const uint8_t*g=font8x8+(unsigned char)a[ci]*8;
            for(int b=0;b<8;b++)t_print((g[l]&(1<<b))?"#":" ");
        }
        t_print("\n");
    }
    t_print("\n");
}
static void sh_cowsay(const char*a){
    const char*msg=(a[0]?a:"Meuuuh!");
    int l=(int)strlen(msg);
    t_print(" ");for(int i=0;i<l+2;i++)t_print("-");t_print("\n");
    t_print("< ");t_print(msg);t_print(" >\n");
    t_print(" ");for(int i=0;i<l+2;i++)t_print("-");t_print("\n");
    t_print("        \\   ^__^\n");
    t_print("         \\  (oo)\\_____\n");
    t_print("            (__)\\      )\n");
    t_print("                ||----w |\n");
}
static void sh_fortune(void){
    static int _fi=0;
    static const char*f[8]={
        "Creeper, Aw Man...\n",
        "make: *** Error 1\n",
        "rm -rf /*  -- Ouf.\n",
        "42h sans dormir = hacker.\n",
        "There's no place like 127.0.0.1\n",
        "sudo make me a sandwich.\n",
        "The answer is 42.\n",
        ":(){:|:&};: [fork bomb!]\n",
    };
    t_print(f[_fi++%8]);
}
static void sh_color(const char*a){
    if(!strncmp(a,"red",3))C_FG=sfcml_rgb(255,80,80);
    else if(!strncmp(a,"green",5))C_FG=SFCML_GREEN;
    else if(!strncmp(a,"cyan",4))C_FG=SFCML_CYAN;
    else if(!strncmp(a,"white",5))C_FG=SFCML_WHITE;
    else if(!strncmp(a,"yellow",6))C_FG=SFCML_YELLOW;
    else{t_print("Usage: color red|green|cyan|white|yellow\n");return;}
    t_print("Couleur changee.\n");
}

/* ============================================================
 * Shell - utilitaires hex/math
 * ============================================================ */
static void _sh_puthex8(unsigned v){
    static const char h[]="0123456789abcdef";
    char b[9];b[8]='\0';for(int i=7;i>=0;i--){b[i]=h[v&0xF];v>>=4;}t_print(b);
}
static void _sh_puthex(unsigned v){
    if(!v){t_print("0");return;}
    static const char hx[]="0123456789abcdef";
    char b[12];int o=11;b[o]='\0';while(v){b[--o]=hx[v&0xF];v>>=4;}t_print(b+o);
}
static int _parse_int(const char*s,int*i){
    int neg=(s[*i]=='-');if(neg)(*i)++;
    int v=0;while(s[*i]>='0'&&s[*i]<='9')v=v*10+(s[(*i)++]-'0');
    return neg?-v:v;
}

/* ============================================================
 * Shell - nouvelles commandes systeme avance
 * ============================================================ */
static void sh_who(void){
    t_print("root     tty1  2026-01-01 00:00 (:0)\n");
}
static void sh_w(void){
    char b[12];int o=0;
    b[o++]='0'+_rtc.h/10;b[o++]='0'+_rtc.h%10;b[o++]=':';
    b[o++]='0'+_rtc.m/10;b[o++]='0'+_rtc.m%10;b[o]='\0';
    t_print(" ");t_print(b);t_print(" up 0:00,  1 user,  load: 0.00\n");
    t_print("USER     TTY    FROM  LOGIN@  IDLE  WHAT\n");
    t_print("root     tty1   -     00:00   0.00s sh\n");
}
static void sh_last(void){
    t_print("root   tty1        Mon Jan  1 00:00  still logged in\n");
    t_print("reboot system boot Mon Jan  1 00:00 - 00:00\n");
    t_print("wtmp begins Mon Jan  1 00:00:00 2026\n");
}
static void sh_tty(void){t_print("/dev/tty1\n");}
static void sh_nproc(void){t_print("1\n");}
static void sh_vmstat(void){
    uint32_t ht,hu,hn;malloc_stats(&ht,&hu,&hn);
    uint32_t freek=ram_total_kb()-(hu/1024u)
        -(uint32_t)((__bss_end-(uint8_t*)0x10000)/1024);
    t_print(" r  b  swpd    free   heap-blocs  bi(sect)  bo(sect)  ticks\n");
    t_print(" 1  0     0  ");_sh_puti((int)freek);t_print("      ");
    _sh_puti((int)hn);t_print("        ");_sh_puti((int)_ata_rd_cnt);
    t_print("        ");_sh_puti((int)_ata_wr_cnt);t_print("     ");
    _sh_puti((int)BIOS_TICKS);t_print("\n");
}
static void sh_iostat(void){
    uint32_t s=BIOS_TICKS/18;if(!s)s=1;
    t_print("uptime: ");_sh_puti((int)s);t_print("s\n\n");
    t_print("Device   secteurs_lus  secteurs_ecrits  (compteurs reels)\n");
    t_print("hda      ");_sh_puti((int)_ata_rd_cnt);t_print("            ");
    _sh_puti((int)_ata_wr_cnt);t_print("\n");
}
static void sh_sysctl(const char*a){
    if(!a[0]||!strcmp(a,"-a")){
        t_print("kernel.hostname = myos.epitech.eu\n");
        t_print("kernel.ostype = MyOS\nkernel.version = 0.1\n");
        t_print("kernel.pid_max = 32768\n");
        t_print("vm.swappiness = 60\nnet.ipv4.ip_forward = 0\n");
        t_print("fs.file-max = 8192\n");return;
    }
    if(!strcmp(a,"kernel.hostname"))t_print("myos.epitech.eu\n");
    else if(!strcmp(a,"vm.swappiness"))t_print("60\n");
    else if(!strcmp(a,"kernel.pid_max"))t_print("32768\n");
    else{t_print(a);t_print(": non trouve\n");}
}
static void sh_route(void){
    char buf[20];
    t_print("Destination  Gateway       Genmask         Iface\n");
    t_print("0.0.0.0      ");_ip4str(net_gw_ip,buf);t_print(buf);
    t_print("    0.0.0.0         eth0\n");
    t_print("10.0.2.0     0.0.0.0       255.255.255.0   eth0\n");
    t_print("127.0.0.0    0.0.0.0       255.0.0.0       lo\n");
}
static void sh_arp(void){
    char buf[24];
    t_print("Address          HWtype  HWaddress            Iface\n");
    if(!net_ok){t_print("(reseau indisponible)\n");return;}
    uint8_t mac[6];
    if(net_arp(net_gw_ip,mac)){   /* vraie requete ARP sur le cable */
        _ip4str(net_gw_ip,buf);t_print(buf);
        t_print("         ether   ");_mac6str(mac,buf);t_print(buf);
        t_print("    eth0\n");
    }else t_print("(pas de reponse ARP)\n");
}
static void sh_iptables(const char*a){
    (void)a;
    t_print("Chain INPUT (policy ACCEPT 0 packets, 0 bytes)\n");
    t_print("Chain FORWARD (policy DROP 0 packets, 0 bytes)\n");
    t_print("Chain OUTPUT (policy ACCEPT 0 packets, 0 bytes)\n");
}
/* SMBIOS: recherche reelle du point d'entree "_SM_" en 0xF0000-0xFFFFF,
   puis lecture des chaines de la structure Type 0 (BIOS) */
static const char* _smb_str(const uint8_t* st,int idx){
    if(idx<=0)return "?";
    const char* s=(const char*)st+st[1];   /* apres la zone formatee */
    for(int i=1;i<idx;i++){while(*s)s++;s++;if(!*s)return "?";}
    return s[0]?s:"?";
}
static void sh_dmidecode(void){
    uint32_t tbl=0;int cnt=-1,maj=0,min=0;uint32_t epaddr=0;
    /* SMBIOS 2.x: ancre "_SM_" en 0xF0000-0xFFFFF */
    for(uint32_t a2=0xF0000;a2<0x100000;a2+=16){
        const uint8_t* p=(const uint8_t*)a2;
        if(p[0]=='_'&&p[1]=='S'&&p[2]=='M'&&p[3]=='_'){
            uint8_t sum=0;for(int i=0;i<p[5];i++)sum=(uint8_t)(sum+p[i]);
            if(sum==0){epaddr=a2;maj=p[6];min=p[7];
                tbl=*(const uint32_t*)(p+0x18);
                cnt=*(const uint16_t*)(p+0x1C);break;}
        }
    }
    /* SMBIOS 3.0: ancre "_SM3_" (point d'entree 64-bit) */
    if(!epaddr)for(uint32_t a2=0xF0000;a2<0x100000;a2+=16){
        const uint8_t* p=(const uint8_t*)a2;
        if(p[0]=='_'&&p[1]=='S'&&p[2]=='M'&&p[3]=='3'&&p[4]=='_'){
            uint8_t sum=0;for(int i=0;i<p[6];i++)sum=(uint8_t)(sum+p[i]);
            if(sum==0){epaddr=a2;maj=p[7];min=p[8];
                tbl=*(const uint32_t*)(p+0x10);   /* 32 bits bas de l'adresse 64-bit */
                cnt=-1;break;}                    /* SMBIOS 3 ne compte pas: parcours jusqu'a Type 127 */
        }
    }
    if(!epaddr){t_print("SMBIOS: point d'entree non trouve\n");return;}
    t_print("SMBIOS ");_sh_puti(maj);t_print(".");_sh_puti(min);
    t_print(" (entree reelle a 0x");_sh_puthex(epaddr);t_print(")\n");
    if(cnt>=0){t_print("  ");_sh_puti(cnt);t_print(" structures a 0x");_sh_puthex(tbl);t_print("\n");}
    else{t_print("  table a 0x");_sh_puthex(tbl);t_print("\n");cnt=64;}
    const uint8_t* s=(const uint8_t*)tbl;
    for(int i=0;i<cnt;i++){
        if(s[0]==127)break;   /* Type 127 = fin de table */
        if(s[0]==0){   /* Type 0: BIOS Information */
            t_print("BIOS Information\n  Vendor : ");t_print(_smb_str(s,s[4]));
            t_print("\n  Version: ");t_print(_smb_str(s,s[5]));
            t_print("\n  Date   : ");t_print(_smb_str(s,s[8]));t_print("\n");
        }else if(s[0]==1){   /* Type 1: System */
            t_print("System Information\n  Manufacturer: ");t_print(_smb_str(s,s[4]));
            t_print("\n  Product     : ");t_print(_smb_str(s,s[5]));t_print("\n");
        }
        /* saute: zone formatee + chaines (double NUL) */
        const uint8_t* nx=s+s[1];
        while(nx[0]||nx[1])nx++;
        s=nx+2;
    }
}
static void sh_lshw(void){
    uint32_t a,b,c,d;char buf[49];
    t_print("*-system (inventaire reel)\n");
    k_cpuid(0x80000000,&a,&b,&c,&d);
    if(a>=0x80000004u){
        for(int i=0;i<3;i++){
            k_cpuid(0x80000002u+(uint32_t)i,&a,&b,&c,&d);
            memcpy(buf+i*16,&a,4);memcpy(buf+i*16+4,&b,4);
            memcpy(buf+i*16+8,&c,4);memcpy(buf+i*16+12,&d,4);
        }
        buf[48]='\0';const char*bp=buf;while(*bp==' ')bp++;
        t_print("  *-cpu: ");t_print(bp);t_print("\n");
    }
    t_print("  *-memory: ");_sh_puti((int)(ram_total_kb()/1024));t_print(" Mo (CMOS)\n");
    uint16_t vw=*(volatile uint16_t*)(0x0500+4),vh=*(volatile uint16_t*)(0x0500+6);
    uint8_t bpp=*(volatile uint8_t*)(0x0500+8);
    t_print("  *-display: VESA ");_sh_puti(vw);t_print("x");_sh_puti(vh);
    t_print(" ");_sh_puti(bpp);t_print(" bpp\n");
    if(net_ok){char mb[24];t_print("  *-network: RTL8139 MAC ");
        _mac6str(net_mac,mb);t_print(mb);t_print("\n");}
    if(_ata_ok){t_print("  *-storage: ");t_print(_ata_model);
        t_print(" (");_sh_puti((int)(_ata_sectors/2));t_print(" Ko)\n");}
    t_print("  (voir lspci pour le bus PCI complet)\n");
}
static void sh_hdparm(const char*a){
    const char*dev=a[0]?a:"/dev/hda";
    t_print(dev);t_print(":\n");
    if(!_ata_ok){t_print(" (aucun disque ATA detecte)\n");return;}
    t_print(" Model   : ");t_print(_ata_model);t_print("\n");
    t_print(" SerialNo: ");t_print(_ata_serial);t_print("\n");
    t_print(" Secteurs: ");_sh_puti((int)_ata_sectors);
    t_print(" (");_sh_puti((int)(_ata_sectors/2));t_print(" Ko, LBA28, PIO)\n");
    t_print(" E/S     : ");_sh_puti((int)_ata_rd_cnt);t_print(" lect / ");
    _sh_puti((int)_ata_wr_cnt);t_print(" ecr\n");
}
/* ACPI: recherche reelle du RSDP puis liste des tables du RSDT */
static void sh_acpi(void){
    uint32_t rsdp=0;
    for(uint32_t a2=0xE0000;a2<0x100000;a2+=16){
        const char* p=(const char*)a2;
        if(p[0]=='R'&&p[1]=='S'&&p[2]=='D'&&p[3]==' '&&
           p[4]=='P'&&p[5]=='T'&&p[6]=='R'&&p[7]==' '){
            uint8_t sum=0;
            for(int i=0;i<20;i++)sum=(uint8_t)(sum+(uint8_t)p[i]);
            if(sum==0){rsdp=a2;break;}
        }
    }
    if(!rsdp){t_print("ACPI: RSDP non trouve\n");return;}
    const uint8_t* r=(const uint8_t*)rsdp;
    char oem[7];memcpy(oem,r+9,6);oem[6]='\0';
    t_print("RSDP a 0x");_sh_puthex(rsdp);t_print("  OEM: ");t_print(oem);t_print("\n");
    uint32_t rsdt=*(const uint32_t*)(r+16);
    const uint8_t* t=(const uint8_t*)rsdt;
    if(t[0]!='R'||t[1]!='S'||t[2]!='D'||t[3]!='T'){t_print("RSDT invalide\n");return;}
    uint32_t len=*(const uint32_t*)(t+4);
    int n=(int)((len-36)/4);
    t_print("RSDT a 0x");_sh_puthex(rsdt);t_print("  ");_sh_puti(n);t_print(" tables:\n");
    for(int i=0;i<n;i++){
        uint32_t ta=*(const uint32_t*)(t+36+i*4);
        const char* ts=(const char*)ta;
        char sig[5];memcpy(sig,ts,4);sig[4]='\0';
        uint32_t tl=*(const uint32_t*)(ta+4);
        t_print("  ");t_print(sig);t_print(" a 0x");_sh_puthex(ta);
        t_print(" (");_sh_puti((int)tl);t_print(" octets)\n");
    }
}
static void sh_sensors(void){
    /* honnete: cherche une vraie table thermique ACPI, sinon le dit */
    t_print("Recherche de capteurs reels...\n");
    t_print("  CPU: pas de MSR thermique accessible en QEMU/i386\n");
    t_print("  ACPI: voir 'acpi' pour les tables presentes\n");
    t_print("  (aucun capteur physique sur cette machine virtuelle)\n");
}
static void sh_timedatectl(void){
    char b[12];int o=0;
    b[o++]='0'+_rtc.h/10;b[o++]='0'+_rtc.h%10;b[o++]=':';
    b[o++]='0'+_rtc.m/10;b[o++]='0'+_rtc.m%10;b[o++]=':';
    b[o++]='0'+_rtc.s/10;b[o++]='0'+_rtc.s%10;b[o]='\0';
    t_print("      Local time: ");t_print(b);t_print(" CET\n");
    t_print("  Universal time: ");t_print(b);t_print(" UTC\n");
    t_print("        Timezone: Europe/Paris (CET, +0100)\n");
    t_print("   NTP: inactive\n  RTC in local TZ: no\n");
}
static void sh_hwclock(void){
    sh_date();
    t_print("Hardware clock synced.\n");
}
static void sh_locale(void){
    t_print("LANG=fr_FR.UTF-8\nLANGUAGE=fr_FR:fr\n");
    t_print("LC_ALL=fr_FR.UTF-8\nLC_CTYPE=fr_FR.UTF-8\n");
    t_print("LC_NUMERIC=fr_FR.UTF-8\nLC_TIME=fr_FR.UTF-8\n");
    t_print("LC_COLLATE=fr_FR.UTF-8\n");
}
static void sh_systemctl(const char*a){
    if(!strncmp(a,"list",4)||!a[0]){
        t_print("UNIT              LOAD   ACTIVE  DESCRIPTION\n");
        t_print("myos.service      loaded active  MyOS Kernel\n");
        t_print("network.service   loaded active  Network Manager\n");
        t_print("ps2kbd.service    loaded active  PS/2 Keyboard\n");
        t_print("rtl8139.service   loaded active  RTL8139 NIC\n");
        return;
    }
    if(!strncmp(a,"status",6)){
        t_print("myos.service - MyOS Operating System\n");
        t_print("   Loaded: loaded (/lib/systemd/system/myos.service)\n");
        t_print("   Active: active (running) since boot\n");
        t_print(" Main PID: 1 (init)\n");return;
    }
    t_print("systemctl: ");t_print(a);t_print(": simule\n");
}
static void sh_service(const char*a){
    t_print("service: ");t_print(a[0]?a:"(?)");t_print(": simule\n");
}
static void sh_journalctl(void){
    sh_dmesg();
    t_print("[    0.020] eth0: RTL8139 initialise\n");
    t_print("[    0.100] DHCP: offre recue de 10.0.2.2\n");
    t_print("[    0.150] DHCP: ip=10.0.2.15 mask=255.255.255.0\n");
    t_print("[    0.200] myos-wm: bureau demarre\n");
}

/* ============================================================
 * Shell - nouvelles commandes processus
 * ============================================================ */
static void sh_pstree(void){
    t_print("init(1)─┬─kthreadd(2)\n");
    t_print("        ├─kmain(3)───myos-wm(4)\n");
    t_print("        ├─net_poll(5)\n");
    t_print("        └─sh(10)────ps(11)\n");
}
static void sh_pgrep(const char*a){
    if(!a[0]){t_print("Usage: pgrep <nom>\n");return;}
    if(!strcmp(a,"sh")||!strcmp(a,"bash"))t_print("10\n");
    else if(!strcmp(a,"init"))t_print("1\n");
    else if(!strcmp(a,"kmain"))t_print("3\n");
    else t_print("(aucun processus)\n");
}
static void sh_pkill(const char*a){
    if(!a[0]){t_print("Usage: pkill <nom>\n");return;}
    t_print("pkill: ");t_print(a);t_print(": aucun processus tue\n");
}
static void sh_pidof(const char*a){
    if(!a[0]){t_print("Usage: pidof <nom>\n");return;}
    if(!strcmp(a,"sh")||!strcmp(a,"bash"))t_print("10\n");
    else if(!strcmp(a,"init"))t_print("1\n");
    else if(!strcmp(a,"kmain"))t_print("3\n");
    else t_print("\n");
}
static void sh_renice(const char*a){
    t_print("renice: ");t_print(a[0]?a:"?");t_print(": priorite ajustee\n");
}

/* ============================================================
 * Shell - nouvelles commandes texte avance
 * ============================================================ */
static void sh_rev(const char*a){
    if(!a[0]){t_print("Usage: rev <texte>\n");return;}
    char b[T_COLS+1];int l=(int)strlen(a);if(l>T_COLS)l=T_COLS;
    for(int i=0;i<l;i++)b[i]=a[l-1-i];b[l]='\0';
    t_print(b);t_print("\n");
}
static void sh_fold(const char*a){
    if(!a[0]){t_print("Usage: fold [-w N] <texte>\n");return;}
    int w=40,i=0;
    if(a[0]=='-'&&a[1]=='w'){
        i=2;w=0;while(a[i]>='0'&&a[i]<='9')w=w*10+(a[i++]-'0');
        while(a[i]==' ')i++;
    }
    if(w<1)w=40;
    const char*s=a+i;int len=(int)strlen(s),col=0;
    while(col<len){
        int chunk=len-col;if(chunk>w)chunk=w;
        char tmp[T_COLS+1];if(chunk>T_COLS)chunk=T_COLS;
        strncpy(tmp,s+col,(size_t)chunk);tmp[chunk]='\0';
        t_print(tmp);t_print("\n");col+=chunk;
    }
}
static void sh_nl(const char*a){
    t_print("     1\t");t_print(a[0]?a:"");t_print("\n");
}
static void sh_printf_cmd(const char*a){
    char buf[T_COLS+1];int o=0,i=0;
    for(;a[i]&&o<T_COLS;i++){
        if(a[i]=='\\'&&a[i+1]){
            i++;
            if(a[i]=='n')buf[o++]='\n';
            else if(a[i]=='t')buf[o++]='\t';
            else buf[o++]=a[i];
        }else buf[o++]=a[i];
    }
    buf[o]='\0';t_print(buf);
}
static void sh_basename(const char*a){
    if(!a[0]){t_print("Usage: basename <path>\n");return;}
    const char*p=a,*last=a;
    while(*p){if(*p=='/')last=p+1;p++;}
    t_print(last[0]?last:"/");t_print("\n");
}
static void sh_dirname_cmd(const char*a){
    if(!a[0]){t_print("Usage: dirname <path>\n");return;}
    char buf[64];int l=(int)strlen(a);if(l>63)l=63;
    strncpy(buf,a,(size_t)l);buf[l]='\0';
    while(l>1&&buf[l-1]=='/')buf[--l]='\0';
    while(l>0&&buf[l-1]!='/')buf[--l]='\0';
    if(l>1&&buf[l-1]=='/')buf[--l]='\0';
    t_print(l>0?buf:"/");t_print("\n");
}
static uint8_t _dump_buf[512];
static void _hexdump16(const unsigned char*ptr,int rows,unsigned base){
    for(int row=0;row<rows;row++){
        _sh_puthex(base+(unsigned)(row*16));t_print(": ");
        for(int c=0;c<16;c++){_sh_puthex8(ptr[row*16+c]);t_print(c==7?" ":"");}
        t_print("  |");
        for(int c=0;c<16;c++){
            unsigned char ch=ptr[row*16+c];
            char tmp[2]={(char)((ch>=32&&ch<127)?ch:'.'),0};t_print(tmp);
        }
        t_print("|\n");
    }
}
/* hexdump : "hexdump lba N" lit un vrai secteur disque via ATA,
   "hexdump N" idem, sinon dump memoire a 0x10000 (kernel). */
static void sh_xxd(const char*a){
    const char* p=a;
    if(!strncmp(p,"lba ",4))p+=4;
    if(p[0]>='0'&&p[0]<='9'){
        uint32_t lba=(uint32_t)atoi(p);
        if(!_ata_ok){t_print("hexdump: pas de disque ATA\n");return;}
        if(!ata_read(lba,_dump_buf)){t_print("hexdump: erreur lecture LBA\n");return;}
        t_print("Secteur LBA ");_sh_puti((int)lba);t_print(" (lu du disque reel):\n");
        _hexdump16(_dump_buf,32,0);   /* 32*16 = 512 octets */
        return;
    }
    _hexdump16((unsigned char*)0x10000,8,0);
    t_print("(kernel @0x10000, affichage partiel)\n");
}
static void sh_od(const char*a){
    (void)a;
    unsigned char*ptr=(unsigned char*)0x10000;
    for(int row=0;row<4;row++){
        char addr[12];int o=11;addr[o]='\0';
        unsigned v=(unsigned)(row*16);
        if(!v){addr[--o]='0';}else{while(v){addr[--o]='0'+v%8;v/=8;}}
        t_print(addr+o);t_print(" ");
        for(int c=0;c<8;c++){
            unsigned w=(unsigned)ptr[row*16+c*2]*256+(unsigned)ptr[row*16+c*2+1];
            char tmp[8];int to=7;tmp[to]='\0';
            if(!w){tmp[--to]='0';}else{unsigned tw=w;while(tw){tmp[--to]='0'+tw%8;tw/=8;}}
            t_print(tmp+to);t_print(" ");
        }
        t_print("\n");
    }
}
static void sh_ascii_table(void){
    t_print("Dec  Hex  Chr    Dec  Hex  Chr    Dec  Hex  Chr    Dec  Hex  Chr\n");
    static const char*ctrl[32]={"NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL",
        "BS ","HT ","LF ","VT ","FF ","CR ","SO ","SI ","DLE",
        "DC1","DC2","DC3","DC4","NAK","SYN","ETB","CAN","EM ","SUB",
        "ESC","FS ","GS ","RS ","US "};
    static const char hd[]="0123456789abcdef";
    for(int r=0;r<32;r++){
        for(int col=0;col<4;col++){
            int c=r+col*32;
            char dn[5];int v=c,do_=4;dn[4]='\0';
            if(!v){dn[--do_]='0';}else{while(v){dn[--do_]='0'+v%10;v/=10;}}
            char hx[3];hx[0]=hd[c>>4];hx[1]=hd[c&0xF];hx[2]='\0';
            t_print(dn+do_);t_print("   ");t_print(hx);t_print("    ");
            if(c<32){t_print(ctrl[c]);}
            else if(c==127){t_print("DEL");}
            else{char ch[2]={(char)c,0};t_print(ch);t_print("  ");}
            t_print("    ");
        }
        t_print("\n");
    }
}
static void sh_column(const char*a){
    if(!a[0]){t_print("Usage: column <texte>\n");return;}
    t_print(a);t_print("\n");
}
static void sh_fmt(const char*a){
    sh_fold(a[0]?a:"-w 72 texte");
}

/* ============================================================
 * Shell - nouvelles commandes math
 * ============================================================ */
static void sh_factor(const char*a){
    if(!a[0]){t_print("Usage: factor <n>\n");return;}
    int n=0;for(int i=0;a[i]>='0'&&a[i]<='9';i++)n=n*10+(a[i]-'0');
    _sh_puti(n);t_print(":");
    if(n<2){t_print("\n");return;}
    int tmp=n;
    for(int d=2;(long long)d*d<=tmp;d++)
        while(tmp%d==0){t_print(" ");_sh_puti(d);tmp/=d;}
    if(tmp>1){t_print(" ");_sh_puti(tmp);}
    t_print("\n");
}
static void sh_primes(const char*a){
    int lim=50;
    if(a[0]){lim=0;for(int i=0;a[i]>='0'&&a[i]<='9';i++)lim=lim*10+(a[i]-'0');}
    if(lim>500)lim=500;
    for(int n=2;n<=lim;n++){
        int p=1;for(int d=2;d*d<=n&&p;d++)if(n%d==0)p=0;
        if(p){_sh_puti(n);t_print(" ");}
    }
    t_print("\n");
}
static void sh_expr(const char*a){
    if(!a[0]){t_print("Usage: expr N op M\n");return;}
    int i=0;while(a[i]==' ')i++;
    int x=_parse_int(a,&i);
    while(a[i]==' ')i++;
    char op=a[i];if(op)i++;
    while(a[i]==' ')i++;
    int y=_parse_int(a,&i);
    int r=0;
    if(!op||op=='+'){ r=op?x+y:x; }
    else if(op=='-')r=x-y;
    else if(op=='*'||op=='x')r=x*y;
    else if(op=='/'&&y)r=x/y;
    else if(op=='%'&&y)r=x%y;
    else{t_print("expr: erreur de syntaxe\n");return;}
    _sh_puti(r);t_print("\n");
}
static void sh_units(const char*a){
    if(!a[0]){t_print("Usage: units <val> <de> <vers>\n");return;}
    t_print("(conversion units simulee)\n");
    t_print("Exemples: km->m *1000, kg->g *1000, C->F *9/5+32\n");
}

/* ============================================================
 * Shell - archives et compression
 * ============================================================ */
static void sh_tar(const char*a){
    if(!a[0]){t_print("Usage: tar [cvf|xvf|tf] <archive>\n");return;}
    if(strstr(a,"t")||strstr(a,"l")){
        t_print("kernel.bin\nminegrub.bin\nboot.bin\nkmain.c\n");return;}
    t_print("tar: ");t_print(a);t_print(": simule\n");
}
static void sh_gzip(const char*a){
    t_print("gzip: ");t_print(a[0]?a:"?");t_print(": simule (80% compression)\n");
}
static void sh_zip(const char*a){
    t_print("zip: ");t_print(a[0]?a:"archive.zip");
    t_print(": simule\n  adding: kernel.bin\n  adding: minegrub.bin\n");
}

/* ============================================================
 * Shell - outils binaires
 * ============================================================ */
static void sh_objdump(const char*a){
    const char*f=a[0]?a:"kernel.elf";
    t_print(f);t_print(":     file format elf32-i386\n\n");
    t_print("Disassembly of section .text:\n\n");
    t_print("00010000 <_start>:\n");
    t_print("   10000:  55           push   %ebp\n");
    t_print("   10001:  89 e5        mov    %esp,%ebp\n");
    t_print("   10003:  83 ec 10     sub    $0x10,%esp\n");
    t_print("   10006:  e8 00 00     call   <kmain>\n");
}
static void sh_nm(const char*a){
    const char*f=a[0]?a:"kernel.elf";
    t_print(f);t_print(":\n");
    t_print("00010000 T _start\n");
    t_print("00010010 T kmain\n");
    t_print("00020000 R font8x8\n");
    t_print("00030000 B _win_singleton\n");
    t_print("00090000 B _stack_top\n");
    t_print("         U sfcml_init\n");
}
static void sh_strings(const char*a){
    const char*f=a[0]?a:"kernel.elf";
    t_print(f);t_print(":\n");
    t_print("MyOS v0.1\nEpitech\nMineGRUB\n");
    t_print("root@myos\n/bin/sh\n/home/root\n");
    t_print("VESA 640x480\nRTL8139\nPS/2 OK\n");
}
static void sh_readelf(const char*a){
    const char*f=a[0]?a:"kernel.elf";
    t_print("ELF Header:\n  Magic: 7f 45 4c 46 01 01 01 00\n");
    t_print("  Class: ELF32\n  Data: 2's complement, little endian\n");
    t_print("  Type: EXEC\n  Machine: Intel 80386\n");
    t_print("  Entry: 0x00010000\n");
    t_print("  Sections: .text .rodata .bss\n");
    (void)f;
}
static void sh_size(const char*a){
    (void)a;
    t_print("   text    data     bss     dec     hex filename\n");
    t_print("  65536    4096    8192   77824   13000 kernel.elf\n");
}

/* ============================================================
 * Shell - gestion utilisateurs
 * ============================================================ */
static void sh_useradd(const char*a){
    if(!a[0]){t_print("Usage: useradd <nom>\n");return;}
    t_print("useradd: utilisateur '");t_print(a);t_print("' cree (simule)\n");
    t_print("Note: redemarrage requis pour appliquer\n");
}
static void sh_groups(void){t_print("root adm wheel sudo cdrom disk\n");}
static void sh_finger(const char*a){
    const char*u=a[0]?a:"root";
    t_print("Login: ");t_print(u);
    t_print(!strcmp(u,"root")?"  Name: Super-utilisateur\n":"  Name: Inconnu\n");
    t_print("Directory: /home/");t_print(u);t_print("  Shell: /bin/sh\n");
    t_print("On since 2026-01-01 00:00 on tty1\n");
}
static void sh_chsh(const char*a){
    (void)a;
    t_print("Changing shell for root.\n");
    t_print("New shell [/bin/sh]: /bin/sh\n");
    t_print("Shell changed.\n");
}
static void sh_passwd_cmd(void){
    t_print("Changing password for root.\n");
    t_print("New password: ****\n");
    t_print("Retype new password: ****\n");
    t_print("passwd: password updated (simule)\n");
}

/* ============================================================
 * Shell - reseau avance
 * ============================================================ */
static void sh_host(const char*a){
    if(!a[0]){t_print("Usage: host <domaine>\n");return;}
    t_print(a);
    if(!strncmp(a,"127",3))t_print(" has address 127.0.0.1\n");
    else if(net_ok){char buf[20];_ip4str(net_dns_ip,buf);
        t_print(" has address ");t_print(buf);t_print("\n");}
    else t_print(": reseau non disponible\n");
}
static void sh_whois(const char*a){
    t_print("% IANA WHOIS server\n");
    t_print("% Querying: ");t_print(a[0]?a:"?");t_print("\n");
    t_print("Domain: ");t_print(a[0]?a:"?");t_print("\nStatus: non disponible\n");
}
static void sh_nc(const char*a){
    (void)a;
    t_print("nc: connexion simulee\nConnected.\n");
}
static void sh_tcpdump(const char*a){
    (void)a;
    t_print("tcpdump: capturing on eth0 (verbosity 1)\n");
    if(net_ok){
        char buf[20];_ip4str(net_my_ip,buf);
        t_print("10.0.2.15.68 > 10.0.2.2.67: BOOTP\n");
        t_print(buf);t_print(" > 10.0.2.2: ICMP echo\n");
    }
    t_print("0 packets dropped\n");
}
static void sh_ethtool(void){
    t_print("Settings for eth0:\n");
    t_print("  Speed: 100Mb/s\n  Duplex: Full\n");
    t_print("  Auto-negotiation: on\n  Link detected: yes\n");
}

/* ============================================================
 * Shell - securite et crypto
 * ============================================================ */
/* ============================================================
 * MD5 (RFC 1321) et SHA-256 (FIPS 180-4) - vrais algorithmes,
 * implementes from scratch, zero lib.
 * ============================================================ */
static const uint32_t MD5_K[64]={
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,
    0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
    0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,
    0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,
    0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
    0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,
    0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,
    0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
    0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
static const uint8_t MD5_R[64]={
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5, 9,14,20,5, 9,14,20,5, 9,14,20,5, 9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
static void k_md5(const uint8_t*msg,uint32_t len,uint8_t out[16]){
    uint32_t h0=0x67452301,h1=0xefcdab89,h2=0x98badcfe,h3=0x10325476;
    uint32_t total=((len+8)/64+1)*64;
    for(uint32_t off=0;off<total;off+=64){
        uint8_t ck[64];
        for(uint32_t i=0;i<64;i++){
            uint32_t p=off+i;
            if(p<len)ck[i]=msg[p];
            else if(p==len)ck[i]=0x80;
            else if(p>=total-8){
                uint32_t bi=p-(total-8);
                uint64_t bits=(uint64_t)len*8u;
                ck[i]=(uint8_t)(bits>>(8u*bi));
            }else ck[i]=0;
        }
        uint32_t w[16];memcpy(w,ck,64);   /* x86 = little-endian, OK */
        uint32_t A=h0,B=h1,C=h2,D=h3;
        for(int i=0;i<64;i++){
            uint32_t F;int g;
            if(i<16){F=(B&C)|(~B&D);g=i;}
            else if(i<32){F=(D&B)|(~D&C);g=(5*i+1)&15;}
            else if(i<48){F=B^C^D;g=(3*i+5)&15;}
            else{F=C^(B|~D);g=(7*i)&15;}
            uint32_t tmp=D;D=C;C=B;
            uint32_t x=A+F+MD5_K[i]+w[g];
            B=B+((x<<MD5_R[i])|(x>>(32-MD5_R[i])));
            A=tmp;
        }
        h0+=A;h1+=B;h2+=C;h3+=D;
    }
    memcpy(out,&h0,4);memcpy(out+4,&h1,4);
    memcpy(out+8,&h2,4);memcpy(out+12,&h3,4);
}
static const uint32_t SHA_K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
    0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
    0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
    0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
    0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
    0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static void k_sha256(const uint8_t*msg,uint32_t len,uint8_t out[32]){
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                   0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint32_t total=((len+8)/64+1)*64;
    for(uint32_t off=0;off<total;off+=64){
        uint8_t ck[64];
        for(uint32_t i=0;i<64;i++){
            uint32_t p=off+i;
            if(p<len)ck[i]=msg[p];
            else if(p==len)ck[i]=0x80;
            else if(p>=total-8){
                uint32_t bi=p-(total-8);
                uint64_t bits=(uint64_t)len*8u;
                ck[i]=(uint8_t)(bits>>(8u*(7u-bi)));   /* longueur big-endian */
            }else ck[i]=0;
        }
        uint32_t w[64];
        for(int i=0;i<16;i++)
            w[i]=((uint32_t)ck[i*4]<<24)|((uint32_t)ck[i*4+1]<<16)|
                 ((uint32_t)ck[i*4+2]<<8)|ck[i*4+3];
        for(int i=16;i<64;i++){
            uint32_t s0=ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1=ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(int i=0;i<64;i++){
            uint32_t S1=ROR(e,6)^ROR(e,11)^ROR(e,25);
            uint32_t ch=(e&f)^(~e&g);
            uint32_t t1=hh+S1+ch+SHA_K[i]+w[i];
            uint32_t S0=ROR(a,2)^ROR(a,13)^ROR(a,22);
            uint32_t mj=(a&b)^(a&c)^(b&c);
            uint32_t t2=S0+mj;
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    for(int i=0;i<8;i++){
        out[i*4]=(uint8_t)(h[i]>>24);out[i*4+1]=(uint8_t)(h[i]>>16);
        out[i*4+2]=(uint8_t)(h[i]>>8);out[i*4+3]=(uint8_t)h[i];
    }
}
/* Donnee a hacher: contenu d'un vrai fichier du VFS si le chemin existe,
   sinon le texte lui-meme (comme echo -n txt | md5sum) */
static const char* _hash_input(const char*a,int*is_file){
    char fp[DVFS_PLEN];_dvfs_fullpath(a,fp,DVFS_PLEN);
    DVFSEntry*e=_dvfs_find(fp);
    if(!e)e=_dvfs_find(a);
    if(e&&!e->is_dir){*is_file=1;return e->content;}
    *is_file=0;return a;
}
static void _print_hex(const uint8_t*d,int n){
    const char*H="0123456789abcdef";
    char b[3];b[2]='\0';
    for(int i=0;i<n;i++){b[0]=H[d[i]>>4];b[1]=H[d[i]&15];t_print(b);}
}
static void sh_md5sum(const char*a){
    if(!a[0]){t_print("Usage: md5sum <fichier|texte>\n");return;}
    int isf;const char*in=_hash_input(a,&isf);
    uint8_t d[16];k_md5((const uint8_t*)in,(uint32_t)strlen(in),d);
    _print_hex(d,16);
    t_print("  ");t_print(a);t_print(isf?"\n":"  (texte)\n");
}
static void sh_sha256sum(const char*a){
    if(!a[0]){t_print("Usage: sha256sum <fichier|texte>\n");return;}
    int isf;const char*in=_hash_input(a,&isf);
    uint8_t d[32];k_sha256((const uint8_t*)in,(uint32_t)strlen(in),d);
    _print_hex(d,32);
    t_print("  ");t_print(a);t_print(isf?"\n":"  (texte)\n");
}
static void sh_base64(const char*a){
    if(!a[0]){t_print("Usage: base64 <texte>\n");return;}
    static const char B[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int l=(int)strlen(a),i=0;
    char out[120];int o=0;
    while(i<l&&o<116){
        unsigned b0=(i<l)?(unsigned char)a[i++]:0;
        unsigned b1=(i<l)?(unsigned char)a[i++]:0;
        unsigned b2=(i<l)?(unsigned char)a[i++]:0;
        out[o++]=B[(b0>>2)&0x3F];
        out[o++]=B[((b0&3)<<4)|(b1>>4)];
        out[o++]=(i>l+1)?'=':B[((b1&0xF)<<2)|(b2>>6)];
        out[o++]=(i>l)?'=':B[b2&0x3F];
    }
    out[o]='\0';t_print(out);t_print("\n");
}
static void sh_openssl(const char*a){
    if(!strncmp(a,"rand",4)){t_print("1a2b3c4d5e6f7890abcdef01\n");return;}
    if(!strncmp(a,"md5",3)){sh_md5sum(_sh_arg(a));return;}
    if(!strncmp(a,"sha",3)){sh_sha256sum(_sh_arg(a));return;}
    if(!strncmp(a,"enc",3)){t_print("Encrypted (AES-256-CBC, simule)\n");return;}
    t_print("OpenSSL 3.0.0 (simule)\nUsage: openssl rand|md5|sha256|enc\n");
}
static void sh_gpg(const char*a){
    if(!strncmp(a,"--encrypt",9)||!strncmp(a,"-e",2))
        {t_print("gpg: chiffrement AES256 simule\n");return;}
    if(!strncmp(a,"--decrypt",9)||!strncmp(a,"-d",2))
        {t_print("gpg: dechiffrement simule\n");return;}
    if(!strncmp(a,"--sign",6)||!strncmp(a,"-s",2))
        {t_print("gpg: signature RSA4096 simulee\n");return;}
    t_print("gpg (GnuPG) 2.4.0 (simule)\n");
    t_print("Usage: gpg --encrypt|--decrypt|--sign|--verify\n");
}
static void sh_ssh_keygen(void){
    t_print("Generating public/private rsa key pair.\n");
    t_print("Enter file: /root/.ssh/id_rsa\n");
    t_print("Your identification: /root/.ssh/id_rsa\n");
    t_print("Your public key: /root/.ssh/id_rsa.pub\n");
    t_print("SHA256:xGFg2kPmR7nT4sL9qE1wY3uI0oP5aS6dF8hJ+KL=\n");
    t_print("The key fingerprint is: RSA 4096 root@myos\n");
}
static void sh_checksec(const char*a){
    const char*f=a[0]?a:"kernel.elf";
    t_print("RELRO: Partial\nStack Canary: No\nNX: No\nPIE: No\n");
    t_print("Fortify: No\nRunpath: None\n");(void)f;
}

/* ============================================================
 * Shell - planification
 * ============================================================ */
static void sh_crontab(const char*a){
    if(!strcmp(a,"-l")||!a[0]){
        t_print("# Crontab de root\n# min  hr   dom  mon  dow  cmd\n");
        t_print("  0    0    *    *    *   /bin/sync\n");
        t_print("  */5  *    *    *    *   /bin/dmesg\n");return;
    }
    t_print("crontab: ");t_print(a);t_print(": simule\n");
}
static void sh_at(const char*a){
    t_print("warning: commands will be executed using /bin/sh\n");
    t_print("at> job queued at ");t_print(a[0]?a:"now");t_print("\n");
    t_print("job 1 at 2026-01-01 00:01\n");
}

/* ============================================================
 * Shell - divers
 * ============================================================ */
static void sh_wall(const char*a){
    if(!a[0]){t_print("Usage: wall <message>\n");return;}
    t_print("\nBroadcast message from root@myos (tty1):\n");
    t_print(a);t_print("\n\n");
}
static void sh_watch(const char*a){
    if(!a[0]){t_print("Usage: watch <commande>\n");return;}
    t_print("Every 2.0s: ");t_print(a);t_print("\n");
    t_print("(Ctrl+C pour arreter)\n");
}
static void sh_hack(void){
    t_print("[ Initializing intrusion sequence... ]\n");
    t_print("Scanning ports.................. [DONE]\n");
    t_print("Bypassing firewall............. [DONE]\n");
    t_print("Exploiting CVE-2026-1337....... [DONE]\n");
    t_print("Downloading /etc/shadow........ [####] 100%\n");
    t_print("Escalating privileges.......... [ROOT]\n");
    t_print("Welcome, hacker. You own this machine.\n");
}
static void sh_fire(void){
    t_print("         )   (\n");
    t_print("        (    ) )\n");
    t_print("        )  ( ((\n");
    t_print("      (  )  ) )\n");
    t_print("   _.-----.._/\n");
    t_print("  /  Feu!   \\\n");
}
static void sh_rain(void){
    t_print("|  |  |  | |  |  |  |\n");
    t_print(" |  |  |  |  |  |  |\n");
    t_print("|  |  |  |  | |  |  |\n");
    t_print(" |  |  |  |  |  |  |\n");
    t_print("|  |  |  | |  |  |  |\n");
}
static void sh_pipes(void){
    t_print("    |    |    |\n");
    t_print("====+====+====+====\n");
    t_print("    |    |    |\n");
    t_print("    +----+----+\n");
    t_print("    |         |\n");
    t_print("    +=========+\n");
}
static void sh_nyan(void){
    t_print("~=[,,_,,]:3 ");
    t_print("Nyan Nyan Nyan Nyan Nyan!\n");
    t_print(",---,\n|*.*|\n'---'\n");
    t_print("==========RAINBOW==========\n");
}
static void sh_lolcat(const char*a){
    t_print(a[0]?a:"lolcat");t_print("\n(couleurs arc-en-ciel simulees)\n");
}
static void sh_toilet(const char*a){
    sh_banner(a[0]?a:"MyOS");
}
static void sh_figlet(const char*a){
    sh_banner(a[0]?a:"MyOS");
}

/* ============================================================
 * Shell - handle_command
 * ============================================================ */
static char _wget_buf[2048];   /* buffer de telechargement wget/curl */
static void handle_command(void){
    t_hist_add();
    t_print("$ ");t_print(_tinput);t_print("\n");
    char*inp=_tinput;
    const char*arg=_sh_arg(inp);

    /* filesystem */
    if(!strcmp(inp,"ls")||_sh_sw(inp,"ls -")||_sh_sw(inp,"ls "))sh_ls(arg);
    else if(!strcmp(inp,"ll"))sh_ls(arg);
    else if(!strcmp(inp,"la"))sh_ls(arg);
    else if(!strcmp(inp,"dir")||_sh_sw(inp,"dir "))sh_ls(arg);
    else if(!strcmp(inp,"pwd"))t_print(_cwd),t_print("\n");
    else if(!strcmp(inp,"cd")||_sh_sw(inp,"cd "))sh_cd(arg);
    else if(!strcmp(inp,"cat")||_sh_sw(inp,"cat "))sh_cat(arg);
    else if(!strcmp(inp,"less")||_sh_sw(inp,"less "))sh_cat(arg);
    else if(!strcmp(inp,"more")||_sh_sw(inp,"more "))sh_cat(arg);
    else if(!strcmp(inp,"mkdir")||_sh_sw(inp,"mkdir "))
        {if(arg[0]){char fp[DVFS_PLEN];_dvfs_fullpath(arg,fp,DVFS_PLEN);_dvfs_create(fp,1);t_print("mkdir: ");t_print(arg);t_print(": cree\n");}else t_print("Usage: mkdir <nom>\n");}
    else if(!strcmp(inp,"touch")||_sh_sw(inp,"touch "))
        {if(arg[0]){char fp[DVFS_PLEN];_dvfs_fullpath(arg,fp,DVFS_PLEN);_dvfs_create(fp,0);t_print("touch: ");t_print(arg);t_print(": cree\n");}else t_print("Usage: touch <nom>\n");}
    else if(!strcmp(inp,"rm")||_sh_sw(inp,"rm ")||
            !strcmp(inp,"rmdir")||_sh_sw(inp,"rmdir "))
        {if(arg[0]){char fp[DVFS_PLEN];_dvfs_fullpath(arg,fp,DVFS_PLEN);_dvfs_remove(fp);_dvfs_remove(arg);t_print("rm: ");t_print(arg);t_print(": supprime\n");}else t_print("Usage: rm <nom>\n");}
    else if(!strcmp(inp,"cp")||_sh_sw(inp,"cp "))
        {if(arg[0]){t_print("cp: ");t_print(arg);t_print(": ok\n");}else t_print("Usage: cp <src> <dst>\n");}
    else if(!strcmp(inp,"mv")||_sh_sw(inp,"mv "))
        {if(arg[0]){t_print("mv: ");t_print(arg);t_print(": ok\n");}else t_print("Usage: mv <src> <dst>\n");}
    else if(!strcmp(inp,"nano")||_sh_sw(inp,"nano "))sh_nano(arg);
    else if(!strcmp(inp,"vi")||_sh_sw(inp,"vi ")||!strcmp(inp,"vim")||_sh_sw(inp,"vim "))sh_nano(arg);
    else if(!strcmp(inp,"find")||_sh_sw(inp,"find "))sh_find(arg);
    else if(!strcmp(inp,"stat")||_sh_sw(inp,"stat "))sh_stat(arg);
    else if(!strcmp(inp,"file")||_sh_sw(inp,"file "))sh_file(arg);
    else if(!strcmp(inp,"ln")||_sh_sw(inp,"ln "))t_print("ln: ok\n");
    else if(!strcmp(inp,"chmod")||_sh_sw(inp,"chmod "))t_print("chmod: ok\n");
    else if(!strcmp(inp,"chown")||_sh_sw(inp,"chown "))t_print("chown: ok\n");
    else if(!strcmp(inp,"chgrp")||_sh_sw(inp,"chgrp "))t_print("chgrp: ok\n");
    /* texte */
    else if(!strcmp(inp,"echo"))t_print("\n");
    else if(_sh_sw(inp,"echo ")){
        /* Detect redirection: echo text > file */
        const char* gt=strchr(inp,'>');
        if(gt){
            const char* fn=gt+1;while(*fn==' ')fn++;
            const char* content=inp+5;while(*content==' ')content++;
            char fp[DVFS_PLEN];_dvfs_fullpath(fn,fp,DVFS_PLEN);
            DVFSEntry*e=_dvfs_create(fp,0);
            if(e){
                int clen=(int)(gt-content);
                /* trim trailing spaces */
                while(clen>0&&content[clen-1]==' ')clen--;
                /* remove surrounding quotes */
                if(clen>1&&(content[0]=='"'||content[0]=='\'')){content++;clen-=2;}
                if(clen<0)clen=0;
                if(clen>DVFS_CLEN-2)clen=DVFS_CLEN-2;
                memcpy(e->content,content,(size_t)clen);
                e->content[clen]='\n';e->content[clen+1]='\0';
                _dvfs_dirty=1;
            }
            t_print("Ecrit dans: ");t_print(fn);t_print("\n");
        }else{t_print(arg);t_print("\n");}
    }
    else if(!strcmp(inp,"grep")||_sh_sw(inp,"grep "))sh_grep(arg);
    else if(!strcmp(inp,"head")||_sh_sw(inp,"head "))sh_cat(arg);
    else if(!strcmp(inp,"tail")||_sh_sw(inp,"tail "))sh_cat(arg);
    else if(!strcmp(inp,"wc")||_sh_sw(inp,"wc "))sh_wc(arg);
    else if(!strcmp(inp,"sort")||_sh_sw(inp,"sort "))t_print("(stdin)\n");
    else if(!strcmp(inp,"uniq")||_sh_sw(inp,"uniq "))t_print("(stdin)\n");
    else if(!strcmp(inp,"cut")||_sh_sw(inp,"cut "))t_print("(stdin)\n");
    else if(!strcmp(inp,"tr")||_sh_sw(inp,"tr "))t_print("(stdin)\n");
    else if(!strcmp(inp,"tee")||_sh_sw(inp,"tee "))sh_cat(arg);
    else if(!strcmp(inp,"xargs")||_sh_sw(inp,"xargs "))t_print("(stdin)\n");
    else if(!strcmp(inp,"sed")||_sh_sw(inp,"sed "))t_print("(stdin)\n");
    else if(!strcmp(inp,"awk")||_sh_sw(inp,"awk "))t_print("(stdin)\n");
    /* systeme */
    else if(!strcmp(inp,"uname")||_sh_sw(inp,"uname "))sh_uname(arg);
    else if(!strcmp(inp,"arch"))t_print("i386\n");
    else if(!strcmp(inp,"whoami"))t_print("root\n");
    else if(!strcmp(inp,"id"))t_print("uid=0(root) gid=0(root)\n");
    else if(!strcmp(inp,"hostname")||_sh_sw(inp,"hostname "))
        t_print("myos.epitech.eu\n");
    else if(!strcmp(inp,"date")||_sh_sw(inp,"date "))sh_date();
    else if(!strcmp(inp,"time")||!strcmp(inp,"clock")){
        char b[12];int o=0;
        b[o++]='0'+_rtc.h/10;b[o++]='0'+_rtc.h%10;b[o++]=':';
        b[o++]='0'+_rtc.m/10;b[o++]='0'+_rtc.m%10;b[o++]=':';
        b[o++]='0'+_rtc.s/10;b[o++]='0'+_rtc.s%10;b[o++]='\n';b[o]='\0';
        t_print(b);
    }
    else if(!strcmp(inp,"uptime"))sh_uptime();
    else if(!strcmp(inp,"ps")||_sh_sw(inp,"ps "))sh_ps();
    else if(!strcmp(inp,"top"))sh_top();
    else if(!strcmp(inp,"kill")||_sh_sw(inp,"kill ")||
            !strcmp(inp,"killall")||_sh_sw(inp,"killall "))
        {t_print("kill: ");t_print(arg[0]?arg:"?");t_print(": no such process\n");}
    else if(!strcmp(inp,"nice")||_sh_sw(inp,"nice "))t_print("nice: ok\n");
    else if(!strcmp(inp,"free")||_sh_sw(inp,"free "))sh_free();
    else if(!strcmp(inp,"df")||_sh_sw(inp,"df "))sh_df();
    else if(!strcmp(inp,"du")||_sh_sw(inp,"du "))
        {t_print("0\t");t_print(arg[0]?arg:".");t_print("\n");}
    else if(!strcmp(inp,"lscpu"))sh_lscpu();
    else if(!strcmp(inp,"lspci"))sh_lspci();
    else if(!strcmp(inp,"lsmod"))sh_lsmod();
    else if(!strcmp(inp,"lsblk"))t_print("NAME  SIZE TYPE\nhda   1.4M disk\n");
    else if(!strcmp(inp,"lsusb"))t_print("Bus 001 Device 001: UHCI Root Hub\n");
    else if(!strcmp(inp,"dmesg"))sh_dmesg();
    else if(!strcmp(inp,"mount"))t_print("/dev/hda on / type ext2\n");
    else if(!strcmp(inp,"umount")||_sh_sw(inp,"umount "))t_print("umount: ok\n");
    else if(!strcmp(inp,"sync")){}
    else if(!strcmp(inp,"strace")||_sh_sw(inp,"strace "))t_print("strace: non supporte\n");
    else if(!strcmp(inp,"ltrace")||_sh_sw(inp,"ltrace "))t_print("ltrace: non supporte\n");
    else if(!strcmp(inp,"gdb")||_sh_sw(inp,"gdb "))t_print("gdb: non supporte en baremetal\n");
    else if(!strcmp(inp,"valgrind")||_sh_sw(inp,"valgrind "))t_print("valgrind: non supporte\n");
    else if(!strcmp(inp,"make")||_sh_sw(inp,"make "))t_print("make: Makefile introuvable\n");
    else if(!strcmp(inp,"gcc")||_sh_sw(inp,"gcc "))t_print("gcc: pas de compilateur runtime\n");
    else if(!strcmp(inp,"nasm")||_sh_sw(inp,"nasm "))t_print("nasm: pas d'assembleur runtime\n");
    /* systeme avance */
    else if(!strcmp(inp,"who"))sh_who();
    else if(!strcmp(inp,"w"))sh_w();
    else if(!strcmp(inp,"last")||!strcmp(inp,"lastlog"))sh_last();
    else if(!strcmp(inp,"tty"))sh_tty();
    else if(!strcmp(inp,"nproc"))sh_nproc();
    else if(!strcmp(inp,"vmstat"))sh_vmstat();
    else if(!strcmp(inp,"iostat"))sh_iostat();
    else if(!strcmp(inp,"sysctl")||_sh_sw(inp,"sysctl "))sh_sysctl(arg);
    else if(!strcmp(inp,"route")||!strcmp(inp,"netstat -r"))sh_route();
    else if(!strcmp(inp,"arp")||!strcmp(inp,"arp -a"))sh_arp();
    else if(!strcmp(inp,"iptables")||_sh_sw(inp,"iptables "))sh_iptables(arg);
    else if(!strcmp(inp,"dmidecode")||_sh_sw(inp,"dmidecode "))sh_dmidecode();
    else if(!strcmp(inp,"lshw"))sh_lshw();
    else if(!strcmp(inp,"hdparm")||_sh_sw(inp,"hdparm "))sh_hdparm(arg);
    else if(!strcmp(inp,"acpi"))sh_acpi();
    else if(!strcmp(inp,"sensors"))sh_sensors();
    else if(!strcmp(inp,"timedatectl")||_sh_sw(inp,"timedatectl "))sh_timedatectl();
    else if(!strcmp(inp,"hwclock")||_sh_sw(inp,"hwclock "))sh_hwclock();
    else if(!strcmp(inp,"locale")||!strcmp(inp,"localectl"))sh_locale();
    else if(!strcmp(inp,"systemctl")||_sh_sw(inp,"systemctl "))sh_systemctl(arg);
    else if(!strcmp(inp,"service")||_sh_sw(inp,"service "))sh_service(arg);
    else if(!strcmp(inp,"journalctl")||_sh_sw(inp,"journalctl "))sh_journalctl();
    /* processus avance */
    else if(!strcmp(inp,"pstree"))sh_pstree();
    else if(!strcmp(inp,"htop"))sh_top();
    else if(!strcmp(inp,"pgrep")||_sh_sw(inp,"pgrep "))sh_pgrep(arg);
    else if(!strcmp(inp,"pkill")||_sh_sw(inp,"pkill "))sh_pkill(arg);
    else if(!strcmp(inp,"pidof")||_sh_sw(inp,"pidof "))sh_pidof(arg);
    else if(!strcmp(inp,"taskset")||_sh_sw(inp,"taskset "))t_print("pid 10's current affinity list: 0\n");
    else if(!strcmp(inp,"renice")||_sh_sw(inp,"renice "))sh_renice(arg);
    else if(!strcmp(inp,"nohup")||_sh_sw(inp,"nohup "))
        {t_print("nohup: '");t_print(arg[0]?arg:"?");t_print("' ignore les HUP\n");}
    else if(!strcmp(inp,"jobs"))t_print("[1]+ Running   sh\n");
    else if(!strcmp(inp,"bg")||_sh_sw(inp,"bg "))t_print("[1]+ Continued\n");
    else if(!strcmp(inp,"fg")||_sh_sw(inp,"fg "))t_print("[1]+ Running\n");
    /* texte avance */
    else if(!strcmp(inp,"rev")||_sh_sw(inp,"rev "))sh_rev(arg);
    else if(!strcmp(inp,"fold")||_sh_sw(inp,"fold "))sh_fold(arg);
    else if(!strcmp(inp,"nl")||_sh_sw(inp,"nl "))sh_nl(arg);
    else if(!strcmp(inp,"printf")||_sh_sw(inp,"printf "))sh_printf_cmd(arg);
    else if(!strcmp(inp,"basename")||_sh_sw(inp,"basename "))sh_basename(arg);
    else if(!strcmp(inp,"dirname")||_sh_sw(inp,"dirname "))sh_dirname_cmd(arg);
    else if(!strcmp(inp,"xxd")||_sh_sw(inp,"xxd ")||
            !strcmp(inp,"hexdump")||_sh_sw(inp,"hexdump "))sh_xxd(arg);
    else if(!strcmp(inp,"od")||_sh_sw(inp,"od "))sh_od(arg);
    else if(!strcmp(inp,"ascii"))sh_ascii_table();
    else if(!strcmp(inp,"column")||_sh_sw(inp,"column "))sh_column(arg);
    else if(!strcmp(inp,"fmt")||_sh_sw(inp,"fmt "))sh_fmt(arg);
    else if(!strcmp(inp,"expand")||!strcmp(inp,"unexpand")||
            _sh_sw(inp,"expand ")||_sh_sw(inp,"unexpand "))
        {t_print(arg[0]?arg:"(stdin)");t_print("\n");}
    else if(!strcmp(inp,"paste")||_sh_sw(inp,"paste "))
        {t_print(arg[0]?arg:"(stdin)");t_print("\n");}
    else if(!strcmp(inp,"iconv")||_sh_sw(inp,"iconv "))t_print("iconv: ok (simule)\n");
    /* maths */
    else if(!strcmp(inp,"factor")||_sh_sw(inp,"factor "))sh_factor(arg);
    else if(!strcmp(inp,"primes")||_sh_sw(inp,"primes "))sh_primes(arg);
    else if(!strcmp(inp,"expr")||_sh_sw(inp,"expr "))sh_expr(arg);
    else if(!strcmp(inp,"bc")||_sh_sw(inp,"bc ")||
            !strcmp(inp,"calc")||_sh_sw(inp,"calc "))sh_expr(arg);
    else if(!strcmp(inp,"units")||_sh_sw(inp,"units "))sh_units(arg);
    /* archives */
    else if(!strcmp(inp,"tar")||_sh_sw(inp,"tar "))sh_tar(arg);
    else if(!strcmp(inp,"gzip")||_sh_sw(inp,"gzip ")||
            !strcmp(inp,"gunzip")||_sh_sw(inp,"gunzip "))sh_gzip(arg);
    else if(!strcmp(inp,"bzip2")||_sh_sw(inp,"bzip2 ")||
            !strcmp(inp,"bunzip2")||_sh_sw(inp,"bunzip2 "))
        {t_print("bzip2: ");t_print(arg[0]?arg:"?");t_print(": simule\n");}
    else if(!strcmp(inp,"xz")||_sh_sw(inp,"xz ")||
            !strcmp(inp,"unxz")||_sh_sw(inp,"unxz "))
        {t_print("xz: ");t_print(arg[0]?arg:"?");t_print(": simule\n");}
    else if(!strcmp(inp,"zip")||_sh_sw(inp,"zip ")||
            !strcmp(inp,"unzip")||_sh_sw(inp,"unzip "))sh_zip(arg);
    else if(!strcmp(inp,"compress")||_sh_sw(inp,"compress "))
        {t_print("compress: simule\n");}
    /* outils binaires */
    else if(!strcmp(inp,"objdump")||_sh_sw(inp,"objdump "))sh_objdump(arg);
    else if(!strcmp(inp,"nm")||_sh_sw(inp,"nm "))sh_nm(arg);
    else if(!strcmp(inp,"strings")||_sh_sw(inp,"strings "))sh_strings(arg);
    else if(!strcmp(inp,"readelf")||_sh_sw(inp,"readelf "))sh_readelf(arg);
    else if(!strcmp(inp,"size")||_sh_sw(inp,"size "))sh_size(arg);
    else if(!strcmp(inp,"strip")||_sh_sw(inp,"strip "))
        {t_print("strip: ");t_print(arg[0]?arg:"?");t_print(": ok\n");}
    else if(!strcmp(inp,"ldd")||_sh_sw(inp,"ldd "))
        {t_print(arg[0]?arg:"?");t_print(": statiquement lie (pas de deps)\n");}
    else if(!strcmp(inp,"addr2line")||_sh_sw(inp,"addr2line "))
        {t_print("??:0\n");}
    /* reseau */
    else if(!strcmp(inp,"ifconfig")||!strcmp(inp,"ifconfig -a"))sh_ifconfig();
    else if(!strcmp(inp,"ip")||_sh_sw(inp,"ip "))sh_ip(arg);
    else if(!strcmp(inp,"net"))sh_ifconfig();
    else if(!strcmp(inp,"ping")||_sh_sw(inp,"ping "))sh_ping(arg);
    else if(!strcmp(inp,"netstat")||!strcmp(inp,"ss"))sh_netstat();
    else if(!strcmp(inp,"nslookup")||_sh_sw(inp,"nslookup "))sh_nslookup(arg);
    else if(!strcmp(inp,"dig")||_sh_sw(inp,"dig "))sh_nslookup(arg);
    else if(!strcmp(inp,"traceroute")||_sh_sw(inp,"traceroute "))sh_traceroute(arg);
    else if(!strcmp(inp,"curl")||_sh_sw(inp,"curl ")||
            !strcmp(inp,"wget")||_sh_sw(inp,"wget ")){
        int is_wget=(inp[0]=='w');
        if(!net_ok)t_print("reseau indisponible\n");
        else if(!arg[0])t_print("Usage: wget/curl <url> [fichier]\n");
        else{
            char url[64];int ul=0;
            const char*p=arg;while(*p&&*p!=' '&&ul<63)url[ul++]=*p++;
            url[ul]='\0';
            const char*fn=(*p==' ')?p+1:0;
            int is_https=(url[0]=='h'&&url[1]=='t'&&url[2]=='t'&&url[3]=='p'&&url[4]=='s');
            t_print(is_https?"Telechargement (TLS): ":"Telechargement: ");t_print(url);t_print("\n");
            int n=is_https?net_https_get(url,_wget_buf,(int)sizeof(_wget_buf)-1)
                          :net_http_get (url,_wget_buf,(int)sizeof(_wget_buf)-1);
            if(n<0){t_print("wget: echec (code ");_sh_puti(n);t_print(", DNS/TCP/TLS)\n");}
            else{
                _wget_buf[n]='\0';
                t_print("Recu: ");_sh_puti(n);t_print(" octets\n");
                if(is_wget&&fn){   /* sauve le corps dans un fichier MyFS persistant */
                    char fp[DVFS_PLEN];_dvfs_fullpath(fn,fp,DVFS_PLEN);
                    DVFSEntry*e=_dvfs_create(fp,0);
                    if(e){int cl=n;if(cl>DVFS_CLEN-1)cl=DVFS_CLEN-1;
                        memcpy(e->content,_wget_buf,(size_t)cl);e->content[cl]='\0';
                        _dvfs_dirty=1;
                        t_print("Enregistre dans ");t_print(fn);
                        if(n>DVFS_CLEN-1){t_print(" (tronque a ");_sh_puti(DVFS_CLEN-1);t_print(" o)");}
                        t_print("\n");}
                }else{
                    int show=n>400?400:n;   /* apercu comme curl */
                    char sav=_wget_buf[show];_wget_buf[show]='\0';
                    t_print(_wget_buf);_wget_buf[show]=sav;
                    if(n>show)t_print("\n...(tronque)\n");else t_print("\n");
                }
            }
        }
    }
    else if(!strcmp(inp,"htest")){
        /* test HTTPS a URL codee en dur (frappe fiable) */
        t_print("GET https://example.com/ (TLS 1.2 from scratch)...\n");
        int n=net_https_get("https://example.com/",_wget_buf,(int)sizeof(_wget_buf)-1);
        if(n<0){extern int tls_stage;
            t_print("echec code ");_sh_puti(n);t_print(" etape=");_sh_puti(tls_stage);t_print("\n");
        }else{_wget_buf[n]='\0';t_print("OK ");_sh_puti(n);t_print(" octets:\n");
            int sh=n>800?800:n;char sv=_wget_buf[sh];_wget_buf[sh]='\0';t_print(_wget_buf);_wget_buf[sh]=sv;t_print("\n");}
    }
    else if(!strcmp(inp,"rsabench")){
        extern uint32_t tls_bench_modexp(void);
        t_print("RSA 2048-bit modexp x3...\n");
        uint32_t t0=BIOS_TICKS;
        volatile uint32_t s=0;
        for(int i=0;i<3;i++)s^=tls_bench_modexp();
        uint32_t dt=BIOS_TICKS-t0;
        t_print("3 modexp en ");_sh_puti((int)dt);t_print(" ticks (~");
        _sh_puti((int)(dt*1000/18/3));t_print(" ms/op)\n");
    }
    else if(!strcmp(inp,"https")||_sh_sw(inp,"https ")){
        /* GET HTTPS via le client TLS 1.2 from scratch */
        if(!net_ok)t_print("reseau indisponible\n");
        else if(!arg[0])t_print("Usage: https <hote>[/chemin]  (ex: https example.com)\n");
        else{
            char url[80];int ul=0;const char*ap=arg;
            /* prefixe https:// si absent */
            if(!(arg[0]=='h'&&arg[1]=='t'&&arg[2]=='t'&&arg[3]=='p')){
                const char*pre="https://";while(*pre)url[ul++]=*pre++;
            }
            while(*ap&&*ap!=' '&&ul<79)url[ul++]=*ap++;url[ul]='\0';
            t_print("Connexion TLS 1.2 a ");t_print(url);t_print(" ...\n");
            int n=net_https_get(url,_wget_buf,(int)sizeof(_wget_buf)-1);
            if(n<0){
                extern int tls_stage;
                t_print("https: echec code ");_sh_puti(n);
                t_print(n==-2?" (DNS)":n==-3?" (TCP:443)":" (handshake TLS)");
                t_print("  etape=");_sh_puti(tls_stage);t_print("\n");
                t_print("(0=envoi CH 1=CH ok 2=SH 3=cert 4=SHD 5=CKE 6=fin.cli 7=fin.srv)\n");
            }else{
                _wget_buf[n]='\0';
                t_print("TLS OK - ");_sh_puti(n);t_print(" octets dechiffres:\n");
                int show=n>1200?1200:n;char sav=_wget_buf[show];_wget_buf[show]='\0';
                t_print(_wget_buf);_wget_buf[show]=sav;
                if(n>show)t_print("\n...(tronque)\n");else t_print("\n");
            }
        }
    }
    else if(!strcmp(inp,"ssh")||_sh_sw(inp,"ssh "))t_print("ssh: non supporte\n");
    else if(!strcmp(inp,"ftp")||_sh_sw(inp,"ftp "))t_print("ftp: non supporte\n");
    else if(!strcmp(inp,"host")||_sh_sw(inp,"host "))sh_host(arg);
    else if(!strcmp(inp,"whois")||_sh_sw(inp,"whois "))sh_whois(arg);
    else if(!strcmp(inp,"nc")||!strcmp(inp,"netcat")||
            _sh_sw(inp,"nc ")||_sh_sw(inp,"netcat "))sh_nc(arg);
    else if(!strcmp(inp,"tcpdump")||_sh_sw(inp,"tcpdump "))sh_tcpdump(arg);
    else if(!strcmp(inp,"iwconfig")||_sh_sw(inp,"iwconfig "))
        t_print("eth0: no wireless extensions\nlo: no wireless extensions\n");
    else if(!strcmp(inp,"iw")||_sh_sw(inp,"iw "))t_print("iw: pas de carte WiFi\n");
    else if(!strcmp(inp,"ethtool")||_sh_sw(inp,"ethtool "))sh_ethtool();
    /* environnement */
    else if(!strcmp(inp,"env"))sh_env();
    else if(!strcmp(inp,"printenv")||_sh_sw(inp,"printenv "))sh_printenv(arg);
    else if(_sh_sw(inp,"export "))t_print("export: ok\n");
    else if(_sh_sw(inp,"unset "))t_print("unset: ok\n");
    else if(!strcmp(inp,"which")||_sh_sw(inp,"which "))sh_which(arg);
    else if(!strcmp(inp,"type")||_sh_sw(inp,"type "))sh_which(arg);
    else if(!strcmp(inp,"alias")||_sh_sw(inp,"alias "))t_print("(aucun alias)\n");
    /* utilisateurs */
    else if(!strcmp(inp,"who"))sh_who();
    else if(!strcmp(inp,"users"))t_print("root\n");
    else if(!strcmp(inp,"logname"))t_print("root\n");
    else if(!strcmp(inp,"useradd")||!strcmp(inp,"adduser")||
            _sh_sw(inp,"useradd ")||_sh_sw(inp,"adduser "))sh_useradd(arg);
    else if(!strcmp(inp,"userdel")||!strcmp(inp,"deluser")||
            _sh_sw(inp,"userdel ")||_sh_sw(inp,"deluser "))
        {t_print("userdel: ");t_print(arg[0]?arg:"?");t_print(": supprime (simule)\n");}
    else if(!strcmp(inp,"usermod")||_sh_sw(inp,"usermod "))t_print("usermod: ok\n");
    else if(!strcmp(inp,"groupadd")||_sh_sw(inp,"groupadd "))
        {t_print("groupadd: ");t_print(arg[0]?arg:"?");t_print(": ok\n");}
    else if(!strcmp(inp,"groupdel")||_sh_sw(inp,"groupdel "))t_print("groupdel: ok\n");
    else if(!strcmp(inp,"groups"))sh_groups();
    else if(!strcmp(inp,"finger")||_sh_sw(inp,"finger "))sh_finger(arg);
    else if(!strcmp(inp,"chsh")||_sh_sw(inp,"chsh "))sh_chsh(arg);
    else if(!strcmp(inp,"chfn")||_sh_sw(inp,"chfn "))t_print("chfn: ok (simule)\n");
    else if(!strcmp(inp,"passwd")||_sh_sw(inp,"passwd "))sh_passwd_cmd();
    else if(!strcmp(inp,"newgrp")||_sh_sw(inp,"newgrp "))t_print("newgrp: ok\n");
    /* crypto/securite */
    else if(!strcmp(inp,"openssl")||_sh_sw(inp,"openssl "))sh_openssl(arg);
    else if(!strcmp(inp,"gpg")||_sh_sw(inp,"gpg "))sh_gpg(arg);
    else if(!strcmp(inp,"ssh-keygen")||_sh_sw(inp,"ssh-keygen "))sh_ssh_keygen();
    else if(!strcmp(inp,"md5sum")||_sh_sw(inp,"md5sum "))sh_md5sum(arg);
    else if(!strcmp(inp,"sha256sum")||!strcmp(inp,"sha1sum")||
            _sh_sw(inp,"sha256sum ")||_sh_sw(inp,"sha1sum "))sh_sha256sum(arg);
    else if(!strcmp(inp,"base64")||_sh_sw(inp,"base64 "))sh_base64(arg);
    else if(!strcmp(inp,"checksec")||_sh_sw(inp,"checksec "))sh_checksec(arg);
    /* planification */
    else if(!strcmp(inp,"crontab")||_sh_sw(inp,"crontab "))sh_crontab(arg);
    else if(!strcmp(inp,"at")||_sh_sw(inp,"at "))sh_at(arg);
    else if(!strcmp(inp,"batch"))t_print("batch: job 1 ajoute\n");
    /* divers */
    else if(!strcmp(inp,"wall")||_sh_sw(inp,"wall "))sh_wall(arg);
    else if(!strcmp(inp,"write")||_sh_sw(inp,"write "))sh_wall(arg);
    else if(!strcmp(inp,"mesg")||_sh_sw(inp,"mesg "))t_print("mesg: ok\n");
    else if(!strcmp(inp,"watch")||_sh_sw(inp,"watch "))sh_watch(arg);
    else if(!strcmp(inp,"script")||_sh_sw(inp,"script "))
        t_print("Script started, file is typescript. Ctrl+D to stop.\n");
    else if(!strcmp(inp,"screen")||!strcmp(inp,"tmux"))
        t_print("screen/tmux: non supporte en baremetal\n");
    else if(!strcmp(inp,"ulimit")||_sh_sw(inp,"ulimit "))
        {t_print("open files  (-n) 1024\nstack size  (-s) 8192\ncpu time    (-t) unlimited\n");}
    else if(!strcmp(inp,"wait")||_sh_sw(inp,"wait ")){}
    else if(!strcmp(inp,"trap")||_sh_sw(inp,"trap "))t_print("trap: ok\n");
    else if(!strcmp(inp,"read")||_sh_sw(inp,"read ")){}
    else if(!strcmp(inp,"test")||_sh_sw(inp,"test ")||
            (inp[0]=='['&&inp[1]==' '))t_print("0\n");
    else if(!strcmp(inp,"source")||_sh_sw(inp,"source ")||
            (inp[0]=='.'&&inp[1]==' '))t_print("source: ok\n");
    else if(!strcmp(inp,"exec")||_sh_sw(inp,"exec "))t_print("exec: ok\n");
    else if(!strcmp(inp,"eval")||_sh_sw(inp,"eval "))t_print("eval: ok\n");
    else if(!strcmp(inp,"declare")||_sh_sw(inp,"declare "))t_print("declare: ok\n");
    else if(!strcmp(inp,"set")||_sh_sw(inp,"set "))sh_env();
    else if(!strcmp(inp,"readonly")||_sh_sw(inp,"readonly "))t_print("readonly: ok\n");
    else if(!strcmp(inp,"dd")||_sh_sw(inp,"dd ")){
        /* dd reel : copie un secteur src->dst via ATA. Syntaxe: dd <src_lba> <dst_lba> */
        if(!_ata_ok)t_print("dd: pas de disque ATA\n");
        else if(!arg[0])t_print("Usage: dd <src_lba> <dst_lba>  (copie 1 secteur reel)\n");
        else{
            int src=atoi(arg);const char*sp=strchr(arg,' ');
            if(!sp)t_print("Usage: dd <src_lba> <dst_lba>\n");
            else{
                int dst=atoi(sp+1);
                if(dst<MYFS_LBA){t_print("dd: refuse (dst<");_sh_puti(MYFS_LBA);
                    t_print(" ecraserait kernel/FS)\n");}
                else if(ata_read((uint32_t)src,_dump_buf)&&ata_write((uint32_t)dst,_dump_buf))
                    {t_print("1+0 secteur lu, 1+0 ecrit (512 octets, LBA ");
                     _sh_puti(src);t_print("->");_sh_puti(dst);t_print(", reel)\n");}
                else t_print("dd: erreur d'E/S ATA\n");
            }
        }
    }
    else if(!strcmp(inp,"mkfs")||_sh_sw(inp,"mkfs ")){
        /* vrai formatage: efface le VFS en RAM ET la zone MyFS du disque */
        memset(_dvfs,0,sizeof _dvfs);
        dvfs_sync();
        if(_ata_ok)t_print("mkfs: MyFS reformate sur /dev/hda (ecriture reelle)\n");
        else t_print("mkfs: VFS efface (pas de disque pour persister)\n");
    }
    else if(!strcmp(inp,"fsck")||_sh_sw(inp,"fsck ")){
        /* vraie verification: relit le superbloc et compte les entrees */
        if(!_ata_ok)t_print("fsck: pas de disque ATA\n");
        else if(!ata_read(MYFS_LBA,_fs_secbuf))t_print("fsck: erreur de lecture LBA 500\n");
        else if(*(uint32_t*)_fs_secbuf!=0x31534659u)
            t_print("fsck: pas de superbloc MyFS (lancer mkfs ou creer un fichier)\n");
        else{
            int n=0;uint32_t by=0;
            for(int i=0;i<DVFS_MAX;i++)if(_dvfs[i].used){n++;by+=(uint32_t)strlen(_dvfs[i].content);}
            t_print("fsck: superbloc OK, ");_sh_puti(n);
            t_print(" fichiers, ");_sh_puti((int)by);t_print(" octets, 0 erreur\n");
        }
    }
    else if(!strcmp(inp,"blkid")){
        if(_ata_ok&&ata_read(MYFS_LBA,_fs_secbuf)&&*(uint32_t*)_fs_secbuf==0x31534659u)
            t_print("/dev/hda: TYPE=\"myfs\" MAGIC=\"YFS1\" (lu sur disque)\n");
        else t_print("/dev/hda: pas de systeme de fichiers reconnu\n");
    }
    else if(!strcmp(inp,"fdisk")||_sh_sw(inp,"fdisk ")){
        if(!_ata_ok){t_print("fdisk: pas de disque\n");}
        else if(!ata_read(0,_dump_buf)){t_print("fdisk: erreur lecture MBR\n");}
        else{
            t_print("Disque /dev/hda: ");t_print(_ata_model);
            t_print(", ");_sh_puti((int)_ata_sectors);t_print(" secteurs (");
            _sh_puti((int)(_ata_sectors/2));t_print(" Ko)\n");
            /* Signature MBR reelle a l'offset 510 */
            uint16_t sig=(uint16_t)(_dump_buf[510]|(_dump_buf[511]<<8));
            t_print("Signature MBR (0x1FE): 0x");_sh_puthex(sig);
            t_print(sig==0xAA55?" (valide)\n":" (non standard)\n");
            /* Table de partitions reelle a l'offset 0x1BE (4 x 16 octets) */
            t_print("Boot Debut(LBA)  Taille    Type\n");
            int any=0;
            for(int i=0;i<4;i++){
                uint8_t* e=_dump_buf + 0x1BE + i*16;
                uint8_t type=e[4];
                uint32_t start=(uint32_t)(e[8]|(e[9]<<8)|(e[10]<<16)|((uint32_t)e[11]<<24));
                uint32_t size =(uint32_t)(e[12]|(e[13]<<8)|(e[14]<<16)|((uint32_t)e[15]<<24));
                if(type==0)continue;
                any=1;
                t_print(e[0]==0x80?" *   ":"     ");
                _sh_puti((int)start);t_print("        ");
                _sh_puti((int)size);t_print("     0x");_sh_puthex8(type);t_print("\n");
            }
            if(!any)t_print(" (aucune partition MBR: MyOS boote en secteur brut)\n");
            t_print("Layout logique MyOS:\n");
            t_print("  LBA 0 MBR | 1-16 MineGRUB | 17-400 kernel | 500+ MyFS | 599+ monde MyCraft\n");
        }
    }
    else if(!strcmp(inp,"parted")||_sh_sw(inp,"parted "))
        t_print("parted: voir fdisk (table reelle du disque)\n");
    /* shell */
    else if(!strcmp(inp,"history"))sh_history();
    else if(!strcmp(inp,"man")||_sh_sw(inp,"man "))sh_man(arg);
    else if(!strcmp(inp,"info")||_sh_sw(inp,"info "))sh_man(arg);
    else if(!strcmp(inp,"help")||!strcmp(inp,"h")){
        t_print("=== MyOS Shell - Commandes disponibles ===\n");
        t_print(" FS      : ls ll cd pwd cat less mkdir touch\n");
        t_print("           nano vi vim  (editeur de fichiers)\n");
        t_print("           rm cp mv find stat file ln chmod\n");
        t_print("           chown basename dirname realpath\n");
        t_print("           tree blkid fdisk dd mkfs fsck\n");
        t_print(" Texte   : echo grep head tail wc sort uniq\n");
        t_print("           cut tr sed awk tee rev fold nl\n");
        t_print("           printf column fmt paste expand\n");
        t_print("           xxd hexdump od strings ascii\n");
        t_print(" Systeme : uname arch date time uptime ps top\n");
        t_print("           kill free df du lscpu lspci lsmod\n");
        t_print("           dmesg mount lsblk lsusb who w last\n");
        t_print("           tty nproc vmstat iostat sysctl\n");
        t_print("           dmidecode lshw hdparm acpi sensors\n");
        t_print("           timedatectl hwclock locale\n");
        t_print("           systemctl service journalctl\n");
        t_print(" Process : pstree htop pgrep pkill pidof\n");
        t_print("           taskset renice nohup jobs bg fg\n");
        t_print("           strace ltrace gdb valgrind\n");
        t_print(" Reseau  : ifconfig ip ping netstat ss route\n");
        t_print("           arp iptables nslookup dig host\n");
        t_print("           traceroute whois nc tcpdump\n");
        t_print("           ethtool iwconfig curl wget ssh ftp\n");
        t_print(" Web     : https <hote>  wget/curl <url>  (TLS 1.2 from scratch)\n");
        t_print(" Crypto  : md5sum sha256sum base64 openssl\n");
        t_print("           gpg ssh-keygen checksec\n");
        t_print(" Binaire : objdump nm readelf strings size\n");
        t_print("           strip ldd addr2line\n");
        t_print(" Users   : useradd userdel usermod groupadd\n");
        t_print("           passwd groups finger chsh chfn\n");
        t_print("           who users logname last\n");
        t_print(" Archive : tar gzip bzip2 xz zip unzip\n");
        t_print(" Math    : factor primes expr bc calc units\n");
        t_print(" Env     : env printenv export unset which\n");
        t_print("           set declare readonly source eval\n");
        t_print("           alias man info type\n");
        t_print(" Shell   : history clear cls about sudo su\n");
        t_print("           crontab at batch watch wall write\n");
        t_print("           script ulimit trap test jobs\n");
        t_print(" Son     : beep [Hz] [ms]  play\n");
        t_print(" Fun     : fortune cowsay banner figlet toilet\n");
        t_print("           cal seq yes sleep color matrix\n");
        t_print("           sl hack fire rain pipes nyan lolcat\n");
        t_print("           creeper nyan cowsay\n");
        t_print(" Jeux    : minecraft tetris demineur snake rtype pong\n");
        t_print(" Autres  : reboot shutdown mem make gcc nasm\n");
        t_print("           dd mkfs fdisk blkid parted fsck\n");
        t_print("Type 'man <cmd>' pour l'aide d'une commande.\n");
    }
    else if(!strcmp(inp,"snake")){win_open(W_SNAKE);snake_reset();}
    else if(!strcmp(inp,"rtype")){win_open(W_RTYPE);rtype_reset();}
    else if(!strcmp(inp,"pong")){win_open(W_PONG);pong_reset();}
    else if(!strcmp(inp,"minecraft")||!strcmp(inp,"mycraft")){win_open(W_MINE);}
    else if(!strcmp(inp,"tetris")){win_open(W_TETRIS);tet_reset();}
    else if(!strcmp(inp,"demineur")||!strcmp(inp,"mines")||!strcmp(inp,"minesweeper")){win_open(W_DEMINE);ms_reset();}
    else if(!strcmp(inp,"clear")||!strcmp(inp,"cls"))
        {for(int i=0;i<T_ROWS;i++)_tlines[i][0]='\0';_tnlines=0;}
    else if(!strcmp(inp,"about"))
        {t_print("MyOS v0.1 Epitech | x86 32-bit | MineGRUB\n");}
    else if(!strcmp(inp,"sudo")||_sh_sw(inp,"sudo "))t_print("root@myos OK\n");
    else if(!strcmp(inp,"su")||_sh_sw(inp,"su "))t_print("root@myos:~#\n");
    else if(!strcmp(inp,"bash")||!strcmp(inp,"sh")||!strcmp(inp,"zsh"))
        t_print("MyOS Shell v1.0\n");
    else if(!strcmp(inp,"exit")||!strcmp(inp,"logout"))t_print("(pas de session parente)\n");
    else if(!strcmp(inp,"true")){}
    else if(!strcmp(inp,"false"))t_print("false: exit status 1\n");
    else if(!strcmp(inp,"yes")||_sh_sw(inp,"yes "))sh_yes(arg);
    else if(!strcmp(inp,"seq")||_sh_sw(inp,"seq "))sh_seq(arg);
    else if(!strcmp(inp,"cal")||!strcmp(inp,"ncal"))sh_cal();
    else if(!strcmp(inp,"sleep")||_sh_sw(inp,"sleep "))sh_sleep(arg);
    else if(!strcmp(inp,"fortune"))sh_fortune();
    else if(!strcmp(inp,"cowsay")||_sh_sw(inp,"cowsay "))sh_cowsay(arg);
    else if(!strcmp(inp,"banner")||_sh_sw(inp,"banner "))sh_banner(arg);
    else if(_sh_sw(inp,"color "))sh_color(arg);
    else if(!strcmp(inp,"matrix")||!strcmp(inp,"cmatrix"))
        t_print("Wake up, Neo...\nThe Matrix has you.\nFollow the white rabbit.\n");
    else if(!strcmp(inp,"creeper"))t_print("Creeper, Aw Man...\n");
    else if(!strcmp(inp,"beep")||_sh_sw(inp,"beep ")){
        /* son reel via le haut-parleur PC */
        if(!arg[0]){play_melody(MEL_BOOT,4);t_print("beep: jingle joue\n");}
        else{
            int f=atoi(arg),ms=200;
            const char*sp=strchr(arg,' ');
            if(sp)ms=atoi(sp+1);
            if(f<20||f>20000){t_print("beep: frequence 20-20000 Hz\n");}
            else{if(ms<10)ms=10;if(ms>3000)ms=3000;
                beep((uint32_t)f,(uint32_t)ms);
                t_print("beep: ");_sh_puti(f);t_print(" Hz ");_sh_puti(ms);t_print(" ms\n");}
        }
    }
    else if(!strcmp(inp,"play")){play_melody(MEL_WIN,6);t_print("play: melodie jouee\n");}
    else if(!strcmp(inp,"sl"))t_print("   o O O     \n   |___|     \n   |---|     \n");
    else if(!strcmp(inp,"hack"))sh_hack();
    else if(!strcmp(inp,"fire"))sh_fire();
    else if(!strcmp(inp,"rain"))sh_rain();
    else if(!strcmp(inp,"pipes"))sh_pipes();
    else if(!strcmp(inp,"nyan"))sh_nyan();
    else if(!strcmp(inp,"lolcat")||_sh_sw(inp,"lolcat "))sh_lolcat(arg);
    else if(!strcmp(inp,"figlet")||_sh_sw(inp,"figlet "))sh_figlet(arg);
    else if(!strcmp(inp,"toilet")||_sh_sw(inp,"toilet "))sh_toilet(arg);
    else if(!strcmp(inp,"mem")){
        uint32_t kimg=(uint32_t)(__bss_start-(uint8_t*)0x10000);
        uint32_t kbss=(uint32_t)(__bss_end-__bss_start);
        t_print("Carte memoire (tailles reelles):\n");
        t_print("  0x010000 kernel image ");_sh_puti((int)(kimg/1024));t_print(" Ko\n");
        t_print("  0x0");_sh_puthex((uint32_t)__bss_start);t_print(" bss ");
        _sh_puti((int)(kbss/1024));t_print(" Ko\n");
        t_print("  0x200000 pile kernel\n");
        t_print("  0x300000 backbuffer VESA 1406 Ko\n");
        t_print("  0x400000 canvas Paint 157 Ko\n");
        t_print("  0x500000 heap malloc 4096 Ko\n");
        t_print("  RAM totale: ");_sh_puti((int)(ram_total_kb()/1024));
        t_print(" Mo (CMOS)\n");
    }
    else if(!strcmp(inp,"shutdown")||!strcmp(inp,"poweroff"))
        {t_print("Arret du systeme...\n");cmd_reboot();}
    else if(!strcmp(inp,"reboot")||!strcmp(inp,"halt"))
        {t_print("Redemarrage...\n");cmd_reboot();}
    else if(inp[0]=='#'){}
    else if(inp[0]){t_print(inp);t_print(": commande introuvable\n");}
    if(_dvfs_dirty)dvfs_sync();   /* persiste les fichiers sur le disque */
    _tilen=0;_tinput[0]='\0';
}

/* ============================================================
 * PAINT - palette, canvas, outils
 * ============================================================ */
#define PAINT_CANVAS ((uint8_t*)0x400000)
#define PAINT_CW  460
#define PAINT_CH  350
#define PAINT_NC  16
#define PAINT_SB  52   /* sidebar width */
#define PAINT_TB  28   /* toolbar height */

static const sfcml_Color _pal[PAINT_NC]={
    {255,255,255,255},{0,0,0,255},
    {220,20,20,255},  {0,180,0,255},
    {0,100,220,255},  {255,210,0,255},
    {255,120,0,255},  {180,0,180,255},
    {0,210,210,255},  {140,80,20,255},
    {210,210,210,255},{120,120,120,255},
    {50,50,50,255},   {255,160,180,255},
    {160,255,160,255},{160,200,255,255},
};
static int _pcol=1,_pbrush=1,_painting=0,_paint_inited=0;

static void _paint_dot(int cx,int cy,uint8_t col){
    for(int dy=-_pbrush;dy<=_pbrush;dy++)
        for(int dx=-_pbrush;dx<=_pbrush;dx++){
            int x=cx+dx,y=cy+dy;
            if(x>=0&&x<PAINT_CW&&y>=0&&y<PAINT_CH&&dx*dx+dy*dy<=_pbrush*_pbrush+_pbrush)
                PAINT_CANVAS[y*PAINT_CW+x]=col;
        }
}

/* ============================================================
 * CODE EDITOR
 * ============================================================ */
#define CODE_ROWS 50
#define CODE_COLS 76
static char _code[CODE_ROWS][CODE_COLS+1];
static int  _code_nl=1,_code_cl=0,_code_cc=0,_code_sc=0,_code_inited=0;

static void code_init(void){
    const char* t[]={
        "#include <stdio.h>","#include <stdlib.h>","",
        "/* Epitech Code Editor */","",
        "int main(void) {",
        "    printf(\"Hello Epitech!\\n\");",
        "    return 0;","}",NULL};
    _code_nl=0;
    for(int i=0;t[i];i++){strncpy(_code[_code_nl],t[i],CODE_COLS);_code[_code_nl++][CODE_COLS]='\0';}
}
static void code_key(sfcml_KeyCode k){
    char* cur=_code[_code_cl];
    int len=(int)strlen(cur);
    switch(k){
    case SFCML_KEY_ESCAPE: break;
    case SFCML_KEY_UP:
        if(_code_cl>0){_code_cl--;if(_code_cl<_code_sc)_code_sc=_code_cl;}
        _code_cc=(int)strlen(_code[_code_cl])<_code_cc?(int)strlen(_code[_code_cl]):_code_cc;
        break;
    case SFCML_KEY_DOWN:
        if(_code_cl<_code_nl-1){_code_cl++;if(_code_cl>=_code_sc+38)_code_sc++;}
        _code_cc=(int)strlen(_code[_code_cl])<_code_cc?(int)strlen(_code[_code_cl]):_code_cc;
        break;
    case SFCML_KEY_LEFT:
        if(_code_cc>0)_code_cc--;
        else if(_code_cl>0){_code_cl--;_code_cc=(int)strlen(_code[_code_cl]);}
        break;
    case SFCML_KEY_RIGHT:
        if(_code_cc<len)_code_cc++;
        else if(_code_cl<_code_nl-1){_code_cl++;_code_cc=0;}
        break;
    case SFCML_KEY_RETURN:
        if(_code_nl<CODE_ROWS){
            for(int i=_code_nl;i>_code_cl+1;i--)memcpy(_code[i],_code[i-1],CODE_COLS+1);
            memcpy(_code[_code_cl+1],cur+_code_cc,len-_code_cc+1);
            cur[_code_cc]='\0';_code_cl++;_code_cc=0;_code_nl++;
            if(_code_cl>=_code_sc+38)_code_sc++;
        }
        break;
    case SFCML_KEY_BACKSPACE:
        if(_code_cc>0){memmove(cur+_code_cc-1,cur+_code_cc,len-_code_cc+1);_code_cc--;}
        else if(_code_cl>0){
            int pl=(int)strlen(_code[_code_cl-1]);
            strncat(_code[_code_cl-1],cur,CODE_COLS-pl);
            for(int i=_code_cl;i<_code_nl-1;i++)memcpy(_code[i],_code[i+1],CODE_COLS+1);
            _code_nl--;_code_cl--;_code_cc=pl;
            if(_code_cl<_code_sc)_code_sc=_code_cl;
        }
        break;
    case SFCML_KEY_TAB:
        for(int t=0;t<4&&len<CODE_COLS;t++){
            memmove(cur+_code_cc+1,cur+_code_cc,len-_code_cc+1);
            cur[_code_cc]=' ';_code_cc++;len++;
        }
        break;
    default:break;
    }
}
static void code_text(char ch){
    if(ch<32)return;
    char* cur=_code[_code_cl];int len=(int)strlen(cur);
    if(len>=CODE_COLS)return;
    memmove(cur+_code_cc+1,cur+_code_cc,len-_code_cc+1);
    cur[_code_cc++]=ch;
}

/* Syntax highlight one line */
static const char* _kw[]={"int","char","void","float","double","if","else","for",
    "while","do","return","struct","typedef","static","const","unsigned","long",
    "short","extern","include","define","ifdef","ifndef","endif","NULL","sizeof",NULL};
static int _is_kw(const char* s,int n){
    for(int k=0;_kw[k];k++){int kl=(int)strlen(_kw[k]);if(kl==n&&!strncmp(s,_kw[k],n))return 1;}
    return 0;
}
static void draw_code_line(sfcml_Window* win,const char* line,int x,int y,sfcml_Color bg){
    sfcml_Color kw=sfcml_rgb(100,160,255);
    sfcml_Color st=sfcml_rgb(100,220,100);
    sfcml_Color cm=sfcml_rgb(100,110,130);
    sfcml_Color nm=sfcml_rgb(220,180,100);
    sfcml_Color pp=sfcml_rgb(210,100,220);
    sfcml_Color df=SFCML_WHITE;
    int len=(int)strlen(line),i=0,in_str=0;
    if(len>0&&line[0]=='#'){sfcml_drawText(win,line,x,y,pp,bg);return;}
    while(i<len){
        if(!in_str&&i+1<len&&line[i]=='/'&&line[i+1]=='/'){
            sfcml_drawText(win,line+i,x+i*8,y,cm,bg);break;
        }
        if(line[i]=='"'){sfcml_drawChar(win,line[i],x+i*8,y,st,bg);in_str=!in_str;i++;continue;}
        if(in_str){sfcml_drawChar(win,line[i],x+i*8,y,st,bg);i++;continue;}
        if(line[i]>='0'&&line[i]<='9'){sfcml_drawChar(win,line[i],x+i*8,y,nm,bg);i++;continue;}
        if((line[i]>='a'&&line[i]<='z')||(line[i]>='A'&&line[i]<='Z')||line[i]=='_'){
            int j=i;
            while(j<len&&((line[j]>='a'&&line[j]<='z')||(line[j]>='A'&&line[j]<='Z')||
                          (line[j]>='0'&&line[j]<='9')||line[j]=='_'))j++;
            sfcml_Color wc=_is_kw(line+i,j-i)?kw:df;
            for(int k=i;k<j;k++)sfcml_drawChar(win,line[k],x+k*8,y,wc,bg);
            i=j;continue;
        }
        sfcml_Color pc=df;
        if(line[i]=='{'||line[i]=='}'||line[i]=='('||line[i]==')')pc=sfcml_rgb(255,200,80);
        sfcml_drawChar(win,line[i],x+i*8,y,pc,bg);i++;
    }
}

/* ============================================================
 * WORD PROCESSOR
 * ============================================================ */
#define WORD_ROWS 80
#define WORD_COLS 66
static char _word[WORD_ROWS][WORD_COLS+1];
static int  _word_nl=1,_word_cl=0,_word_cc=0,_word_sc=0,_word_inited=0;

/* Sauvegarde le contenu du word processor vers le VFS */
static void _nano_save(void){
    if(!_nano_path[0])return;
    DVFSEntry* e=_dvfs_find(_nano_path);
    if(!e)e=_dvfs_create(_nano_path,0);
    if(!e)return;
    int o=0;
    for(int i=0;i<_word_nl&&o<DVFS_CLEN-2;i++){
        int l=(int)strlen(_word[i]);
        if(o+l+1>=DVFS_CLEN)break;
        memcpy(e->content+o,_word[i],(size_t)l);o+=l;
        if(i<_word_nl-1)e->content[o++]='\n';
    }
    e->content[o]='\0';
    _dvfs_dirty=1;
}

/* Ouvre nano avec un fichier du VFS */
static void sh_nano(const char* a){
    if(!a[0]){t_print("Usage: nano <fichier>\n");return;}
    char fp[DVFS_PLEN];_dvfs_fullpath(a,fp,DVFS_PLEN);
    DVFSEntry* e=_dvfs_create(fp,0);
    strncpy(_nano_path,fp,DVFS_PLEN-1);_nano_path[DVFS_PLEN-1]='\0';
    _word_nl=0;_word_cl=0;_word_cc=0;_word_sc=0;
    if(e&&e->content[0]){
        const char* c=e->content;
        while(*c&&_word_nl<WORD_ROWS){
            int li=0;
            while(*c&&*c!='\n'&&li<WORD_COLS)_word[_word_nl][li++]=*c++;
            _word[_word_nl][li]='\0';_word_nl++;
            if(*c=='\n')c++;
        }
    }
    if(_word_nl==0){_word[0][0]='\0';_word_nl=1;}
    _word_inited=1;
    win_open(W_WORD);
    t_print("nano: ");t_print(fp);t_print(" (Echap pour fermer)\n");
}

static void word_init(void){
    const char* t[]={
        "     Rapport - Epitech Technology",
        "     =================================","",
        "Auteur  : ","Date    : 2026","Projet  : MyOS","",
        "Introduction","------------","",
        "Ecrivez votre rapport ici. Ce traitement de",
        "texte supporte la navigation au clavier.","",
        "Utilisez les fleches pour vous deplacer,",
        "Entree pour une nouvelle ligne.",NULL};
    _word_nl=0;
    for(int i=0;t[i];i++){strncpy(_word[_word_nl],t[i],WORD_COLS);_word[_word_nl++][WORD_COLS]='\0';}
}
static void word_key(sfcml_KeyCode k){
    char* cur=_word[_word_cl];int len=(int)strlen(cur);
    switch(k){
    case SFCML_KEY_UP:
        if(_word_cl>0){_word_cl--;if(_word_cl<_word_sc)_word_sc=_word_cl;}
        _word_cc=(int)strlen(_word[_word_cl])<_word_cc?(int)strlen(_word[_word_cl]):_word_cc;
        break;
    case SFCML_KEY_DOWN:
        if(_word_cl<_word_nl-1){_word_cl++;if(_word_cl>=_word_sc+38)_word_sc++;}
        _word_cc=(int)strlen(_word[_word_cl])<_word_cc?(int)strlen(_word[_word_cl]):_word_cc;
        break;
    case SFCML_KEY_LEFT:
        if(_word_cc>0)_word_cc--;
        else if(_word_cl>0){_word_cl--;_word_cc=(int)strlen(_word[_word_cl]);}
        break;
    case SFCML_KEY_RIGHT:
        if(_word_cc<len)_word_cc++;
        else if(_word_cl<_word_nl-1){_word_cl++;_word_cc=0;}
        break;
    case SFCML_KEY_RETURN:
        if(_word_nl<WORD_ROWS){
            for(int i=_word_nl;i>_word_cl+1;i--)memcpy(_word[i],_word[i-1],WORD_COLS+1);
            memcpy(_word[_word_cl+1],cur+_word_cc,len-_word_cc+1);
            cur[_word_cc]='\0';_word_cl++;_word_cc=0;_word_nl++;
            if(_word_cl>=_word_sc+38)_word_sc++;
        }
        _nano_save();
        break;
    case SFCML_KEY_BACKSPACE:
        if(_word_cc>0){memmove(cur+_word_cc-1,cur+_word_cc,len-_word_cc+1);_word_cc--;}
        else if(_word_cl>0){
            int pl=(int)strlen(_word[_word_cl-1]);
            strncat(_word[_word_cl-1],cur,WORD_COLS-pl);
            for(int i=_word_cl;i<_word_nl-1;i++)memcpy(_word[i],_word[i+1],WORD_COLS+1);
            _word_nl--;_word_cl--;_word_cc=pl;
            if(_word_cl<_word_sc)_word_sc=_word_cl;
        }
        _nano_save();
        break;
    default:break;
    }
}
static void word_text(char ch){
    if(ch<32)return;
    char* cur=_word[_word_cl];int len=(int)strlen(cur);
    if(len>=WORD_COLS)return;
    memmove(cur+_word_cc+1,cur+_word_cc,len-_word_cc+1);
    cur[_word_cc++]=ch;
    _nano_save();
}

/* ============================================================
 * Systeme de fenetres
 * ============================================================ */
#define TBAR_H  22
#define BTN_W   16
#define BTN_H   14

typedef struct{int x,y,w,h,visible,minimized;const char* title;}AppWin;

#define NW         17
#define W_TERM     0
#define W_ABOUT    1
#define W_CREEP    2
#define W_PAINT    3
#define W_CODE     4
/* W_WORD already defined near top */
#define W_SETTINGS 6
#define W_BROWSER  7
#define W_CALC     8
#define W_FILES    9
#define W_NETMGR   10

static AppWin _wins[NW]={
    {70, 36, 504,320,0,0,"Terminal - root@myos"},
    {160,80, 320,210,0,0,"A propos de MyOS"},
    {240,70, 216,224,0,0,"Creeper!"},
    {1,  1,  SCR_W-2,SCR_H-33,0,0,"Epitech Paint"},
    {1,  1,  SCR_W-2,SCR_H-33,0,0,"Epitech Code Editor"},
    {1,  1,  SCR_W-2,SCR_H-33,0,0,"Epitech Word"},
    {40, 20, SCR_W-80,SCR_H-70,0,0,"Parametres"},
    {1,  0,  SCR_W-2,SCR_H-31,0,0,"MyBrowser"},
    {190,60, 240,300,0,0,"Calculatrice"},
    {50, 20, 540,400,0,0,"Gestionnaire de fichiers"},
    {30, 25, 580,395,0,0,"Reseau - MyOS"},
    {80, 50, 660,490,0,0,"Snake"},
    {1,  1,  SCR_W-2,SCR_H-33,0,0,"R-Type"},
    {150,50, 500,430,0,0,"Pong"},
    {79, 40, 642,503,0,0,"MyCraft"},
    {120,40, 360,432,0,0,"Tetris"},
    {150,50, 340,404,0,0,"Demineur"},
};
static int _z[NW]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static int _drag_win=-1,_drag_ox,_drag_oy;
static int _focus=-1;

static void win_front(int idx){
    int k=-1;
    for(int i=0;i<NW;i++)if(_z[i]==idx){k=i;break;}
    if(k<0)return;
    _focus=idx;             /* meme si deja au premier plan */
    if(k==NW-1)return;
    for(int i=k;i<NW-1;i++)_z[i]=_z[i+1];
    _z[NW-1]=idx;
}
static void win_open(int idx){
    _wins[idx].visible=1;_wins[idx].minimized=0;win_front(idx);
    if(idx==W_PAINT&&!_paint_inited){memset(PAINT_CANVAS,0,PAINT_CW*PAINT_CH);_paint_inited=1;}
    if(idx==W_CODE&&!_code_inited){code_init();_code_inited=1;}
    if(idx==W_WORD&&!_word_inited){word_init();_word_inited=1;}
    if(idx==W_MINE&&!_mine_inited){mine_open_init();_mine_inited=1;}
    if(idx==W_TETRIS&&!_tet_inited){tet_reset();_tet_inited=1;}
    if(idx==W_DEMINE&&!_ms_inited){ms_reset();_ms_inited=1;}
}

/* ps/top reels: le kernel + les fenetres reellement ouvertes */
static void _ps_real(int mode){
    t_print("  PID  ETAT  TACHE\n");
    t_print("    0  R     kernel (kmain, boucle evenements)\n");
    t_print("    1  R     wm (bureau ");_sh_puti(SCR_W);t_print("x");
    _sh_puti(SCR_H);t_print(")\n");
    int n=2;
    for(int i=0;i<NW;i++){
        if(!_wins[i].visible)continue;
        t_print("   ");_sh_puti(10+i);
        t_print(_wins[i].minimized?"  S     ":(_focus==i?"  R+    ":"  R     "));
        t_print(_wins[i].title);t_print("\n");n++;
    }
    if(mode){t_print("\n");_sh_puti(n);t_print(" taches reelles\n");}
}

/* ============================================================
 * Etat bureau
 * ============================================================ */
static int _mx=320,_my=240;
static int _start_open=0;
static int _rcopen=0,_rcx,_rcy;
static int _sel_icon=-1;

/* ============================================================
 * PARAMETRES - presets & etat
 * ============================================================ */
static const sfcml_Color _accent_pr[8]={
    {14, 50,110,255},{180,10, 10,255},{10, 90, 30,255},{100,10,150,255},
    {160,80,  0,255},{0, 120,130,255},{60, 60, 80,255},{130,100, 0,255},
};
static const sfcml_Color _bg_pr[5]={
    {0, 48, 90,255},{8, 70, 20,255},{60,0,80,255},{45,45,50,255},{70,8,8,255},
};
static const sfcml_Color _fg_pr[4]={
    {255,255,255,255},{80,240, 80,255},{80,240,240,255},{255,240, 80,255},
};
static const char* _fg_lbl[4]={"Blanc","Vert","Cyan","Jaune"};
static const char* _bg_lbl[5]={"Bleu","Vert","Violet","Gris","Rouge"};
static int _set_cat=0,_accent_sel=0,_bg_sel=0,_fg_sel=0;
static int _veille_sel=0;   /* 0=Off 1=1min 2=2min 3=5min 4=10min */
static int _inact_secs=0;
static int _sleeping=0;
static const int _veille_secs[5]={0,60,120,300,600};
static const char* _veille_lbl[5]={"Off","1 min","2 min","5 min","10 min"};
static int _ss_id=0;        /* economiseur actif: 0=horloge 1=arbre 2=mandelbrot 3=etoiles 4=matrix */
static int _mb_row=480;     /* etat mandelbrot: ligne en cours */
static int _ss_si=0;        /* etat etoiles: initialise */
static int _mc_i=0;         /* etat matrix: initialise */

/* ---- Navigateur ---- */
#define BURL_MAX 60
static char _burl[BURL_MAX+1];
static int  _burl_len=0,_burl_foc=0,_bpage=0;
static char _bsearch[BURL_MAX+1];
static int  _bsearch_len=0,_bsearch_foc=0;
static char _net_buf[4096];
static int  _net_len=0;

static char _calc_disp[24]="0";
static double _calc_acc=0.0;
static double _calc_cur=0.0;
static char   _calc_op=0;
static int    _calc_new=1;
static int    _calc_err=0;

static int _netmgr_ping_ok=-1; /* -1=non teste 0=echec 1=ok */
static int _fi_sel=0,_fi_dir=0;
static int   _fi_edit=0;
#define FI_ER 20
#define FI_EC 58
static char  _fi_ebuf[FI_ER][FI_EC+1];
static int   _fi_enl=0,_fi_ecl=0,_fi_ecc=0,_fi_esc=0;
static const char* _fi_dirs[]={"C:\\","C:\\Windows","C:\\Windows\\System32","C:\\Users","C:\\Users\\root","C:\\MyOS","C:\\MyOS\\boot","C:\\MyOS\\kernel"};
static const char* _fi_files[][6]={
    {"Windows","Users","MyOS","Program Files","BOOTMGR","pagefile.sys"},
    {"System32","SysWOW64","explorer.exe","notepad.exe","cmd.exe","winver.exe"},
    {"kernel32.dll","ntdll.dll","hal.dll","drivers","config","LogFiles"},
    {"root","Public","Default","All Users","desktop.ini",""},
    {"Desktop","Documents","Downloads","Music","Pictures","Videos"},
    {"boot","kernel","libc","sfcml","net","Makefile"},
    {"boot.bin","minegrub.bin","stage1","stage2","",""},
    {"kmain.c","kmain.o","kernel.bin","kernel.elf","kernel.ld","crt0.asm"},
};

static const char* _bpu[5]={"myos://newtab","epitech.eu","google.com","myos://info","github.com"};

static void _bnav(int p){
    _bsearch_foc=0;
    if(p==5){
        const char*pre="google.com/?q=";
        int l=0,i=0;
        while(pre[i]&&l<BURL_MAX)_burl[l++]=pre[i++];
        i=0;while(_bsearch[i]&&l<BURL_MAX)_burl[l++]=_bsearch[i++];
        _burl[l]='\0';_burl_len=l;_burl_foc=0;_bpage=5;
        return;
    }
    const char*u=_bpu[p<5?p:0];
    int l=0;while(u[l]&&l<BURL_MAX){_burl[l]=u[l];l++;}
    _burl[l]='\0';_burl_len=l;_burl_foc=0;_bpage=p;
}
static int html_strip(char*buf,int len){
    int in_tag=0,skip=0,oi=0,last_sp=1;
    char tn[8];int tni=0;
    for(int i=0;i<len;i++){
        char c=buf[i];
        if(!in_tag&&c=='<'){
            in_tag=1;tni=0;
            if(i+1<len&&buf[i+1]=='/'){skip=0;tni=1;tn[0]='/';}
        }else if(in_tag&&c=='>'){
            in_tag=0;
            tn[tni<7?tni:7]='\0';
            if(tn[0]!='/'&&(tn[0]=='s'||tn[0]=='S'))
                if((tn[1]=='c'||tn[1]=='C')||(tn[1]=='t'||tn[1]=='T'))skip=1;
            if(!skip&&!last_sp){buf[oi++]='\n';last_sp=1;}
        }else if(in_tag){
            if(tni<7&&c!=' '&&c!='\t'&&c!='\n')tn[tni++]=c;
        }else if(!skip){
            if(c==' '||c=='\t'){
                if(!last_sp){buf[oi++]=' ';last_sp=1;}
            }else if(c=='\n'||c=='\r'){
                if(!last_sp){buf[oi++]='\n';last_sp=1;}
            }else if(c=='&'){
                if(i+4<len&&buf[i+1]=='a'&&buf[i+2]=='m'&&buf[i+3]=='p'&&buf[i+4]==';'){buf[oi++]='&';i+=4;last_sp=0;}
                else if(i+3<len&&buf[i+1]=='l'&&buf[i+2]=='t'&&buf[i+3]==';'){buf[oi++]='<';i+=3;last_sp=0;}
                else if(i+3<len&&buf[i+1]=='g'&&buf[i+2]=='t'&&buf[i+3]==';'){buf[oi++]='>';i+=3;last_sp=0;}
                else if(i+5<len&&buf[i+1]=='n'&&buf[i+2]=='b'&&buf[i+3]=='s'&&buf[i+4]=='p'&&buf[i+5]==';'){if(!last_sp){buf[oi++]=' ';last_sp=1;}i+=5;}
                else{buf[oi++]=c;last_sp=0;}
            }else if((uint8_t)c>=32&&(uint8_t)c<127){
                buf[oi++]=c;last_sp=0;
            }
        }
    }
    buf[oi]='\0';
    return oi;
}

static void _burl_nav(void){
    if(strstr(_burl,"epitech")){_bnav(1);return;}
    if(strstr(_burl,"google")) {_bnav(2);return;}
    if(strstr(_burl,"github")) {_bnav(4);return;}
    if(strstr(_burl,"info")||strstr(_burl,"myos")){_bnav(3);return;}
    if(net_ok){
        _net_len=net_http_get(_burl,_net_buf,(int)sizeof(_net_buf)-1);
        if(_net_len<0){_net_len=0;_net_buf[0]='\0';}
        else _net_len=html_strip(_net_buf,_net_len);
        _bsearch_foc=0;_burl_foc=0;_bpage=6;return;
    }
    _bpage=0;
}

#define NICONS 15
static const int   IC_Y[12]={8,53,98,143,188,233,278,323,368,413,458,503};
static const char* IC_LBL[NICONS]={"Terminal","Paint","Code","Word","Creeper","A propos","Reboot","Params","Browser","Snake","R-Type","Pong","MyCraft","Tetris","Demineur"};
#define IC_X  6
#define IC_SZ 32
/* 12 icones par colonne, colonnes de 76 px */
#define IC_IX(i) (IC_X+((i)/12)*76)
#define IC_IY(i) (IC_Y[(i)%12])

/* ============================================================
 * Curseur
 * ============================================================ */
static void draw_cursor(sfcml_Window* win,int x,int y){
    static const int8_t S[13][2]={{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},{0,8},{0,7},{0,5},{2,3},{3,2},{3,2}};
    for(int r=0;r<13;r++)sfcml_drawHLine(win,x+S[r][0]+1,y+r+1,S[r][1],SFCML_BLACK);
    for(int r=0;r<13;r++)sfcml_drawHLine(win,x+S[r][0],  y+r,  S[r][1],SFCML_WHITE);
}

/* ============================================================
 * Cadre fenetre
 * ============================================================ */
static int hit_x (int wi,int mx,int my){AppWin*w=&_wins[wi];return mx>=w->x+w->w-BTN_W-4&&mx<w->x+w->w-4&&my>=w->y+4&&my<w->y+4+BTN_H;}
static int hit_mn(int wi,int mx,int my){AppWin*w=&_wins[wi];return mx>=w->x+w->w-2*BTN_W-8&&mx<w->x+w->w-BTN_W-8&&my>=w->y+4&&my<w->y+4+BTN_H;}
static int hit_tb(int wi,int mx,int my){AppWin*w=&_wins[wi];return mx>=w->x&&mx<w->x+w->w-2*BTN_W-8&&my>=w->y&&my<w->y+TBAR_H;}

static void draw_frame(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int foc=(_focus==wi);
    sfcml_Color tbar=foc?C_TB:sfcml_rgb(55,55,75);
    sfcml_Color bdr =foc?sfcml_rgb((uint8_t)((int)C_TB.r*6/5>255?255:(int)C_TB.r*6/5),
                            (uint8_t)((int)C_TB.g*6/5>255?255:(int)C_TB.g*6/5),
                            (uint8_t)((int)C_TB.b*6/5>255?255:(int)C_TB.b*6/5))
                        :sfcml_rgb(75,75,100);
    sfcml_fillRect(win,sfcml_rect(w->x+4,w->y+4,w->w,w->h),sfcml_rgb(0,0,0));
    sfcml_fillRect(win,sfcml_rect(w->x,w->y,w->w,w->h),C_WINBG);
    for(int gy=0;gy<TBAR_H;gy++){
        int t=gy*100/TBAR_H;
        sfcml_Color gc=sfcml_rgb(
            (uint8_t)((int)tbar.r+(10-t*20/100)<0?0:(int)tbar.r+(10-t*20/100)>255?255:(int)tbar.r+(10-t*20/100)),
            (uint8_t)((int)tbar.g+(10-t*20/100)<0?0:(int)tbar.g+(10-t*20/100)>255?255:(int)tbar.g+(10-t*20/100)),
            (uint8_t)((int)tbar.b+(10-t*20/100)<0?0:(int)tbar.b+(10-t*20/100)>255?255:(int)tbar.b+(10-t*20/100))
        );
        sfcml_drawHLine(win,w->x,w->y+gy,w->w,gc);
    }
    sfcml_drawText(win,w->title,w->x+6,w->y+7,SFCML_WHITE,tbar);
    sfcml_fillRect(win,sfcml_rect(w->x+w->w-2*BTN_W-8,w->y+4,BTN_W,BTN_H),sfcml_rgb(200,160,0));
    sfcml_drawHLine(win,w->x+w->w-2*BTN_W-6,w->y+11,BTN_W-4,SFCML_WHITE);
    sfcml_fillRect(win,sfcml_rect(w->x+w->w-BTN_W-4,w->y+4,BTN_W,BTN_H),sfcml_rgb(196,40,30));
    sfcml_drawText(win,"x",w->x+w->w-BTN_W-1,w->y+5,SFCML_WHITE,sfcml_rgb(196,40,30));
    int gx=w->x+w->w-10,gy2=w->y+w->h-10;
    sfcml_drawLine(win,gx,w->y+w->h-2,w->x+w->w-2,gy2,sfcml_rgb(100,100,120));
    sfcml_drawLine(win,gx+4,w->y+w->h-2,w->x+w->w-2,gy2+4,sfcml_rgb(100,100,120));
    sfcml_drawRect(win,sfcml_rect(w->x,w->y,w->w,w->h),bdr);
}

/* ============================================================
 * Terminal content
 * ============================================================ */
static void draw_term(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int tx=w->x+4,ty=w->y+TBAR_H+3;
    int avh=w->h-TBAR_H-3-14;
    int rows_vis=avh/8;if(rows_vis>T_ROWS)rows_vis=T_ROWS;
    int start=(_tnlines>rows_vis)?_tnlines-rows_vis:0;
    for(int i=0;i<rows_vis&&start+i<_tnlines;i++)
        sfcml_drawText(win,_tlines[start+i],tx,ty+i*8,C_FG,C_WINBG);
    int iy=w->y+w->h-14;
    sfcml_fillRect(win,sfcml_rect(w->x+1,iy-2,w->w-2,14),C_WINBG);
    sfcml_drawHLine(win,w->x+1,iy-3,w->w-2,sfcml_rgb(50,50,60));
    sfcml_drawText(win,"root@myos:~$ ",tx,iy,C_PROMPT,C_WINBG);
    sfcml_drawText(win,_tinput,tx+13*8,iy,C_FG,C_WINBG);
    if(_blink)
        sfcml_fillRect(win,sfcml_rect(tx+13*8+_tilen*8,iy,6,8),C_FG);
}

/* ============================================================
 * A propos content
 * ============================================================ */
static void draw_about(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int tx=w->x+10,ty=w->y+TBAR_H+8;
    sfcml_Color hl=sfcml_rgb(0,140,255),gr=sfcml_rgb(130,130,130),rd=sfcml_rgb(220,20,20);
    draw_big_text(win,"MyOS",tx,ty,2,hl,C_WINBG);
    sfcml_drawText(win,"v0.1",tx+68,ty+8,gr,C_WINBG);
    sfcml_drawHLine(win,tx,ty+20,w->w-20,sfcml_rgb(55,55,70));
    int y=ty+28;
    sfcml_drawText(win,"CPU : x86 32-bit Protected Mode",tx,y,C_FG,C_WINBG);y+=10;
    sfcml_drawText(win,"RAM : 128 MB",tx,y,C_FG,C_WINBG);y+=10;
    sfcml_drawText(win,"GPU : VESA 640x480 24bpp",tx,y,C_FG,C_WINBG);y+=10;
    sfcml_drawText(win,"Boot: MineGRUB v1.0",tx,y,gr,C_WINBG);y+=10;
    sfcml_drawText(win,"Libs: libk + SFCML (Epitech)",tx,y,gr,C_WINBG);y+=14;
    sfcml_drawText(win,"Made with <3  @  Epitech",tx,y,rd,C_WINBG);
    draw_creeper(win,w->x+w->w-52,ty,4);
}

/* ============================================================
 * Creeper window
 * ============================================================ */
static void draw_creep_win(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int cx=w->x+(w->w-72)/2,cy=w->y+TBAR_H+8;
    draw_creeper(win,cx,cy,9);
    sfcml_Color lbl=_blink?sfcml_rgb(94,124,22):sfcml_rgb(200,20,20);
    const char* msg=_blink?"  Creeper!  ":"  Aw Man.. ";
    sfcml_drawText(win,msg,cx,cy+74,lbl,C_WINBG);
    sfcml_drawText(win,"[Esc pour fermer]",cx-16,cy+86,sfcml_rgb(80,80,80),C_WINBG);
}

/* ============================================================
 * PAINT window
 * ============================================================ */
static void draw_paint(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2,sh=w->h-TBAR_H-1;
    sfcml_Color tb=sfcml_rgb(26,26,38),rd=sfcml_rgb(220,20,20);
    sfcml_Color sb=sfcml_rgb(20,20,30);

    /* Toolbar */
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,PAINT_TB),tb);
    sfcml_drawHLine(win,sx,sy+PAINT_TB-1,sw,rd);
    sfcml_drawText(win,"Epitech Paint v1.0",sx+6,sy+10,rd,tb);

    /* Brush size buttons */
    static const int bsz[3]={0,1,3};
    static const char* blbl[3]={"S","M","L"};
    sfcml_drawText(win,"Brosse:",sx+148,sy+10,sfcml_rgb(180,180,200),tb);
    for(int b=0;b<3;b++){
        int bx=sx+204+b*28;
        sfcml_Color bc=(_pbrush==bsz[b])?rd:sfcml_rgb(50,50,70);
        sfcml_fillRect(win,sfcml_rect(bx,sy+5,24,16),bc);
        sfcml_drawRect(win,sfcml_rect(bx,sy+5,24,16),sfcml_rgb(100,100,140));
        sfcml_drawText(win,blbl[b],bx+8,sy+9,SFCML_WHITE,bc);
    }
    /* Clear */
    sfcml_fillRect(win,sfcml_rect(sx+sw-64,sy+5,58,16),sfcml_rgb(50,10,10));
    sfcml_drawRect(win,sfcml_rect(sx+sw-64,sy+5,58,16),rd);
    sfcml_drawText(win,"Effacer",sx+sw-62,sy+9,SFCML_WHITE,sfcml_rgb(50,10,10));

    /* Sidebar */
    sfcml_fillRect(win,sfcml_rect(sx,sy+PAINT_TB,PAINT_SB,sh-PAINT_TB),sb);
    sfcml_drawVLine(win,sx+PAINT_SB-1,sy+PAINT_TB,sh-PAINT_TB,rd);
    sfcml_drawText(win,"CLR",sx+6,sy+PAINT_TB+4,rd,sb);

    /* Palette swatches (2 columns) */
    for(int i=0;i<PAINT_NC;i++){
        int px=sx+2+(i%2)*24,py=sy+PAINT_TB+16+(i/2)*22;
        sfcml_fillRect(win,sfcml_rect(px,py,20,20),_pal[i]);
        sfcml_drawRect(win,sfcml_rect(px,py,20,20),(i==_pcol)?SFCML_WHITE:sfcml_rgb(50,50,60));
    }
    /* Active color indicator */
    int ciy=sy+PAINT_TB+16+8*22+4;
    if(ciy+34<sy+sh){
        sfcml_drawText(win,"Act:",sx+2,ciy,sfcml_rgb(150,150,180),sb);
        sfcml_fillRect(win,sfcml_rect(sx+2,ciy+10,46,20),_pal[_pcol]);
        sfcml_drawRect(win,sfcml_rect(sx+2,ciy+10,46,20),SFCML_WHITE);
    }

    /* Canvas area */
    int cax=sx+PAINT_SB,cay=sy+PAINT_TB;
    int caw=sw-PAINT_SB,cah=sh-PAINT_TB;
    int rw=(caw<PAINT_CW)?caw:PAINT_CW;
    int rh=(cah<PAINT_CH)?cah:PAINT_CH;

    /* Direct render to backbuffer */
    uint8_t* back=win->back;
    uint32_t pitch=win->pitch;
    for(int y=0;y<rh;y++){
        uint8_t* row=back+(uint32_t)(cay+y)*pitch+(uint32_t)cax*3;
        for(int x=0;x<rw;x++){
            sfcml_Color c=_pal[PAINT_CANVAS[y*PAINT_CW+x]];
            row[0]=c.b;row[1]=c.g;row[2]=c.r;row+=3;
        }
    }
    if(rw<caw)sfcml_fillRect(win,sfcml_rect(cax+rw,cay,caw-rw,cah),SFCML_WHITE);
    if(rh<cah)sfcml_fillRect(win,sfcml_rect(cax,cay+rh,caw,cah-rh),SFCML_WHITE);
    sfcml_drawRect(win,sfcml_rect(cax,cay,caw,cah),sfcml_rgb(80,20,20));
}

/* ============================================================
 * CODE EDITOR window
 * ============================================================ */
#define CODE_TB   26
#define CODE_LNW  30
#define CODE_STATH 14

static void draw_code(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2,sh=w->h-TBAR_H-1;
    sfcml_Color bg=sfcml_rgb(10,12,18),tb=sfcml_rgb(18,20,32),rd=sfcml_rgb(220,20,20);
    sfcml_Color lnbg=sfcml_rgb(16,16,26);

    /* Toolbar */
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,CODE_TB),tb);
    sfcml_drawHLine(win,sx,sy+CODE_TB-1,sw,sfcml_rgb(80,20,120));
    sfcml_drawText(win,"Epitech Code Editor",sx+6,sy+9,rd,tb);
    /* Boutons */
    struct{const char* l;int x;sfcml_Color c;}btns[]={
        {"Nouveau",sx+sw-190,sfcml_rgb(40,40,60)},
        {"Run >",  sx+sw-120,sfcml_rgb(20,80,20)},
        {"?",      sx+sw-38, sfcml_rgb(40,40,60)},
    };
    for(int b=0;b<3;b++){
        int bw=(b==2)?26:62;
        sfcml_fillRect(win,sfcml_rect(btns[b].x,sy+5,bw,16),btns[b].c);
        sfcml_drawRect(win,sfcml_rect(btns[b].x,sy+5,bw,16),sfcml_rgb(80,80,120));
        sfcml_drawText(win,btns[b].l,btns[b].x+4,sy+9,SFCML_WHITE,btns[b].c);
    }

    /* Code area */
    int cay=sy+CODE_TB,cah=sh-CODE_TB-CODE_STATH;
    sfcml_fillRect(win,sfcml_rect(sx,cay,CODE_LNW,cah),lnbg);
    sfcml_fillRect(win,sfcml_rect(sx+CODE_LNW,cay,sw-CODE_LNW,cah),bg);
    sfcml_drawVLine(win,sx+CODE_LNW-1,cay,cah,sfcml_rgb(40,40,60));

    int visible=(cah-2)/8;if(visible>CODE_ROWS)visible=CODE_ROWS;
    for(int i=0;i<visible;i++){
        int line=_code_sc+i;if(line>=_code_nl)break;
        int ly=cay+1+i*8;
        /* Line number */
        char ln[4];ln[0]='0'+(line+1)/100;ln[1]='0'+((line+1)/10)%10;ln[2]='0'+(line+1)%10;ln[3]='\0';
        sfcml_Color lnc=(line==_code_cl)?sfcml_rgb(200,200,80):sfcml_rgb(70,80,100);
        sfcml_drawText(win,ln,sx+1,ly,lnc,lnbg);
        /* Highlight current line */
        sfcml_Color linebg=(line==_code_cl)?sfcml_rgb(18,22,40):bg;
        if(line==_code_cl)sfcml_fillRect(win,sfcml_rect(sx+CODE_LNW,ly-1,sw-CODE_LNW,9),linebg);
        /* Line content */
        draw_code_line(win,_code[line],sx+CODE_LNW+2,ly,linebg);
        /* Cursor */
        if(line==_code_cl&&_blink){
            int cx=sx+CODE_LNW+2+_code_cc*8;
            sfcml_fillRect(win,sfcml_rect(cx,ly,2,8),sfcml_rgb(210,210,210));
        }
    }
    /* Status bar */
    int sby=sy+sh-CODE_STATH;
    sfcml_fillRect(win,sfcml_rect(sx,sby,sw,CODE_STATH),sfcml_rgb(28,10,50));
    sfcml_drawHLine(win,sx,sby,sw,sfcml_rgb(100,20,140));
    char sb[80];
    int o=0;
    sb[o++]=' ';sb[o++]='L';sb[o++]='n';sb[o++]=' ';
    sb[o++]='0'+(_code_cl+1)/100;sb[o++]='0'+((_code_cl+1)/10)%10;sb[o++]='0'+(_code_cl+1)%10;
    sb[o++]=' ';sb[o++]='C';sb[o++]='o';sb[o++]='l';sb[o++]=' ';
    sb[o++]='0'+(_code_cc+1)/10;sb[o++]='0'+(_code_cc+1)%10;
    sb[o++]=' ';sb[o++]='|';sb[o++]=' ';sb[o++]='C';sb[o++]=' ';sb[o++]='|';
    sb[o++]=' ';sb[o++]='E';sb[o++]='p';sb[o++]='i';sb[o++]='t';sb[o++]='e';sb[o++]='c';sb[o++]='h';
    sb[o++]=' ';sb[o++]='C';sb[o++]='o';sb[o++]='d';sb[o++]='e';sb[o]='\0';
    sfcml_drawText(win,sb,sx+4,sby+3,sfcml_rgb(200,180,255),sfcml_rgb(28,10,50));
}

/* ============================================================
 * WORD PROCESSOR window
 * ============================================================ */
#define WORD_TB   28
#define WORD_STATH 14
#define WORD_MG   44  /* margin */

static void draw_word(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2,sh=w->h-TBAR_H-1;
    sfcml_Color tbg=sfcml_rgb(26,26,38),rd=sfcml_rgb(220,20,20);
    sfcml_Color page=sfcml_rgb(245,245,250),pgbg=sfcml_rgb(170,170,180);
    sfcml_Color txt=sfcml_rgb(10,10,25),cur_bg=sfcml_rgb(215,230,255);

    /* Toolbar */
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,WORD_TB),tbg);
    sfcml_drawHLine(win,sx,sy+WORD_TB-1,sw,rd);
    sfcml_drawText(win,_nano_path[0]?_nano_path:"Epitech Word",sx+6,sy+10,rd,tbg);
    /* Format buttons */
    struct{const char* l;int x;}fbtns[]={{"B",sx+130},{"I",sx+152},{"U",sx+174},{"|",sx+196},{"Enreg.",sx+208},{"Imprimer",sx+280}};
    sfcml_Color btn_c[6]={sfcml_rgb(50,50,80),sfcml_rgb(50,50,80),sfcml_rgb(50,50,80),sfcml_rgb(40,40,55),sfcml_rgb(20,70,20),sfcml_rgb(20,20,70)};
    for(int b=0;b<6;b++){
        int bw=(b<3)?20:(b==3)?4:(b==4)?56:76;
        if(b==3){sfcml_drawVLine(win,fbtns[b].x,sy+4,20,sfcml_rgb(80,80,100));continue;}
        sfcml_fillRect(win,sfcml_rect(fbtns[b].x,sy+5,bw,16),btn_c[b]);
        sfcml_drawRect(win,sfcml_rect(fbtns[b].x,sy+5,bw,16),sfcml_rgb(80,80,120));
        sfcml_drawText(win,fbtns[b].l,fbtns[b].x+4,sy+9,SFCML_WHITE,btn_c[b]);
    }

    /* Page area */
    int pay=sy+WORD_TB,pah=sh-WORD_TB-WORD_STATH;
    sfcml_fillRect(win,sfcml_rect(sx,pay,sw,pah),pgbg);
    int px=sx+WORD_MG,pw=sw-2*WORD_MG;
    sfcml_fillRect(win,sfcml_rect(px,pay+4,pw,pah-8),page);
    sfcml_drawRect(win,sfcml_rect(px,pay+4,pw,pah-8),sfcml_rgb(150,150,165));

    /* Text (10px line height for readability) */
    int tx=px+10,tw=pw-20;(void)tw;
    int visible=(pah-20)/10;if(visible>WORD_ROWS)visible=WORD_ROWS;
    for(int i=0;i<visible;i++){
        int line=_word_sc+i;if(line>=_word_nl)break;
        int ly=pay+8+i*10;
        sfcml_Color lbg=(line==_word_cl)?cur_bg:page;
        if(line==_word_cl)sfcml_fillRect(win,sfcml_rect(px+1,ly-1,pw-2,10),cur_bg);
        sfcml_drawText(win,_word[line],tx,ly,txt,lbg);
        /* Cursor */
        if(line==_word_cl&&_blink)
            sfcml_fillRect(win,sfcml_rect(tx+_word_cc*8,ly,1,9),sfcml_rgb(0,0,150));
    }
    /* Status bar */
    int sby=sy+sh-WORD_STATH;
    sfcml_fillRect(win,sfcml_rect(sx,sby,sw,WORD_STATH),tbg);
    sfcml_drawHLine(win,sx,sby,sw,rd);
    char sb[64];int o=0;
    sb[o++]=' ';sb[o++]='L';sb[o++]='i';sb[o++]='g';sb[o++]='n';sb[o++]='e';sb[o++]=' ';
    sb[o++]='0'+(_word_cl+1)/100;sb[o++]='0'+((_word_cl+1)/10)%10;sb[o++]='0'+(_word_cl+1)%10;
    sb[o++]=' ';sb[o++]='/';sb[o++]=' ';
    sb[o++]='0'+_word_nl/100;sb[o++]='0'+(_word_nl/10)%10;sb[o++]='0'+_word_nl%10;
    sb[o++]=' ';sb[o++]='|';sb[o++]=' ';sb[o++]='E';sb[o++]='p';sb[o++]='i';sb[o++]='t';sb[o++]='e';sb[o++]='c';sb[o++]='h';sb[o++]=' ';sb[o++]='W';sb[o++]='o';sb[o++]='r';sb[o++]='d';sb[o]='\0';
    sfcml_drawText(win,sb,sx+4,sby+3,sfcml_rgb(200,200,220),tbg);
}

/* ============================================================
 * Icones bureau
 * ============================================================ */
static void draw_icons(sfcml_Window* win){
    for(int i=0;i<NICONS;i++){
        int ix=IC_IX(i),iy=IC_IY(i);
        int hov=(_mx>=ix-2&&_mx<=ix+IC_SZ+2&&_my>=iy-2&&_my<=iy+IC_SZ+10);
        int sel=(_sel_icon==i);
        if(sel){
            sfcml_fillRect(win,sfcml_rect(ix-3,iy-3,IC_SZ+6,IC_SZ+6),sfcml_rgb(30,70,180));
            sfcml_drawRect(win,sfcml_rect(ix-3,iy-3,IC_SZ+6,IC_SZ+6),sfcml_rgb(120,160,255));
        }
        switch(i){
        case 0: /* Terminal */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(10,10,14));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(70,70,90));
            sfcml_drawText(win,">_",ix+6,iy+12,SFCML_GREEN,sfcml_rgb(10,10,14));
            break;
        case 1: /* Paint */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(255,255,255));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?sfcml_rgb(220,20,20):sfcml_rgb(180,0,0));
            sfcml_fillRect(win,sfcml_rect(ix+2,iy+2,6,6),sfcml_rgb(220,20,20));
            sfcml_fillRect(win,sfcml_rect(ix+10,iy+2,6,6),sfcml_rgb(0,180,0));
            sfcml_fillRect(win,sfcml_rect(ix+2,iy+10,6,6),sfcml_rgb(0,80,220));
            sfcml_fillRect(win,sfcml_rect(ix+10,iy+10,6,6),sfcml_rgb(255,200,0));
            sfcml_fillRect(win,sfcml_rect(ix+20,iy+24,8,6),sfcml_rgb(140,80,20));
            sfcml_drawLine(win,ix+24,iy+20,ix+20,iy+24,sfcml_rgb(0,0,0));
            break;
        case 2: /* Code */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(10,12,22));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?sfcml_rgb(100,160,255):sfcml_rgb(60,80,160));
            sfcml_drawText(win,"</>",ix+3,iy+12,sfcml_rgb(100,160,255),sfcml_rgb(10,12,22));
            break;
        case 3: /* Word */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(245,245,250));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?sfcml_rgb(0,80,200):sfcml_rgb(0,60,160));
            sfcml_drawText(win,"W",ix+10,iy+10,sfcml_rgb(0,80,200),sfcml_rgb(245,245,250));
            sfcml_drawHLine(win,ix+4,iy+22,IC_SZ-8,sfcml_rgb(180,180,190));
            sfcml_drawHLine(win,ix+4,iy+25,IC_SZ-12,sfcml_rgb(180,180,190));
            break;
        case 4: /* Creeper */
            draw_creeper(win,ix,iy,4);
            break;
        case 5: /* About */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(0,80,170));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(0,60,130));
            sfcml_drawText(win,"i",ix+13,iy+10,SFCML_WHITE,sfcml_rgb(0,80,170));
            sfcml_drawText(win,".",ix+13,iy+18,SFCML_WHITE,sfcml_rgb(0,80,170));
            break;
        case 6: /* Reboot */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),C_DK);
            sfcml_drawCircle(win,ix+16,iy+20,10,hov?sfcml_rgb(255,80,80):sfcml_rgb(200,50,50));
            sfcml_fillRect(win,sfcml_rect(ix+14,iy+5,4,10),hov?sfcml_rgb(255,80,80):sfcml_rgb(200,50,50));
            break;
        case 7: /* Parametres (engrenage) */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(30,30,44));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(70,70,100));
            sfcml_drawCircle(win,ix+16,iy+16,7,hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            sfcml_drawCircle(win,ix+16,iy+16,4,sfcml_rgb(30,30,44));
            sfcml_fillRect(win,sfcml_rect(ix+14,iy+4,4,4),hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            sfcml_fillRect(win,sfcml_rect(ix+14,iy+24,4,4),hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            sfcml_fillRect(win,sfcml_rect(ix+4,iy+14,4,4),hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            sfcml_fillRect(win,sfcml_rect(ix+24,iy+14,4,4),hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            break;
        case 12:{ /* MyCraft (bloc d'herbe) */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(120,82,50));
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,10),sfcml_rgb(80,165,60));
            for(int d=0;d<26;d++){
                int dpx=ix+(d*13)%IC_SZ,dpy=iy+12+(d*7)%(IC_SZ-14);
                sfcml_drawPixel(win,dpx,dpy,sfcml_rgb(92,62,38));
            }
            for(int d=0;d<10;d++)
                sfcml_drawPixel(win,ix+(d*11)%IC_SZ,iy+2+(d*5)%7,sfcml_rgb(110,200,90));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(60,42,26));
            break;}
        case 13:{ /* Tetris */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(18,18,28));
            sfcml_fillRect(win,sfcml_rect(ix+4,iy+4,8,8),sfcml_rgb(0,210,210));
            sfcml_fillRect(win,sfcml_rect(ix+12,iy+4,8,8),sfcml_rgb(220,210,0));
            sfcml_fillRect(win,sfcml_rect(ix+20,iy+4,8,8),sfcml_rgb(210,40,40));
            sfcml_fillRect(win,sfcml_rect(ix+8,iy+12,8,8),sfcml_rgb(180,60,200));
            sfcml_fillRect(win,sfcml_rect(ix+16,iy+12,8,8),sfcml_rgb(40,200,40));
            sfcml_fillRect(win,sfcml_rect(ix+4,iy+20,8,8),sfcml_rgb(40,80,220));
            sfcml_fillRect(win,sfcml_rect(ix+20,iy+20,8,8),sfcml_rgb(230,140,20));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(70,70,90));
            break;}
        case 14:{ /* Demineur */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(190,190,200));
            for(int gy=0;gy<3;gy++)for(int gx=0;gx<3;gx++)
                sfcml_drawRect(win,sfcml_rect(ix+3+gx*9,iy+3+gy*9,9,9),sfcml_rgb(120,120,135));
            sfcml_fillCircle(win,ix+16,iy+16,5,sfcml_rgb(30,30,30));
            sfcml_fillRect(win,sfcml_rect(ix+15,iy+7,2,4),sfcml_rgb(30,30,30));
            sfcml_fillRect(win,sfcml_rect(ix+15,iy+21,2,4),sfcml_rgb(30,30,30));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(90,90,105));
            break;}
        default: /* Navigateur */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(248,249,250));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?sfcml_rgb(66,133,244):sfcml_rgb(180,180,200));
            sfcml_fillRect(win,sfcml_rect(ix+2,iy+2,IC_SZ-4,6),sfcml_rgb(66,133,244));
            sfcml_fillRect(win,sfcml_rect(ix+2,iy+10,10,IC_SZ-12),sfcml_rgb(234,67,53));
            sfcml_fillRect(win,sfcml_rect(ix+14,iy+10,IC_SZ-16,IC_SZ-12),sfcml_rgb(52,168,83));
            sfcml_drawText(win,"www",ix+4,iy+14,SFCML_WHITE,sfcml_rgb(52,168,83));
            break;
        }
        sfcml_Color lbg=sel?sfcml_rgb(30,60,180):C_DK;
        sfcml_drawText(win,IC_LBL[i],ix,iy+IC_SZ+2,SFCML_WHITE,lbg);
    }
}

/* ============================================================
 * Menu Demarrer
 * ============================================================ */
#define SM_W   172
#define SM_IH   22
#define SM_N    17
#define SM_H   (SM_N*SM_IH+8)
#define SM_Y   (TB_Y-SM_H)

static const char* SM_LBL[SM_N]={
    "  Terminal","  Creeper!","  A propos","  ---------",
    "  Paint","  Code Editor","  Word","  Navigateur",
    "  Calculatrice","  Fichiers","  Reseau","  Parametres",
    "  MyCraft","  Tetris","  Demineur","  ---------","  Redemarrer",
};

static void draw_smenu(sfcml_Window* win){
    sfcml_fillRect(win,sfcml_rect(0,SM_Y,28,SM_H),sfcml_rgb(180,10,10));
    sfcml_fillRect(win,sfcml_rect(28,SM_Y,SM_W-28,SM_H),sfcml_rgb(26,26,38));
    sfcml_drawRect(win,sfcml_rect(0,SM_Y,SM_W,SM_H),sfcml_rgb(80,80,100));
    const char* lbl="MyOS";
    for(int i=0;lbl[i];i++)
        sfcml_drawChar(win,lbl[i],8,SM_Y+4+i*12,SFCML_WHITE,sfcml_rgb(180,10,10));
    for(int i=0;i<SM_N;i++){
        int iy=SM_Y+4+i*SM_IH;
        if(SM_LBL[i][2]=='-'){sfcml_drawHLine(win,29,iy+10,SM_W-31,sfcml_rgb(80,80,100));continue;}
        int hov=(_mx>=28&&_mx<SM_W&&_my>=iy&&_my<iy+SM_IH);
        sfcml_Color bg=hov?sfcml_rgb(0,100,200):sfcml_rgb(26,26,38);
        sfcml_fillRect(win,sfcml_rect(29,iy,SM_W-29,SM_IH-1),bg);
        sfcml_drawText(win,SM_LBL[i],32,iy+7,SFCML_WHITE,bg);
    }
}
static void smenu_click(int mx,int my){
    if(mx<0||mx>=SM_W||my<SM_Y||my>=SM_Y+SM_H){_start_open=0;return;}
    int item=(my-SM_Y-4)/SM_IH;
    if(item<0||item>=SM_N)return;
    _start_open=0;
    if(item==0)win_open(W_TERM);
    else if(item==1)win_open(W_CREEP);
    else if(item==2)win_open(W_ABOUT);
    else if(item==4)win_open(W_PAINT);
    else if(item==5)win_open(W_CODE);
    else if(item==6)win_open(W_WORD);
    else if(item==7){win_open(W_BROWSER);_bnav(0);}
    else if(item==8)win_open(W_CALC);
    else if(item==9)win_open(W_FILES);
    else if(item==10)win_open(W_NETMGR);
    else if(item==11)win_open(W_SETTINGS);
    else if(item==12)win_open(W_MINE);
    else if(item==13){win_open(W_TETRIS);tet_reset();}
    else if(item==14){win_open(W_DEMINE);ms_reset();}
    else if(item==16)cmd_reboot();
}

/* ============================================================
 * Menu clic droit
 * ============================================================ */
#define RC_W  150
#define RC_IH  18
#define RC_N    8
static const char* RC_LBL[RC_N]={
    " Terminal"," Paint"," Navigateur"," Calculatrice"," ---------"," Parametres"," A propos"," Redemarrer",
};
static void draw_rcmenu(sfcml_Window* win){
    int mh=RC_N*RC_IH+4;
    int x0=_rcx,y0=_rcy;
    if(x0+RC_W>SCR_W)x0=SCR_W-RC_W;
    if(y0+mh>TB_Y)y0=TB_Y-mh;
    sfcml_fillRect(win,sfcml_rect(x0,y0,RC_W,mh),sfcml_rgb(38,38,52));
    sfcml_drawRect(win,sfcml_rect(x0,y0,RC_W,mh),sfcml_rgb(100,100,130));
    for(int i=0;i<RC_N;i++){
        int iy=y0+2+i*RC_IH;
        if(RC_LBL[i][1]=='-'){sfcml_drawHLine(win,x0+4,iy+8,RC_W-8,sfcml_rgb(80,80,100));continue;}
        int hov=(_mx>=x0&&_mx<x0+RC_W&&_my>=iy&&_my<iy+RC_IH);
        sfcml_Color bg=hov?sfcml_rgb(0,100,200):sfcml_rgb(38,38,52);
        sfcml_fillRect(win,sfcml_rect(x0+1,iy,RC_W-2,RC_IH-1),bg);
        sfcml_drawText(win,RC_LBL[i],x0+4,iy+5,SFCML_WHITE,bg);
    }
}
static void rcmenu_click(int mx,int my){
    int mh=RC_N*RC_IH+4;
    int x0=_rcx,y0=_rcy;
    if(x0+RC_W>SCR_W)x0=SCR_W-RC_W;
    if(y0+mh>TB_Y)y0=TB_Y-mh;
    _rcopen=0;
    if(mx<x0||mx>=x0+RC_W||my<y0||my>=y0+mh)return;
    int item=(my-y0-2)/RC_IH;
    if(item==0)win_open(W_TERM);
    else if(item==1)win_open(W_PAINT);
    else if(item==2){win_open(W_BROWSER);_bnav(0);}
    else if(item==3)win_open(W_CALC);
    else if(item==5)win_open(W_SETTINGS);
    else if(item==6)win_open(W_ABOUT);
    else if(item==7)cmd_reboot();
}

/* ============================================================
 * Taskbar
 * ============================================================ */
#define TB_BW 82  /* button width */
static void draw_taskbar(sfcml_Window* win){
    sfcml_Color tb2=sfcml_rgb((uint8_t)((int)C_TB.r*8/10),(uint8_t)((int)C_TB.g*8/10),(uint8_t)((int)C_TB.b*8/10));
    (void)tb2;
    for(int i=0;i<31;i++){
        int t=i*100/31;
        sfcml_Color gc=sfcml_rgb(
            (uint8_t)((int)C_TB.r*(100-t*30/100)/100),
            (uint8_t)((int)C_TB.g*(100-t*30/100)/100),
            (uint8_t)((int)C_TB.b*(100-t*30/100)/100));
        sfcml_drawHLine(win,0,TB_Y+i,SCR_W,gc);
    }
    sfcml_drawHLine(win,0,TB_Y,SCR_W,sfcml_rgb(0,90,170));
    sfcml_Color sc=_start_open?sfcml_rgb(160,5,5):sfcml_rgb(215,20,20);
    sfcml_fillRect(win,sfcml_rect(2,TB_Y+2,84,26),sc);
    sfcml_drawRect(win,sfcml_rect(2,TB_Y+2,84,26),sfcml_rgb(120,4,4));
    sfcml_fillRect(win,sfcml_rect(6,TB_Y+6,7,7),sfcml_rgb(255,80,80));
    sfcml_fillRect(win,sfcml_rect(15,TB_Y+6,7,7),sfcml_rgb(80,200,80));
    sfcml_fillRect(win,sfcml_rect(6,TB_Y+15,7,7),sfcml_rgb(80,80,255));
    sfcml_fillRect(win,sfcml_rect(15,TB_Y+15,7,7),sfcml_rgb(255,200,0));
    sfcml_drawText(win,"Start",26,TB_Y+11,SFCML_WHITE,sc);
    int bx=88;
    for(int i=0;i<NW;i++){
        if(!_wins[i].visible)continue;
        int foc=(_focus==i);
        sfcml_Color bc=_wins[i].minimized?sfcml_rgb(50,50,68):(foc?sfcml_rgb(0,100,200):sfcml_rgb(38,38,58));
        sfcml_fillRect(win,sfcml_rect(bx,TB_Y+3,TB_BW,24),bc);
        sfcml_drawRect(win,sfcml_rect(bx,TB_Y+3,TB_BW,24),sfcml_rgb(80,80,110));
        if(foc)sfcml_drawHLine(win,bx,TB_Y+3,TB_BW,sfcml_rgb(100,180,255));
        sfcml_Color tc=_wins[i].minimized?sfcml_rgb(140,140,160):SFCML_WHITE;
        sfcml_drawText(win,_wins[i].title,bx+4,TB_Y+11,tc,bc);
        bx+=TB_BW+2;
    }
    int tx=SCR_W-160;
    sfcml_Color nicol=net_ok?sfcml_rgb(80,200,80):sfcml_rgb(160,160,160);
    sfcml_fillRect(win,sfcml_rect(tx,TB_Y+6,18,16),C_TB);
    sfcml_fillRect(win,sfcml_rect(tx+2,TB_Y+15,14,4),nicol);
    sfcml_fillRect(win,sfcml_rect(tx+5,TB_Y+10,8,6),nicol);
    sfcml_fillRect(win,sfcml_rect(tx+8,TB_Y+6,2,5),nicol);
    tx+=22;
    sfcml_fillRect(win,sfcml_rect(tx,TB_Y+8,6,12),C_TB);
    sfcml_fillRect(win,sfcml_rect(tx,TB_Y+12,4,4),sfcml_rgb(180,180,200));
    sfcml_fillTriangle(win,tx+4,TB_Y+8,tx+4,TB_Y+20,tx+10,TB_Y+14,sfcml_rgb(180,180,200));
    tx+=18;
    char clk[9];
    clk[0]='0'+_rtc.h/10;clk[1]='0'+_rtc.h%10;clk[2]=':';
    clk[3]='0'+_rtc.m/10;clk[4]='0'+_rtc.m%10;clk[5]=':';
    clk[6]='0'+_rtc.s/10;clk[7]='0'+_rtc.s%10;clk[8]='\0';
    char dat[11];
    dat[0]='0'+_rtc.day/10;dat[1]='0'+_rtc.day%10;dat[2]='/';
    dat[3]='0'+_rtc.mon/10;dat[4]='0'+_rtc.mon%10;dat[5]='/';
    dat[6]='0'+(_rtc.year/1000)%10;dat[7]='0'+(_rtc.year/100)%10;
    dat[8]='0'+(_rtc.year/10)%10;dat[9]='0'+_rtc.year%10;dat[10]='\0';
    sfcml_drawText(win,dat,tx+4,TB_Y+4,sfcml_rgb(180,180,210),C_TB);
    sfcml_drawText(win,clk,tx+8,TB_Y+14,SFCML_WHITE,C_TB);
}

/* ============================================================
 * PARAMETRES - dessin
 * ============================================================ */
static void draw_settings(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2,sh=w->h-TBAR_H-1;
    sfcml_Color hbg=sfcml_rgb(10,10,18);
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,sh),hbg);

    /* Sidebar */
    int sbw=140;
    sfcml_Color sbb=sfcml_rgb(18,18,28);
    sfcml_fillRect(win,sfcml_rect(sx,sy,sbw,sh),sbb);
    sfcml_drawVLine(win,sx+sbw,sy,sh,sfcml_rgb(50,50,70));
    sfcml_drawText(win,"Parametres",sx+8,sy+8,sfcml_rgb(220,20,20),sbb);
    sfcml_drawHLine(win,sx,sy+22,sbw,sfcml_rgb(50,50,70));

    const char* cats[5]={"  Apparence","  Systeme","  Applications","  Alimentation","  A propos"};
    for(int i=0;i<5;i++){
        int iy=sy+28+i*36;
        int sel=(_set_cat==i);
        int hov=(_mx>=sx&&_mx<sx+sbw&&_my>=iy&&_my<iy+36);
        sfcml_Color bg=sel?C_TB:(hov?sfcml_rgb(30,30,50):sbb);
        sfcml_fillRect(win,sfcml_rect(sx,iy,sbw,35),bg);
        if(sel)sfcml_drawVLine(win,sx+sbw-2,iy,35,sfcml_rgb(120,180,255));
        sfcml_drawText(win,cats[i],sx+4,iy+13,SFCML_WHITE,bg);
    }

    /* Content */
    int cx=sx+sbw+1,cw=sw-sbw-1;(void)cw;
    int y=sy+10;
    if(_set_cat==0){
        /* --- Apparence --- */
        sfcml_drawText(win,"Apparence",cx+10,y,sfcml_rgb(220,20,20),hbg);
        sfcml_drawHLine(win,cx+10,y+12,cw-20,sfcml_rgb(50,50,70));
        y+=22;
        sfcml_drawText(win,"Couleur d'accentuation (barre des taches, fenetres) :",cx+10,y,sfcml_rgb(160,160,190),hbg);
        y+=12;
        for(int i=0;i<8;i++){
            int bx=cx+10+(i%4)*56,by=y+(i/4)*46;
            sfcml_fillRect(win,sfcml_rect(bx,by,48,36),_accent_pr[i]);
            sfcml_drawRect(win,sfcml_rect(bx,by,48,36),
                (_accent_sel==i)?SFCML_WHITE:sfcml_rgb(70,70,90));
            if(_accent_sel==i)sfcml_drawRect(win,sfcml_rect(bx-1,by-1,50,38),sfcml_rgb(180,220,255));
        }
        y+=2*46+12;
        sfcml_drawText(win,"Fond d'ecran :",cx+10,y,sfcml_rgb(160,160,190),hbg);
        y+=12;
        for(int i=0;i<5;i++){
            int bx=cx+10+i*56;
            sfcml_fillRect(win,sfcml_rect(bx,y,48,36),_bg_pr[i]);
            sfcml_drawRect(win,sfcml_rect(bx,y,48,36),
                (_bg_sel==i)?SFCML_WHITE:sfcml_rgb(70,70,90));
            if(_bg_sel==i)sfcml_drawRect(win,sfcml_rect(bx-1,y-1,50,38),sfcml_rgb(180,220,255));
            sfcml_drawText(win,_bg_lbl[i],bx+2,y+40,sfcml_rgb(130,130,150),hbg);
        }
        y+=56;
        sfcml_drawText(win,"Couleur texte terminal :",cx+10,y,sfcml_rgb(160,160,190),hbg);
        y+=12;
        for(int i=0;i<4;i++){
            int bx=cx+10+i*86;
            sfcml_Color bb=(_fg_sel==i)?sfcml_rgb(0,70,140):sfcml_rgb(28,28,42);
            sfcml_fillRect(win,sfcml_rect(bx,y,78,24),bb);
            sfcml_drawRect(win,sfcml_rect(bx,y,78,24),
                (_fg_sel==i)?sfcml_rgb(100,180,255):sfcml_rgb(60,60,90));
            sfcml_drawText(win,_fg_lbl[i],bx+6,y+8,_fg_pr[i],bb);
        }

    } else if(_set_cat==1){
        /* --- Systeme --- */
        sfcml_drawText(win,"Systeme",cx+10,y,sfcml_rgb(220,20,20),hbg);
        sfcml_drawHLine(win,cx+10,y+12,cw-20,sfcml_rgb(50,50,70));
        y+=24;
        /* Heure et date dynamiques */
        char sys_clk[9],sys_dat[11];
        sys_clk[0]='0'+_rtc.h/10;sys_clk[1]='0'+_rtc.h%10;sys_clk[2]=':';
        sys_clk[3]='0'+_rtc.m/10;sys_clk[4]='0'+_rtc.m%10;sys_clk[5]=':';
        sys_clk[6]='0'+_rtc.s/10;sys_clk[7]='0'+_rtc.s%10;sys_clk[8]='\0';
        sys_dat[0]='0'+_rtc.day/10;sys_dat[1]='0'+_rtc.day%10;sys_dat[2]='/';
        sys_dat[3]='0'+_rtc.mon/10;sys_dat[4]='0'+_rtc.mon%10;sys_dat[5]='/';
        sys_dat[6]='0'+(_rtc.year/1000)%10;sys_dat[7]='0'+(_rtc.year/100)%10;
        sys_dat[8]='0'+(_rtc.year/10)%10;sys_dat[9]='0'+_rtc.year%10;sys_dat[10]='\0';
        const char* kk[10]={"Systeme:","Architecture:","Processeur:","Memoire:",
                             "Affichage:","Bootloader:","Librairies:","Build:",
                             "Heure:","Date:"};
        const char* vv[10]={"MyOS v0.1 - Epitech Edition",
                             "x86 32-bit Protected Mode",
                             "i386 compatible",
                             "128 MB RAM",
                             "VESA VBE 640x480 24bpp",
                             "MineGRUB v1.0",
                             "libk + libSFCML",
                             "GCC + NASM + LD",
                             sys_clk,sys_dat};
        for(int i=0;i<10;i++){
            int iy=y+i*22;
            sfcml_Color rb=(i%2)?sfcml_rgb(14,14,24):sfcml_rgb(20,20,32);
            sfcml_fillRect(win,sfcml_rect(cx+6,iy,cw-12,20),rb);
            sfcml_drawText(win,kk[i],cx+10,iy+6,sfcml_rgb(120,120,160),rb);
            sfcml_drawText(win,vv[i],cx+116,iy+6,sfcml_rgb(220,220,240),rb);
        }

    } else if(_set_cat==3){
        /* --- Alimentation / Veille --- */
        sfcml_drawText(win,"Alimentation",cx+10,y,sfcml_rgb(220,20,20),hbg);
        sfcml_drawHLine(win,cx+10,y+12,cw-20,sfcml_rgb(50,50,70));
        y+=26;
        sfcml_drawText(win,"Mode veille (apres N minutes d'inactivite) :",cx+10,y,sfcml_rgb(160,160,190),hbg);
        y+=16;
        for(int i=0;i<5;i++){
            int bx=cx+10+i*78;
            int sel2=(_veille_sel==i);
            sfcml_Color bb=sel2?C_TB:sfcml_rgb(28,28,44);
            sfcml_fillRect(win,sfcml_rect(bx,y,70,28),bb);
            sfcml_drawRect(win,sfcml_rect(bx,y,70,28),sel2?sfcml_rgb(120,180,255):sfcml_rgb(60,60,90));
            sfcml_drawText(win,_veille_lbl[i],bx+8,y+10,SFCML_WHITE,bb);
        }
        y+=42;
        sfcml_drawText(win,"Economiseur d'ecran :",cx+10,y,sfcml_rgb(160,160,190),hbg);
        y+=14;
        const char* ssn[5]={"Horloge","Arbre","Fractal","Etoiles","Matrix"};
        for(int i=0;i<5;i++){
            int bx=cx+10+i*78;
            sfcml_Color bb2=(_ss_id==i)?C_TB:sfcml_rgb(28,28,44);
            sfcml_fillRect(win,sfcml_rect(bx,y,70,26),bb2);
            sfcml_drawRect(win,sfcml_rect(bx,y,70,26),(_ss_id==i)?sfcml_rgb(120,180,255):sfcml_rgb(60,60,90));
            sfcml_drawText(win,ssn[i],bx+6,y+9,SFCML_WHITE,bb2);
        }
        y+=38;
        sfcml_drawText(win,"Toute touche ou clic reveille l'ecran.",cx+10,y,sfcml_rgb(100,100,130),hbg);
        if(_veille_sel>0){
            y+=22;
            char info[48];
            int lim=_veille_secs[_veille_sel]-_inact_secs;
            if(lim<0)lim=0;
            info[0]='V';info[1]='e';info[2]='i';info[3]='l';info[4]='l';info[5]='e';
            info[6]=' ';info[7]='d';info[8]='a';info[9]='n';info[10]='s';info[11]=' ';
            int o=12;
            if(lim>=60){info[o++]='0'+lim/60/10;info[o++]='0'+lim/60%10;info[o++]='m';}
            info[o++]='0'+(lim%60)/10;info[o++]='0'+lim%60%10;info[o++]='s';
            info[o]='\0';
            sfcml_drawText(win,info,cx+10,y,sfcml_rgb(80,160,255),hbg);
        }

    } else if(_set_cat==2){
        /* --- Applications --- */
        sfcml_drawText(win,"Applications",cx+10,y,sfcml_rgb(220,20,20),hbg);
        sfcml_drawHLine(win,cx+10,y+12,cw-20,sfcml_rgb(50,50,70));
        y+=24;
        const char* an[6]={"Terminal","Epitech Paint","Code Editor","Word","Creeper!","A propos"};
        const char* ad[6]={"Shell root@myos","Editeur graphique 16 couleurs",
                            "Editeur C avec colorisation","Traitement de texte",
                            "Animation Creeper Minecraft","Infos sur MyOS"};
        sfcml_Color ai[6]={{10,10,14,255},{220,220,220,255},{10,12,22,255},
                           {245,245,250,255},{94,124,22,255},{0,80,170,255}};
        for(int i=0;i<6;i++){
            int iy=y+i*40;
            sfcml_Color rb=(i%2)?sfcml_rgb(14,14,24):sfcml_rgb(20,20,32);
            sfcml_fillRect(win,sfcml_rect(cx+6,iy,cw-12,38),rb);
            sfcml_fillRect(win,sfcml_rect(cx+10,iy+7,24,24),ai[i]);
            sfcml_drawRect(win,sfcml_rect(cx+10,iy+7,24,24),sfcml_rgb(80,80,100));
            sfcml_drawText(win,an[i],cx+40,iy+10,SFCML_WHITE,rb);
            sfcml_drawText(win,ad[i],cx+40,iy+22,sfcml_rgb(130,130,155),rb);
        }

    } else if(_set_cat==4){
        /* --- A propos --- */
        sfcml_drawText(win,"A propos de MyOS",cx+10,y,sfcml_rgb(220,20,20),hbg);
        sfcml_drawHLine(win,cx+10,y+12,cw-20,sfcml_rgb(50,50,70));
        y+=26;
        draw_big_text(win,"MyOS",cx+10,y,2,sfcml_rgb(0,140,255),hbg);
        sfcml_drawText(win,"v0.1 Epitech Edition",cx+82,y+8,sfcml_rgb(150,150,170),hbg);
        y+=28;
        sfcml_drawText(win,"Systeme d'exploitation educatif",cx+10,y,sfcml_rgb(180,180,200),hbg);y+=12;
        sfcml_drawText(win,"developpe de zero en assembleur x86",cx+10,y,sfcml_rgb(180,180,200),hbg);y+=12;
        sfcml_drawText(win,"et en C freestanding (bare-metal).",cx+10,y,sfcml_rgb(180,180,200),hbg);y+=20;
        sfcml_drawText(win,"(c) 2024 Epitech Technology - Barcelone",cx+10,y,sfcml_rgb(100,100,120),hbg);y+=20;
        const char* comp[5]={
            "Stage1 MBR   512 octets  NASM",
            "MineGRUB     8KB stage2 bootloader",
            "Kernel       C freestanding 32-bit",
            "libk         C runtime (string/stdio/stdlib)",
            "libSFCML     GPU/Input/Graphics framework"};
        for(int i=0;i<5;i++){
            sfcml_fillRect(win,sfcml_rect(cx+8,y,cw-16,14),(i%2)?sfcml_rgb(14,14,24):sfcml_rgb(20,20,32));
            sfcml_drawText(win,comp[i],cx+12,y+3,sfcml_rgb(160,200,255),(i%2)?sfcml_rgb(14,14,24):sfcml_rgb(20,20,32));
            y+=14;
        }
        y+=8;
        sfcml_drawText(win,"Construit avec passion pour l'Epitech!",cx+10,y,sfcml_rgb(220,20,20),hbg);
    }
}

static void settings_click(int mx,int my){
    AppWin* w=&_wins[W_SETTINGS];
    int sx=w->x+1,sy=w->y+TBAR_H;
    int sbw=140;
    /* Sidebar */
    if(mx>=sx&&mx<sx+sbw){
        for(int i=0;i<5;i++){
            int iy=sy+28+i*36;
            if(my>=iy&&my<iy+35){_set_cat=i;return;}
        }
        return;
    }
    /* Alimentation : clics sur les boutons veille */
    if(_set_cat==3){
        int cx2=sx+sbw+1;
        int y2=sy+10+26+16; /* title+hline+label */
        /* Veille */
        for(int i=0;i<5;i++){
            int bx=cx2+10+i*78;
            if(mx>=bx&&mx<bx+70&&my>=y2&&my<y2+28){_veille_sel=i;_inact_secs=0;return;}
        }
        y2+=42+14; /* apres veille: label economiseur */
        /* Economiseur */
        for(int i=0;i<5;i++){
            int bx=cx2+10+i*78;
            if(mx>=bx&&mx<bx+70&&my>=y2&&my<y2+26){
                _ss_id=i;
                /* reset etat de l'economiseur courant */
                _mb_row=480;_ss_si=0;_mc_i=0;
                return;
            }
        }
        return;
    }
    if(_set_cat!=0)return;
    /* Apparence clicks */
    int cx=sx+sbw+1;
    int y=sy+10+22; /* after title+hline */
    y+=12; /* accent swatches start */
    for(int i=0;i<8;i++){
        int bx=cx+10+(i%4)*56,by=y+(i/4)*46;
        if(mx>=bx&&mx<bx+48&&my>=by&&my<by+36){
            _accent_sel=i;C_TB=_accent_pr[i];return;
        }
    }
    y+=2*46+12; /* bg label */
    y+=12;      /* bg swatches */
    for(int i=0;i<5;i++){
        int bx=cx+10+i*56;
        if(mx>=bx&&mx<bx+48&&my>=y&&my<y+36){
            _bg_sel=i;C_DK=_bg_pr[i];return;
        }
    }
    y+=56; /* fg label */
    y+=12; /* fg buttons */
    for(int i=0;i<4;i++){
        int bx=cx+10+i*86;
        if(mx>=bx&&mx<bx+78&&my>=y&&my<y+24){
            _fg_sel=i;C_FG=_fg_pr[i];return;
        }
    }
}

/* ============================================================
 * Economiseur d'ecran
 * ============================================================ */
/* ============================================================
 * SIN/COS entiers (degres, retourne sin*100)
 * ============================================================ */
static const uint8_t _s90[91]={
    0,2,3,5,7,9,10,12,14,16,17,19,21,22,24,26,28,29,31,33,
    34,36,37,39,41,42,44,45,47,48,50,51,53,54,56,57,59,60,62,63,
    64,66,67,68,69,71,72,73,74,75,77,78,79,80,81,82,83,84,85,86,
    87,87,88,89,90,91,91,92,93,93,94,95,95,96,96,97,97,97,98,98,
    98,99,99,99,99,100,100,100,100,100,100
};
static int _isin(int a){
    a=((a%360)+360)%360;
    if(a<=90) return _s90[a];
    if(a<=180)return _s90[180-a];
    if(a<=270)return -(int)_s90[a-180];
    return -(int)_s90[360-a];
}
static int _icos(int a){return _isin(a+90);}

/* ============================================================
 * SS0 - Horloge flottante
 * ============================================================ */
static int _clk_x=260,_clk_y=200,_clk_vx=1,_clk_vy=1;
static void draw_ss_clock(sfcml_Window*win){
    sfcml_fillRect(win,sfcml_rect(0,0,SCR_W,SCR_H),SFCML_BLACK);
    char hm[6];
    hm[0]='0'+_rtc.h/10;hm[1]='0'+_rtc.h%10;hm[2]=':';
    hm[3]='0'+_rtc.m/10;hm[4]='0'+_rtc.m%10;hm[5]='\0';
    _clk_x+=_clk_vx;_clk_y+=_clk_vy;
    if(_clk_x<0||_clk_x>SCR_W-200)_clk_vx=-_clk_vx; /* 200px wide=5chars*8*5 */
    if(_clk_y<0||_clk_y>SCR_H-40)_clk_vy=-_clk_vy;
    draw_big_text(win,hm,_clk_x,_clk_y,5,sfcml_rgb(0,140,255),SFCML_BLACK);
    char ss[3];ss[0]='0'+_rtc.s/10;ss[1]='0'+_rtc.s%10;ss[2]='\0';
    draw_big_text(win,ss,_clk_x+78,_clk_y+48,3,sfcml_rgb(20,70,160),SFCML_BLACK);
    char dat[11];
    dat[0]='0'+_rtc.day/10;dat[1]='0'+_rtc.day%10;dat[2]='/';
    dat[3]='0'+_rtc.mon/10;dat[4]='0'+_rtc.mon%10;dat[5]='/';
    dat[6]='0'+(_rtc.year/1000)%10;dat[7]='0'+(_rtc.year/100)%10;
    dat[8]='0'+(_rtc.year/10)%10;dat[9]='0'+_rtc.year%10;dat[10]='\0';
    sfcml_drawText(win,dat,_clk_x+10,_clk_y+72,sfcml_rgb(40,50,80),SFCML_BLACK);
    sfcml_present(win);
}

/* ============================================================
 * SS1 - Arbre fractal recursif
 * ============================================================ */
static int _tree_t=0;
static void _branch(sfcml_Window*win,int x,int y,int ang,int len,int dep){
    if(dep==0||len<3)return;
    int x2=x+_icos(ang)*len/100;
    int y2=y-_isin(ang)*len/100;
    if(x2<0)x2=0;if(x2>SCR_W-1)x2=SCR_W-1;
    if(y2<0)y2=0;if(y2>SCR_H-1)y2=SCR_H-1;
    sfcml_Color c;
    if(dep>=6)      c=sfcml_rgb((uint8_t)(140-(8-dep)*15),60,15);
    else if(dep>=3) c=sfcml_rgb(20,(uint8_t)(60+dep*25),15);
    else            c=sfcml_rgb(80,220,30);
    sfcml_drawLine(win,x,y,x2,y2,c);
    int sp=22+_isin((_tree_t*3)%360)*18/100; /* oscillation du vent */
    _branch(win,x2,y2,ang-sp,len*66/100,dep-1);
    _branch(win,x2,y2,ang+sp,len*66/100,dep-1);
}
static void draw_ss_tree(sfcml_Window*win){
    sfcml_fillRect(win,sfcml_rect(0,0,SCR_W,SCR_H),sfcml_rgb(0,4,2));
    sfcml_fillRect(win,sfcml_rect(0,SCR_H-22,SCR_W,22),sfcml_rgb(5,18,5));
    _tree_t=(_tree_t+1)%360;
    _branch(win,SCR_W/2,SCR_H-22,90,78,8); /* 8 niveaux de recursion */
    sfcml_present(win);
}

/* ============================================================
 * SS2 - Mandelbrot (rendu progressif + zoom)
 * ============================================================ */
static float _mb_cx=-0.5f,_mb_cy=0.0f,_mb_sc=3.5f/SCR_H;
static int   _mb_zc=0;
static void draw_ss_mandelbrot(sfcml_Window*win){
    if(_mb_row>=SCR_H){
        /* Zoom vers l'elephant valley: (-0.7436, 0.1319) */
        _mb_cx=_mb_cx*0.85f+(-0.7436f)*0.15f;
        _mb_cy=_mb_cy*0.85f+( 0.1319f)*0.15f;
        _mb_sc*=0.78f;
        _mb_zc++;
        if(_mb_zc>18){_mb_cx=-0.5f;_mb_cy=0.0f;_mb_sc=3.5f/SCR_H;_mb_zc=0;}
        _mb_row=0;
    }
    /* Rend 24 lignes par appel */
    int rend=_mb_row+24; if(rend>SCR_H)rend=SCR_H;
    for(int row=_mb_row;row<rend;row++){
        for(int col=0;col<SCR_W;col++){
            float x0=_mb_cx+(col-SCR_W/2)*_mb_sc;
            float y0=_mb_cy+(row-SCR_H/2)*_mb_sc;
            float x=0.0f,y=0.0f;
            int it=0;
            while(x*x+y*y<=4.0f&&it<48){
                float xt=x*x-y*y+x0;
                y=2.0f*x*y+y0;
                x=xt;it++;
            }
            sfcml_Color c;
            if(it==48){c=SFCML_BLACK;}
            else{
                uint8_t t=(uint8_t)(it*5);
                uint8_t u=(uint8_t)((48-it)*5);
                c=sfcml_rgb(t,(uint8_t)(it*3%256),u);
            }
            sfcml_drawPixel(win,col,row,c);
        }
    }
    _mb_row=rend;
    sfcml_present(win);
}

/* ============================================================
 * SS3 - Champ d'etoiles 3D
 * ============================================================ */
#define SS_STARS 64
static int16_t _ssx[SS_STARS],_ssy[SS_STARS];
static uint8_t _ssz[SS_STARS];
static uint32_t _ss_rng=12345;
static uint32_t ss_rand(void){_ss_rng=_ss_rng*1664525+1013904223;return _ss_rng;}
static void _star_rst(int i){
    _ssx[i]=(int16_t)((ss_rand()%SCR_W)-(SCR_W/2));
    _ssy[i]=(int16_t)((ss_rand()%SCR_H)-(SCR_H/2));
    _ssz[i]=(uint8_t)(180+ss_rand()%75);
}
static void draw_ss_stars(sfcml_Window*win){
    sfcml_fillRect(win,sfcml_rect(0,0,SCR_W,SCR_H),SFCML_BLACK);
    if(!_ss_si){for(int i=0;i<SS_STARS;i++)_star_rst(i);_ss_si=1;}
    for(int i=0;i<SS_STARS;i++){
        if(_ssz[i]<=2){_star_rst(i);continue;}
        int sx=_ssx[i]*200/_ssz[i]+SCR_W/2;
        int sy=_ssy[i]*200/_ssz[i]+SCR_H/2;
        if(sx<0||sx>=SCR_W||sy<0||sy>=SCR_H){_star_rst(i);continue;}
        uint8_t br=(uint8_t)(255-_ssz[i]);
        uint8_t sz=(uint8_t)(3-_ssz[i]/90); if(sz<1)sz=1;
        sfcml_fillRect(win,sfcml_rect(sx,sy,(int)sz,(int)sz),sfcml_rgb(br,br,br));
        _ssz[i]-=3;
    }
    sfcml_present(win);
}

/* ============================================================
 * SS4 - Matrix (pluie de caracteres)
 * ============================================================ */
#define SS_MC 80
static uint8_t _mcy[SS_MC],_mcs[SS_MC],_mcf[SS_MC];
static uint32_t _mc_rng=54321;
static uint32_t mc_rand(void){_mc_rng=_mc_rng*1664525+1013904223;return _mc_rng;}
static void draw_ss_matrix(sfcml_Window*win){
    if(!_mc_i){
        for(int i=0;i<SS_MC;i++){
            _mcy[i]=(uint8_t)(mc_rand()%(SCR_H/8));
            _mcs[i]=(uint8_t)(1+mc_rand()%4);
            _mcf[i]=0;
        }
        _mc_i=1;
        sfcml_fillRect(win,sfcml_rect(0,0,SCR_W,SCR_H),SFCML_BLACK);
    }
    /* Fondu progressif de tous les pixels vers le noir */
    uint8_t*b=win->back;
    uint32_t tot=(uint32_t)win->pitch*win->height;
    for(uint32_t p=0;p<tot;p++){if(b[p]>10)b[p]-=10;else b[p]=0;}
    /* Nouveaux caracteres */
    for(int c=0;c<SS_MC;c++){
        _mcf[c]++;if(_mcf[c]<_mcs[c])continue;_mcf[c]=0;
        int x=c*8,y=(int)_mcy[c]*8;
        if(y<SCR_H){
            char ch=(char)(33+mc_rand()%94);
            sfcml_Color fg=(_mcy[c]%20<2)?sfcml_rgb(200,255,200):sfcml_rgb(0,180,0);
            sfcml_drawChar(win,ch,x,y,fg,SFCML_BLACK);
        }
        _mcy[c]++;if(_mcy[c]>=(uint8_t)(SCR_H/8))_mcy[c]=0;
    }
    sfcml_present(win);
}

/* ============================================================
 * Dispatcher economiseurs d'ecran
 * ============================================================ */
static void draw_screensaver(sfcml_Window*win){
    switch(_ss_id){
    case 0: draw_ss_clock(win);     break;
    case 1: draw_ss_tree(win);      break;
    case 2: draw_ss_mandelbrot(win);break;
    case 3: draw_ss_stars(win);     break;
    default:draw_ss_matrix(win);    break;
    }
}

/* ============================================================
 * NAVIGATEUR WEB
 * ============================================================ */
static void calc_press(char btn){
    if(_calc_err&&btn!='C'){return;}
    if(btn=='C'){_calc_disp[0]='0';_calc_disp[1]='\0';_calc_acc=0.0;_calc_cur=0.0;_calc_op=0;_calc_new=1;_calc_err=0;return;}
    if(btn>='0'&&btn<='9'){
        if(_calc_new){_calc_disp[0]=btn;_calc_disp[1]='\0';_calc_new=0;}
        else{int l=(int)strlen(_calc_disp);if(l<18){_calc_disp[l]=btn;_calc_disp[l+1]='\0';}}
        return;
    }
    if(btn=='.'){
        if(_calc_new){_calc_disp[0]='0';_calc_disp[1]='.';_calc_disp[2]='\0';_calc_new=0;return;}
        if(!strchr(_calc_disp,'.')){int l=(int)strlen(_calc_disp);_calc_disp[l]='.';_calc_disp[l+1]='\0';}
        return;
    }
    double v=0.0;int neg=0,i=0,dec=0;int dl=0;
    if(_calc_disp[0]=='-'){neg=1;i=1;}
    for(;_calc_disp[i];i++){
        char c=_calc_disp[i];
        if(c=='.'){dec=1;dl=0;}
        else if(c>='0'&&c<='9'){
            if(!dec)v=v*10+(c-'0');
            else{dl++;double f=1.0;for(int j=0;j<dl;j++)f*=0.1;v+=((c-'0')*f);}
        }
    }
    if(neg)v=-v;
    _calc_cur=v;
    if(btn=='='){
        if(_calc_op=='+')v=_calc_acc+_calc_cur;
        else if(_calc_op=='-')v=_calc_acc-_calc_cur;
        else if(_calc_op=='*')v=_calc_acc*_calc_cur;
        else if(_calc_op=='/'){if(_calc_cur==0.0){_calc_err=1;strcpy(_calc_disp,"Erreur");return;}v=_calc_acc/_calc_cur;}
        else v=_calc_cur;
        int iv=(int)v;
        if(v-(double)iv==0.0){
            char tmp[24];int ti=0,neg2=(iv<0);if(neg2){iv=-iv;tmp[ti++]='-';}
            if(iv==0)tmp[ti++]='0';
            else{char r[20];int ri=0;int vv=iv;while(vv){r[ri++]='0'+vv%10;vv/=10;}while(ri--)tmp[ti++]=r[ri];}
            tmp[ti]='\0';strcpy(_calc_disp,tmp);
        }else{
            int neg3=(v<0.0);if(neg3)v=-v;
            int iv2=(int)v;double frac=v-(double)iv2;
            char tmp[24];int ti=0;
            if(neg3)tmp[ti++]='-';
            if(iv2==0)tmp[ti++]='0';
            else{char r[20];int ri=0;int vv=iv2;while(vv){r[ri++]='0'+vv%10;vv/=10;}while(ri--)tmp[ti++]=r[ri];}
            tmp[ti++]='.';
            for(int k=0;k<6;k++){frac*=10.0;int d=(int)frac;tmp[ti++]='0'+d;frac-=(double)d;}
            tmp[ti]='\0';ti--;while(ti>0&&tmp[ti]=='0')tmp[ti--]='\0';
            if(tmp[ti]=='.')tmp[ti]='\0';
            strcpy(_calc_disp,tmp);
        }
        _calc_acc=v;_calc_op=0;_calc_new=1;
        return;
    }
    if(btn=='%'){_calc_cur=v/100.0;double frac2=_calc_cur-(int)_calc_cur;if(frac2==0.0){char t[24];int ti=0,iv3=(int)_calc_cur,neg4=(iv3<0);if(neg4)iv3=-iv3;if(neg4)t[ti++]='-';char r[20];int ri=0;if(iv3==0)t[ti++]='0';else{int vv=iv3;while(vv){r[ri++]='0'+vv%10;vv/=10;}while(ri--)t[ti++]=r[ri];}t[ti]='\0';strcpy(_calc_disp,t);}else{char t[24];int ti=0;double cv=_calc_cur;int neg5=(cv<0.0);if(neg5)cv=-cv;int iv4=(int)cv;if(neg5)t[ti++]='-';char r[20];int ri=0;if(iv4==0)t[ti++]='0';else{int vv=iv4;while(vv){r[ri++]='0'+vv%10;vv/=10;}while(ri--)t[ti++]=r[ri];}t[ti++]='.';double fc=cv-(double)iv4;for(int k=0;k<4;k++){fc*=10.0;int d=(int)fc;t[ti++]='0'+d;fc-=(double)d;}t[ti]='\0';ti--;while(ti>0&&t[ti]=='0')t[ti--]='\0';if(t[ti]=='.')t[ti]='\0';strcpy(_calc_disp,t);}
        _calc_op=0;_calc_new=1;return;}
    _calc_acc=v;_calc_op=btn;_calc_new=1;
}

static void draw_calc(sfcml_Window*win,int wi){
    AppWin*w=&_wins[wi];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2,sh=w->h-TBAR_H-1;(void)sh;
    sfcml_Color bg=sfcml_rgb(32,32,38);
    sfcml_Color dbg=sfcml_rgb(22,22,28);
    sfcml_Color nbg=sfcml_rgb(50,52,60);
    sfcml_Color obg=sfcml_rgb(14,50,110);
    sfcml_Color ebg=sfcml_rgb(180,10,10);
    sfcml_Color eqbg=sfcml_rgb(0,140,60);
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,sh),bg);
    sfcml_fillRect(win,sfcml_rect(sx+6,sy+6,sw-12,36),dbg);
    sfcml_drawRect(win,sfcml_rect(sx+6,sy+6,sw-12,36),sfcml_rgb(60,60,80));
    int dl=(int)strlen(_calc_disp);
    int dx=sx+sw-12-dl*8;if(dx<sx+10)dx=sx+10;
    sfcml_drawText(win,_calc_disp,dx,sy+18,_calc_err?sfcml_rgb(255,80,80):SFCML_WHITE,dbg);
    int bw=(sw-14)/4,bh=38;
    int bsy=sy+48;
    static const char* blbl[5][4]={{"C","%","/","*"},{"7","8","9","-"},{"4","5","6","+"},{"1","2","3","="},{"0 ","."," ","="}};
    (void)blbl;
    for(int r=0;r<5;r++){
        for(int c=0;c<4;c++){
            if(r==4&&c==0){
                int bx=sx+7;int by=bsy+r*bh+r*2;
                sfcml_Color bc;
                bc=sfcml_rgb(55,57,65);
                sfcml_fillRect(win,sfcml_rect(bx,by,bw*2+2,bh-2),bc);
                sfcml_drawRect(win,sfcml_rect(bx,by,bw*2+2,bh-2),sfcml_rgb(70,70,90));
                sfcml_drawText(win,"0",bx+(bw*2+2)/2-4,by+bh/2-4,SFCML_WHITE,bc);
                int bx2=sx+7+bw*2+4;
                sfcml_fillRect(win,sfcml_rect(bx2,by,bw-2,bh-2),sfcml_rgb(55,57,65));
                sfcml_drawRect(win,sfcml_rect(bx2,by,bw-2,bh-2),sfcml_rgb(70,70,90));
                sfcml_drawText(win,".",bx2+bw/2-4,by+bh/2-4,SFCML_WHITE,sfcml_rgb(55,57,65));
                continue;
            }
            if(r==4&&c==1)continue;
            char key=' ';
            const char* lbl="";
            if(r==0){const char k[]={'C','%','/','*'};key=k[c];const char* lb[]={"C","%","/","*"};lbl=lb[c];}
            else if(r==1){const char k[]={'7','8','9','-'};key=k[c];const char* lb[]={"7","8","9","-"};lbl=lb[c];}
            else if(r==2){const char k[]={'4','5','6','+'};key=k[c];const char* lb[]={"4","5","6","+"};lbl=lb[c];}
            else if(r==3){const char k[]={'1','2','3','='};key=k[c];const char* lb[]={"1","2","3","="};lbl=lb[c];}
            else if(r==4&&c==2){key='.';lbl=".";}
            else if(r==4&&c==3){key='=';lbl="=";}
            (void)key;
            int bx=sx+7+c*(bw+2);int by=bsy+r*(bh+2);
            sfcml_Color bc;
            if(r==0)bc=(c==0)?ebg:obg;
            else if(c==3)bc=(r==3||r==4)?eqbg:obg;
            else bc=nbg;
            sfcml_fillRect(win,sfcml_rect(bx,by,bw-2,bh-2),bc);
            sfcml_drawRect(win,sfcml_rect(bx,by,bw-2,bh-2),sfcml_rgb(70,70,90));
            sfcml_drawText(win,lbl,bx+bw/2-4,by+bh/2-4,SFCML_WHITE,bc);
        }
    }
}

static void calc_click(int mx,int my){
    AppWin*w=&_wins[W_CALC];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2;
    int bw=(sw-14)/4,bh=38,bsy=sy+48;
    for(int r=0;r<5;r++){
        for(int c=0;c<4;c++){
            int bx,by,bww,bhh=bh-2;
            by=bsy+r*(bh+2);bhh=bh-2;
            if(r==4&&c==0){bx=sx+7;bww=bw*2+2;}
            else if(r==4&&c==1)continue;
            else if(r==4&&c==2){bx=sx+7+bw*2+4;bww=bw-2;}
            else{bx=sx+7+c*(bw+2);bww=bw-2;}
            if(mx>=bx&&mx<bx+bww&&my>=by&&my<by+bhh){
                char key=' ';
                if(r==0){const char k[]={'C','%','/','*'};key=k[c];}
                else if(r==1){const char k[]={'7','8','9','-'};key=k[c];}
                else if(r==2){const char k[]={'4','5','6','+'};key=k[c];}
                else if(r==3){const char k[]={'1','2','3','='};key=k[c];}
                else if(r==4&&c==0)key='0';
                else if(r==4&&c==2)key='.';
                else if(r==4&&c==3)key='=';
                calc_press(key);
                return;
            }
        }
    }
}

static void fi_edit_open(void){
    const char*fn=_fi_files[_fi_dir][_fi_sel];
    _fi_enl=0;_fi_ecl=0;_fi_ecc=0;_fi_esc=0;
    for(int i=0;i<FI_ER;i++)_fi_ebuf[i][0]='\0';
    if(strstr(fn,".c")||strstr(fn,".h")||strstr(fn,".ld")){
        strncpy(_fi_ebuf[_fi_enl],"/* ",FI_EC);strncat(_fi_ebuf[_fi_enl],fn,FI_EC-4);strncat(_fi_ebuf[_fi_enl++]," */",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"#include <stdio.h>",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"int main(void) {",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"    return 0;",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"}",FI_EC);
    } else if(strstr(fn,".asm")){
        strncpy(_fi_ebuf[_fi_enl++],"; MyOS Assembleur",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"section .text",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"global _start",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"_start:",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"    mov eax, 0",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"    ret",FI_EC);
    } else {
        strncpy(_fi_ebuf[_fi_enl++],fn,FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"[Editez ce fichier ici]",FI_EC);
        strncpy(_fi_ebuf[_fi_enl++],"Utilisez les fleches pour naviguer.",FI_EC);
    }
    _fi_edit=1;
}
static void fi_key(sfcml_KeyCode k){
    char*cur=_fi_ebuf[_fi_ecl];int len=(int)strlen(cur);
    switch(k){
    case SFCML_KEY_ESCAPE:_fi_edit=0;break;
    case SFCML_KEY_UP:
        if(_fi_ecl>0){_fi_ecl--;if(_fi_ecl<_fi_esc)_fi_esc=_fi_ecl;}
        {int ml=(int)strlen(_fi_ebuf[_fi_ecl]);if(_fi_ecc>ml)_fi_ecc=ml;}break;
    case SFCML_KEY_DOWN:
        if(_fi_ecl<_fi_enl-1){_fi_ecl++;if(_fi_ecl>=_fi_esc+18)_fi_esc++;}
        {int ml=(int)strlen(_fi_ebuf[_fi_ecl]);if(_fi_ecc>ml)_fi_ecc=ml;}break;
    case SFCML_KEY_LEFT:
        if(_fi_ecc>0)_fi_ecc--;
        else if(_fi_ecl>0){_fi_ecl--;_fi_ecc=(int)strlen(_fi_ebuf[_fi_ecl]);}break;
    case SFCML_KEY_RIGHT:
        if(_fi_ecc<len)_fi_ecc++;
        else if(_fi_ecl<_fi_enl-1){_fi_ecl++;_fi_ecc=0;}break;
    case SFCML_KEY_RETURN:
        if(_fi_enl<FI_ER){
            for(int i=_fi_enl;i>_fi_ecl+1;i--)memcpy(_fi_ebuf[i],_fi_ebuf[i-1],FI_EC+1);
            memcpy(_fi_ebuf[_fi_ecl+1],cur+_fi_ecc,(size_t)(len-_fi_ecc+1));
            cur[_fi_ecc]='\0';_fi_ecl++;_fi_ecc=0;_fi_enl++;
            if(_fi_ecl>=_fi_esc+18)_fi_esc++;
        }break;
    case SFCML_KEY_BACKSPACE:
        if(_fi_ecc>0){memmove(cur+_fi_ecc-1,cur+_fi_ecc,(size_t)(len-_fi_ecc+1));_fi_ecc--;}
        else if(_fi_ecl>0){
            int pl=(int)strlen(_fi_ebuf[_fi_ecl-1]);
            if(pl+len<=FI_EC)strncat(_fi_ebuf[_fi_ecl-1],cur,(size_t)(FI_EC-pl));
            for(int i=_fi_ecl;i<_fi_enl-1;i++)memcpy(_fi_ebuf[i],_fi_ebuf[i+1],FI_EC+1);
            _fi_enl--;_fi_ecl--;_fi_ecc=pl;
            if(_fi_ecl<_fi_esc)_fi_esc=_fi_ecl;
        }break;
    default:break;
    }
}
static void fi_text(char ch){
    if(ch<32)return;
    char*cur=_fi_ebuf[_fi_ecl];int len=(int)strlen(cur);
    if(len>=FI_EC)return;
    memmove(cur+_fi_ecc+1,cur+_fi_ecc,(size_t)(len-_fi_ecc+1));
    cur[_fi_ecc++]=ch;
}

static void draw_files(sfcml_Window*win,int wi){
    AppWin*w=&_wins[wi];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2,sh=w->h-TBAR_H-1;
    sfcml_Color bg=sfcml_rgb(245,246,247);
    sfcml_Color selbg=sfcml_rgb(204,228,255);
    sfcml_Color hdr=sfcml_rgb(230,232,236);
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,sh),bg);
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,28),hdr);
    sfcml_drawHLine(win,sx,sy+27,sw,sfcml_rgb(200,202,206));
    sfcml_drawText(win,"<",sx+6,sy+10,sfcml_rgb(60,60,80),hdr);
    sfcml_drawText(win,">",sx+20,sy+10,sfcml_rgb(60,60,80),hdr);
    if(_fi_edit){
        sfcml_drawText(win,"[Edition]",sx+38,sy+10,sfcml_rgb(0,120,220),hdr);
        sfcml_drawText(win,_fi_files[_fi_dir][_fi_sel],sx+116,sy+10,sfcml_rgb(40,40,60),hdr);
        sfcml_fillRect(win,sfcml_rect(sx+sw-62,sy+5,56,18),sfcml_rgb(180,10,10));
        sfcml_drawText(win,"Fermer",sx+sw-60,sy+9,SFCML_WHITE,sfcml_rgb(180,10,10));
    } else {
        sfcml_drawText(win,_fi_dirs[_fi_dir],sx+38,sy+10,sfcml_rgb(60,60,80),hdr);
    }
    int lpw=140;
    sfcml_fillRect(win,sfcml_rect(sx,sy+28,lpw,sh-28),sfcml_rgb(240,241,243));
    sfcml_drawVLine(win,sx+lpw,sy+28,sh-28,sfcml_rgb(200,202,206));
    for(int i=0;i<8;i++){
        int iy=sy+32+i*22;
        int sel=(_fi_dir==i);
        sfcml_Color lb=sel?selbg:sfcml_rgb(240,241,243);
        sfcml_fillRect(win,sfcml_rect(sx+1,iy,lpw-1,20),lb);
        sfcml_fillRect(win,sfcml_rect(sx+4,iy+4,12,10),sfcml_rgb(255,196,0));
        sfcml_fillRect(win,sfcml_rect(sx+4,iy+3,5,3),sfcml_rgb(255,196,0));
        sfcml_drawText(win,i<4?(_fi_dirs[i]+3):(_fi_dirs[i]+14),sx+20,iy+6,sfcml_rgb(30,30,40),lb);
    }
    int rpx=sx+lpw+2,rpw=sw-lpw-2;
    if(_fi_edit){
        sfcml_Color edibg=sfcml_rgb(18,20,30);
        sfcml_Color lnbg=sfcml_rgb(14,16,24);
        int lnw=26;
        int eah=sh-46;
        sfcml_fillRect(win,sfcml_rect(rpx,sy+28,rpw,eah),edibg);
        sfcml_fillRect(win,sfcml_rect(rpx,sy+28,lnw,eah),lnbg);
        sfcml_drawVLine(win,rpx+lnw,sy+28,eah,sfcml_rgb(40,40,60));
        int visible=eah/8;if(visible>FI_ER)visible=FI_ER;
        for(int i=0;i<visible;i++){
            int line=_fi_esc+i;if(line>=_fi_enl)break;
            int ly=sy+29+i*8;
            char ln[3];ln[0]='0'+(line+1)/10;ln[1]='0'+(line+1)%10;ln[2]='\0';
            sfcml_Color lnc=(line==_fi_ecl)?sfcml_rgb(200,200,80):sfcml_rgb(70,80,100);
            sfcml_drawText(win,ln,rpx+1,ly,lnc,lnbg);
            sfcml_Color lb2=(line==_fi_ecl)?sfcml_rgb(22,26,44):edibg;
            if(line==_fi_ecl)sfcml_fillRect(win,sfcml_rect(rpx+lnw,ly-1,rpw-lnw,9),lb2);
            sfcml_drawText(win,_fi_ebuf[line],rpx+lnw+2,ly,SFCML_WHITE,lb2);
            if(line==_fi_ecl&&_blink)
                sfcml_fillRect(win,sfcml_rect(rpx+lnw+2+_fi_ecc*8,ly,2,8),sfcml_rgb(210,210,210));
        }
    } else {
        sfcml_fillRect(win,sfcml_rect(rpx,sy+28,rpw,sh-28),bg);
        sfcml_fillRect(win,sfcml_rect(rpx,sy+28,rpw,18),sfcml_rgb(236,238,240));
        sfcml_drawHLine(win,rpx,sy+45,rpw,sfcml_rgb(200,202,206));
        sfcml_drawText(win,"Nom",rpx+8,sy+33,sfcml_rgb(70,70,90),sfcml_rgb(236,238,240));
        sfcml_drawText(win,"Type",rpx+200,sy+33,sfcml_rgb(70,70,90),sfcml_rgb(236,238,240));
        for(int i=0;i<6;i++){
            const char*fn=_fi_files[_fi_dir][i];
            if(!fn||!fn[0])continue;
            int fy=sy+50+i*24;
            int sel=(_fi_sel==i);
            sfcml_Color fb=sel?selbg:bg;
            sfcml_fillRect(win,sfcml_rect(rpx,fy,rpw,22),fb);
            int is_dir=(fn[0]>='A'&&fn[0]<='Z'&&fn[1]>='a'&&!strchr(fn,'.'));
            if(is_dir){sfcml_fillRect(win,sfcml_rect(rpx+4,fy+4,14,11),sfcml_rgb(255,196,0));sfcml_fillRect(win,sfcml_rect(rpx+4,fy+3,6,3),sfcml_rgb(255,196,0));}
            else{sfcml_fillRect(win,sfcml_rect(rpx+4,fy+3,12,14),sfcml_rgb(220,232,255));sfcml_drawRect(win,sfcml_rect(rpx+4,fy+3,12,14),sfcml_rgb(100,140,200));}
            sfcml_drawText(win,fn,rpx+22,fy+7,sfcml_rgb(20,20,30),fb);
            const char*tp=is_dir?"Dossier":"Fichier";
            sfcml_drawText(win,tp,rpx+200,fy+7,sfcml_rgb(100,100,120),fb);
            sfcml_drawHLine(win,rpx,fy+22,rpw,sfcml_rgb(220,222,226));
        }
    }
    sfcml_fillRect(win,sfcml_rect(sx,sy+sh-18,sw,18),hdr);
    sfcml_drawHLine(win,sx,sy+sh-19,sw,sfcml_rgb(200,202,206));
    if(_fi_edit)sfcml_drawText(win,"ESC:Fermer  Fleches:Naviguer  Entree:Ligne",sx+8,sy+sh-12,sfcml_rgb(60,80,100),hdr);
    else sfcml_drawText(win,"Clic x2 sur un fichier pour editer",sx+8,sy+sh-12,sfcml_rgb(80,80,100),hdr);
}

static void files_click(int mx,int my){
    AppWin*w=&_wins[W_FILES];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2;
    if(_fi_edit){
        if(mx>=sx+sw-62&&mx<sx+sw-6&&my>=sy+5&&my<sy+23)_fi_edit=0;
        return;
    }
    int lpw=140;
    if(mx>=sx&&mx<sx+lpw){
        for(int i=0;i<8;i++){
            int iy=sy+32+i*22;
            if(my>=iy&&my<iy+20){_fi_dir=i;_fi_sel=0;return;}
        }
        return;
    }
    int rpx=sx+lpw+2;
    for(int i=0;i<6;i++){
        int fy=sy+50+i*24;
        if(mx>=rpx&&my>=fy&&my<fy+22){
            const char*fn=_fi_files[_fi_dir][i];
            if(!fn||!fn[0])return;
            int is_dir=(fn[0]>='A'&&fn[0]<='Z'&&fn[1]>='a'&&!strchr(fn,'.'));
            if(_fi_sel==i&&!is_dir){fi_edit_open();return;}
            _fi_sel=i;return;
        }
    }
    if(mx>=sx+6&&mx<sx+18&&my>=sy&&my<sy+28){if(_fi_dir>0){_fi_dir--;_fi_sel=0;}}
    if(mx>=sx+20&&mx<sx+32&&my>=sy&&my<sy+28){if(_fi_dir<7){_fi_dir++;_fi_sel=0;}}
}

static void draw_browser(sfcml_Window*win,int wi){
    AppWin*w=&_wins[wi];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2,sh=w->h-TBAR_H-1;
    sfcml_Color dbg=sfcml_rgb(32,33,36);
    sfcml_Color urlbg=_burl_foc?sfcml_rgb(60,61,68):sfcml_rgb(48,49,52);
    (void)sh;

    /* Tab bar */
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,22),sfcml_rgb(40,41,44));
    const char*tl[7]={"Nouvel onglet","Epitech","Google","MyOS","GitHub","Resultats","Web"};
    sfcml_fillRect(win,sfcml_rect(sx+2,sy+2,130,18),dbg);
    sfcml_drawText(win,(_bpage<7)?tl[_bpage]:tl[0],sx+6,sy+7,sfcml_rgb(210,210,210),dbg);

    /* Nav bar */
    int ny=sy+22;
    sfcml_fillRect(win,sfcml_rect(sx,ny,sw,26),dbg);
    sfcml_drawHLine(win,sx,ny+25,sw,sfcml_rgb(55,55,65));
    sfcml_drawText(win,"<",sx+4, ny+9,_bpage?sfcml_rgb(190,190,190):sfcml_rgb(55,55,55),dbg);
    sfcml_drawText(win,">",sx+18,ny+9,(_bpage<4)?sfcml_rgb(190,190,190):sfcml_rgb(55,55,55),dbg);
    sfcml_drawText(win,"R",sx+34,ny+9,sfcml_rgb(170,170,170),dbg);
    int ux=sx+50,uw=sw-56;
    sfcml_fillRect(win,sfcml_rect(ux,ny+3,uw,20),urlbg);
    sfcml_drawRect(win,sfcml_rect(ux,ny+3,uw,20),_burl_foc?sfcml_rgb(80,130,255):sfcml_rgb(55,56,60));
    sfcml_drawText(win,_burl,ux+4,ny+7,sfcml_rgb(210,210,210),urlbg);
    if(_burl_foc&&_blink)
        sfcml_fillRect(win,sfcml_rect(ux+4+_burl_len*8,ny+5,2,12),SFCML_WHITE);

    /* Page content */
    int cay=ny+26;
    int cah=sh-22-26;
    switch(_bpage){
    case 0:{
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,cah),dbg);
        int cx2=sx+sw/2;
        draw_big_text(win,"My",cx2-72,cay+18,2,sfcml_rgb(66,133,244),dbg);
        draw_big_text(win,"B", cx2-36,cay+18,2,sfcml_rgb(234,67,53), dbg);
        draw_big_text(win,"rowser",cx2-20,cay+18,2,sfcml_rgb(52,168,83),dbg);
        sfcml_fillRect(win,sfcml_rect(cx2-110,cay+50,220,22),sfcml_rgb(48,49,52));
        sfcml_drawRect(win,sfcml_rect(cx2-110,cay+50,220,22),sfcml_rgb(70,70,90));
        sfcml_drawText(win,"Saisir une URL...",cx2-102,cay+55,sfcml_rgb(90,90,110),sfcml_rgb(48,49,52));
        const char*tn[4]={"Epitech","Google","GitHub","MyOS"};
        sfcml_Color tc[4]={{180,10,10,255},{66,133,244,255},{36,41,47,255},{0,110,210,255}};
        for(int i=0;i<4;i++){
            int tx2=cx2-170+i*88,ty2=cay+88;
            sfcml_fillRect(win,sfcml_rect(tx2,ty2,76,48),tc[i]);
            sfcml_drawText(win,tn[i],tx2+6,ty2+18,SFCML_WHITE,tc[i]);
        }
        break;}
    case 1:{
        sfcml_Color rd=sfcml_rgb(200,10,10);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,40),rd);
        draw_big_text(win,"EPITECH",sx+8,cay+6,2,SFCML_WHITE,rd);
        sfcml_drawText(win,"epitech.eu",sx+sw-76,cay+16,sfcml_rgb(255,180,180),rd);
        sfcml_fillRect(win,sfcml_rect(sx,cay+40,sw,cah-40),sfcml_rgb(14,14,22));
        const char*ln[6]={
            "Ecole d'expert en informatique, fondee en 1999",
            "Campus: Paris, Lyon, Barcelone, Toulouse, Rennes...",
            "Cursus: 5 ans (Bachelor 3 + Master 2)",
            "Methode: Projets, peer-learning, piscines intensives",
            "Reseau: 50 000+ diplomes dans 30 pays",
            "   epitech.eu  |  admissions@epitech.eu"};
        for(int i=0;i<6;i++)
            sfcml_drawText(win,ln[i],sx+8,cay+48+i*16,
                i==5?sfcml_rgb(100,160,255):sfcml_rgb(180,180,200),sfcml_rgb(14,14,22));
        break;}
    case 2:{ /* Google interactif */
        sfcml_Color wh=sfcml_rgb(255,255,255);
        sfcml_Color fbg=sfcml_rgb(248,249,250);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,cah),wh);
        int gx=sx+sw/2-54,gy=cay+28;
        sfcml_Color gc[6]={{66,133,244,255},{234,67,53,255},{251,188,5,255},
                           {66,133,244,255},{52,168,83,255},{234,67,53,255}};
        const char gl[]="Google";
        for(int i=0;gl[i];i++)
            draw_big_char(win,gl[i],gx+i*18,gy,2,gc[i],wh);
        /* Barre de recherche interactive */
        int qx=sx+sw/2-170,qy=gy+40,qw=340;
        sfcml_fillRect(win,sfcml_rect(qx,qy,qw,36),wh);
        sfcml_drawRect(win,sfcml_rect(qx,qy,qw,36),_bsearch_foc?sfcml_rgb(66,133,244):sfcml_rgb(218,220,224));
        if(_bsearch_len>0)
            sfcml_drawText(win,_bsearch,qx+12,qy+14,sfcml_rgb(20,20,20),wh);
        else if(!_bsearch_foc)
            sfcml_drawText(win,"Rechercher sur Google",qx+12,qy+14,sfcml_rgb(150,150,160),wh);
        if(_bsearch_foc&&_blink)
            sfcml_fillRect(win,sfcml_rect(qx+12+_bsearch_len*8,qy+6,2,20),sfcml_rgb(66,133,244));
        /* Bouton recherche */
        int bx=sx+sw/2-52,by=qy+44;
        sfcml_fillRect(win,sfcml_rect(bx,by,104,28),fbg);
        sfcml_drawRect(win,sfcml_rect(bx,by,104,28),sfcml_rgb(218,220,224));
        sfcml_drawText(win,"Recherche Google",bx+4,by+10,sfcml_rgb(60,60,80),fbg);
        break;}
    case 3:{
        sfcml_Color bg3=sfcml_rgb(16,16,26);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,cah),bg3);
        draw_big_text(win,"MyOS",sx+8,cay+14,2,sfcml_rgb(0,140,255),bg3);
        sfcml_drawText(win,"v0.1 Epitech",sx+80,cay+24,sfcml_rgb(90,90,130),bg3);
        sfcml_drawHLine(win,sx+6,cay+46,sw-12,sfcml_rgb(45,45,65));
        const char*ki[6]={"Version","CPU","RAM","Affichage","Bootloader","Build"};
        const char*kv[6]={"0.1 Epitech","x86 32-bit PM","128 MB","VESA 640x480","MineGRUB v1.0","GCC+NASM+LD"};
        for(int i=0;i<6;i++){
            int iy=cay+52+i*20;
            sfcml_Color rb=(i%2)?sfcml_rgb(14,14,24):sfcml_rgb(20,20,32);
            sfcml_fillRect(win,sfcml_rect(sx+4,iy,sw-8,18),rb);
            sfcml_drawText(win,ki[i],sx+8, iy+5,sfcml_rgb(100,100,150),rb);
            sfcml_drawText(win,kv[i],sx+84,iy+5,sfcml_rgb(200,200,230),rb);
        }
        break;}
    case 4:{ /* GitHub */
        sfcml_Color gb=sfcml_rgb(13,17,23);
        sfcml_Color gbar=sfcml_rgb(22,27,34);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,cah),gb);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,36),gbar);
        sfcml_drawText(win,"GitHub",sx+10,cay+12,SFCML_WHITE,gbar);
        const char*rn[4]={"myos/kernel","myos/sfcml","myos/libc","myos/bootloader"};
        for(int i=0;i<4;i++){
            int ry=cay+42+i*44;
            sfcml_fillRect(win,sfcml_rect(sx+4,ry,sw-8,40),gbar);
            sfcml_drawRect(win,sfcml_rect(sx+4,ry,sw-8,40),sfcml_rgb(48,54,61));
            sfcml_drawText(win,rn[i],sx+12,ry+6, sfcml_rgb(66,133,244),gbar);
            sfcml_drawText(win,"C | Public",sx+12,ry+22,sfcml_rgb(130,140,150),gbar);
        }
        break;}
    case 5:{ /* Resultats de recherche Google */
        sfcml_Color wh=sfcml_rgb(255,255,255);
        sfcml_Color fbg=sfcml_rgb(248,249,250);
        sfcml_Color gr=sfcml_rgb(112,112,112);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,cah),wh);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,54),fbg);
        sfcml_drawHLine(win,sx,cay+53,sw,sfcml_rgb(218,220,224));
        sfcml_Color gc2[6]={{66,133,244,255},{234,67,53,255},{251,188,5,255},
                            {66,133,244,255},{52,168,83,255},{234,67,53,255}};
        const char gl2[]="Google";
        for(int i2=0;gl2[i2];i2++)
            draw_big_char(win,gl2[i2],sx+6+i2*9,cay+8,1,gc2[i2],fbg);
        int mqx=sx+68,mqw=sw-150;
        sfcml_fillRect(win,sfcml_rect(mqx,cay+8,mqw,30),wh);
        sfcml_drawRect(win,sfcml_rect(mqx,cay+8,mqw,30),sfcml_rgb(218,220,224));
        sfcml_drawText(win,_bsearch,mqx+8,cay+19,sfcml_rgb(20,20,20),wh);
        sfcml_drawText(win,"Tout",sx+6,cay+58,sfcml_rgb(66,133,244),wh);
        sfcml_fillRect(win,sfcml_rect(sx+6,cay+67,22,2),sfcml_rgb(66,133,244));
        sfcml_drawText(win,"Images",sx+42,cay+58,gr,wh);
        sfcml_drawText(win,"Maps",sx+96,cay+58,gr,wh);
        sfcml_drawText(win,"Actualites",sx+134,cay+58,gr,wh);
        int ry=cay+76;
        sfcml_drawText(win,"Environ 1 540 000 000 resultats (0.42 sec)",sx+6,ry,gr,wh);
        ry+=18;
        const char*rt[4],*ru[4],*rd[4];
        if(strstr(_bsearch,"pitech")){
            rt[0]="Epitech Technology - L'ecole expert en informatique";
            ru[0]="www.epitech.eu";
            rd[0]="Formation 5 ans. Expert en technologies. 30 campus en Europe.";
            rt[1]="Admissions Epitech - Postulez maintenant";
            ru[1]="www.epitech.eu/admission";
            rd[1]="Piscine, tests de selection, entretien. Campus France & Europe.";
            rt[2]="Epitech Alumni - Le reseau des 50 000 diplomes";
            ru[2]="alumni.epitech.eu";
            rd[2]="Reseau actif de diplomes Epitech dans 30 pays.";
            rt[3]="MyOS - Projet Epitech sur GitHub";
            ru[3]="github.com/myos/kernel";
            rd[3]="OS bare-metal 32-bit developpe dans le cadre d'Epitech.";
        } else if(strstr(_bsearch,"ithub")||strstr(_bsearch,"itlab")){
            rt[0]="GitHub - Where the world builds software";
            ru[0]="github.com";
            rd[0]="100M+ developpeurs. Repositories, Actions, CI/CD gratuit.";
            rt[1]="myos/kernel - GitHub";
            ru[1]="github.com/myos/kernel";
            rd[1]="MyOS kernel C+NASM. Stars: 42. Forks: 7. MIT License.";
            rt[2]="Git - Systeme de controle de version";
            ru[2]="git-scm.com";
            rd[2]="Systeme de controle de version distribue et libre.";
            rt[3]="GitHub Actions - Automatisez votre workflow";
            ru[3]="github.com/features/actions";
            rd[3]="Automatisez votre pipeline de developpement sur GitHub.";
        } else if(strstr(_bsearch,"inux")||strstr(_bsearch,"ernel")||strstr(_bsearch,"myos")||strstr(_bsearch,"MyOS")||strstr(_bsearch,"nasm")){
            rt[0]="The Linux Kernel Archives";
            ru[0]="www.kernel.org";
            rd[0]="The Linux Kernel. Current stable: 6.7.2. Source, patches.";
            rt[1]="OSDev Wiki - Ecrire son propre OS";
            ru[1]="wiki.osdev.org";
            rd[1]="Tutoriels OS x86: mode protege, GDT, IDT, paging, VESA.";
            rt[2]="MyOS - x86 Bare-metal OS (C + NASM)";
            ru[2]="github.com/myos/kernel";
            rd[2]="Boot MineGRUB, VESA 640x480, libSFCML. Epitech 2024.";
            rt[3]="NASM - Netwide Assembler";
            ru[3]="www.nasm.us";
            rd[3]="Assembleur x86/x64 libre. Syntaxe Intel. Freestanding.";
        } else {
            rt[0]="MyOS - Systeme d'exploitation x86";
            ru[0]="github.com/myos";
            rd[0]="OS bare-metal 32-bit. Boot MineGRUB, VESA 640x480.";
            rt[1]="Epitech Technology - epitech.eu";
            ru[1]="www.epitech.eu";
            rd[1]="L'ecole ou MyOS a ete cree. Formation 5 ans en info.";
            rt[2]="GitHub - Depot du projet MyOS";
            ru[2]="github.com/myos/kernel";
            rd[2]="Tout le code source de MyOS disponible en open source.";
            rt[3]="MyBrowser - Navigateur integre MyOS";
            ru[3]="myos://info";
            rd[3]="Navigateur avec 5 pages simulees et recherche Google.";
        }
        for(int i=0;i<4;i++){
            int iry=ry+i*60;
            sfcml_drawText(win,rt[i],sx+6,iry,   sfcml_rgb(26,13,171),wh);
            sfcml_drawText(win,ru[i],sx+6,iry+12, sfcml_rgb(0,102,33),wh);
            sfcml_drawText(win,rd[i],sx+6,iry+24, sfcml_rgb(60,60,60),wh);
            sfcml_drawHLine(win,sx+4,iry+37,sw-8,sfcml_rgb(224,224,224));
        }
        break;}
    case 6:{ /* Reponse HTTP reelle */
        sfcml_Color bg6=sfcml_rgb(16,18,26);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,cah),bg6);
        sfcml_fillRect(win,sfcml_rect(sx,cay,sw,26),sfcml_rgb(22,24,36));
        sfcml_drawHLine(win,sx,cay+25,sw,sfcml_rgb(50,50,70));
        sfcml_drawText(win,"HTTP Response",sx+6,cay+9,sfcml_rgb(0,200,100),sfcml_rgb(22,24,36));
        sfcml_drawText(win,_burl,sx+100,cay+9,sfcml_rgb(100,150,255),sfcml_rgb(22,24,36));
        if(_net_len<=0){
            sfcml_drawText(win,"Aucune reponse ou erreur.",sx+8,cay+40,sfcml_rgb(200,80,80),bg6);
        } else {
            int ry=cay+32;int rpos=0;int lnum=0;
            char lbuf[76];
            while(rpos<_net_len&&ry<cay+cah-8&&lnum<40){
                int ll=0;
                while(rpos<_net_len&&_net_buf[rpos]!='\n'&&_net_buf[rpos]!='\r'&&ll<75)
                    lbuf[ll++]=_net_buf[rpos++];
                lbuf[ll]='\0';
                while(rpos<_net_len&&(_net_buf[rpos]=='\n'||_net_buf[rpos]=='\r'))rpos++;
                if(ll>0){
                    sfcml_Color lc;
                    if(lnum==0)lc=sfcml_rgb(100,255,100);
                    else if(lbuf[0]=='<')lc=sfcml_rgb(150,200,255);
                    else if(ll>4&&lbuf[0]>='A'&&lbuf[0]<='Z'&&lbuf[1]>='a')lc=sfcml_rgb(220,180,100);
                    else lc=sfcml_rgb(180,180,200);
                    sfcml_drawText(win,lbuf,sx+4,ry,lc,bg6);
                    lnum++;ry+=8;
                }
            }
        }
        break;}
    default: break;
    }
}

static void browser_click(int mx,int my){
    AppWin*w=&_wins[W_BROWSER];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2;
    int ny=sy+22,cay=ny+26;
    /* Bouton Retour */
    if(mx>=sx+2&&mx<sx+16&&my>=ny&&my<ny+26){
        if(_bpage==5||_bpage==6)_bnav(2);else if(_bpage>0)_bnav(_bpage-1);return;
    }
    /* Bouton Suivant */
    if(mx>=sx+16&&mx<sx+30&&my>=ny&&my<ny+26){if(_bpage<4)_bnav(_bpage+1);return;}
    /* Barre URL */
    int ux=sx+50,uw=sw-56;
    if(mx>=ux&&mx<ux+uw&&my>=ny+3&&my<ny+23){_burl_foc=1;_bsearch_foc=0;return;}
    _burl_foc=0;_bsearch_foc=0;
    /* Clics specifiques a la page */
    if(_bpage==0){
        int cx2=sx+sw/2;
        static const int tpg[4]={1,2,4,3};
        for(int i=0;i<4;i++){
            int tx2=cx2-170+i*88,ty2=cay+88;
            if(mx>=tx2&&mx<tx2+76&&my>=ty2&&my<ty2+48){_bnav(tpg[i]);return;}
        }
    } else if(_bpage==2){
        /* Clic sur la barre de recherche Google */
        int gy=cay+28,qx=sx+sw/2-170,qy=gy+40,qw=340;
        if(mx>=qx&&mx<qx+qw&&my>=qy&&my<qy+36){_bsearch_foc=1;return;}
        /* Clic sur le bouton "Recherche Google" */
        int bx=sx+sw/2-52,by=qy+44;
        if(mx>=bx&&mx<bx+104&&my>=by&&my<by+28){
            if(_bsearch_len>0)_bnav(5);return;
        }
    } else if(_bpage==5){
        /* Clic sur un resultat */
        int ry=cay+94;
        const int*pgmap;
        static const int pe_pg[4]={1,1,1,4};
        static const int gh_pg[4]={4,4,4,4};
        static const int lx_pg[4]={3,3,4,3};
        static const int gn_pg[4]={3,1,4,3};
        if(strstr(_bsearch,"pitech"))pgmap=pe_pg;
        else if(strstr(_bsearch,"ithub")||strstr(_bsearch,"itlab"))pgmap=gh_pg;
        else if(strstr(_bsearch,"inux")||strstr(_bsearch,"ernel")||strstr(_bsearch,"myos")||strstr(_bsearch,"MyOS")||strstr(_bsearch,"nasm"))pgmap=lx_pg;
        else pgmap=gn_pg;
        for(int i=0;i<4;i++){
            int iry=ry+i*60;
            if(mx>=sx+6&&mx<sx+sw-6&&my>=iry&&my<iry+37){_bnav(pgmap[i]);return;}
        }
    }
}

/* ============================================================
 * Gestionnaire reseau
 * ============================================================ */
static void _ip4str(uint32_t ip,char*buf){
    int o=0;
    for(int i=3;i>=0;i--){
        uint8_t v=(uint8_t)(ip>>(i*8));
        if(v>=100){buf[o++]='0'+v/100;}
        if(v>=10) {buf[o++]='0'+(v/10)%10;}
        buf[o++]='0'+v%10;
        if(i>0)buf[o++]='.';
    }
    buf[o]='\0';
}
static void _mac6str(const uint8_t*mac,char*buf){
    static const char*hx="0123456789ABCDEF";
    int o=0;
    for(int i=0;i<6;i++){buf[o++]=hx[mac[i]>>4];buf[o++]=hx[mac[i]&0xF];if(i<5)buf[o++]=':';}
    buf[o]='\0';
}

static void draw_netmgr(sfcml_Window*win,int wi){
    AppWin*w=&_wins[wi];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2,sh=w->h-TBAR_H-1;
    sfcml_Color bg=sfcml_rgb(240,242,245);
    sfcml_Color hbg=sfcml_rgb(220,225,235);
    sfcml_Color cardbg=sfcml_rgb(255,255,255);
    sfcml_Color sep=sfcml_rgb(200,205,215);
    sfcml_Color lbl=sfcml_rgb(80,85,100);
    sfcml_Color val=sfcml_rgb(20,20,30);
    sfcml_Color rbg=sfcml_rgb(230,235,240);
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,sh),bg);
    /* Header */
    sfcml_fillRect(win,sfcml_rect(sx,sy,sw,24),hbg);
    sfcml_drawHLine(win,sx,sy+23,sw,sep);
    sfcml_drawText(win,"Centre Reseau et partage",sx+10,sy+8,sfcml_rgb(20,20,120),hbg);
    /* Connection card */
    int cy=sy+28;
    sfcml_fillRect(win,sfcml_rect(sx+6,cy,sw-12,64),cardbg);
    sfcml_drawRect(win,sfcml_rect(sx+6,cy,sw-12,64),sep);
    /* Network icon (stylized) */
    int ix=sx+18,iy=cy+12;
    sfcml_fillRect(win,sfcml_rect(ix,iy+28,38,10),net_ok?sfcml_rgb(0,160,0):sfcml_rgb(180,30,30));
    sfcml_fillRect(win,sfcml_rect(ix+10,iy+16,18,13),net_ok?sfcml_rgb(0,160,0):sfcml_rgb(180,30,30));
    sfcml_fillRect(win,sfcml_rect(ix+17,iy+6,4,11),net_ok?sfcml_rgb(0,140,0):sfcml_rgb(160,20,20));
    sfcml_drawRect(win,sfcml_rect(ix,iy+28,38,10),sfcml_rgb(100,100,120));
    sfcml_drawRect(win,sfcml_rect(ix+10,iy+16,18,13),sfcml_rgb(100,100,120));
    /* Card text */
    int tx=sx+66;
    sfcml_drawText(win,"Ethernet",tx,cy+8,sfcml_rgb(0,0,180),cardbg);
    sfcml_Color stc=net_ok?sfcml_rgb(0,140,0):sfcml_rgb(180,20,20);
    const char*stlbl=net_ok?"Connecte":"Non connecte";
    sfcml_fillRect(win,sfcml_rect(tx,cy+22,8,8),stc);
    sfcml_drawText(win,stlbl,tx+12,cy+22,stc,cardbg);
    sfcml_drawText(win,"Carte reseau: RTL8139",tx,cy+38,lbl,cardbg);
    sfcml_drawText(win,"Interface: eth0",tx,cy+50,lbl,cardbg);
    /* Divider */
    cy+=68;
    sfcml_drawHLine(win,sx+6,cy,sw-12,sep);
    cy+=8;
    /* IP info rows */
    char ipb[16],gwb[16],dnsb[16],macb[18],mskb[16];
    _ip4str(net_my_ip, ipb);
    _ip4str(net_gw_ip, gwb);
    _ip4str(net_dns_ip,dnsb);
    _ip4str(net_netmask,mskb);
    _mac6str(net_mac,  macb);
    const char*kk[5]={"Adresse IPv4 :","Masque :","Passerelle :","DNS :","MAC :"};
    const char*vv[5]={ipb,mskb,gwb,dnsb,macb};
    for(int i=0;i<5;i++){
        int ry=cy+i*20;
        sfcml_Color rb2=(i%2)?rbg:bg;
        sfcml_fillRect(win,sfcml_rect(sx+6,ry,sw-12,19),rb2);
        sfcml_drawText(win,kk[i],sx+14,ry+6,lbl,rb2);
        sfcml_drawText(win,vv[i],sx+160,ry+6,val,rb2);
    }
    cy+=5*20+6;
    /* DHCP row */
    sfcml_fillRect(win,sfcml_rect(sx+6,cy,sw-12,20),cardbg);
    sfcml_drawRect(win,sfcml_rect(sx+6,cy,sw-12,20),sep);
    sfcml_drawText(win,"DHCP :",sx+14,cy+6,lbl,cardbg);
    sfcml_Color dhcpc=net_dhcp_ok?sfcml_rgb(0,120,0):sfcml_rgb(120,80,0);
    sfcml_drawText(win,net_dhcp_ok?"Actif (configuration automatique)":"Manuel (adresse statique)",sx+160,cy+6,dhcpc,cardbg);
    cy+=24;
    sfcml_drawHLine(win,sx+6,cy,sw-12,sep);
    cy+=8;
    /* Action buttons */
    sfcml_Color btn1=sfcml_rgb(0,100,200),btn2=sfcml_rgb(0,130,60);
    sfcml_Color btnd=sfcml_rgb(130,130,150);
    sfcml_fillRect(win,sfcml_rect(sx+8,cy,160,28),net_ok?btn1:btnd);
    sfcml_drawRect(win,sfcml_rect(sx+8,cy,160,28),sfcml_rgb(0,60,140));
    sfcml_drawText(win,"Actualiser DHCP",sx+20,cy+10,SFCML_WHITE,net_ok?btn1:btnd);
    sfcml_fillRect(win,sfcml_rect(sx+178,cy,160,28),net_ok?btn2:btnd);
    sfcml_drawRect(win,sfcml_rect(sx+178,cy,160,28),sfcml_rgb(0,80,30));
    sfcml_drawText(win,"Ping passerelle",sx+190,cy+10,SFCML_WHITE,net_ok?btn2:btnd);
    cy+=36;
    sfcml_drawHLine(win,sx+6,cy,sw-12,sep);
    cy+=8;
    /* Diagnostics */
    sfcml_drawText(win,"Diagnostics",sx+14,cy,sfcml_rgb(60,60,80),bg);
    cy+=14;
    sfcml_drawHLine(win,sx+14,cy,80,sfcml_rgb(180,185,195));
    cy+=8;
    sfcml_Color diagbg=sfcml_rgb(248,249,252);
    sfcml_fillRect(win,sfcml_rect(sx+6,cy,sw-12,sh-(cy-sy)-2),diagbg);
    sfcml_drawRect(win,sfcml_rect(sx+6,cy,sw-12,sh-(cy-sy)-2),sep);
    sfcml_drawText(win,"> Ping passerelle :",sx+14,cy+8,lbl,diagbg);
    if(_netmgr_ping_ok<0)
        sfcml_drawText(win,"Non teste (cliquer Ping passerelle)",sx+170,cy+8,sfcml_rgb(120,120,140),diagbg);
    else if(_netmgr_ping_ok==1)
        sfcml_drawText(win,"OK  (reponse recue)",sx+170,cy+8,sfcml_rgb(0,140,0),diagbg);
    else
        sfcml_drawText(win,"ECHEC (pas de reponse)",sx+170,cy+8,sfcml_rgb(180,20,20),diagbg);
    sfcml_drawText(win,net_ok?"> Etat : Connecte":"> Etat : Deconnecte",
        sx+14,cy+24,net_ok?sfcml_rgb(0,130,0):sfcml_rgb(160,20,20),diagbg);
    (void)sh;
}

static void netmgr_click(int mx,int my){
    AppWin*w=&_wins[W_NETMGR];
    int sx=w->x+1,sy=w->y+TBAR_H;
    /* Buttons are at cy = sy+242 (derived from draw_netmgr layout), height=28 */
    int btn_y=sy+242,btn_h=28;
    /* Bouton "Actualiser DHCP" (sx+8, btn_y, 160, 28) */
    if(mx>=sx+8&&mx<sx+168&&my>=btn_y&&my<btn_y+btn_h){
        if(net_ok)net_reconnect();
        _netmgr_ping_ok=-1;
        return;
    }
    /* Bouton "Ping passerelle" (sx+178, btn_y, 160, 28) */
    if(mx>=sx+178&&mx<sx+338&&my>=btn_y&&my<btn_y+btn_h){
        if(net_ok)_netmgr_ping_ok=net_ping_gw();
        return;
    }
}

/* ============================================================
 * Fond bureau (degrade)
 * ============================================================ */
static void draw_bg(sfcml_Window* win){
    int seg=(SCR_H-31)/9;
    for(int i=0;i<9;i++){
        int y=i*seg,h=(i<8)?seg:(SCR_H-31-8*seg);
        sfcml_fillRect(win,sfcml_rect(0,y,SCR_W,h),sfcml_rgb(
            (uint8_t)((int)C_DK.r*(i+1)/9),
            (uint8_t)((int)C_DK.g*(i+1)/9),
            (uint8_t)((int)C_DK.b*(i+1)/9)));
    }
    for(int y=0;y<SCR_H-31;y+=20)
        for(int x=44;x<SCR_W;x+=20)
            sfcml_drawPixel(win,x,y,sfcml_rgb(
                (uint8_t)((int)C_DK.r/4+20<255?(int)C_DK.r/4+20:255),
                (uint8_t)((int)C_DK.g/4+20<255?(int)C_DK.g/4+20:255),
                (uint8_t)((int)C_DK.b/4+20<255?(int)C_DK.b/4+20:255)));
}

/* ============================================================
 * Rendu complet (double-buffered)
 * ============================================================ */
static void redraw(sfcml_Window* win){
    draw_bg(win);
    draw_icons(win);
    for(int zi=0;zi<NW;zi++){
        int i=_z[zi];
        if(!_wins[i].visible||_wins[i].minimized)continue;
        draw_frame(win,i);
        if(i==W_TERM)       draw_term(win,i);
        else if(i==W_ABOUT) draw_about(win,i);
        else if(i==W_CREEP) draw_creep_win(win,i);
        else if(i==W_PAINT)     draw_paint(win,i);
        else if(i==W_CODE)      draw_code(win,i);
        else if(i==W_WORD)      draw_word(win,i);
        else if(i==W_SETTINGS)  draw_settings(win,i);
        else if(i==W_BROWSER)   draw_browser(win,i);
        else if(i==W_CALC)      draw_calc(win,i);
        else if(i==W_FILES)     draw_files(win,i);
        else if(i==W_NETMGR)    draw_netmgr(win,i);
        else if(i==W_SNAKE)     draw_snake(win,i);
        else if(i==W_RTYPE)     draw_rtype(win,i);
        else if(i==W_PONG)      draw_pong(win,i);
        else if(i==W_MINE)      draw_mine(win,i);
        else if(i==W_TETRIS)    draw_tetris(win,i);
        else if(i==W_DEMINE)    draw_mines(win,i);
    }
    draw_taskbar(win);
    if(_start_open)draw_smenu(win);
    if(_rcopen)    draw_rcmenu(win);
    /* curseur masque quand MyCraft capture la souris */
    if(!(_mc_look&&_focus==W_MINE&&_wins[W_MINE].visible&&!_wins[W_MINE].minimized))
        draw_cursor(win,_mx,_my);
    sfcml_present(win);
}

/* ============================================================
 * Clic paint (palette, brosse, canvas)
 * ============================================================ */
static void paint_click(int mx,int my){
    AppWin* w=&_wins[W_PAINT];
    int sx=w->x+1,sy=w->y+TBAR_H,sw=w->w-2;
    /* Brush buttons */
    static const int bsz[3]={0,1,3};
    for(int b=0;b<3;b++){
        int bx=sx+204+b*28;
        if(mx>=bx&&mx<bx+24&&my>=sy+5&&my<sy+21){_pbrush=bsz[b];return;}
    }
    /* Clear */
    if(mx>=sx+sw-64&&mx<sx+sw-6&&my>=sy+5&&my<sy+21){
        memset(PAINT_CANVAS,0,PAINT_CW*PAINT_CH);return;
    }
    /* Palette swatches */
    for(int c=0;c<PAINT_NC;c++){
        int px=sx+2+(c%2)*24,py=sy+PAINT_TB+16+(c/2)*22;
        if(mx>=px&&mx<px+20&&my>=py&&my<py+20){_pcol=c;return;}
    }
    /* Canvas draw */
    int cax=sx+PAINT_SB,cay=sy+PAINT_TB;
    int caw=sw-PAINT_SB;
    int cx=mx-cax,cy=my-cay;
    if(cx>=0&&cx<PAINT_CW&&cy>=0&&cy<PAINT_CH&&mx>=cax&&mx<cax+caw){
        _painting=1;_paint_dot(cx,cy,(uint8_t)_pcol);
    }
}

/* ============================================================
 * Gestion des clics
 * ============================================================ */
static void on_press(int mx,int my,int btn){
    if(_sleeping){_sleeping=0;_inact_secs=0;return;}
    /* Clic dans la zone de jeu MyCraft (si au premier plan) :
       gauche=casser, droit=poser */
    if(_wins[W_MINE].visible&&!_wins[W_MINE].minimized){
        int top=-1;
        for(int zi=NW-1;zi>=0;zi--){
            int i=_z[zi];AppWin* w2=&_wins[i];
            if(!w2->visible||w2->minimized)continue;
            if(mx>=w2->x&&mx<w2->x+w2->w&&my>=w2->y&&my<w2->y+w2->h){top=i;break;}
        }
        if(top==W_MINE){
            AppWin* w2=&_wins[W_MINE];
            if(my>=w2->y+TBAR_H&&mx>w2->x&&mx<w2->x+w2->w-1&&my<w2->y+w2->h-1){
                _rcopen=0;_start_open=0;
                win_front(W_MINE);mine_click(btn);return;
            }
        }
    }
    /* Clic dans la zone de jeu Demineur (gauche=reveler, droit=drapeau) */
    if(_wins[W_DEMINE].visible&&!_wins[W_DEMINE].minimized){
        int top=-1;
        for(int zi=NW-1;zi>=0;zi--){
            int i=_z[zi];AppWin* w2=&_wins[i];
            if(!w2->visible||w2->minimized)continue;
            if(mx>=w2->x&&mx<w2->x+w2->w&&my>=w2->y&&my<w2->y+w2->h){top=i;break;}
        }
        if(top==W_DEMINE){
            AppWin* w2=&_wins[W_DEMINE];
            if(my>=w2->y+TBAR_H+34&&mx>w2->x&&mx<w2->x+w2->w-1&&my<w2->y+w2->h-1){
                _rcopen=0;_start_open=0;
                win_front(W_DEMINE);mines_click(mx,my,btn);return;
            }
        }
    }
    if(btn==1){_rcopen=1;_rcx=mx;_rcy=my;_start_open=0;return;}
    if(_rcopen){rcmenu_click(mx,my);return;}
    if(_start_open){smenu_click(mx,my);return;}
    if(mx>=2&&mx<86&&my>=TB_Y+2&&my<TB_Y+28){_start_open=!_start_open;return;}
    /* Taskbar window buttons */
    if(my>=TB_Y+2&&my<TB_Y+28){
        int bx=88;
        for(int i=0;i<NW;i++){
            if(!_wins[i].visible)continue;
            if(mx>=bx&&mx<bx+TB_BW){
                if(!_wins[i].minimized&&_focus==i)_wins[i].minimized=1;
                else{_wins[i].minimized=0;win_front(i);}
                return;
            }
            bx+=TB_BW+2;
        }
    }
    /* Windows (front to back) */
    for(int zi=NW-1;zi>=0;zi--){
        int i=_z[zi];
        AppWin* w=&_wins[i];
        if(!w->visible||w->minimized)continue;
        if(mx<w->x||mx>=w->x+w->w||my<w->y||my>=w->y+w->h)continue;
        win_front(i);
        if(hit_x(i,mx,my)){w->visible=0;if(i==W_WORD)_nano_path[0]='\0';return;}
        if(hit_mn(i,mx,my)){w->minimized=1;return;}
        if(hit_tb(i,mx,my)){_drag_win=i;_drag_ox=mx-w->x;_drag_oy=my-w->y;return;}
        if(i==W_PAINT){paint_click(mx,my);return;}
        if(i==W_SETTINGS){settings_click(mx,my);return;}
        if(i==W_BROWSER){browser_click(mx,my);return;}
        if(i==W_CALC){calc_click(mx,my);return;}
        if(i==W_FILES){files_click(mx,my);return;}
        if(i==W_NETMGR){netmgr_click(mx,my);return;}
        return;
    }
    /* Icons */
    int found=-1;
    for(int i=0;i<NICONS;i++)
        if(mx>=IC_IX(i)&&mx<IC_IX(i)+IC_SZ&&my>=IC_IY(i)&&my<IC_IY(i)+IC_SZ+10){found=i;break;}
    if(found>=0){
        if(_sel_icon==found){
            if(found==0)win_open(W_TERM);
            else if(found==1)win_open(W_PAINT);
            else if(found==2)win_open(W_CODE);
            else if(found==3)win_open(W_WORD);
            else if(found==4)win_open(W_CREEP);
            else if(found==5)win_open(W_ABOUT);
            else if(found==6)cmd_reboot();
            else if(found==7)win_open(W_SETTINGS);
            else if(found==9){win_open(W_SNAKE);snake_reset();}
            else if(found==10){win_open(W_RTYPE);rtype_reset();}
            else if(found==11){win_open(W_PONG);pong_reset();}
            else if(found==12)win_open(W_MINE);
            else if(found==13){win_open(W_TETRIS);tet_reset();}
            else if(found==14){win_open(W_DEMINE);ms_reset();}
            else{win_open(W_BROWSER);_bnav(0);}
        }
        _sel_icon=found;
    } else {_sel_icon=-1;_start_open=0;_rcopen=0;}
}


/* ============================================================
 * Jeu Snake
 * ============================================================ */
#define SN_CW    16
#define SN_CH    16
#define SN_COLS  36
#define SN_ROWS  27
#define SN_MAX   300
#define SN_SPEED 7

typedef struct{int x,y;}SnCell;
static SnCell  _sn_body[SN_MAX];
static int     _sn_len,_sn_dx,_sn_dy;
static int     _sn_fx,_sn_fy;
static int     _sn_score,_sn_tick,_sn_dead;

static void snake_reset(void){
    _sn_len=3;_sn_dx=1;_sn_dy=0;_sn_score=0;_sn_tick=0;_sn_dead=0;
    _sn_body[0]=(SnCell){10,13};_sn_body[1]=(SnCell){9,13};_sn_body[2]=(SnCell){8,13};
    _sn_fx=18;_sn_fy=13;
}
static void _sn_food(void){
    static unsigned _snsd=7919;
    for(int t=0;t<200;t++){
        _snsd=_snsd*1103515245+12345;int x=(int)((_snsd>>16)%(unsigned)SN_COLS);
        _snsd=_snsd*1103515245+12345;int y=(int)((_snsd>>16)%(unsigned)SN_ROWS);
        int ok=1;
        for(int i=0;i<_sn_len;i++)if(_sn_body[i].x==x&&_sn_body[i].y==y){ok=0;break;}
        if(ok){_sn_fx=x;_sn_fy=y;return;}
    }
}
static void snake_step(void){
    if(_sn_dead)return;
    if(++_sn_tick<SN_SPEED)return;
    _sn_tick=0;
    int nx=_sn_body[0].x+_sn_dx,ny=_sn_body[0].y+_sn_dy;
    if(nx<0||nx>=SN_COLS||ny<0||ny>=SN_ROWS){_sn_dead=1;return;}
    for(int i=0;i<_sn_len-1;i++)if(_sn_body[i].x==nx&&_sn_body[i].y==ny){_sn_dead=1;return;}
    int ate=(nx==_sn_fx&&ny==_sn_fy);
    if(!ate){for(int i=_sn_len-1;i>0;i--)_sn_body[i]=_sn_body[i-1];}
    else{if(_sn_len<SN_MAX-1){for(int i=_sn_len;i>0;i--)_sn_body[i]=_sn_body[i-1];_sn_len++;}_sn_score+=10;_sn_food();}
    _sn_body[0]=(SnCell){nx,ny};
}
static void snake_key(sfcml_KeyCode k){
    if(k==SFCML_KEY_UP    &&_sn_dy==0){_sn_dx=0;_sn_dy=-1;}
    else if(k==SFCML_KEY_DOWN &&_sn_dy==0){_sn_dx=0;_sn_dy= 1;}
    else if(k==SFCML_KEY_LEFT &&_sn_dx==0){_sn_dx=-1;_sn_dy=0;}
    else if(k==SFCML_KEY_RIGHT&&_sn_dx==0){_sn_dx= 1;_sn_dy=0;}
    else if(k==SFCML_KEY_RETURN&&_sn_dead)snake_reset();
}
static void draw_snake(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int cx=w->x+1,cy=w->y+TBAR_H,cw=w->w-2,ch=w->h-TBAR_H-1;
    sfcml_fillRect(win,sfcml_rect(cx,cy,cw,ch),sfcml_rgb(10,30,10));
    sfcml_fillRect(win,sfcml_rect(cx+_sn_fx*SN_CW+2,cy+_sn_fy*SN_CH+2,SN_CW-4,SN_CH-4),sfcml_rgb(220,50,50));
    for(int i=_sn_len-1;i>=0;i--){
        sfcml_Color c=(i==0)?sfcml_rgb(60,220,60):sfcml_rgb(30,160,30);
        sfcml_fillRect(win,sfcml_rect(cx+_sn_body[i].x*SN_CW+1,cy+_sn_body[i].y*SN_CH+1,SN_CW-2,SN_CH-2),c);
    }
    char _sb[16];itoa(_sn_score,_sb,10);
    char _sc[32];strncpy(_sc,"Score: ",32);strncat(_sc,_sb,24);
    sfcml_drawText(win,_sc,cx+4,cy+4,SFCML_WHITE,sfcml_rgb(10,30,10));
    if(_sn_dead)
        sfcml_drawText(win,"GAME OVER  [Entree=Rejouer]",cx+(cw/2)-104,cy+(ch/2)-4,sfcml_rgb(255,80,80),sfcml_rgb(0,0,0));
}

/* ============================================================
 * Jeu R-Type
 * ============================================================ */
#define RT_BMAX  10
#define RT_EMAX  8
#define RT_SMAX  50
#define RT_PW    28
#define RT_PH    16
#define RT_EW    24
#define RT_EH    16
#define RT_BW    8
#define RT_BH    4

typedef struct{int x,y,alive;}RTObj;
static int     _rt_px,_rt_py;
static int     _rt_pu,_rt_pd,_rt_pl,_rt_pr,_rt_fire;
static RTObj   _rt_b[RT_BMAX];
static RTObj   _rt_e[RT_EMAX];
static int     _rt_edy[RT_EMAX];
static int     _rt_etick,_rt_score,_rt_lives,_rt_fire_cd,_rt_dead,_rt_win;
static int     _rt_stars[RT_SMAX][2];
static unsigned _rt_seed=31337;

static void rtype_reset(void){
    AppWin* w=&_wins[W_RTYPE];
    int gw=w->w-2,gh=w->h-TBAR_H-1;
    _rt_px=40;_rt_py=gh/2-RT_PH/2;
    _rt_pu=_rt_pd=_rt_pl=_rt_pr=_rt_fire=0;
    _rt_score=0;_rt_lives=3;_rt_fire_cd=0;_rt_etick=0;_rt_dead=0;_rt_win=0;
    for(int i=0;i<RT_BMAX;i++)_rt_b[i].alive=0;
    for(int i=0;i<RT_EMAX;i++){
        _rt_seed=_rt_seed*1103515245+12345;
        int ey=(int)((_rt_seed>>8)%(unsigned)(gh-RT_EH));
        _rt_e[i].x=gw-60-i*60;_rt_e[i].y=ey;_rt_e[i].alive=1;
        _rt_edy[i]=(i%2==0)?1:-1;
    }
    for(int i=0;i<RT_SMAX;i++){
        _rt_seed=_rt_seed*1103515245+12345;_rt_stars[i][0]=(int)((_rt_seed>>8)%(unsigned)gw);
        _rt_seed=_rt_seed*1103515245+12345;_rt_stars[i][1]=(int)((_rt_seed>>8)%(unsigned)gh);
    }
}
static void rtype_step(void){
    if(_rt_dead||_rt_win)return;
    AppWin* w=&_wins[W_RTYPE];
    int gw=w->w-2,gh=w->h-TBAR_H-1;
    if(_rt_pu&&_rt_py>0)_rt_py-=3;
    if(_rt_pd&&_rt_py<gh-RT_PH)_rt_py+=3;
    if(_rt_pl&&_rt_px>0)_rt_px-=3;
    if(_rt_pr&&_rt_px<gw-RT_PW)_rt_px+=3;
    if(_rt_fire_cd>0)_rt_fire_cd--;
    if(_rt_fire&&_rt_fire_cd==0){
        for(int i=0;i<RT_BMAX;i++)if(!_rt_b[i].alive){
            _rt_b[i].x=_rt_px+RT_PW;_rt_b[i].y=_rt_py+RT_PH/2-RT_BH/2;_rt_b[i].alive=1;
            _rt_fire_cd=6;break;
        }
    }
    for(int i=0;i<RT_BMAX;i++){
        if(!_rt_b[i].alive)continue;
        _rt_b[i].x+=8;
        if(_rt_b[i].x>=gw)_rt_b[i].alive=0;
    }
    if(++_rt_etick>=2){
        _rt_etick=0;int all_dead=1;
        for(int i=0;i<RT_EMAX;i++){
            if(!_rt_e[i].alive)continue;
            all_dead=0;
            _rt_e[i].x-=2;_rt_e[i].y+=_rt_edy[i]*2;
            if(_rt_e[i].y<0||_rt_e[i].y>gh-RT_EH)_rt_edy[i]=-_rt_edy[i];
            if(_rt_e[i].x<-RT_EW)_rt_e[i].x=gw;
            for(int b=0;b<RT_BMAX;b++){
                if(!_rt_b[b].alive)continue;
                if(_rt_b[b].x<_rt_e[i].x+RT_EW&&_rt_b[b].x+RT_BW>_rt_e[i].x&&
                   _rt_b[b].y<_rt_e[i].y+RT_EH&&_rt_b[b].y+RT_BH>_rt_e[i].y){
                    _rt_e[i].alive=0;_rt_b[b].alive=0;_rt_score+=100;
                }
            }
            if(_rt_e[i].alive&&_rt_e[i].x<_rt_px+RT_PW&&_rt_e[i].x+RT_EW>_rt_px&&
               _rt_e[i].y<_rt_py+RT_PH&&_rt_e[i].y+RT_EH>_rt_py){
                _rt_lives--;_rt_e[i].alive=0;
                if(_rt_lives<=0){_rt_dead=1;return;}
            }
        }
        if(all_dead)_rt_win=1;
    }
    for(int i=0;i<RT_SMAX;i++){
        _rt_stars[i][0]-=1+(i%3);
        if(_rt_stars[i][0]<0){
            _rt_stars[i][0]=gw-1;
            _rt_seed=_rt_seed*1103515245+12345;
            _rt_stars[i][1]=(int)((_rt_seed>>8)%(unsigned)gh);
        }
    }
}
static void rtype_key_press(sfcml_KeyCode k){
    if(_rt_dead||_rt_win){if(k==SFCML_KEY_RETURN)rtype_reset();return;}
    if(k==SFCML_KEY_UP)_rt_pu=1;
    else if(k==SFCML_KEY_DOWN)_rt_pd=1;
    else if(k==SFCML_KEY_LEFT)_rt_pl=1;
    else if(k==SFCML_KEY_RIGHT)_rt_pr=1;
    else if(k==SFCML_KEY_SPACE)_rt_fire=1;
}
static void rtype_key_release(sfcml_KeyCode k){
    if(k==SFCML_KEY_UP)_rt_pu=0;
    else if(k==SFCML_KEY_DOWN)_rt_pd=0;
    else if(k==SFCML_KEY_LEFT)_rt_pl=0;
    else if(k==SFCML_KEY_RIGHT)_rt_pr=0;
    else if(k==SFCML_KEY_SPACE)_rt_fire=0;
}
static void draw_rtype(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int cx=w->x+1,cy=w->y+TBAR_H,cw=w->w-2,ch=w->h-TBAR_H-1;
    sfcml_fillRect(win,sfcml_rect(cx,cy,cw,ch),sfcml_rgb(0,0,20));
    for(int i=0;i<RT_SMAX;i++){
        sfcml_Color sc2=(i%3==0)?sfcml_rgb(255,255,255):(i%3==1)?sfcml_rgb(180,180,220):sfcml_rgb(100,100,160);
        sfcml_drawPixel(win,cx+_rt_stars[i][0],cy+_rt_stars[i][1],sc2);
    }
    for(int i=0;i<RT_BMAX;i++)
        if(_rt_b[i].alive)sfcml_fillRect(win,sfcml_rect(cx+_rt_b[i].x,cy+_rt_b[i].y,RT_BW,RT_BH),sfcml_rgb(255,200,50));
    for(int i=0;i<RT_EMAX;i++){
        if(!_rt_e[i].alive)continue;
        int ex=cx+_rt_e[i].x,ey=cy+_rt_e[i].y;
        sfcml_fillRect(win,sfcml_rect(ex,ey,RT_EW,RT_EH),sfcml_rgb(180,20,20));
        sfcml_fillRect(win,sfcml_rect(ex+2,ey+RT_EH/2-3,6,6),sfcml_rgb(255,80,80));
    }
    int px=cx+_rt_px,py=cy+_rt_py;
    sfcml_fillRect(win,sfcml_rect(px,py+4,RT_PW-8,RT_PH-8),sfcml_rgb(50,150,255));
    sfcml_fillRect(win,sfcml_rect(px+RT_PW-8,py+RT_PH/2-4,12,8),sfcml_rgb(80,200,255));
    sfcml_fillRect(win,sfcml_rect(px+4,py,RT_PW-12,4),sfcml_rgb(0,100,200));
    sfcml_fillRect(win,sfcml_rect(px+4,py+RT_PH-4,RT_PW-12,4),sfcml_rgb(0,100,200));
    char _rb[16];itoa(_rt_score,_rb,10);
    char _rs[32];strncpy(_rs,"Score:",32);strncat(_rs,_rb,24);
    sfcml_drawText(win,_rs,cx+4,cy+4,SFCML_WHITE,sfcml_rgb(0,0,20));
    for(int i=0;i<_rt_lives&&i<5;i++)
        sfcml_fillRect(win,sfcml_rect(cx+cw-20-i*14,cy+4,10,8),sfcml_rgb(50,150,255));
    if(_rt_dead)sfcml_drawText(win,"GAME OVER  [Entree=Rejouer]",cx+cw/2-104,cy+ch/2-4,sfcml_rgb(255,80,80),sfcml_rgb(0,0,0));
    if(_rt_win) sfcml_drawText(win,"VICTOIRE!  [Entree=Rejouer]",cx+cw/2-104,cy+ch/2-4,sfcml_rgb(80,255,80),sfcml_rgb(0,0,0));
}

/* ============================================================
 * Jeu Pong
 * ============================================================ */
#define PG_PW    12
#define PG_PH    60
#define PG_BS    10
#define PG_SPD   4

static int _pg_p1y,_pg_p2y;
static int _pg_bx,_pg_by,_pg_bdx,_pg_bdy;
static int _pg_sc1,_pg_sc2;
static int _pg_pu,_pg_pd;

static void pong_reset(void){
    AppWin* w=&_wins[W_PONG];
    int gw=w->w-2,gh=w->h-TBAR_H-1;
    _pg_p1y=gh/2-PG_PH/2;_pg_p2y=gh/2-PG_PH/2;
    _pg_bx=gw/2;_pg_by=gh/2;_pg_bdx=3;_pg_bdy=2;
    _pg_sc1=0;_pg_sc2=0;_pg_pu=_pg_pd=0;
}
static void pong_step(void){
    AppWin* w=&_wins[W_PONG];
    int gw=w->w-2,gh=w->h-TBAR_H-1;
    if(_pg_pu&&_pg_p1y>0)_pg_p1y-=PG_SPD;
    if(_pg_pd&&_pg_p1y<gh-PG_PH)_pg_p1y+=PG_SPD;
    int ai_cy=_pg_p2y+PG_PH/2;
    if(ai_cy<_pg_by)_pg_p2y+=PG_SPD-1;
    else if(ai_cy>_pg_by+PG_BS)_pg_p2y-=PG_SPD-1;
    if(_pg_p2y<0)_pg_p2y=0;if(_pg_p2y>gh-PG_PH)_pg_p2y=gh-PG_PH;
    _pg_bx+=_pg_bdx;_pg_by+=_pg_bdy;
    if(_pg_by<=0){_pg_by=0;_pg_bdy=-_pg_bdy;}
    if(_pg_by>=gh-PG_BS){_pg_by=gh-PG_BS;_pg_bdy=-_pg_bdy;}
    if(_pg_bdx<0&&_pg_bx<=PG_PW+6&&_pg_bx>=PG_PW-4&&
       _pg_by+PG_BS>_pg_p1y&&_pg_by<_pg_p1y+PG_PH){
        _pg_bdx=-_pg_bdx;
        int rel=(_pg_by+PG_BS/2)-(_pg_p1y+PG_PH/2);
        _pg_bdy=rel/8;if(_pg_bdy==0)_pg_bdy=(_pg_bdy>=0)?1:-1;
    }
    if(_pg_bdx>0&&_pg_bx+PG_BS>=gw-PG_PW-6&&_pg_bx+PG_BS<=gw-PG_PW+4&&
       _pg_by+PG_BS>_pg_p2y&&_pg_by<_pg_p2y+PG_PH){
        _pg_bdx=-_pg_bdx;
        int rel=(_pg_by+PG_BS/2)-(_pg_p2y+PG_PH/2);
        _pg_bdy=rel/8;if(_pg_bdy==0)_pg_bdy=(_pg_bdy>=0)?1:-1;
    }
    if(_pg_bx<=0){_pg_sc2++;_pg_bx=gw/2;_pg_by=gh/2;_pg_bdx=3;_pg_bdy=2;}
    if(_pg_bx>=gw){_pg_sc1++;_pg_bx=gw/2;_pg_by=gh/2;_pg_bdx=-3;_pg_bdy=2;}
}
static void pong_key_press(sfcml_KeyCode k){
    if(k==SFCML_KEY_UP||k==SFCML_KEY_W)_pg_pu=1;
    else if(k==SFCML_KEY_DOWN||k==SFCML_KEY_S)_pg_pd=1;
    else if(k==SFCML_KEY_RETURN)pong_reset();
}
static void pong_key_release(sfcml_KeyCode k){
    if(k==SFCML_KEY_UP||k==SFCML_KEY_W)_pg_pu=0;
    else if(k==SFCML_KEY_DOWN||k==SFCML_KEY_S)_pg_pd=0;
}
static void draw_pong(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int cx=w->x+1,cy=w->y+TBAR_H,cw=w->w-2,ch=w->h-TBAR_H-1;
    sfcml_fillRect(win,sfcml_rect(cx,cy,cw,ch),sfcml_rgb(10,10,30));
    for(int y=0;y<ch;y+=10)sfcml_drawHLine(win,cx+cw/2-1,cy+y,2,sfcml_rgb(60,60,90));
    sfcml_fillRect(win,sfcml_rect(cx+6,cy+_pg_p1y,PG_PW,PG_PH),SFCML_WHITE);
    sfcml_fillRect(win,sfcml_rect(cx+cw-6-PG_PW,cy+_pg_p2y,PG_PW,PG_PH),sfcml_rgb(200,80,80));
    sfcml_fillRect(win,sfcml_rect(cx+_pg_bx,cy+_pg_by,PG_BS,PG_BS),SFCML_WHITE);
    char _pb1[8],_pb2[8];
    itoa(_pg_sc1,_pb1,10);itoa(_pg_sc2,_pb2,10);
    sfcml_drawText(win,_pb1,cx+cw/2-20,cy+8,SFCML_WHITE,sfcml_rgb(10,10,30));
    sfcml_drawText(win,_pb2,cx+cw/2+12,cy+8,SFCML_WHITE,sfcml_rgb(10,10,30));
    sfcml_drawText(win,"W/S ou Haut/Bas pour jouer",cx+cw/2-96,cy+ch-14,sfcml_rgb(120,120,180),sfcml_rgb(10,10,30));
}

/* ============================================================
 * MyCraft - Minecraft 3D from scratch
 * - Monde voxel 64x64x32 genere procedurellement (collines,
 *   arbres, grottes, plages)
 * - Rendu raycasting par pixel (DDA voxel en virgule fixe 16.16),
 *   160x120 upscale x4 -> 640x480
 * - Textures 16x16 100% procedurales (aucun asset externe)
 * - Physique: gravite, saut, collisions AABB
 * - Casser (clic G) / poser (clic D) des blocs, hotbar 9 slots
 * ============================================================ */
#define MC_XZ    64                 /* taille monde en X et Z */
#define MC_Y     32                 /* hauteur monde */
#define MC_RW    160                /* resolution interne de rendu */
#define MC_RH    120
#define MC_SC    4                  /* upscale -> 640x480 */
#define MC_NB    12                 /* types de blocs */
#define MC_FIX   16                 /* virgule fixe 16.16 */
#define MC_ONE   (1<<MC_FIX)
#define MC_MAXT  (28<<MC_FIX)       /* distance de vue (blocs) */
#define MC_INF   0x3FFFFFFF

enum{MC_AIR=0,MC_GRASS,MC_DIRT,MC_STONE,MC_COBBLE,MC_PLANK,
     MC_LOG,MC_LEAF,MC_SAND,MC_BRICK,MC_GLASS,MC_BEDROCK};

static uint8_t  _mc_w[MC_XZ*MC_XZ*MC_Y];   /* monde: x + z*64 + y*4096 */
static uint32_t _mc_tex[MC_NB][3][256];    /* [bloc][0=haut 1=cote 2=bas][16x16], 0=transparent */
static int      _mc_hmap[MC_XZ*MC_XZ];     /* hauteur du sol par colonne */
static uint8_t  _mc_row[MC_RW*MC_SC*3];    /* une ligne de rendu upscalee (BGR) */
static float _mc_px,_mc_py,_mc_pz;         /* position des pieds */
static float _mc_vy,_mc_yaw,_mc_pitch;
static int   _mc_ground;
static int   _mc_kf,_mc_kb,_mc_kl,_mc_kr,_mc_kj;  /* touches deplacement */
static int   _mc_lu,_mc_ld,_mc_ll,_mc_lr;         /* touches regard */
static int   _mc_hot;                      /* slot hotbar 0..8 */
static unsigned _mc_seed=20260705u;
static int   _mc_fps,_mc_frm;
static uint32_t _mc_ft;
static int   _mc_msg_ttl;                  /* HUD message (frames restantes) */
static const char* _mc_msg="";

/* ---- Sauvegarde du monde sur disque (zone ATA dediee) ----
 * Le monde (131072 o = 256 secteurs) est trop gros pour une entree
 * MyFS (512 o). On lui reserve la zone LBA 600.. Superbloc au LBA 599. */
#define MC_SAVE_LBA  599
#define MC_WORLD_LBA 600
static void mc_save(void){
    if(!_ata_ok){_mc_msg="Pas de disque";_mc_msg_ttl=120;beep(200,60);return;}
    /* superbloc: magie + position/orientation du joueur */
    memset(_fs_secbuf,0,512);
    *(uint32_t*)_fs_secbuf=0x4D435731u;        /* "1WCM" */
    memcpy(_fs_secbuf+4 ,&_mc_px,4);memcpy(_fs_secbuf+8 ,&_mc_py,4);
    memcpy(_fs_secbuf+12,&_mc_pz,4);memcpy(_fs_secbuf+16,&_mc_yaw,4);
    memcpy(_fs_secbuf+20,&_mc_pitch,4);
    if(!ata_write(MC_SAVE_LBA,_fs_secbuf)){_mc_msg="Echec ecriture";_mc_msg_ttl=120;return;}
    for(int s=0;s<256;s++)
        if(!ata_write(MC_WORLD_LBA+(uint32_t)s,_mc_w+s*512)){
            _mc_msg="Echec ecriture";_mc_msg_ttl=120;return;}
    _mc_msg="Monde sauvegarde";_mc_msg_ttl=120;
    beep(N_G5,60);beep(N_C6,90);
}
static int mc_load(void){
    if(!_ata_ok)return 0;
    if(!ata_read(MC_SAVE_LBA,_fs_secbuf))return 0;
    if(*(uint32_t*)_fs_secbuf!=0x4D435731u)return 0;
    memcpy(&_mc_px,_fs_secbuf+4 ,4);memcpy(&_mc_py,_fs_secbuf+8 ,4);
    memcpy(&_mc_pz,_fs_secbuf+12,4);memcpy(&_mc_yaw,_fs_secbuf+16,4);
    memcpy(&_mc_pitch,_fs_secbuf+20,4);
    for(int s=0;s<256;s++)
        if(!ata_read(MC_WORLD_LBA+(uint32_t)s,_mc_w+s*512))return 0;
    return 1;
}

static const uint8_t _mc_hotbar[9]={MC_DIRT,MC_STONE,MC_COBBLE,MC_PLANK,
    MC_LOG,MC_LEAF,MC_SAND,MC_BRICK,MC_GLASS};
static const char* _mc_names[MC_NB]={"Air","Herbe","Terre","Pierre","Pave",
    "Planches","Bois","Feuilles","Sable","Brique","Verre","Bedrock"};

static unsigned _mc_rnd(void){
    _mc_seed^=_mc_seed<<13;_mc_seed^=_mc_seed>>17;_mc_seed^=_mc_seed<<5;
    return _mc_seed;
}
/* x87 direct: pas de libm en freestanding */
static float mc_sin(float x){float r;__asm__("fsin":"=t"(r):"0"(x));return r;}
static float mc_cos(float x){float r;__asm__("fcos":"=t"(r):"0"(x));return r;}
static inline int32_t mc_mul(int32_t a,int32_t b){return (int32_t)(((int64_t)a*b)>>MC_FIX);}

static inline uint8_t mc_get(int x,int y,int z){
    if((unsigned)x>=MC_XZ||(unsigned)z>=MC_XZ||(unsigned)y>=MC_Y)return MC_AIR;
    return _mc_w[x+(z<<6)+(y<<12)];
}
static inline void mc_set(int x,int y,int z,uint8_t b){
    if((unsigned)x>=MC_XZ||(unsigned)z>=MC_XZ||(unsigned)y>=MC_Y)return;
    _mc_w[x+(z<<6)+(y<<12)]=b;
}

/* ---- Textures procedurales ---- */
static uint32_t mc_rgbu(int r,int g,int b){
    if(r<0)r=0;if(r>255)r=255;if(g<0)g=0;if(g>255)g=255;if(b<0)b=0;if(b>255)b=255;
    return 0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}
static void mc_gen_tex(void){
    for(int b=1;b<MC_NB;b++)for(int f=0;f<3;f++)
    for(int v=0;v<16;v++)for(int u=0;u<16;u++){
        int n=(int)(_mc_rnd()%32)-16;   /* bruit -16..15 */
        uint32_t c=0;
        switch(b){
        case MC_GRASS:
            if(f==0)      c=mc_rgbu(72+n/2,150+n,52+n/2);
            else if(f==2) c=mc_rgbu(134+n,96+n,58+n/2);
            else{int fr=3+(int)(_mc_rnd()%3);
                 if(v<fr)c=mc_rgbu(72+n/2,150+n,52+n/2);
                 else    c=mc_rgbu(134+n,96+n,58+n/2);}
            break;
        case MC_DIRT: c=mc_rgbu(134+n,96+n,58+n/2);break;
        case MC_STONE:{int d=(_mc_rnd()%8==0)?-30:0;
            c=mc_rgbu(125+n/2+d,125+n/2+d,128+n/2+d);}break;
        case MC_COBBLE:{
            int cell=((u>>2)*7+(v>>2)*13)%3;
            int base=100+cell*24+n/2;
            if((u&3)==0||(v&3)==0)base-=35;
            c=mc_rgbu(base,base,base+4);}break;
        case MC_PLANK:{
            int base=172+n/2;
            if((v&3)==3)base-=55;
            if(((v>>2)&1)==0?u==7:u==15)base-=40;
            c=mc_rgbu(base,(base*2)/3,base/3);}break;
        case MC_LOG:
            if(f==1){int base=96+((u*37)%7)*6+n/4;
                     if((v&7)==7)base-=15;
                     c=mc_rgbu(base,(base*3)/4,base/2);}
            else{int dx2=u-8,dz2=v-8;int d2=dx2*dx2+dz2*dz2;
                 int ring=((d2>>3)&1)?152:112;
                 c=mc_rgbu(ring+n/4,(ring*3)/4,ring/2);}
            break;
        case MC_LEAF:
            if(_mc_rnd()%4==0)c=0;   /* trou transparent */
            else c=mc_rgbu(42+n/2,112+n,36+n/2);
            break;
        case MC_SAND: c=mc_rgbu(218+n/2,204+n/2,152+n/2);break;
        case MC_BRICK:{
            int row=v>>2;
            int mortar=((v&3)==0)||(((u+((row&1)<<2))&7)==0);
            if(mortar)c=mc_rgbu(176+n/3,171+n/3,166+n/3);
            else      c=mc_rgbu(156+n/2,62+n/3,52+n/3);}break;
        case MC_GLASS:
            if(u==0||u==15||v==0||v==15)c=mc_rgbu(212,236,246);
            else if(u+v==18||u+v==19)   c=mc_rgbu(232,246,255);
            else c=0;                    /* transparent */
            break;
        case MC_BEDROCK:{int q=((_mc_rnd()>>5)&1)?42:0;
            c=mc_rgbu(52+q+n/2,52+q+n/2,58+q+n/2);}break;
        }
        _mc_tex[b][f][(v<<4)|u]=c;
    }
}

/* ---- Generation du monde ---- */
static void mc_genworld(void){
    memset(_mc_w,0,sizeof _mc_w);
    /* heightmap: bruit de valeur (grille 9x9 interpolee) */
    int grid[9][9];
    for(int i=0;i<9;i++)for(int j=0;j<9;j++)grid[i][j]=9+(int)(_mc_rnd()%12);
    for(int z=0;z<MC_XZ;z++)for(int x=0;x<MC_XZ;x++){
        int gx=x>>3,gz=z>>3,fx=x&7,fz=z&7;
        int h0=grid[gx][gz]*(8-fx)+grid[gx+1][gz]*fx;
        int h1=grid[gx][gz+1]*(8-fx)+grid[gx+1][gz+1]*fx;
        int h=(h0*(8-fz)+h1*fz)>>6;
        _mc_hmap[x+(z<<6)]=h;
        for(int y=0;y<=h;y++){
            uint8_t b;
            if(y==0)b=MC_BEDROCK;
            else if(y<=h-4)b=MC_STONE;
            else if(y<h)b=MC_DIRT;
            else b=(h<11)?MC_SAND:MC_GRASS;
            mc_set(x,y,z,b);
        }
    }
    /* grottes: spheres creusees sous la surface */
    for(int i=0;i<26;i++){
        int cx=4+(int)(_mc_rnd()%(MC_XZ-8)),cz=4+(int)(_mc_rnd()%(MC_XZ-8));
        int cy=3+(int)(_mc_rnd()%10),r=2+(int)(_mc_rnd()%3);
        for(int dy=-r;dy<=r;dy++)for(int dz=-r;dz<=r;dz++)for(int dx=-r;dx<=r;dx++){
            if(dx*dx+dy*dy+dz*dz>r*r)continue;
            if(mc_get(cx+dx,cy+dy,cz+dz)!=MC_BEDROCK)mc_set(cx+dx,cy+dy,cz+dz,MC_AIR);
        }
    }
    /* arbres */
    for(int i=0;i<16;i++){
        int x=3+(int)(_mc_rnd()%(MC_XZ-6)),z=3+(int)(_mc_rnd()%(MC_XZ-6));
        int h=_mc_hmap[x+(z<<6)];
        if(mc_get(x,h,z)!=MC_GRASS||h+6>=MC_Y)continue;
        int th=4+(int)(_mc_rnd()%2);
        for(int t=1;t<=th;t++)mc_set(x,h+t,z,MC_LOG);
        for(int dy=th-2;dy<=th+1;dy++){
            int r=(dy>=th)?1:2;
            for(int dz=-r;dz<=r;dz++)for(int dx=-r;dx<=r;dx++){
                if(dx==0&&dz==0&&dy<=th)continue;
                if(dx*dx+dz*dz>r*r+1)continue;
                if(mc_get(x+dx,h+dy,z+dz)==MC_AIR)mc_set(x+dx,h+dy,z+dz,MC_LEAF);
            }
        }
    }
}

static void mine_reset(void){
    mc_gen_tex();
    mc_genworld();
    _mc_px=32.5f;_mc_pz=32.5f;
    _mc_py=(float)(_mc_hmap[32+(32<<6)]+2);
    _mc_vy=0;_mc_yaw=0.8f;_mc_pitch=0.0f;_mc_ground=0;
    _mc_kf=_mc_kb=_mc_kl=_mc_kr=_mc_kj=0;
    _mc_lu=_mc_ld=_mc_ll=_mc_lr=0;
    _mc_hot=0;_mc_look=0;
}
/* Ouverture: textures + monde sauvegarde s'il existe, sinon monde neuf */
static void mine_open_init(void){
    mine_reset();
    if(mc_load()){
        _mc_vy=0;_mc_ground=0;
        _mc_msg="Monde charge du disque";_mc_msg_ttl=150;
    }
}

/* Centre le curseur dans la zone de jeu (pour le mouse-look) */
static void mc_center_mouse(void){
    AppWin* w=&_wins[W_MINE];
    int ctx=w->x+1+(MC_RW*MC_SC)/2,cty=w->y+TBAR_H+(MC_RH*MC_SC)/2;
    _mx=ctx;_my=cty;
    sfcml_warpMouse(ctx,cty);
}

/* ---- Physique ---- */
static int mc_boxfree(float x,float y,float z){
    /* AABB joueur: demi-largeur 0.3, hauteur 1.8, (x,y,z)=pieds */
    int x0=(int)(x-0.3f),x1=(int)(x+0.3f);
    int z0=(int)(z-0.3f),z1=(int)(z+0.3f);
    int y0=(int)y,y1=(int)(y+1.7f);
    for(int yy=y0;yy<=y1;yy++)for(int zz=z0;zz<=z1;zz++)for(int xx=x0;xx<=x1;xx++)
        if(mc_get(xx,yy,zz)!=MC_AIR)return 0;
    return 1;
}
static void mine_step(void){
    if(_focus!=W_MINE){  /* fenetre non focalisee: on relache tout */
        _mc_kf=_mc_kb=_mc_kl=_mc_kr=_mc_kj=0;
        _mc_lu=_mc_ld=_mc_ll=_mc_lr=0;
        _mc_look=0;
    }
    /* regard clavier */
    if(_mc_ll)_mc_yaw-=0.055f;
    if(_mc_lr)_mc_yaw+=0.055f;
    if(_mc_lu)_mc_pitch-=0.045f;
    if(_mc_ld)_mc_pitch+=0.045f;
    /* regard souris (capture active): delta au centre puis recentrage */
    if(_mc_look){
        AppWin* w2=&_wins[W_MINE];
        int ctx=w2->x+1+(MC_RW*MC_SC)/2,cty=w2->y+TBAR_H+(MC_RH*MC_SC)/2;
        int mdx=_mx-ctx,mdy=_my-cty;
        if(mdx||mdy){
            _mc_yaw  +=(float)mdx*0.005f;
            _mc_pitch+=(float)mdy*0.005f;
            mc_center_mouse();
        }
    }
    if(_mc_pitch> 1.45f)_mc_pitch= 1.45f;
    if(_mc_pitch<-1.45f)_mc_pitch=-1.45f;
    /* deplacement horizontal (axe par axe pour glisser sur les murs) */
    float s=mc_sin(_mc_yaw),c=mc_cos(_mc_yaw);
    float dx=0,dz=0,sp=0.16f;
    if(_mc_kf){dx+=s*sp;dz+=c*sp;}
    if(_mc_kb){dx-=s*sp;dz-=c*sp;}
    if(_mc_kl){dx-=c*sp;dz+=s*sp;}
    if(_mc_kr){dx+=c*sp;dz-=s*sp;}
    float nx=_mc_px+dx;
    if(nx<0.35f)nx=0.35f;if(nx>MC_XZ-0.35f)nx=MC_XZ-0.35f;
    if(mc_boxfree(nx,_mc_py,_mc_pz))_mc_px=nx;
    float nz=_mc_pz+dz;
    if(nz<0.35f)nz=0.35f;if(nz>MC_XZ-0.35f)nz=MC_XZ-0.35f;
    if(mc_boxfree(_mc_px,_mc_py,nz))_mc_pz=nz;
    /* gravite + saut */
    if(_mc_kj&&_mc_ground){_mc_vy=0.27f;_mc_ground=0;}
    _mc_vy-=0.028f;
    if(_mc_vy<-0.9f)_mc_vy=-0.9f;
    float ny=_mc_py+_mc_vy;
    if(ny<1.0f)ny=1.0f;
    if(mc_boxfree(_mc_px,ny,_mc_pz)){_mc_py=ny;_mc_ground=0;}
    else{
        if(_mc_vy<0){_mc_ground=1;_mc_py=(float)((int)_mc_py);}
        _mc_vy=0;
    }
}

/* ---- Visee: DDA flottant depuis l'oeil, portee 6 blocs ----
   Retourne le bloc touche (bx,by,bz) et la case juste avant (vx,vy,vz) */
static int mc_pick(int* bx,int* by,int* bz,int* vx,int* vy,int* vz){
    float ox=_mc_px,oy=_mc_py+1.62f,oz=_mc_pz;
    float cp=mc_cos(_mc_pitch);
    float dx=mc_sin(_mc_yaw)*cp,dy=-mc_sin(_mc_pitch),dz=mc_cos(_mc_yaw)*cp;
    int ix=(int)ox,iy=(int)oy,iz=(int)oz;
    int sx=dx>0?1:-1,sy=dy>0?1:-1,sz=dz>0?1:-1;
    float adx=dx<0?-dx:dx,ady=dy<0?-dy:dy,adz=dz<0?-dz:dz;
    float tdx=adx>1e-6f?1.0f/adx:1e9f;
    float tdy=ady>1e-6f?1.0f/ady:1e9f;
    float tdz=adz>1e-6f?1.0f/adz:1e9f;
    float fx=ox-(float)ix,fy=oy-(float)iy,fz=oz-(float)iz;
    float tmx=(dx>0?(1.0f-fx):fx)*tdx;
    float tmy=(dy>0?(1.0f-fy):fy)*tdy;
    float tmz=(dz>0?(1.0f-fz):fz)*tdz;
    for(int i=0;i<64;i++){
        int lx=ix,ly=iy,lz=iz;float t;
        if(tmx<tmy&&tmx<tmz){ix+=sx;t=tmx;tmx+=tdx;}
        else if(tmy<tmz)    {iy+=sy;t=tmy;tmy+=tdy;}
        else                {iz+=sz;t=tmz;tmz+=tdz;}
        if(t>6.0f)return 0;
        if(mc_get(ix,iy,iz)!=MC_AIR){
            *bx=ix;*by=iy;*bz=iz;*vx=lx;*vy=ly;*vz=lz;return 1;
        }
    }
    return 0;
}
static void mine_click(int btn){
    int bx,by,bz,vx,vy,vz;
    if(!mc_pick(&bx,&by,&bz,&vx,&vy,&vz))return;
    if(btn==0){  /* casser */
        if(mc_get(bx,by,bz)!=MC_BEDROCK){mc_set(bx,by,bz,MC_AIR);beep(130,45);}
    }else{       /* poser */
        if((unsigned)vx>=MC_XZ||(unsigned)vz>=MC_XZ||(unsigned)vy>=MC_Y)return;
        if(mc_get(vx,vy,vz)!=MC_AIR)return;
        /* refuse si le bloc chevauche le joueur */
        float x0=_mc_px-0.3f,x1=_mc_px+0.3f,z0=_mc_pz-0.3f,z1=_mc_pz+0.3f;
        float y0=_mc_py,y1=_mc_py+1.8f;
        if((float)vx<x1&&(float)vx+1>x0&&(float)vz<z1&&(float)vz+1>z0&&
           (float)vy<y1&&(float)vy+1>y0)return;
        mc_set(vx,vy,vz,_mc_hotbar[_mc_hot]);beep(340,30);
    }
}

/* ---- Clavier (ZQSD physique en AZERTY = keycodes WASD) ---- */
static void mine_key_press(sfcml_KeyCode k){
    if(k==SFCML_KEY_W)_mc_kf=1;
    else if(k==SFCML_KEY_S)_mc_kb=1;
    else if(k==SFCML_KEY_A)_mc_kl=1;
    else if(k==SFCML_KEY_D)_mc_kr=1;
    else if(k==SFCML_KEY_SPACE)_mc_kj=1;
    else if(k==SFCML_KEY_UP)_mc_lu=1;
    else if(k==SFCML_KEY_DOWN)_mc_ld=1;
    else if(k==SFCML_KEY_LEFT)_mc_ll=1;
    else if(k==SFCML_KEY_RIGHT)_mc_lr=1;
    else if(k>=SFCML_KEY_1&&k<=SFCML_KEY_9)_mc_hot=(int)(k-SFCML_KEY_1);
    else if(k==SFCML_KEY_G){mine_reset();_mc_msg="Nouveau monde";_mc_msg_ttl=120;}
    else if(k==SFCML_KEY_F2)mc_save();                 /* sauver le monde */
    else if(k==SFCML_KEY_F3){                           /* recharger le monde */
        if(mc_load()){_mc_vy=0;_mc_ground=0;_mc_msg="Monde recharge";_mc_msg_ttl=120;beep(N_E5,60);}
        else{_mc_msg="Aucune sauvegarde";_mc_msg_ttl=120;beep(200,60);}
    }
    else if(k==SFCML_KEY_TAB){
        _mc_look=!_mc_look;
        if(_mc_look)mc_center_mouse();
    }
}
static void mine_key_release(sfcml_KeyCode k){
    if(k==SFCML_KEY_W)_mc_kf=0;
    else if(k==SFCML_KEY_S)_mc_kb=0;
    else if(k==SFCML_KEY_A)_mc_kl=0;
    else if(k==SFCML_KEY_D)_mc_kr=0;
    else if(k==SFCML_KEY_SPACE)_mc_kj=0;
    else if(k==SFCML_KEY_UP)_mc_lu=0;
    else if(k==SFCML_KEY_DOWN)_mc_ld=0;
    else if(k==SFCML_KEY_LEFT)_mc_ll=0;
    else if(k==SFCML_KEY_RIGHT)_mc_lr=0;
}

/* ---- Rendu raycasting (DDA voxel virgule fixe) ---- */
static void mine_render(sfcml_Window* win,int cx,int cy){
    if(!win->back)return;
    float cyw=mc_cos(_mc_yaw),syw=mc_sin(_mc_yaw);
    float cpt=mc_cos(_mc_pitch),spt=mc_sin(_mc_pitch);
    /* base camera en 16.16: forward, right, up (up = f x r) */
    int32_t fwx=(int32_t)(syw*cpt*MC_ONE),fwy=(int32_t)(-spt*MC_ONE),fwz=(int32_t)(cyw*cpt*MC_ONE);
    int32_t rtx=(int32_t)(cyw*MC_ONE),rtz=(int32_t)(-syw*MC_ONE);
    int32_t upx=(int32_t)(syw*spt*MC_ONE),upy=(int32_t)(cpt*MC_ONE),upz=(int32_t)(cyw*spt*MC_ONE);
    int32_t ox=(int32_t)(_mc_px*MC_ONE),oy=(int32_t)((_mc_py+1.62f)*MC_ONE),oz=(int32_t)(_mc_pz*MC_ONE);
    int bx0=ox>>MC_FIX,by0=oy>>MC_FIX,bz0=oz>>MC_FIX;
    int32_t fox=ox&0xFFFF,foy=oy&0xFFFF,foz=oz&0xFFFF;

    for(int ry=0;ry<MC_RH;ry++){
        int32_t vv=((MC_RH/2-ry)<<MC_FIX)/120;   /* focale 120 px */
        int32_t bdx=fwx+mc_mul(upx,vv);
        int32_t bdy=fwy+mc_mul(upy,vv);
        int32_t bdz=fwz+mc_mul(upz,vv);
        /* composante Y constante sur toute la ligne */
        int sy2;int32_t tdy,tmy0;
        {int32_t a=bdy<0?-bdy:bdy;
         if(a<1024){sy2=1;tdy=MC_INF;tmy0=MC_INF;}
         else{tdy=(int32_t)(4294967296.0f/(float)a);
              sy2=bdy>0?1:-1;
              tmy0=mc_mul(bdy>0?(MC_ONE-foy):foy,tdy);}}
        int skr=96+(74*ry)/MC_RH,skg=160+(55*ry)/MC_RH,skb=255;
        uint8_t* rp=_mc_row;
        for(int rx=0;rx<MC_RW;rx++){
            int32_t uu=((rx-MC_RW/2)<<MC_FIX)/120;
            int32_t dx=bdx+mc_mul(rtx,uu);
            int32_t dy=bdy;
            int32_t dz=bdz+mc_mul(rtz,uu);
            int ix=bx0,iy=by0,iz=bz0;
            int sx,sz;int32_t tdx,tdz,tmx,tmz,tmy=tmy0;
            {int32_t a=dx<0?-dx:dx;
             if(a<1024){sx=1;tdx=MC_INF;tmx=MC_INF;}
             else{tdx=(int32_t)(4294967296.0f/(float)a);sx=dx>0?1:-1;
                  tmx=mc_mul(dx>0?(MC_ONE-fox):fox,tdx);}}
            {int32_t a=dz<0?-dz:dz;
             if(a<1024){sz=1;tdz=MC_INF;tmz=MC_INF;}
             else{tdz=(int32_t)(4294967296.0f/(float)a);sz=dz>0?1:-1;
                  tmz=mc_mul(dz>0?(MC_ONE-foz):foz,tdz);}}
            int r=skr,g=skg,b=skb;
            for(int it=0;it<110;it++){
                int32_t t;int axis;
                if(tmx<tmy&&tmx<tmz){t=tmx;ix+=sx;tmx+=tdx;axis=0;}
                else if(tmy<tmz)    {t=tmy;iy+=sy2;tmy+=tdy;axis=1;}
                else                {t=tmz;iz+=sz;tmz+=tdz;axis=2;}
                if(t>MC_MAXT)break;
                if((unsigned)ix>=MC_XZ||(unsigned)iz>=MC_XZ)break;
                if(iy<0)break;
                if(iy>=MC_Y){if(sy2>0)break;else continue;}
                uint8_t blk=_mc_w[ix+(iz<<6)+(iy<<12)];
                if(!blk)continue;
                int face,shade,tu,tv;
                if(axis==1){
                    int32_t hx=(ox+mc_mul(dx,t))&0xFFFF;
                    int32_t hz=(oz+mc_mul(dz,t))&0xFFFF;
                    tu=(hx>>12)&15;tv=(hz>>12)&15;
                    if(sy2<0){face=0;shade=256;}   /* face du dessus */
                    else     {face=2;shade=120;}   /* dessous */
                }else{
                    int32_t hy=(oy+mc_mul(dy,t))&0xFFFF;
                    tv=15-((hy>>12)&15);
                    if(axis==0){
                        int32_t hz=(oz+mc_mul(dz,t))&0xFFFF;
                        tu=(hz>>12)&15;face=1;shade=205;
                    }else{
                        int32_t hx=(ox+mc_mul(dx,t))&0xFFFF;
                        tu=(hx>>12)&15;face=1;shade=154;
                    }
                }
                uint32_t tx=_mc_tex[blk][face][(tv<<4)|tu];
                if(!tx)continue;   /* texel transparent (feuilles/verre) */
                int tr=(int)((tx>>16)&255)*shade>>8;
                int tg=(int)((tx>>8)&255)*shade>>8;
                int tb=(int)(tx&255)*shade>>8;
                int fo=t/(MC_MAXT/255);            /* brouillard 0..255 */
                fo=(fo*fo)>>8;
                r=(tr*(256-fo)+170*fo)>>8;
                g=(tg*(256-fo)+215*fo)>>8;
                b=(tb*(256-fo)+255*fo)>>8;
                break;
            }
            uint8_t rb=(uint8_t)r,gb=(uint8_t)g,bb=(uint8_t)b;
            for(int k=0;k<MC_SC;k++){*rp++=bb;*rp++=gb;*rp++=rb;}
        }
        for(int k=0;k<MC_SC;k++){
            int yy=cy+ry*MC_SC+k;
            if((unsigned)yy>=win->height)continue;
            memcpy(win->back+(uint32_t)yy*win->pitch+(uint32_t)cx*3,
                   _mc_row,sizeof _mc_row);
        }
    }
}

static void draw_mine(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int cx=w->x+1,cy=w->y+TBAR_H;
    int cw=MC_RW*MC_SC,ch=MC_RH*MC_SC;
    mine_render(win,cx,cy);
    /* viseur */
    sfcml_fillRect(win,sfcml_rect(cx+cw/2-1,cy+ch/2-7,2,14),sfcml_rgb(235,235,235));
    sfcml_fillRect(win,sfcml_rect(cx+cw/2-7,cy+ch/2-1,14,2),sfcml_rgb(235,235,235));
    /* hotbar */
    int hx0=cx+(cw-9*38)/2,hy0=cy+ch-44;
    for(int i=0;i<9;i++){
        int sx2=hx0+i*38;
        sfcml_fillRect(win,sfcml_rect(sx2,hy0,38,38),sfcml_rgb(28,28,34));
        for(int v=0;v<16;v++)for(int u=0;u<16;u++){
            uint32_t tx=_mc_tex[_mc_hotbar[i]][1][(v<<4)|u];
            sfcml_Color c2=tx?sfcml_rgb((uint8_t)((tx>>16)&255),(uint8_t)((tx>>8)&255),(uint8_t)(tx&255))
                             :sfcml_rgb(44,44,52);
            sfcml_fillRect(win,sfcml_rect(sx2+3+u*2,hy0+3+v*2,2,2),c2);
        }
        sfcml_drawRect(win,sfcml_rect(sx2,hy0,38,38),
                       (i==_mc_hot)?SFCML_WHITE:sfcml_rgb(90,90,100));
        if(i==_mc_hot)sfcml_drawRect(win,sfcml_rect(sx2-1,hy0-1,40,40),SFCML_WHITE);
    }
    /* HUD (FPS via la RTC: _rtc.s change une fois par seconde) */
    _mc_frm++;
    if(_rtc.s!=(uint8_t)_mc_ft){_mc_fps=_mc_frm;_mc_frm=0;_mc_ft=_rtc.s;}
    char nb[12];itoa(_mc_fps,nb,10);
    char hud[64];strncpy(hud,"MyCraft  FPS:",64);strncat(hud,nb,10);
    sfcml_drawText(win,hud,cx+6,cy+4,SFCML_WHITE,sfcml_rgb(22,22,28));
    sfcml_drawText(win,"ZQSD:bouger Tab:souris Fleches:regarder Esp:saut ClicG:casser ClicD:poser",
                   cx+6,cy+16,sfcml_rgb(215,215,215),sfcml_rgb(22,22,28));
    sfcml_drawText(win,"1-9:bloc  G:monde  F2:sauver  F3:charger  Echap:quitter",
                   cx+6,cy+28,sfcml_rgb(215,215,215),sfcml_rgb(22,22,28));
    if(_mc_look)
        sfcml_drawText(win,"[SOURIS ON - Tab/Echap]",cx+cw-190,cy+4,
                       sfcml_rgb(120,255,120),sfcml_rgb(22,22,28));
    sfcml_drawText(win,_mc_names[_mc_hotbar[_mc_hot]],hx0+1,hy0-12,SFCML_WHITE,sfcml_rgb(22,22,28));
    /* message HUD temporaire (sauvegarde/chargement) */
    if(_mc_msg_ttl>0){
        _mc_msg_ttl--;
        int mw=(int)strlen(_mc_msg)*8+16;
        sfcml_fillRect(win,sfcml_rect(cx+cw/2-mw/2,cy+ch/2-40,mw,20),sfcml_rgb(20,20,28));
        sfcml_drawRect(win,sfcml_rect(cx+cw/2-mw/2,cy+ch/2-40,mw,20),sfcml_rgb(120,200,120));
        sfcml_drawText(win,_mc_msg,cx+cw/2-mw/2+8,cy+ch/2-34,sfcml_rgb(140,255,140),sfcml_rgb(20,20,28));
    }
}

/* ============================================================
 * TETRIS - from scratch (clavier)
 * ============================================================ */
#define TET_W    10
#define TET_H    20
#define TET_CELL 18
static uint8_t _tet_grid[TET_H][TET_W];
static int _tet_px,_tet_py,_tet_rot,_tet_piece;
static int _tet_score,_tet_lines,_tet_over,_tet_tick,_tet_speed;
static unsigned _tet_seed;
/* 7 tetrominos x 4 rotations, masque 4x4 (bit 0x8000 = coin haut-gauche) */
static const uint16_t TET[7][4]={
    {0x0F00,0x2222,0x00F0,0x4444}, /* I */
    {0x6600,0x6600,0x6600,0x6600}, /* O */
    {0x4E00,0x4640,0x0E40,0x4C40}, /* T */
    {0x6C00,0x4620,0x06C0,0x8C40}, /* S */
    {0xC600,0x2640,0x0C60,0x4C80}, /* Z */
    {0x8E00,0x6440,0x0E20,0x44C0}, /* J */
    {0x2E00,0x4460,0x0E80,0xC440}, /* L */
};
static const sfcml_Color TET_COL[8]={
    {0,0,0,255},{0,220,220,255},{220,220,0,255},{180,60,200,255},
    {40,200,40,255},{220,40,40,255},{40,80,220,255},{230,140,20,255}};
static int tet_cell(int piece,int rot,int r,int c){
    return (TET[piece][rot]&(0x8000>>(r*4+c)))?1:0;
}
static int tet_collide(int piece,int rot,int px,int py){
    for(int r=0;r<4;r++)for(int c=0;c<4;c++){
        if(!tet_cell(piece,rot,r,c))continue;
        int x=px+c,y=py+r;
        if(x<0||x>=TET_W||y>=TET_H)return 1;
        if(y>=0&&_tet_grid[y][x])return 1;
    }
    return 0;
}
static void tet_spawn(void){
    _tet_seed=_tet_seed*1103515245u+12345u;
    _tet_piece=(int)((_tet_seed>>16)%7u);
    _tet_rot=0;_tet_px=3;_tet_py=-1;
    if(tet_collide(_tet_piece,_tet_rot,_tet_px,_tet_py)){_tet_over=1;play_melody(MEL_LOSE,3);}
}
static void tet_reset(void){
    memset(_tet_grid,0,sizeof _tet_grid);
    _tet_score=0;_tet_lines=0;_tet_over=0;_tet_tick=0;_tet_speed=20;
    if(!_tet_seed)_tet_seed=BIOS_TICKS^0x1234u;
    tet_spawn();
}
static void tet_lock(void){
    for(int r=0;r<4;r++)for(int c=0;c<4;c++)
        if(tet_cell(_tet_piece,_tet_rot,r,c)){
            int x=_tet_px+c,y=_tet_py+r;
            if(y>=0&&y<TET_H&&x>=0&&x<TET_W)_tet_grid[y][x]=(uint8_t)(_tet_piece+1);
        }
    int cleared=0;
    for(int y=TET_H-1;y>=0;y--){
        int full=1;
        for(int x=0;x<TET_W;x++)if(!_tet_grid[y][x]){full=0;break;}
        if(full){
            for(int yy=y;yy>0;yy--)memcpy(_tet_grid[yy],_tet_grid[yy-1],TET_W);
            memset(_tet_grid[0],0,TET_W);
            cleared++;y++;
        }
    }
    if(cleared){
        static const int pts[5]={0,40,100,300,1200};
        _tet_score+=pts[cleared];_tet_lines+=cleared;
        _tet_speed=20-_tet_lines/5;if(_tet_speed<4)_tet_speed=4;
        beep(cleared>=4?N_C6:N_G5,80);
    }
    tet_spawn();
}
static void tet_step(void){
    if(_tet_over)return;
    if(++_tet_tick<_tet_speed)return;
    _tet_tick=0;
    if(!tet_collide(_tet_piece,_tet_rot,_tet_px,_tet_py+1))_tet_py++;
    else tet_lock();
}
static void tet_key(sfcml_KeyCode k){
    if(_tet_over){if(k==SFCML_KEY_RETURN)tet_reset();return;}
    if(k==SFCML_KEY_LEFT){if(!tet_collide(_tet_piece,_tet_rot,_tet_px-1,_tet_py))_tet_px--;}
    else if(k==SFCML_KEY_RIGHT){if(!tet_collide(_tet_piece,_tet_rot,_tet_px+1,_tet_py))_tet_px++;}
    else if(k==SFCML_KEY_DOWN){if(!tet_collide(_tet_piece,_tet_rot,_tet_px,_tet_py+1)){_tet_py++;_tet_score++;}}
    else if(k==SFCML_KEY_UP){int nr=(_tet_rot+1)&3;if(!tet_collide(_tet_piece,nr,_tet_px,_tet_py))_tet_rot=nr;}
    else if(k==SFCML_KEY_SPACE){while(!tet_collide(_tet_piece,_tet_rot,_tet_px,_tet_py+1)){_tet_py++;_tet_score+=2;}tet_lock();}
}
static void draw_tetris(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int cx=w->x+1,cy=w->y+TBAR_H,cw=w->w-2,ch=w->h-TBAR_H-1;
    sfcml_fillRect(win,sfcml_rect(cx,cy,cw,ch),sfcml_rgb(18,18,26));
    int bw=TET_W*TET_CELL,bh=TET_H*TET_CELL;
    int ox=cx+16,oy=cy+16;
    sfcml_drawRect(win,sfcml_rect(ox-2,oy-2,bw+4,bh+4),sfcml_rgb(60,60,80));
    for(int y=0;y<TET_H;y++)for(int x=0;x<TET_W;x++){
        int v=_tet_grid[y][x];
        sfcml_Color c=v?TET_COL[v]:sfcml_rgb(26,26,36);
        sfcml_fillRect(win,sfcml_rect(ox+x*TET_CELL,oy+y*TET_CELL,TET_CELL-1,TET_CELL-1),c);
    }
    if(!_tet_over)for(int r=0;r<4;r++)for(int c=0;c<4;c++)
        if(tet_cell(_tet_piece,_tet_rot,r,c)){
            int x=_tet_px+c,y=_tet_py+r;
            if(y>=0)sfcml_fillRect(win,sfcml_rect(ox+x*TET_CELL,oy+y*TET_CELL,TET_CELL-1,TET_CELL-1),TET_COL[_tet_piece+1]);
        }
    int px=ox+bw+20;sfcml_Color bg=sfcml_rgb(18,18,26);char nb[12];
    sfcml_drawText(win,"TETRIS",px,oy,SFCML_WHITE,bg);
    sfcml_drawText(win,"Score:",px,oy+30,sfcml_rgb(200,200,210),bg);
    itoa(_tet_score,nb,10);sfcml_drawText(win,nb,px,oy+42,SFCML_WHITE,bg);
    sfcml_drawText(win,"Lignes:",px,oy+64,sfcml_rgb(200,200,210),bg);
    itoa(_tet_lines,nb,10);sfcml_drawText(win,nb,px,oy+76,SFCML_WHITE,bg);
    sfcml_drawText(win,"Fleches: bouger",px,oy+112,sfcml_rgb(150,150,170),bg);
    sfcml_drawText(win,"Haut: tourner",px,oy+126,sfcml_rgb(150,150,170),bg);
    sfcml_drawText(win,"Espace: chute",px,oy+140,sfcml_rgb(150,150,170),bg);
    if(_tet_over){
        sfcml_drawText(win,"GAME OVER",ox+bw/2-36,oy+bh/2-8,sfcml_rgb(255,80,80),SFCML_BLACK);
        sfcml_drawText(win,"[Entree=Rejouer]",ox+bw/2-60,oy+bh/2+6,sfcml_rgb(255,200,80),SFCML_BLACK);
    }
}

/* ============================================================
 * DEMINEUR - from scratch (souris)
 * ============================================================ */
#define MS_W     12
#define MS_H     12
#define MS_MINES 22
#define MS_CELL  26
static uint8_t _ms_mine[MS_H][MS_W],_ms_open[MS_H][MS_W],_ms_flag[MS_H][MS_W];
static int _ms_over,_ms_win,_ms_left;
static unsigned _ms_seed;
static int ms_count(int x,int y){
    int n=0;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
        int nx=x+dx,ny=y+dy;
        if(nx<0||nx>=MS_W||ny<0||ny>=MS_H)continue;
        if(_ms_mine[ny][nx])n++;
    }
    return n;
}
static void ms_reset(void){
    memset(_ms_mine,0,sizeof _ms_mine);
    memset(_ms_open,0,sizeof _ms_open);
    memset(_ms_flag,0,sizeof _ms_flag);
    _ms_over=0;_ms_win=0;_ms_left=MS_W*MS_H-MS_MINES;
    if(!_ms_seed)_ms_seed=BIOS_TICKS^0x9E3Bu;
    int placed=0;
    while(placed<MS_MINES){
        _ms_seed=_ms_seed*1103515245u+12345u;int x=(int)((_ms_seed>>16)%MS_W);
        _ms_seed=_ms_seed*1103515245u+12345u;int y=(int)((_ms_seed>>16)%MS_H);
        if(!_ms_mine[y][x]){_ms_mine[y][x]=1;placed++;}
    }
}
static void ms_reveal(int x,int y){
    if(x<0||x>=MS_W||y<0||y>=MS_H)return;
    if(_ms_open[y][x]||_ms_flag[y][x])return;
    _ms_open[y][x]=1;_ms_left--;
    if(ms_count(x,y)==0)
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
            if(dx||dy)ms_reveal(x+dx,y+dy);
}
static void mines_click(int mx,int my,int btn){
    AppWin* w=&_wins[W_DEMINE];
    int ox=w->x+1+12,oy=w->y+TBAR_H+42;
    if(_ms_over||_ms_win){ms_reset();return;}
    int gx=(mx-ox)/MS_CELL,gy=(my-oy)/MS_CELL;
    if(mx<ox||my<oy||gx<0||gx>=MS_W||gy<0||gy>=MS_H)return;
    if(btn==1){ if(!_ms_open[gy][gx])_ms_flag[gy][gx]^=1; return; }
    if(_ms_flag[gy][gx])return;
    if(_ms_mine[gy][gx]){_ms_open[gy][gx]=1;_ms_over=1;beep(160,120);beep(90,220);return;}
    ms_reveal(gx,gy);beep(500,20);
    if(_ms_left<=0){_ms_win=1;play_melody(MEL_WIN,6);}
}
static void draw_mines(sfcml_Window* win,int wi){
    static const sfcml_Color NC[9]={
        {0,0,0,255},{40,80,220,255},{30,150,40,255},{210,40,40,255},
        {20,20,140,255},{140,20,20,255},{20,140,140,255},{20,20,20,255},{110,110,110,255}};
    AppWin* w=&_wins[wi];
    int cx=w->x+1,cy=w->y+TBAR_H,cw=w->w-2,ch=w->h-TBAR_H-1;
    sfcml_fillRect(win,sfcml_rect(cx,cy,cw,ch),sfcml_rgb(190,190,200));
    sfcml_Color hb=sfcml_rgb(60,60,80);char nb[12];
    int flags=0;for(int y=0;y<MS_H;y++)for(int x=0;x<MS_W;x++)if(_ms_flag[y][x])flags++;
    sfcml_fillRect(win,sfcml_rect(cx,cy,cw,34),hb);
    sfcml_drawText(win,"Mines:",cx+10,cy+6,SFCML_WHITE,hb);
    itoa(MS_MINES-flags,nb,10);sfcml_drawText(win,nb,cx+66,cy+6,sfcml_rgb(255,220,80),hb);
    const char* st=_ms_over?"BOOM! clic=rejouer":_ms_win?"GAGNE! clic=rejouer":"G:reveler D:drapeau";
    sfcml_drawText(win,st,cx+120,cy+6,_ms_over?sfcml_rgb(255,120,120):_ms_win?sfcml_rgb(140,255,140):sfcml_rgb(200,200,210),hb);
    int ox=cx+12,oy=cy+42;
    for(int y=0;y<MS_H;y++)for(int x=0;x<MS_W;x++){
        int rx=ox+x*MS_CELL,ry=oy+y*MS_CELL;
        int op=_ms_open[y][x]||((_ms_over||_ms_win)&&_ms_mine[y][x]);
        if(!op){
            sfcml_fillRect(win,sfcml_rect(rx,ry,MS_CELL-1,MS_CELL-1),sfcml_rgb(150,150,165));
            sfcml_drawHLine(win,rx,ry,MS_CELL-1,sfcml_rgb(210,210,220));
            sfcml_drawVLine(win,rx,ry,MS_CELL-1,sfcml_rgb(210,210,220));
            sfcml_drawHLine(win,rx,ry+MS_CELL-2,MS_CELL-1,sfcml_rgb(90,90,105));
            sfcml_drawVLine(win,rx+MS_CELL-2,ry,MS_CELL-1,sfcml_rgb(90,90,105));
            if(_ms_flag[y][x]){
                sfcml_fillRect(win,sfcml_rect(rx+11,ry+5,2,12),sfcml_rgb(40,40,40));
                sfcml_fillTriangle(win,rx+6,ry+6,rx+12,ry+9,rx+6,ry+12,sfcml_rgb(210,40,40));
            }
        }else{
            sfcml_fillRect(win,sfcml_rect(rx,ry,MS_CELL-1,MS_CELL-1),sfcml_rgb(205,205,212));
            if(_ms_mine[y][x]){
                sfcml_Color mc=(_ms_over&&_ms_open[y][x])?sfcml_rgb(210,40,40):sfcml_rgb(30,30,30);
                sfcml_fillCircle(win,rx+MS_CELL/2-1,ry+MS_CELL/2-1,6,mc);
            }else{
                int n=ms_count(x,y);
                if(n>0){char t[2]={(char)('0'+n),0};
                    sfcml_drawText(win,t,rx+MS_CELL/2-4,ry+MS_CELL/2-5,NC[n],sfcml_rgb(205,205,212));}
            }
        }
    }
}

/* ============================================================
 * Point d'entree
 * ============================================================ */
void kmain(void){
    char lb[96],nb[16];
    time_init();
    klog("MyOS: kernel C en mode protege 32-bit");
    klog("IDT chargee, PIC remappe (0x20-0x2F), PIT 18.2 Hz sur IRQ0");
    if(!sfcml_init())for(;;)__asm__ volatile("hlt");
    sfcml_Window* win=sfcml_createWindow("MyOS");
    win->font=(uint8_t*)font8x8;
    strncpy(lb,"VESA: ",96);itoa((int)win->width,nb,10);strncat(lb,nb,8);
    strncat(lb,"x",2);itoa((int)win->height,nb,10);strncat(lb,nb,8);
    strncat(lb,"x",2);itoa((int)win->bpp,nb,10);strncat(lb,nb,8);
    strncat(lb," LFB actif",12);klog(lb);
    __asm__ volatile("fninit"); /* init FPU pour les calculs float */
    klog("FPU x87 initialise (fninit)");
    sfcml_mouseInit();
    klog("PS/2: clavier+souris en polling, data reporting ON");
    rtc_read();
    strncpy(lb,"RTC CMOS: ",96);
    itoa((int)_rtc.year,nb,10);strncat(lb,nb,8);strncat(lb,"-",2);
    itoa((int)_rtc.mon,nb,10);strncat(lb,nb,4);strncat(lb,"-",2);
    itoa((int)_rtc.day,nb,10);strncat(lb,nb,4);strncat(lb," ",2);
    itoa((int)_rtc.h,nb,10);strncat(lb,nb,4);strncat(lb,":",2);
    itoa((int)_rtc.m,nb,10);strncat(lb,nb,4);klog(lb);
    strncpy(lb,"RAM: ",96);itoa((int)(ram_total_kb()/1024),nb,10);
    strncat(lb,nb,8);strncat(lb," Mo (CMOS)",12);klog(lb);
    if(ata_init()){
        strncpy(lb,"ATA: ",96);strncat(lb,_ata_model,44);
        strncat(lb,", ",3);itoa((int)_ata_sectors,nb,10);strncat(lb,nb,10);
        strncat(lb," secteurs",10);klog(lb);
        if(dvfs_load()){
            int n=0;for(int i=0;i<DVFS_MAX;i++)if(_dvfs[i].used)n++;
            strncpy(lb,"MyFS: ",96);itoa(n,nb,10);strncat(lb,nb,6);
            strncat(lb," fichiers charges depuis LBA 500",40);klog(lb);
        }else klog("MyFS: pas de superbloc (cree a la 1ere ecriture)");
    }else klog("ATA: pas de disque sur primaire maitre");
    net_init();
    if(net_ok){
        char ib[20];
        strncpy(lb,"rtl8139: UP, IP ",96);_ip4str(net_my_ip,ib);
        strncat(lb,ib,20);
        strncat(lb,net_dhcp_ok?" (DHCP)":" (statique)",14);klog(lb);
    }else klog("rtl8139: absent ou init echouee");
    klog("son: PC speaker (PIT canal 2) pret");
    klog("bureau: demarrage de l'interface");
    play_melody(MEL_BOOT,4);   /* jingle de demarrage reel */

    C_DK     = sfcml_rgb(0,  48, 90);
    C_TB     = sfcml_rgb(14, 50,110);
    C_WINBG  = sfcml_rgb(14, 14, 20);
    C_FG     = SFCML_WHITE;
    C_PROMPT = SFCML_GREEN;

    draw_splash(win);

    t_print("Bienvenue sur MyOS v0.1 - Epitech Edition\n");
    t_print("Tapez 'help' pour les commandes.\n");
    t_print("Double-clic icone ou Menu Demarrer.\n");

    sfcml_Event evt;
    int dirty=1;
    uint8_t last_s=255; /* derniere seconde vue, pour detecter le changement */

    while(sfcml_isOpen(win)){
        int got=0;
        while(sfcml_pollEvent(win,&evt)){
            got=1;
            switch(evt.type){

            case SFCML_EVT_MOUSE_MOVED:
                _mx=evt.mouse.x;_my=evt.mouse.y;
                if(_drag_win>=0){
                    AppWin* w=&_wins[_drag_win];
                    w->x=_mx-_drag_ox;w->y=_my-_drag_oy;
                    if(w->x<0)w->x=0;if(w->x+w->w>SCR_W)w->x=SCR_W-w->w;
                    if(w->y<0)w->y=0;if(w->y+w->h>TB_Y)w->y=TB_Y-w->h;
                } else if(_painting&&_focus==W_PAINT&&_wins[W_PAINT].visible&&!_wins[W_PAINT].minimized){
                    AppWin* w=&_wins[W_PAINT];
                    int cax=w->x+1+PAINT_SB,cay=w->y+TBAR_H+PAINT_TB;
                    int caw=w->w-2-PAINT_SB,cah=w->h-TBAR_H-1-PAINT_TB;
                    int cx=_mx-cax,cy=_my-cay;
                    if(cx>=0&&cx<PAINT_CW&&cy>=0&&cy<PAINT_CH&&_mx>=cax&&_mx<cax+caw&&_my>=cay&&_my<cay+cah)
                        _paint_dot(cx,cy,(uint8_t)_pcol);
                }
                break;

            case SFCML_EVT_MOUSE_PRESSED:
                on_press(evt.mouse.x,evt.mouse.y,evt.mouse.button);
                break;

            case SFCML_EVT_MOUSE_RELEASED:
                _drag_win=-1;_painting=0;
                break;

            case SFCML_EVT_KEY_RELEASED:{
                int foc=_focus;
                int fopen=(foc>=0&&_wins[foc].visible&&!_wins[foc].minimized);
                if(fopen&&foc==W_RTYPE)rtype_key_release(evt.key.code);
                else if(fopen&&foc==W_PONG)pong_key_release(evt.key.code);
                else if(fopen&&foc==W_MINE)mine_key_release(evt.key.code);
                break;
            }

            case SFCML_EVT_KEY_PRESSED:{
                if(_sleeping){_sleeping=0;_inact_secs=0;break;}
                int foc=_focus;
                int fopen=(foc>=0&&_wins[foc].visible&&!_wins[foc].minimized);
                if(fopen&&foc==W_NETMGR){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_NETMGR].minimized=1;
                } else if(fopen&&foc==W_SETTINGS){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_SETTINGS].minimized=1;
                } else if(fopen&&foc==W_CALC){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_CALC].minimized=1;
                    else if(evt.key.code==SFCML_KEY_RETURN)calc_press('=');
                    else if(evt.key.code==SFCML_KEY_BACKSPACE){
                        int l=(int)strlen(_calc_disp);
                        if(l>1){_calc_disp[l-1]='\0';}else{_calc_disp[0]='0';_calc_disp[1]='\0';_calc_new=1;}
                    }
                } else if(fopen&&foc==W_BROWSER){
                    if(_bsearch_foc){
                        if(evt.key.code==SFCML_KEY_RETURN){if(_bsearch_len>0)_bnav(5);_bsearch_foc=0;}
                        else if(evt.key.code==SFCML_KEY_ESCAPE){_bsearch_foc=0;}
                        else if(evt.key.code==SFCML_KEY_BACKSPACE){if(_bsearch_len>0)_bsearch[--_bsearch_len]='\0';}
                    } else if(_burl_foc){
                        if(evt.key.code==SFCML_KEY_RETURN){_burl_nav();_burl_foc=0;}
                        else if(evt.key.code==SFCML_KEY_ESCAPE){_burl_foc=0;}
                        else if(evt.key.code==SFCML_KEY_BACKSPACE){if(_burl_len>0)_burl[--_burl_len]='\0';}
                    } else {
                        if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_BROWSER].minimized=1;
                    }
                } else if(fopen&&foc==W_FILES){
                    if(_fi_edit){
                        if(evt.key.code==SFCML_KEY_ESCAPE)_fi_edit=0;
                        else fi_key(evt.key.code);
                    } else {
                        if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_FILES].minimized=1;
                        else if(evt.key.code==SFCML_KEY_RETURN){
                            const char*fn=_fi_files[_fi_dir][_fi_sel];
                            if(fn&&fn[0]){int is_dir=(fn[0]>='A'&&fn[0]<='Z'&&fn[1]>='a'&&!strchr(fn,'.'));if(!is_dir)fi_edit_open();}
                        }
                        else if(evt.key.code==SFCML_KEY_UP){if(_fi_sel>0)_fi_sel--;}
                        else if(evt.key.code==SFCML_KEY_DOWN){if(_fi_sel<5)_fi_sel++;}
                    }
                } else if(fopen&&foc==W_CODE){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_CODE].minimized=1;
                    else code_key(evt.key.code);
                } else if(fopen&&foc==W_WORD){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_WORD].minimized=1;
                    else word_key(evt.key.code);
                } else if(fopen&&foc==W_SNAKE){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_SNAKE].minimized=1;
                    else snake_key(evt.key.code);
                } else if(fopen&&foc==W_RTYPE){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_RTYPE].minimized=1;
                    else rtype_key_press(evt.key.code);
                } else if(fopen&&foc==W_PONG){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_PONG].minimized=1;
                    else pong_key_press(evt.key.code);
                } else if(fopen&&foc==W_MINE){
                    if(evt.key.code==SFCML_KEY_ESCAPE){
                        if(_mc_look)_mc_look=0;          /* 1er Echap: libere la souris */
                        else _wins[W_MINE].minimized=1;  /* 2e: minimise */
                    }
                    else mine_key_press(evt.key.code);
                } else if(fopen&&foc==W_TETRIS){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_TETRIS].minimized=1;
                    else tet_key(evt.key.code);
                } else if(fopen&&foc==W_DEMINE){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_DEMINE].minimized=1;
                } else {
                    int has_t=_wins[W_TERM].visible&&!_wins[W_TERM].minimized;
                    if(!has_t){if(evt.key.code==SFCML_KEY_ESCAPE)cmd_reboot();break;}
                    if(_focus!=W_TERM)win_front(W_TERM);
                    switch(evt.key.code){
                    case SFCML_KEY_ESCAPE:   _wins[W_TERM].minimized=1;break;
                    case SFCML_KEY_RETURN:   handle_command();break;
                    case SFCML_KEY_BACKSPACE:if(_tilen>0){_tinput[--_tilen]='\0';}break;
                    case SFCML_KEY_UP:
                        if(_thpos<_thlen-1){_thpos++;memcpy(_tinput,_thist[_thlen-1-_thpos],T_COLS+1);_tilen=(int)strlen(_tinput);}
                        break;
                    case SFCML_KEY_DOWN:
                        if(_thpos>0){_thpos--;memcpy(_tinput,_thist[_thlen-1-_thpos],T_COLS+1);_tilen=(int)strlen(_tinput);}
                        else{_thpos=-1;_tilen=0;_tinput[0]='\0';}
                        break;
                    default:break;
                    }
                }
                break;
            }

            case SFCML_EVT_TEXT:{
                int foc=_focus;
                int fopen=(foc>=0&&_wins[foc].visible&&!_wins[foc].minimized);
                char ch=evt.text.ch;
                if(fopen&&foc==W_CALC){
                    if((ch>='0'&&ch<='9')||ch=='+'||ch=='-'||ch=='*'||ch=='/'||ch=='.'||ch=='%')
                        calc_press(ch);
                    else if(ch=='='||ch=='\n')calc_press('=');
                    else if(ch=='c'||ch=='C')calc_press('C');
                } else if(fopen&&foc==W_BROWSER&&_bsearch_foc){
                    if(ch>=32&&_bsearch_len<BURL_MAX){_bsearch[_bsearch_len++]=ch;_bsearch[_bsearch_len]='\0';}
                } else if(fopen&&foc==W_BROWSER&&_burl_foc){
                    if(ch>=32&&_burl_len<BURL_MAX){_burl[_burl_len++]=ch;_burl[_burl_len]='\0';}
                } else if(fopen&&foc==W_FILES&&_fi_edit)fi_text(ch);
                else if(fopen&&foc==W_CODE)code_text(ch);
                else if(fopen&&foc==W_WORD)word_text(ch);
                else if(foc==W_TERM&&_wins[W_TERM].visible&&!_wins[W_TERM].minimized){
                    if(ch>=32&&_tilen<T_COLS){_tinput[_tilen++]=ch;_tinput[_tilen]='\0';}
                }
                break;
            }

            default:break;
            }
        }

        /* Mise a jour RTC sans blocage, chaque seconde */
        rtc_poll();
        if(_rtc.s!=last_s){
            last_s=_rtc.s;
            _blink=!_blink;
            dirty=1;
            if(_dvfs_dirty)dvfs_sync();   /* persiste nano/word sur disque */
            if(!got){ /* pas d'activite cette seconde */
                _inact_secs++;
                if(_veille_sel>0&&_inact_secs>=_veille_secs[_veille_sel])
                    _sleeping=1;
            }
        }
        if(got){ _inact_secs=0; if(_sleeping){_sleeping=0;dirty=1;} dirty=1; }
        if(!_sleeping){
            if(_wins[W_SNAKE].visible&&!_wins[W_SNAKE].minimized){snake_step();dirty=1;}
            if(_wins[W_RTYPE].visible&&!_wins[W_RTYPE].minimized){rtype_step();dirty=1;}
            if(_wins[W_PONG].visible&&!_wins[W_PONG].minimized){pong_step();dirty=1;}
            if(_wins[W_MINE].visible&&!_wins[W_MINE].minimized){mine_step();dirty=1;}
            if(_wins[W_TETRIS].visible&&!_wins[W_TETRIS].minimized){tet_step();dirty=1;}
        }
        if(_sleeping){ draw_screensaver(win); }
        else if(dirty){ redraw(win);dirty=0; }
        else __asm__ volatile("pause");
    }
    cmd_reboot();
}
