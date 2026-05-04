#include <sfcml.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "font8x8.h"

void _putchar(char c) { (void)c; }
#define BIOS_TICKS (*(volatile uint32_t*)0x046C)

/* ============================================================
 * Creeper 8x8
 * ============================================================ */
static const uint8_t CREEPER[8] = {
    0xFF,0xFF,0x99,0x99,0xFF,0xE7,0xC3,0xDB,
};
static void draw_creeper(sfcml_Window* w, int ox, int oy, int sc) {
    sfcml_Color g = sfcml_rgb(94,124,22);
    for (int r=0;r<8;r++) for (int c=0;c<8;c++) {
        sfcml_Color cl = (CREEPER[r]&(0x80>>c)) ? g : SFCML_BLACK;
        sfcml_fillRect(w, sfcml_rect(ox+c*sc, oy+r*sc, sc, sc), cl);
    }
}

/* ============================================================
 * Texte grande taille (utilise font8x8 directement)
 * ============================================================ */
static void draw_big_char(sfcml_Window* w, char c, int x, int y, int sc,
                           sfcml_Color fg, sfcml_Color bg) {
    const uint8_t* glyph = font8x8 + (unsigned char)c * 8;
    for (int r=0;r<8;r++) for (int col=0;col<8;col++) {
        sfcml_Color cl = (glyph[r]&(1<<col)) ? fg : bg;
        sfcml_fillRect(w, sfcml_rect(x+col*sc, y+r*sc, sc, sc), cl);
    }
}
static void draw_big_text(sfcml_Window* w, const char* s, int x, int y,
                           int sc, sfcml_Color fg, sfcml_Color bg) {
    while (*s) { draw_big_char(w, *s++, x, y, sc, fg, bg); x += 8*sc; }
}

/* ============================================================
 * Ecran de demarrage Epitech
 * ============================================================ */
static void splash_delay(uint32_t n) {
    for (uint32_t i=0;i<n;i++) __asm__ volatile("nop");
}
static void draw_splash(sfcml_Window* win) {
    sfcml_Color bk = SFCML_BLACK, rd = sfcml_rgb(220,20,20);
    sfcml_Color gr = sfcml_rgb(150,150,150), wh = SFCML_WHITE;
    sfcml_fillRect(win, sfcml_rect(0,0,640,480), bk);
    draw_big_text(win,"EPITECH",208,158,4,rd,bk);
    sfcml_fillRect(win, sfcml_rect(208,200,224,3), rd);
    sfcml_drawText(win,"L'expertise informatique - epitech.eu",165,218,gr,bk);
    sfcml_drawText(win,"Epitech Technology  |  Paris, France",196,462,gr,bk);
    sfcml_drawText(win,"Demarrage du systeme...",232,240,wh,bk);
    int bx=200,by=390,bw=240,bh=14;
    sfcml_drawRect(win,sfcml_rect(bx,by,bw,bh),sfcml_rgb(60,10,10));
    sfcml_present(win); /* affiche le fond + texte avant la barre */
    for (int s=1;s<=30;s++) {
        sfcml_fillRect(win,sfcml_rect(bx+1,by+1,s*(bw-2)/30,bh-2),rd);
        sfcml_present(win); /* chaque etape visible sans tearing */
        splash_delay(20000000UL);
    }
}

/* ============================================================
 * Terminal — etat et rendu
 * ============================================================ */
#define T_COLS  60
#define T_ROWS  36
static char  _tlines[T_ROWS][T_COLS+1];
static int   _tnlines = 0;
static char  _tinput[T_COLS+1];
static int   _tilen  = 0;
static char  _thist[8][T_COLS+1];
static int   _thlen = 0, _thpos = -1;

static sfcml_Color C_DK, C_TB, C_TBAR_FOC, C_TBAR_BG, C_WINBG, C_BDR, C_FG, C_PROMPT;

