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

void _putchar(char c) { (void)c; }
#define BIOS_TICKS (*(volatile uint32_t*)0x046C)

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
            return &_dvfs[i];
        }
    }
    return 0;
}
static void _dvfs_remove(const char* p){DVFSEntry*e=_dvfs_find(p);if(e)e->used=0;}
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
static void sh_uptime(void){
    uint32_t s=BIOS_TICKS/18;
    t_print(" up ");_sh_puti((int)s/3600);t_print("h");
    _sh_puti((int)(s/60)%60);t_print("m  load avg: 0.00 0.00\n");
}
static void sh_ps(void){
    t_print("  PID TTY  STAT CMD\n");
    t_print("    1 ?    Ss   init\n");
    t_print("    2 ?    S    kthreadd\n");
    t_print("    3 ?    S    kmain\n");
    t_print("    4 ?    S    myos-wm\n");
    t_print("   10 tty0 R    sh\n");
    t_print("   11 tty0 R+   ps\n");
}
static void sh_top(void){
    t_print("Tasks:  6 running\n");
    t_print("CPU: 0.1%us  Mem: 131072k\n\n");
    t_print("  PID  %CPU  %MEM  CMD\n");
    t_print("    1   0.0   0.0  init\n");
    t_print("    3   0.0   0.5  kmain\n");
    t_print("   10   0.1   0.0  sh\n");
}
static void sh_free(void){
    t_print("             total    used    free\n");
    t_print("Mem:        131072   32768   98304\n");
    t_print("Swap:            0       0       0\n");
}
static void sh_df(void){
    t_print("Filesystem   Size  Used Avail Use%\n");
    t_print("/dev/hda     1.4M  890K  510K  64%\n");
    t_print("tmpfs         64M    0K   64M   0%\n");
}
static void sh_lscpu(void){
    t_print("Architecture: i386\n");
    t_print("CPU op-mode : 32-bit\n");
    t_print("CPU(s)      : 1\n");
    t_print("Vendor ID   : GenuineIntel\n");
    t_print("Model name  : i386 compatible\n");
    t_print("CPU MHz     : 1000.000\n");
    t_print("L1d cache   : 16K\nL2 cache    : 256K\n");
}
static void sh_lspci(void){
    t_print("00:00.0 Host bridge: Intel i440FX\n");
    t_print("00:01.0 ISA bridge : Intel PIIX3\n");
    t_print("00:02.0 VGA        : Standard VESA\n");
    t_print("00:03.0 Ethernet   : Realtek RTL8139\n");
    t_print("00:04.0 Audio      : Intel AC97\n");
}
static void sh_lsmod(void){
    t_print("Module      Size  Used by\n");
    t_print("rtl8139     8192  0\n");
    t_print("ps2kbd      4096  0\n");
    t_print("ps2mouse    4096  0\n");
    t_print("vesa        8192  0\n");
}
static void sh_dmesg(void){
    t_print("[    0.000] MyOS kernel started\n");
    t_print("[    0.001] Protected mode active\n");
    t_print("[    0.002] VESA VBE 640x480 24bpp\n");
    t_print("[    0.003] PS/2 keyboard detected\n");
    t_print("[    0.004] PS/2 mouse detected\n");
    t_print("[    0.005] PCI bus scan: 5 devices\n");
    t_print("[    0.006] RTL8139 at 00:03.0\n");
    t_print("[    0.010] MineGRUB handoff done\n");
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
static void sh_ping(const char*a){
    const char*host=a[0]?a:"127.0.0.1";
    t_print("PING ");t_print(host);t_print(" 56(84) bytes\n");
    if(net_ok){
        t_print("64 bytes from ");t_print(host);
        t_print(": icmp_seq=1 ttl=64 time=0.42ms\n");
        t_print("64 bytes from ");t_print(host);
        t_print(": icmp_seq=2 ttl=64 time=0.38ms\n");
        t_print("3 paquets transmis, 0% de perte\n");
    }else{
        t_print("connect: Network unreachable\n");
    }
}
static void sh_netstat(void){
    char buf[20];
    t_print("Proto  LocalAddr          ForeignAddr  State\n");
    if(net_ok){
        t_print("udp    ");_ip4str(net_my_ip,buf);t_print(buf);
        t_print(":68    0.0.0.0:0     LISTEN\n");
    }
    t_print("tcp    127.0.0.1:0       127.0.0.1:0  LISTEN\n");
}
static void sh_nslookup(const char*a){
    if(!a[0]){t_print("Usage: nslookup <host>\n");return;}
    char buf[20];
    t_print("Server:  ");_ip4str(net_dns_ip,buf);t_print(buf);t_print("\n");
    t_print("Address: ");t_print(a);t_print(" = ");
    t_print(net_ok?"10.0.2.15":"(reseau non disponible)");t_print("\n");
}
static void sh_traceroute(const char*a){
    if(!a[0]){t_print("Usage: traceroute <host>\n");return;}
    char buf[20];
    t_print("traceroute to ");t_print(a);t_print("\n");
    t_print(" 1  ");_ip4str(net_gw_ip,buf);t_print(buf);t_print("  1ms\n");
    t_print(" 2  * * *\n 3  * * *\n");
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
    (void)a;
    uint32_t t0=BIOS_TICKS;
    while(BIOS_TICKS-t0<18)__asm__ volatile("pause");
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
    t_print(" r  b  swpd   free   buff  cache  si  so  bi  bo\n");
    t_print(" 1  0     0  98304   4096   8192   0   0   0   0\n");
}
static void sh_iostat(void){
    t_print("avg-cpu: %user  %sys  %iowait  %idle\n");
    t_print("          0.0    0.1     0.0    99.9\n\n");
    t_print("Device  tps  kB_read/s  kB_wrtn/s\n");
    t_print("hda     0.0       0.0        0.0\n");
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
    char buf[20];
    t_print("Address          HWtype  HWaddress            Flags\n");
    _ip4str(net_gw_ip,buf);t_print(buf);
    t_print("  ether   52:54:00:12:34:56   C   eth0\n");
}
static void sh_iptables(const char*a){
    (void)a;
    t_print("Chain INPUT (policy ACCEPT 0 packets, 0 bytes)\n");
    t_print("Chain FORWARD (policy DROP 0 packets, 0 bytes)\n");
    t_print("Chain OUTPUT (policy ACCEPT 0 packets, 0 bytes)\n");
}
static void sh_dmidecode(void){
    t_print("BIOS Information\n  Vendor: SeaBIOS\n  Version: 1.16\n");
    t_print("System Information\n  Manufacturer: MyOS Project\n");
    t_print("  Product: MyOS-Machine\n  Version: 0.1\n");
    t_print("Processor\n  Family: Other\n  Version: i386 compatible\n");
    t_print("  Speed: 1000 MHz\n  Core Count: 1\n");
}
static void sh_lshw(void){
    t_print("*-system MyOS-Machine\n");
    t_print("  *-core\n    *-cpu: i386 1GHz\n");
    t_print("    *-memory: 128MB DRAM\n");
    t_print("    *-pci\n");
    t_print("      *-display: VESA VBE 640x480 24bpp\n");
    t_print("      *-network: RTL8139 100Mbit/s\n");
    t_print("      *-storage: IDE HDA 1.4MB\n");
    t_print("      *-sound: Intel AC97\n");
}
static void sh_hdparm(const char*a){
    const char*dev=a[0]?a:"/dev/hda";
    t_print(dev);t_print(":\n");
    t_print(" Model: MyOS Virtual Disk\n");
    t_print(" SerialNo: 000000000001\n");
    t_print(" Geometry: 3/16/63, sectors=2880, start=0\n");
    t_print(" DMA: mdma0 mdma1 mdma2\n");
}
static void sh_acpi(void){
    t_print("Battery 0: Discharging, 87%, 02:30:00 remaining\n");
    t_print("Thermal 0: ok, 42.0 degrees C\n");
    t_print("AC Adapter: off-line\n");
}
static void sh_sensors(void){
    t_print("coretemp-isa-0000\nAdapter: ISA adapter\n");
    t_print("Core 0: +42.0C  (high = +85.0C, crit = +100.0C)\n\n");
    t_print("i440fx-pci-0000\nAdapter: PCI adapter\n");
    t_print("VCore:  +1.25V\n3.3V:   +3.30V\n12V:   +12.04V\n");
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
static void sh_xxd(const char*a){
    const char*name=a[0]?a:"kernel";
    unsigned char*ptr=(unsigned char*)0x10000;
    for(int row=0;row<8;row++){
        _sh_puthex((unsigned)(row*16));t_print(": ");
        for(int c=0;c<16;c++){_sh_puthex8(ptr[row*16+c]);t_print(c==7?" ":"");}
        t_print("  |");
        for(int c=0;c<16;c++){
            unsigned char ch=ptr[row*16+c];
            char tmp[2]={(char)((ch>=32&&ch<127)?ch:'.'),0};t_print(tmp);
        }
        t_print("|\n");
    }
    t_print("(");t_print(name);t_print(": affichage partiel)\n");
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
static void sh_md5sum(const char*a){
    if(!a[0]){t_print("Usage: md5sum <texte>\n");return;}
    unsigned h0=0x67452301,h1=0xefcdab89,h2=0x98badcfe,h3=0x10325476;
    for(int i=0;a[i];i++){unsigned c=(unsigned char)a[i];
        h0=((h0<<5)|(h0>>27))^c;h1=((h1<<13)|(h1>>19))^(c*3);
        h2=((h2<<7) |(h2>>25))^(c*7);h3=((h3<<11)|(h3>>21))^(c*11);}
    _sh_puthex8(h0);_sh_puthex8(h1);_sh_puthex8(h2);_sh_puthex8(h3);
    t_print("  ");t_print(a);t_print("\n");
}
static void sh_sha256sum(const char*a){
    if(!a[0]){t_print("Usage: sha256sum <texte>\n");return;}
    unsigned h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                   0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for(int i=0;a[i];i++){unsigned c=(unsigned char)a[i];
        for(int j=0;j<8;j++)h[j]=((h[j]<<(j+3))|(h[j]>>(32-j-3)))^(c<<(j&7));}
    for(int i=0;i<8;i++)_sh_puthex8(h[i]);
    t_print("  ");t_print(a);t_print("\n");
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
            !strcmp(inp,"wget")||_sh_sw(inp,"wget "))
        t_print("(reseau HTTP non supporte ici)\n");
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
    else if(!strcmp(inp,"dd")||_sh_sw(inp,"dd "))
        {t_print("1+0 records in\n1+0 records out\n512 bytes copied\n");}
    else if(!strcmp(inp,"mkfs")||_sh_sw(inp,"mkfs "))
        {t_print("mkfs: ");t_print(arg[0]?arg:"?");t_print(": simule\n");}
    else if(!strcmp(inp,"fsck")||_sh_sw(inp,"fsck "))
        t_print("fsck: /dev/hda: prop: 0 erreur\n");
    else if(!strcmp(inp,"blkid"))
        t_print("/dev/hda: TYPE=\"ext2\" LABEL=\"myos\"\n");
    else if(!strcmp(inp,"fdisk")||_sh_sw(inp,"fdisk "))
        t_print("Disk /dev/hda: 1.4 MB, 2880 sectors\n");
    else if(!strcmp(inp,"parted")||_sh_sw(inp,"parted "))
        t_print("GNU Parted: /dev/hda 1.44MB ext2\n");
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
        t_print(" Fun     : fortune cowsay banner figlet toilet\n");
        t_print("           cal seq yes sleep color matrix\n");
        t_print("           sl hack fire rain pipes nyan lolcat\n");
        t_print("           creeper nyan cowsay\n");
        t_print(" Autres  : reboot shutdown mem make gcc nasm\n");
        t_print("           dd mkfs fdisk blkid parted fsck\n");
        t_print("Type 'man <cmd>' pour l'aide d'une commande.\n");
    }
    else if(!strcmp(inp,"snake")){win_open(W_SNAKE);snake_reset();}
    else if(!strcmp(inp,"rtype")){win_open(W_RTYPE);rtype_reset();}
    else if(!strcmp(inp,"pong")){win_open(W_PONG);pong_reset();}
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
        t_print("Heap : 0x400000  4MB\n");
        t_print("Kern : 0x010000 ~88KB\n");
        t_print("BkBuf: 0x300000 900KB\n");
    }
    else if(!strcmp(inp,"shutdown")||!strcmp(inp,"poweroff"))
        {t_print("Arret du systeme...\n");cmd_reboot();}
    else if(!strcmp(inp,"reboot")||!strcmp(inp,"halt"))
        {t_print("Redemarrage...\n");cmd_reboot();}
    else if(inp[0]=='#'){}
    else if(inp[0]){t_print(inp);t_print(": commande introuvable\n");}
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

#define NW         14
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
};
static int _z[NW]={0,1,2,3,4,5,6,7,8,9,10,11,12,13};
static int _drag_win=-1,_drag_ox,_drag_oy;
static int _focus=-1;