static void t_scroll(void) {
    for (int i=0;i<T_ROWS-1;i++) memcpy(_tlines[i],_tlines[i+1],T_COLS+1);
    _tlines[T_ROWS-1][0]='\0';
    if (_tnlines>0) _tnlines=T_ROWS-1;
}
static void t_nl(void) {
    if (_tnlines<T_ROWS) _tlines[_tnlines][0]='\0';
    _tnlines++;
    if (_tnlines>T_ROWS) t_scroll();
}
static void t_print(const char* s) {
    while (*s) {
        if (*s=='\n') { t_nl(); }
        else {
            int row = _tnlines>0 ? _tnlines-1 : 0;
            if (_tnlines==0) { _tlines[0][0]='\0'; _tnlines=1; }
            int len=(int)strlen(_tlines[row]);
            if (len<T_COLS) { _tlines[row][len]=*s; _tlines[row][len+1]='\0'; }
        }
        s++;
    }
}
static void t_hist_add(void) {
    if (_tilen==0) return;
    if (_thlen<8) { memcpy(_thist[_thlen++],_tinput,T_COLS+1); }
    else { for(int i=0;i<7;i++) memcpy(_thist[i],_thist[i+1],T_COLS+1); memcpy(_thist[7],_tinput,T_COLS+1); }
    _thpos=-1;
}

/* ============================================================
 * Commandes shell
 * ============================================================ */
static void cmd_reboot(void) {
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)0xFE),"Nd"((uint16_t)0x64));
    for (;;) __asm__ volatile("hlt");
}
static void cmd_help(void) {
    t_print("Commandes disponibles:\n");
    t_print("  help   clear   about   ls      echo <txt>\n");
    t_print("  uname  time    ping    sudo    fortune\n");
    t_print("  creeper  matrix  color  shutdown  reboot\n");
}
static void cmd_about(void) {
    t_print("=== MyOS v0.1 ===\n");
    t_print("CPU : x86 32-bit Protected Mode\n");
    t_print("RAM : 128 MB  GPU : VESA 640x480 24bpp\n");
    t_print("Boot: MineGRUB v1.0  |  FS: ramdisk\n");
    t_print("Libs: libk + SFCML (homemade @ Epitech)\n");
}
static void cmd_ls(void) {
    t_print("drwxr-xr-x  bin/   boot/  dev/\n");
    t_print("drwxr-xr-x  etc/   home/  lib/\n");
    t_print("-rwxr-xr-x  kernel.bin    initrd\n");
    t_print("drwxr-xr-x  sys/   usr/   var/\n");
}
static void cmd_uname(void) {
    t_print("MyOS 0.1 #1 SMP i386 GNU/Epitech\n");
}
static void cmd_time(void) {
    uint32_t t=BIOS_TICKS, s=t/18;
    char b[32];
    b[0]='U';b[1]='p';b[2]='t';b[3]='i';b[4]='m';b[5]='e';b[6]=':';b[7]=' ';
    int o=8;
    uint32_t h=s/3600, m=(s/60)%60, sc=s%60;
    b[o++]='0'+h/10; b[o++]='0'+h%10; b[o++]='h';
    b[o++]='0'+m/10; b[o++]='0'+m%10; b[o++]='m';
    b[o++]='0'+sc/10;b[o++]='0'+sc%10;b[o++]='s';
    b[o++]='\n'; b[o]='\0';
    t_print(b);
}
static void cmd_ping(const char* arg) {
    t_print("PING "); t_print(arg[0]?arg:"epitech.eu"); t_print("\n");
    t_print("64 bytes: icmp_seq=1 ttl=64 time=0.1 ms\n");
    t_print("64 bytes: icmp_seq=2 ttl=64 time=0.1 ms\n");
    t_print("2 packets transmitted, 2 received, 0% loss\n");
}
static void cmd_sudo(void) {
    t_print("[sudo] password for root: \n");
    t_print("Permission accordee. Vous etes deja root.\n");
}
static void cmd_fortune(void) {
    static int fi=0;
    static const char* forts[4]={
        "Un Creeper ne frappe jamais deux fois... BOOM!\n",
        "make: *** [Makefile:42: life] Error 1\n",
        "rm -rf /* : commande non trouvee. (Ouf.)\n",
        "Epitech: l'endroit ou 42h de sommeil = 1 semaine.\n",
    };
    t_print(forts[fi++ %4]);
}
static void cmd_creeper(void) {
    t_print("  Creeper, Aw Man...\n");
    t_print("  So we back in the mine,\n");
    t_print("  Got our pickaxe swinging side to side!\n");
}
static void cmd_color(const char* arg) {
    if (strncmp(arg,"red",3)==0)   C_FG=sfcml_rgb(255,80,80);
    else if (strncmp(arg,"green",5)==0) C_FG=SFCML_GREEN;
    else if (strncmp(arg,"cyan",4)==0)  C_FG=SFCML_CYAN;
    else if (strncmp(arg,"white",5)==0) C_FG=SFCML_WHITE;
    else if (strncmp(arg,"yellow",6)==0) C_FG=SFCML_YELLOW;
    else { t_print("Usage: color red|green|cyan|white|yellow\n"); return; }
    t_print("Couleur changee.\n");
}
static void cmd_matrix(void) {
    t_print("Wake up, Neo...\n");
    t_print("The Matrix has you.\n");
    t_print("Follow the white rabbit.\n");
    t_print("Knock knock, Neo.\n");
}

static void handle_command(void) {
    t_hist_add();
    t_print("$ "); t_print(_tinput); t_print("\n");
    char* inp = _tinput;
    if      (!strcmp(inp,"help"))             cmd_help();
    else if (!strcmp(inp,"clear"))          { for(int i=0;i<T_ROWS;i++)_tlines[i][0]='\0';_tnlines=0; }
    else if (!strcmp(inp,"about"))            cmd_about();
    else if (!strcmp(inp,"ls")||!strncmp(inp,"ls ",3))  cmd_ls();
    else if (!strcmp(inp,"uname")||!strcmp(inp,"uname -a")) cmd_uname();
    else if (!strcmp(inp,"time"))             cmd_time();
    else if (!strcmp(inp,"ping")||!strncmp(inp,"ping ",5))
        cmd_ping(strlen(inp)>5?inp+5:"");
    else if (!strcmp(inp,"sudo")||!strncmp(inp,"sudo ",5)) cmd_sudo();
    else if (!strncmp(inp,"echo ",5))       { t_print(inp+5); t_print("\n"); }
    else if (!strcmp(inp,"fortune"))          cmd_fortune();
    else if (!strcmp(inp,"creeper"))          cmd_creeper();
    else if (!strncmp(inp,"color ",6))        cmd_color(inp+6);
    else if (!strcmp(inp,"matrix"))           cmd_matrix();
    else if (!strcmp(inp,"shutdown")||!strcmp(inp,"poweroff"))
        { t_print("Arret...\n"); cmd_reboot(); }
    else if (!strcmp(inp,"reboot"))         { t_print("Reboot...\n"); cmd_reboot(); }
    else if (inp[0]!='\0')                  { t_print(inp); t_print(": commande introuvable\n"); }
    _tilen=0; _tinput[0]='\0';
}

/* ============================================================
 * Systeme de fenetres
 * ============================================================ */
#define TBAR_H   22
#define BTN_W    16
#define BTN_H    14

typedef struct { int x,y,w,h,visible,minimized; const char* title; } AppWin;

#define NW   3
#define W_TERM    0
#define W_ABOUT   1
#define W_CREEP   2

static AppWin _wins[NW]={
    {70, 36,504,320,0,0,"Terminal - root@myos"},
    {160,80, 320,210,0,0,"A propos de MyOS"},
    {240,70, 216,224,0,0,"Creeper!"},
};
static int _z[NW]={0,1,2};
static int _drag_win=-1,_drag_ox,_drag_oy;
static int _focus=-1;

static void win_front(int idx){
    int k=-1;
    for(int i=0;i<NW;i++) if(_z[i]==idx){k=i;break;}
    if(k<0||k==NW-1)return;
    for(int i=k;i<NW-1;i++)_z[i]=_z[i+1];
    _z[NW-1]=idx; _focus=idx;
}
static void win_open(int idx){
    _wins[idx].visible=1; _wins[idx].minimized=0; win_front(idx);
}

/* ============================================================
 * Etat bureau
 * ============================================================ */
static int _mx=320,_my=240;
static int _start_open=0;
static int _rcopen=0,_rcx,_rcy;
static int _sel_icon=-1;

#define NICONS 4
static const int   IC_Y[NICONS]={14,90,166,242};
static const char* IC_LBL[NICONS]={"Terminal","Creeper!","A propos","Reboot"};
#define IC_X  14
#define IC_SZ 32

/* ============================================================
 * Curseur
 * ============================================================ */
static void draw_cursor(sfcml_Window* win,int x,int y){
    static const int8_t S[13][2]={{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},{0,8},{0,7},{0,5},{2,3},{3,2},{3,2}};
    for(int r=0;r<13;r++) sfcml_drawHLine(win,x+S[r][0]+1,y+r+1,S[r][1],SFCML_BLACK);
    for(int r=0;r<13;r++) sfcml_drawHLine(win,x+S[r][0],  y+r,  S[r][1],SFCML_WHITE);
}