static void win_front(int idx){
    int k=-1;
    for(int i=0;i<NW;i++)if(_z[i]==idx){k=i;break;}
    if(k<0||k==NW-1)return;
    for(int i=k;i<NW-1;i++)_z[i]=_z[i+1];
    _z[NW-1]=idx;_focus=idx;
}
static void win_open(int idx){
    _wins[idx].visible=1;_wins[idx].minimized=0;win_front(idx);
    if(idx==W_PAINT&&!_paint_inited){memset(PAINT_CANVAS,0,PAINT_CW*PAINT_CH);_paint_inited=1;}
    if(idx==W_CODE&&!_code_inited){code_init();_code_inited=1;}
    if(idx==W_WORD&&!_word_inited){word_init();_word_inited=1;}
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

#define NICONS 12
static const int   IC_Y[NICONS]={8,53,98,143,188,233,278,323,368,413,458,503};
static const char* IC_LBL[NICONS]={"Terminal","Paint","Code","Word","Creeper","A propos","Reboot","Params","Browser","Snake","R-Type","Pong"};
#define IC_X  6
#define IC_SZ 32

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
        int ix=IC_X,iy=IC_Y[i];
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
#define SM_N    14
#define SM_H   (SM_N*SM_IH+8)
#define SM_Y   (TB_Y-SM_H)

static const char* SM_LBL[SM_N]={
    "  Terminal","  Creeper!","  A propos","  ---------",
    "  Paint","  Code Editor","  Word","  Navigateur",
    "  Calculatrice","  Fichiers","  Reseau","  Parametres",
    "  ---------","  Redemarrer",
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
    else if(item==13)cmd_reboot();
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
    }
    draw_taskbar(win);
    if(_start_open)draw_smenu(win);
    if(_rcopen)    draw_rcmenu(win);
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
        if(mx>=IC_X&&mx<IC_X+IC_SZ&&my>=IC_Y[i]&&my<IC_Y[i]+IC_SZ+10){found=i;break;}
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
 * Point d'entree
 * ============================================================ */
void kmain(void){
    if(!sfcml_init())for(;;)__asm__ volatile("hlt");
    sfcml_Window* win=sfcml_createWindow("MyOS");
    win->font=(uint8_t*)font8x8;
    __asm__ volatile("fninit"); /* init FPU pour les calculs float (Mandelbrot) */
    sfcml_mouseInit();
    rtc_read();
    net_init();

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
        }
        if(_sleeping){ draw_screensaver(win); }
        else if(dirty){ redraw(win);dirty=0; }
        else __asm__ volatile("pause");
    }
    cmd_reboot();
}