/* ============================================================
 * Frame de fenetre
 * ============================================================ */
static int hit_x (int wi,int mx,int my){AppWin*w=&_wins[wi];return mx>=w->x+w->w-BTN_W-4&&mx<w->x+w->w-4         &&my>=w->y+4&&my<w->y+4+BTN_H;}
static int hit_mn(int wi,int mx,int my){AppWin*w=&_wins[wi];return mx>=w->x+w->w-2*BTN_W-8&&mx<w->x+w->w-BTN_W-8&&my>=w->y+4&&my<w->y+4+BTN_H;}
static int hit_tb(int wi,int mx,int my){AppWin*w=&_wins[wi];return mx>=w->x&&mx<w->x+w->w-2*BTN_W-8&&my>=w->y&&my<w->y+TBAR_H;}

static void draw_frame(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int foc=(_focus==wi);
    sfcml_Color tbar = foc?sfcml_rgb(0,105,210):sfcml_rgb(55,55,75);
    sfcml_Color bdr  = foc?sfcml_rgb(0,130,230):sfcml_rgb(75,75,100);
    /* ombre */
    sfcml_fillRect(win,sfcml_rect(w->x+4,w->y+4,w->w,w->h),sfcml_rgb(0,0,0));
    /* corps */
    sfcml_fillRect(win,sfcml_rect(w->x,w->y,w->w,w->h),C_WINBG);
    /* titre */
    sfcml_fillRect(win,sfcml_rect(w->x,w->y,w->w,TBAR_H),tbar);
    sfcml_drawText(win,w->title,w->x+6,w->y+7,SFCML_WHITE,tbar);
    /* bouton min */
    sfcml_fillRect(win,sfcml_rect(w->x+w->w-2*BTN_W-8,w->y+4,BTN_W,BTN_H),sfcml_rgb(200,160,0));
    sfcml_drawHLine(win,w->x+w->w-2*BTN_W-6,w->y+11,BTN_W-4,SFCML_WHITE);
    /* bouton close */
    sfcml_fillRect(win,sfcml_rect(w->x+w->w-BTN_W-4,w->y+4,BTN_W,BTN_H),sfcml_rgb(196,40,30));
    sfcml_drawText(win,"x",w->x+w->w-BTN_W-1,w->y+5,SFCML_WHITE,sfcml_rgb(196,40,30));
    /* bordure */
    sfcml_drawRect(win,sfcml_rect(w->x,w->y,w->w,w->h),bdr);
}

/* ============================================================
 * Contenu fenetre terminal
 * ============================================================ */
static void draw_term(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int tx=w->x+4, ty=w->y+TBAR_H+3;
    int avh=w->h-TBAR_H-3-14;
    int rows_vis=avh/8;
    if(rows_vis>T_ROWS) rows_vis=T_ROWS;
    int start=(_tnlines>rows_vis)?_tnlines-rows_vis:0;
    for(int i=0;i<rows_vis&&start+i<_tnlines;i++)
        sfcml_drawText(win,_tlines[start+i],tx,ty+i*8,C_FG,C_WINBG);
    /* ligne input */
    int iy=w->y+w->h-14;
    sfcml_fillRect(win,sfcml_rect(w->x+1,iy-2,w->w-2,14),C_WINBG);
    sfcml_drawHLine(win,w->x+1,iy-3,w->w-2,sfcml_rgb(50,50,60));
    sfcml_drawText(win,"root@myos:~$ ",tx,iy,C_PROMPT,C_WINBG);
    sfcml_drawText(win,_tinput,tx+13*8,iy,C_FG,C_WINBG);
    /* curseur clignotant */
    if((BIOS_TICKS/9)%2==0)
        sfcml_fillRect(win,sfcml_rect(tx+13*8+_tilen*8,iy,6,8),C_FG);
}

/* ============================================================
 * Contenu fenetre A propos
 * ============================================================ */
static void draw_about(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int tx=w->x+10,ty=w->y+TBAR_H+8;
    sfcml_Color hl=sfcml_rgb(0,140,255), gr=sfcml_rgb(130,130,130);
    sfcml_Color rd=sfcml_rgb(220,20,20);
    draw_big_text(win,"MyOS",tx,ty,2,hl,C_WINBG);
    sfcml_drawText(win,"v0.1",tx+68,ty+8,gr,C_WINBG);
    sfcml_drawHLine(win,tx,ty+20,w->w-20,sfcml_rgb(55,55,70));
    int y=ty+28;
    sfcml_drawText(win,"CPU : x86 32-bit Protected Mode",tx,y,C_FG,C_WINBG);y+=10;
    sfcml_drawText(win,"RAM : 128 MB",tx,y,C_FG,C_WINBG);y+=10;
    sfcml_drawText(win,"GPU : VESA 640x480 24bpp",tx,y,C_FG,C_WINBG);y+=10;
    sfcml_drawText(win,"FS  : Aucun (bare-metal)",tx,y,C_FG,C_WINBG);y+=14;
    sfcml_drawText(win,"Bootloader : MineGRUB v1.0",tx,y,gr,C_WINBG);y+=10;
    sfcml_drawText(win,"Kernel     : C + NASM",tx,y,gr,C_WINBG);y+=10;
    sfcml_drawText(win,"Libs       : libk + SFCML",tx,y,gr,C_WINBG);y+=14;
    sfcml_drawText(win,"Made with <3  @  Epitech",tx,y,rd,C_WINBG);
    draw_creeper(win,w->x+w->w-52,ty,4);
}

/* ============================================================
 * Contenu fenetre Creeper
 * ============================================================ */
static void draw_creep_win(sfcml_Window* win,int wi){
    AppWin* w=&_wins[wi];
    int cx=w->x+(w->w-72)/2, cy=w->y+TBAR_H+8;
    draw_creeper(win,cx,cy,9);
    sfcml_Color lbl=((BIOS_TICKS/18)%2==0)?sfcml_rgb(94,124,22):sfcml_rgb(200,20,20);
    const char* msg=(BIOS_TICKS/18)%2==0?"  Creeper!  ":"  Aw Man.. ";
    sfcml_drawText(win,msg,cx,cy+74,lbl,C_WINBG);
    sfcml_drawText(win,"[Esc pour fermer]",cx-16,cy+86,sfcml_rgb(80,80,80),C_WINBG);
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
        if(i==0){
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(10,10,14));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(70,70,90));
            sfcml_drawText(win,">_",ix+8,iy+12,SFCML_GREEN,sfcml_rgb(10,10,14));
        } else if(i==1){
            draw_creeper(win,ix,iy,4);
        } else if(i==2){
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(0,80,170));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(0,60,130));
            sfcml_drawText(win,"i",ix+13,iy+10,SFCML_WHITE,sfcml_rgb(0,80,170));
            sfcml_drawText(win,".",ix+13,iy+18,SFCML_WHITE,sfcml_rgb(0,80,170));
        } else {
            sfcml_Color rc=hov?sfcml_rgb(255,80,80):sfcml_rgb(200,50,50);
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),C_DK);
            sfcml_drawCircle(win,ix+16,iy+20,10,rc);
            sfcml_fillRect(win,sfcml_rect(ix+14,iy+5,4,10),rc);
        }
        sfcml_Color lbg=sel?sfcml_rgb(30,60,180):C_DK;
        sfcml_drawText(win,IC_LBL[i],ix,iy+IC_SZ+2,SFCML_WHITE,lbg);
    }
}

/* ============================================================
 * Menu Demarrer (Windows XP style avec panneau lateral)
 * ============================================================ */
#define SM_W  160
#define SM_IH  22
#define SM_N    7
#define SM_H   (SM_N*SM_IH+8)
#define SM_Y   (450-SM_H)

static const char* SM_LBL[SM_N]={
    "  Terminal",
    "  Creeper!",
    "  A propos",
    "  ---------",
    "  fortune",
    "  Redemarrer",
    "  Eteindre",
};

static void draw_smenu(sfcml_Window* win){
    sfcml_fillRect(win,sfcml_rect(0,SM_Y,26,SM_H),sfcml_rgb(180,10,10));
    sfcml_fillRect(win,sfcml_rect(26,SM_Y,SM_W-26,SM_H),sfcml_rgb(26,26,38));
    sfcml_drawRect(win,sfcml_rect(0,SM_Y,SM_W,SM_H),sfcml_rgb(80,80,100));
    /* Texte vertical panneau rouge */
    const char* lbl="MyOS";
    for(int i=0;lbl[i];i++)
        sfcml_drawChar(win,lbl[i],8,SM_Y+4+i*12,SFCML_WHITE,sfcml_rgb(180,10,10));
    for(int i=0;i<SM_N;i++){
        int iy=SM_Y+4+i*SM_IH;
        if(SM_LBL[i][2]=='-'){
            sfcml_drawHLine(win,27,iy+10,SM_W-29,sfcml_rgb(80,80,100));
            continue;
        }
        int hov=(_mx>=26&&_mx<SM_W&&_my>=iy&&_my<iy+SM_IH);
        sfcml_Color bg=hov?sfcml_rgb(0,100,200):sfcml_rgb(26,26,38);
        sfcml_fillRect(win,sfcml_rect(27,iy,SM_W-27,SM_IH-1),bg);
        sfcml_drawText(win,SM_LBL[i],30,iy+7,SFCML_WHITE,bg);
    }
}
static void smenu_click(int mx,int my){
    if(mx<0||mx>=SM_W||my<SM_Y||my>=SM_Y+SM_H){_start_open=0;return;}
    int item=(my-SM_Y-4)/SM_IH;
    if(item<0||item>=SM_N)return;
    _start_open=0;
    if(item==0) win_open(W_TERM);
    else if(item==1) win_open(W_CREEP);
    else if(item==2) win_open(W_ABOUT);
    else if(item==4){win_open(W_TERM);cmd_fortune();}
    else if(item==5||item==6) cmd_reboot();
}

/* ============================================================
 * Menu clic droit
 * ============================================================ */
#define RC_W  140
#define RC_IH  18
#define RC_N    5
static const char* RC_LBL[RC_N]={
    " Ouvrir Terminal",
    " Ouvrir Creeper",
    " ---------",
    " A propos",
    " Redemarrer",
};
static void draw_rcmenu(sfcml_Window* win){
    int mw=RC_W,mh=RC_N*RC_IH+4;
    int x0=_rcx,y0=_rcy;
    if(x0+mw>640)x0=640-mw;
    if(y0+mh>450)y0=450-mh;
    sfcml_fillRect(win,sfcml_rect(x0,y0,mw,mh),sfcml_rgb(38,38,52));
    sfcml_drawRect(win,sfcml_rect(x0,y0,mw,mh),sfcml_rgb(100,100,130));
    for(int i=0;i<RC_N;i++){
        int iy=y0+2+i*RC_IH;
        if(RC_LBL[i][1]=='-'){sfcml_drawHLine(win,x0+4,iy+8,mw-8,sfcml_rgb(80,80,100));continue;}
        int hov=(_mx>=x0&&_mx<x0+mw&&_my>=iy&&_my<iy+RC_IH);
        sfcml_Color bg=hov?sfcml_rgb(0,100,200):sfcml_rgb(38,38,52);
        sfcml_fillRect(win,sfcml_rect(x0+1,iy,mw-2,RC_IH-1),bg);
        sfcml_drawText(win,RC_LBL[i],x0+4,iy+5,SFCML_WHITE,bg);
    }
}
static void rcmenu_click(int mx,int my){
    int mw=RC_W,mh=RC_N*RC_IH+4;
    int x0=_rcx,y0=_rcy;
    if(x0+mw>640)x0=640-mw;
    if(y0+mh>450)y0=450-mh;
    _rcopen=0;
    if(mx<x0||mx>=x0+mw||my<y0||my>=y0+mh)return;
    int item=(my-y0-2)/RC_IH;
    if(item==0) win_open(W_TERM);
    else if(item==1) win_open(W_CREEP);
    else if(item==3) win_open(W_ABOUT);
    else if(item==4) cmd_reboot();
}

/* ============================================================
 * Taskbar
 * ============================================================ */
static void draw_taskbar(sfcml_Window* win){
    sfcml_fillRect(win,sfcml_rect(0,449,640,31),C_TB);
    sfcml_drawHLine(win,0,449,640,sfcml_rgb(0,90,170));
    /* Start */
    sfcml_Color sc=_start_open?sfcml_rgb(160,5,5):sfcml_rgb(215,20,20);
    sfcml_fillRect(win,sfcml_rect(2,451,84,26),sc);
    sfcml_drawRect(win,sfcml_rect(2,451,84,26),sfcml_rgb(120,4,4));
    sfcml_drawText(win,"Demarrer",8,460,SFCML_WHITE,sc);
    /* Boutons fenetres */
    int bx=92;
    for(int i=0;i<NW;i++){
        if(!_wins[i].visible)continue;
        int foc=(_focus==i);
        sfcml_Color bc=_wins[i].minimized?sfcml_rgb(50,50,68):(foc?sfcml_rgb(0,100,200):sfcml_rgb(38,38,58));
        sfcml_fillRect(win,sfcml_rect(bx,452,92,24),bc);
        sfcml_drawRect(win,sfcml_rect(bx,452,92,24),sfcml_rgb(80,80,110));
        sfcml_Color tc=_wins[i].minimized?sfcml_rgb(140,140,160):SFCML_WHITE;
        sfcml_drawText(win,_wins[i].title,bx+4,460,tc,bc);
        bx+=96;
    }
    /* Horloge */
    uint32_t s=BIOS_TICKS/18;
    char clk[9];
    clk[0]='0'+s/3600/10; clk[1]='0'+s/3600%10; clk[2]=':';
    clk[3]='0'+(s/60)%60/10; clk[4]='0'+(s/60)%60%10; clk[5]=':';
    clk[6]='0'+s%60/10; clk[7]='0'+s%60%10; clk[8]='\0';
    sfcml_drawText(win,clk,576,460,SFCML_WHITE,C_TB);
    /* Barre epitech */
    sfcml_drawText(win,"epitech",480,460,sfcml_rgb(180,10,10),C_TB);
}

/* ============================================================
 * Fond bureau (degrade discret en bandes)
 * ============================================================ */
static void draw_bg(sfcml_Window* win){
    for(int i=0;i<9;i++){
        int y=i*50, h=(i<8)?50:49;
        sfcml_fillRect(win,sfcml_rect(0,y,640,h),
            sfcml_rgb(i,40+i*4,75+i*7));
    }
}

/* ============================================================
 * Rendu complet (double-buffered : tout dans le backbuffer,
 * puis sfcml_present() envoie tout en une passe vers le LFB)
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
    }
    draw_taskbar(win);
    if(_start_open) draw_smenu(win);
    if(_rcopen)     draw_rcmenu(win);
    draw_cursor(win,_mx,_my);
    sfcml_present(win); /* flip : backbuffer → framebuffer VESA */
}

/* ============================================================
 * Gestion des clics souris
 * ============================================================ */
static void on_press(int mx,int my,int btn){
    if(btn==1){ /* clic droit */
        _rcopen=1; _rcx=mx; _rcy=my;
        _start_open=0; return;
    }
    if(_rcopen){rcmenu_click(mx,my);return;}
    if(_start_open){smenu_click(mx,my);return;}

    /* Start button */
    if(mx>=2&&mx<86&&my>=451&&my<477){_start_open=!_start_open;return;}

    /* Boutons taskbar (fenetres) */
    if(my>=451&&my<477){
        int bx=92;
        for(int i=0;i<NW;i++){
            if(!_wins[i].visible){continue;}
            if(mx>=bx&&mx<bx+92){
                if(!_wins[i].minimized&&_focus==i) _wins[i].minimized=1;
                else{_wins[i].minimized=0;win_front(i);}
                return;
            }
            bx+=96;
        }
    }

    /* Fenetres (avant → arriere) */
    for(int zi=NW-1;zi>=0;zi--){
        int i=_z[zi];
        AppWin* w=&_wins[i];
        if(!w->visible||w->minimized)continue;
        if(mx<w->x||mx>=w->x+w->w||my<w->y||my>=w->y+w->h)continue;
        win_front(i);
        if(hit_x(i,mx,my)){w->visible=0;return;}
        if(hit_mn(i,mx,my)){w->minimized=1;return;}
        if(hit_tb(i,mx,my)){_drag_win=i;_drag_ox=mx-w->x;_drag_oy=my-w->y;return;}
        return; /* clic dans le corps */
    }

    /* Icones bureau */
    int found=-1;
    for(int i=0;i<NICONS;i++)
        if(mx>=IC_X&&mx<IC_X+IC_SZ&&my>=IC_Y[i]&&my<IC_Y[i]+IC_SZ+10){found=i;break;}
    if(found>=0){
        if(_sel_icon==found){
            if(found==0) win_open(W_TERM);
            else if(found==1) win_open(W_CREEP);
            else if(found==2) win_open(W_ABOUT);
            else cmd_reboot();
        }
        _sel_icon=found;
    } else {
        _sel_icon=-1; _start_open=0; _rcopen=0;
    }
}

/* ============================================================
 * Point d'entree
 * ============================================================ */
void kmain(void){
    if(!sfcml_init()) for(;;) __asm__ volatile("hlt");

    sfcml_Window* win=sfcml_createWindow("MyOS");
    win->font=(uint8_t*)font8x8;
    sfcml_mouseInit();

    C_DK     = sfcml_rgb(0,  48,  90);
    C_TB     = sfcml_rgb(14, 50, 110);
    C_TBAR_FOC = sfcml_rgb(0, 105,210);
    C_TBAR_BG  = sfcml_rgb(55, 55, 75);
    C_WINBG  = sfcml_rgb(14, 14,  20);
    C_BDR    = sfcml_rgb(80, 80, 105);
    C_FG     = SFCML_WHITE;
    C_PROMPT = SFCML_GREEN;

    draw_splash(win);

    /* Message de bienvenue dans le terminal */
    t_print("Bienvenue sur MyOS v0.1 - Epitech Edition\n");
    t_print("Tapez 'help' pour voir les commandes.\n");
    t_print("Clic droit sur le bureau = menu.\n");
    t_print("Glisser les fenetres par leur barre de titre.\n");

    sfcml_Event evt;
    int dirty=1;
    uint32_t last_tick=0;

    while(sfcml_isOpen(win)){
        int got=0;
        while(sfcml_pollEvent(win,&evt)){
            got=1;
            switch(evt.type){

            case SFCML_EVT_MOUSE_MOVED:
                _mx=evt.mouse.x; _my=evt.mouse.y;
                if(_drag_win>=0){
                    AppWin* w=&_wins[_drag_win];
                    w->x=_mx-_drag_ox; w->y=_my-_drag_oy;
                    if(w->x<0)w->x=0;
                    if(w->x+w->w>640)w->x=640-w->w;
                    if(w->y<0)w->y=0;
                    if(w->y+w->h>448)w->y=448-w->h;
                }
                break;

            case SFCML_EVT_MOUSE_PRESSED:
                on_press(evt.mouse.x,evt.mouse.y,evt.mouse.button);
                break;

            case SFCML_EVT_MOUSE_RELEASED:
                _drag_win=-1;
                break;

            case SFCML_EVT_KEY_PRESSED:{
                int has_t=_wins[W_TERM].visible&&!_wins[W_TERM].minimized;
                if(!has_t){if(evt.key.code==SFCML_KEY_ESCAPE)cmd_reboot();break;}
                if(_focus!=W_TERM) win_front(W_TERM);
                switch(evt.key.code){
                case SFCML_KEY_ESCAPE:   _wins[W_TERM].minimized=1; break;
                case SFCML_KEY_RETURN:   handle_command(); break;
                case SFCML_KEY_BACKSPACE:
                    if(_tilen>0){_tinput[--_tilen]='\0';} break;
                case SFCML_KEY_UP:
                    if(_thpos<_thlen-1){
                        _thpos++;
                        memcpy(_tinput,_thist[_thlen-1-_thpos],T_COLS+1);
                        _tilen=(int)strlen(_tinput);
                    } break;
                case SFCML_KEY_DOWN:
                    if(_thpos>0){
                        _thpos--;
                        memcpy(_tinput,_thist[_thlen-1-_thpos],T_COLS+1);
                        _tilen=(int)strlen(_tinput);
                    } else {_thpos=-1;_tilen=0;_tinput[0]='\0';}
                    break;
                default: break;
                }
                break;
            }

            case SFCML_EVT_TEXT:{
                int has_t=_wins[W_TERM].visible&&!_wins[W_TERM].minimized
                          &&_focus==W_TERM;
                if(has_t){
                    char ch=evt.text.ch;
                    if(ch>=32&&_tilen<T_COLS){_tinput[_tilen++]=ch;_tinput[_tilen]='\0';}
                }
                break;
            }

            default: break;
            }
        }

        /* Rafraichit toutes les ~0.5s pour horloge + creeper anim */
        uint32_t ct=BIOS_TICKS;
        if(ct-last_tick>=9){last_tick=ct;dirty=1;}

        if(got) dirty=1;
        if(dirty){redraw(win);dirty=0;}
        else __asm__ volatile("pause");
    }
    cmd_reboot();
}
