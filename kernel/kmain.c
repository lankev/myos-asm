#include <sfcml.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "font8x8.h"

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
    sfcml_Color bk=SFCML_BLACK,rd=sfcml_rgb(220,20,20);
    sfcml_Color gr=sfcml_rgb(150,150,150),wh=SFCML_WHITE;
    sfcml_fillRect(win,sfcml_rect(0,0,640,480),bk);
    draw_big_text(win,"EPITECH",208,158,4,rd,bk);
    sfcml_fillRect(win,sfcml_rect(208,200,224,3),rd);
    sfcml_drawText(win,"L'expertise informatique - epitech.eu",165,218,gr,bk);
    sfcml_drawText(win,"Epitech Technology  |  Barcelone, Espagne",196,462,gr,bk);
    sfcml_drawText(win,"Demarrage du systeme...",232,240,wh,bk);
    int bx=200,by=390,bw=240,bh=14;
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

/* Commandes shell */
static void cmd_reboot(void){
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)0xFE),"Nd"((uint16_t)0x64));
    for(;;)__asm__ volatile("hlt");
}
static void cmd_help(void){
    t_print("Commandes: help clear about ls uname\n");
    t_print("  time ping sudo echo fortune creeper\n");
    t_print("  color matrix reboot shutdown\n");
}
static void cmd_about(void){
    t_print("=== MyOS v0.1 - Epitech Edition ===\n");
    t_print("CPU: x86 32-bit  RAM: 128MB\n");
    t_print("GPU: VESA 640x480 24bpp\n");
    t_print("Boot: MineGRUB  Libs: libk+SFCML\n");
}
static void cmd_ls(void){
    t_print("drwxr-xr-x  bin/ boot/ dev/ etc/\n");
    t_print("-rwxr-xr-x  kernel.bin  initrd\n");
    t_print("drwxr-xr-x  usr/ var/ home/ sys/\n");
}
static void cmd_time(void){
    uint32_t s=(uint32_t)_rtc.h*3600+(uint32_t)_rtc.m*60+_rtc.s;
    char b[32];int o=0;
    b[o++]='U';b[o++]='p';b[o++]=':';b[o++]=' ';
    b[o++]='0'+s/3600/10;b[o++]='0'+s/3600%10;b[o++]='h';
    b[o++]='0'+(s/60)%60/10;b[o++]='0'+(s/60)%60%10;b[o++]='m';
    b[o++]='0'+s%60/10;b[o++]='0'+s%60%10;b[o++]='s';
    b[o++]='\n';b[o]='\0';t_print(b);
}
static void cmd_ping(const char* a){
    t_print("PING ");t_print(a[0]?a:"epitech.eu");t_print("\n");
    t_print("64 bytes: icmp_seq=1 ttl=64 time=0.1ms\n");
    t_print("2 paquets: 0% perte\n");
}
static void cmd_fortune(void){
    static int fi=0;
    static const char* f[4]={"Creeper, Aw Man...\n","make: Error 1\n",
        "rm -rf /* : Ouf.\n","42h sans dormir = 1 semaine.\n"};
    t_print(f[fi++%4]);
}
static void cmd_color(const char* a){
    if(!strncmp(a,"red",3))C_FG=sfcml_rgb(255,80,80);
    else if(!strncmp(a,"green",5))C_FG=SFCML_GREEN;
    else if(!strncmp(a,"cyan",4))C_FG=SFCML_CYAN;
    else if(!strncmp(a,"white",5))C_FG=SFCML_WHITE;
    else if(!strncmp(a,"yellow",6))C_FG=SFCML_YELLOW;
    else{t_print("Usage: color red|green|cyan|white|yellow\n");return;}
    t_print("Couleur changee.\n");
}
static void handle_command(void){
    t_hist_add();
    t_print("$ ");t_print(_tinput);t_print("\n");
    char* inp=_tinput;
    if(!strcmp(inp,"help"))cmd_help();
    else if(!strcmp(inp,"clear")){for(int i=0;i<T_ROWS;i++)_tlines[i][0]='\0';_tnlines=0;}
    else if(!strcmp(inp,"about"))cmd_about();
    else if(!strcmp(inp,"ls")||!strncmp(inp,"ls ",3))cmd_ls();
    else if(!strcmp(inp,"uname")||!strcmp(inp,"uname -a"))t_print("MyOS 0.1 i386 GNU/Epitech\n");
    else if(!strcmp(inp,"time"))cmd_time();
    else if(!strcmp(inp,"ping")||!strncmp(inp,"ping ",5))cmd_ping(strlen(inp)>5?inp+5:"");
    else if(!strcmp(inp,"sudo")||!strncmp(inp,"sudo ",5))t_print("Permission accordee. Vous etes root.\n");
    else if(!strncmp(inp,"echo ",5)){t_print(inp+5);t_print("\n");}
    else if(!strcmp(inp,"fortune"))cmd_fortune();
    else if(!strcmp(inp,"creeper"))t_print("Creeper, Aw Man...\nGot our pickaxe swinging!\n");
    else if(!strncmp(inp,"color ",6))cmd_color(inp+6);
    else if(!strcmp(inp,"matrix")){t_print("Wake up, Neo...\nThe Matrix has you.\n");}
    else if(!strcmp(inp,"shutdown")||!strcmp(inp,"poweroff")){t_print("Arret...\n");cmd_reboot();}
    else if(!strcmp(inp,"reboot")){t_print("Reboot...\n");cmd_reboot();}
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
}

/* ============================================================
 * Systeme de fenetres
 * ============================================================ */
#define TBAR_H  22
#define BTN_W   16
#define BTN_H   14

typedef struct{int x,y,w,h,visible,minimized;const char* title;}AppWin;

#define NW         7
#define W_TERM     0
#define W_ABOUT    1
#define W_CREEP    2
#define W_PAINT    3
#define W_CODE     4
#define W_WORD     5
#define W_SETTINGS 6

static AppWin _wins[NW]={
    {70, 36, 504,320,0,0,"Terminal - root@myos"},
    {160,80, 320,210,0,0,"A propos de MyOS"},
    {240,70, 216,224,0,0,"Creeper!"},
    {1,  1,  638,447,0,0,"Epitech Paint"},
    {1,  1,  638,447,0,0,"Epitech Code Editor"},
    {1,  1,  638,447,0,0,"Epitech Word"},
    {40, 20, 560,410,0,0,"Parametres"},
};
static int _z[NW]={0,1,2,3,4,5,6};
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
static void win_front7(int idx){win_front(idx);}

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

#define NICONS 8
static const int   IC_Y[NICONS]={8,58,108,158,208,258,308,358};
static const char* IC_LBL[NICONS]={"Terminal","Paint","Code","Word","Creeper","A propos","Reboot","Params"};
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
    sfcml_fillRect(win,sfcml_rect(w->x,w->y,w->w,TBAR_H),tbar);
    sfcml_drawText(win,w->title,w->x+6,w->y+7,SFCML_WHITE,tbar);
    sfcml_fillRect(win,sfcml_rect(w->x+w->w-2*BTN_W-8,w->y+4,BTN_W,BTN_H),sfcml_rgb(200,160,0));
    sfcml_drawHLine(win,w->x+w->w-2*BTN_W-6,w->y+11,BTN_W-4,SFCML_WHITE);
    sfcml_fillRect(win,sfcml_rect(w->x+w->w-BTN_W-4,w->y+4,BTN_W,BTN_H),sfcml_rgb(196,40,30));
    sfcml_drawText(win,"x",w->x+w->w-BTN_W-1,w->y+5,SFCML_WHITE,sfcml_rgb(196,40,30));
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
    sfcml_drawText(win,"Epitech Word",sx+6,sy+10,rd,tbg);
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
        default: /* Parametres (engrenage) */
            sfcml_fillRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),sfcml_rgb(30,30,44));
            sfcml_drawRect(win,sfcml_rect(ix,iy,IC_SZ,IC_SZ),hov?SFCML_WHITE:sfcml_rgb(70,70,100));
            sfcml_drawCircle(win,ix+16,iy+16,7,hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            sfcml_drawCircle(win,ix+16,iy+16,4,sfcml_rgb(30,30,44));
            sfcml_fillRect(win,sfcml_rect(ix+14,iy+4,4,4),hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            sfcml_fillRect(win,sfcml_rect(ix+14,iy+24,4,4),hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            sfcml_fillRect(win,sfcml_rect(ix+4,iy+14,4,4),hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
            sfcml_fillRect(win,sfcml_rect(ix+24,iy+14,4,4),hov?sfcml_rgb(200,200,255):sfcml_rgb(150,150,200));
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
#define SM_N    12
#define SM_H   (SM_N*SM_IH+8)
#define SM_Y   (450-SM_H)

static const char* SM_LBL[SM_N]={
    "  Terminal","  Creeper!","  A propos","  ---------",
    "  Paint","  Code Editor","  Word",
    "  Parametres",
    "  ---------","  Redemarrer","  Eteindre","  ---------",
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
    else if(item==7)win_open(W_SETTINGS);
    else if(item==9||item==10)cmd_reboot();
}

/* ============================================================
 * Menu clic droit
 * ============================================================ */
#define RC_W  150
#define RC_IH  18
#define RC_N    7
static const char* RC_LBL[RC_N]={
    " Terminal"," Paint"," Code Editor"," ---------"," Parametres"," A propos"," Redemarrer",
};
static void draw_rcmenu(sfcml_Window* win){
    int mh=RC_N*RC_IH+4;
    int x0=_rcx,y0=_rcy;
    if(x0+RC_W>640)x0=640-RC_W;
    if(y0+mh>450)y0=450-mh;
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
    if(x0+RC_W>640)x0=640-RC_W;
    if(y0+mh>450)y0=450-mh;
    _rcopen=0;
    if(mx<x0||mx>=x0+RC_W||my<y0||my>=y0+mh)return;
    int item=(my-y0-2)/RC_IH;
    if(item==0)win_open(W_TERM);
    else if(item==1)win_open(W_PAINT);
    else if(item==2)win_open(W_CODE);
    else if(item==4)win_open(W_SETTINGS);
    else if(item==5)win_open(W_ABOUT);
    else if(item==6)cmd_reboot();
}

/* ============================================================
 * Taskbar
 * ============================================================ */
#define TB_BW 82  /* button width */
static void draw_taskbar(sfcml_Window* win){
    sfcml_fillRect(win,sfcml_rect(0,449,640,31),C_TB);
    sfcml_drawHLine(win,0,449,640,sfcml_rgb(0,90,170));
    /* Start */
    sfcml_Color sc=_start_open?sfcml_rgb(160,5,5):sfcml_rgb(215,20,20);
    sfcml_fillRect(win,sfcml_rect(2,451,84,26),sc);
    sfcml_drawRect(win,sfcml_rect(2,451,84,26),sfcml_rgb(120,4,4));
    sfcml_drawText(win,"Demarrer",8,460,SFCML_WHITE,sc);
    /* Window buttons */
    int bx=88;
    for(int i=0;i<NW;i++){
        if(!_wins[i].visible)continue;
        int foc=(_focus==i);
        sfcml_Color bc=_wins[i].minimized?sfcml_rgb(50,50,68):(foc?sfcml_rgb(0,100,200):sfcml_rgb(38,38,58));
        sfcml_fillRect(win,sfcml_rect(bx,452,TB_BW,24),bc);
        sfcml_drawRect(win,sfcml_rect(bx,452,TB_BW,24),sfcml_rgb(80,80,110));
        sfcml_Color tc=_wins[i].minimized?sfcml_rgb(140,140,160):SFCML_WHITE;
        sfcml_drawText(win,_wins[i].title,bx+4,460,tc,bc);
        bx+=TB_BW+2;
    }
    /* Horloge RTC */
    char clk[9];
    clk[0]='0'+_rtc.h/10;clk[1]='0'+_rtc.h%10;clk[2]=':';
    clk[3]='0'+_rtc.m/10;clk[4]='0'+_rtc.m%10;clk[5]=':';
    clk[6]='0'+_rtc.s/10;clk[7]='0'+_rtc.s%10;clk[8]='\0';
    /* Date  DD/MM/YYYY */
    char dat[11];
    dat[0]='0'+_rtc.day/10; dat[1]='0'+_rtc.day%10; dat[2]='/';
    dat[3]='0'+_rtc.mon/10; dat[4]='0'+_rtc.mon%10; dat[5]='/';
    dat[6]='0'+(_rtc.year/1000)%10; dat[7]='0'+(_rtc.year/100)%10;
    dat[8]='0'+(_rtc.year/10)%10;   dat[9]='0'+_rtc.year%10; dat[10]='\0';
    sfcml_drawText(win,dat, 490,453,sfcml_rgb(180,180,210),C_TB);
    sfcml_drawText(win,clk, 494,463,SFCML_WHITE,C_TB);
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
        sfcml_drawText(win,"Ecran de veille : horloge sur fond noir",cx+10,y,sfcml_rgb(120,120,150),hbg);
        y+=14;
        sfcml_drawText(win,"Toute touche ou clic reveil l'ecran.",cx+10,y,sfcml_rgb(120,120,150),hbg);
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
        for(int i=0;i<5;i++){
            int bx=cx2+10+i*78;
            if(mx>=bx&&mx<bx+70&&my>=y2&&my<y2+28){_veille_sel=i;_inact_secs=0;return;}
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
static void draw_screensaver(sfcml_Window* win){
    sfcml_fillRect(win,sfcml_rect(0,0,640,480),SFCML_BLACK);
    /* HH:MM grande taille centree */
    char hm[6];
    hm[0]='0'+_rtc.h/10;hm[1]='0'+_rtc.h%10;hm[2]=':';
    hm[3]='0'+_rtc.m/10;hm[4]='0'+_rtc.m%10;hm[5]='\0';
    /* 5 chars * 8px * scale5 = 200px  => centre x=(640-200)/2=220 */
    draw_big_text(win,hm,220,170,5,sfcml_rgb(0,140,255),SFCML_BLACK);
    /* secondes */
    char ss[3];ss[0]='0'+_rtc.s/10;ss[1]='0'+_rtc.s%10;ss[2]='\0';
    draw_big_text(win,ss,298,245,3,sfcml_rgb(30,80,160),SFCML_BLACK);
    /* date */
    char dat[11];
    dat[0]='0'+_rtc.day/10;dat[1]='0'+_rtc.day%10;dat[2]='/';
    dat[3]='0'+_rtc.mon/10;dat[4]='0'+_rtc.mon%10;dat[5]='/';
    dat[6]='0'+(_rtc.year/1000)%10;dat[7]='0'+(_rtc.year/100)%10;
    dat[8]='0'+(_rtc.year/10)%10;dat[9]='0'+_rtc.year%10;dat[10]='\0';
    sfcml_drawText(win,dat,272,290,sfcml_rgb(50,60,90),SFCML_BLACK);
    /* epitech */
    sfcml_drawText(win,"Epitech Technology",224,430,sfcml_rgb(30,10,10),SFCML_BLACK);
    sfcml_drawText(win,"Cliquez pour reprendre",212,442,sfcml_rgb(25,25,40),SFCML_BLACK);
    sfcml_present(win);
}

/* ============================================================
 * Fond bureau (degrade)
 * ============================================================ */
static void draw_bg(sfcml_Window* win){
    for(int i=0;i<9;i++){
        int y=i*50,h=(i<8)?50:49;
        sfcml_fillRect(win,sfcml_rect(0,y,640,h),sfcml_rgb(
            (uint8_t)((int)C_DK.r*(i+1)/9),
            (uint8_t)((int)C_DK.g*(i+1)/9),
            (uint8_t)((int)C_DK.b*(i+1)/9)));
    }
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
    if(btn==1){_rcopen=1;_rcx=mx;_rcy=my;_start_open=0;return;}
    if(_rcopen){rcmenu_click(mx,my);return;}
    if(_start_open){smenu_click(mx,my);return;}
    if(mx>=2&&mx<86&&my>=451&&my<477){_start_open=!_start_open;return;}
    /* Taskbar window buttons */
    if(my>=451&&my<477){
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
        if(hit_x(i,mx,my)){w->visible=0;return;}
        if(hit_mn(i,mx,my)){w->minimized=1;return;}
        if(hit_tb(i,mx,my)){_drag_win=i;_drag_ox=mx-w->x;_drag_oy=my-w->y;return;}
        if(i==W_PAINT){paint_click(mx,my);return;}
        if(i==W_SETTINGS){settings_click(mx,my);return;}
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
            else win_open(W_SETTINGS);
        }
        _sel_icon=found;
    } else {_sel_icon=-1;_start_open=0;_rcopen=0;}
}

/* ============================================================
 * Point d'entree
 * ============================================================ */
void kmain(void){
    if(!sfcml_init())for(;;)__asm__ volatile("hlt");
    sfcml_Window* win=sfcml_createWindow("MyOS");
    win->font=(uint8_t*)font8x8;
    sfcml_mouseInit();
    rtc_read();

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
                    if(w->x<0)w->x=0;if(w->x+w->w>640)w->x=640-w->w;
                    if(w->y<0)w->y=0;if(w->y+w->h>448)w->y=448-w->h;
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

            case SFCML_EVT_KEY_PRESSED:{
                int foc=_focus;
                int fopen=(foc>=0&&_wins[foc].visible&&!_wins[foc].minimized);
                if(fopen&&foc==W_SETTINGS){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_SETTINGS].minimized=1;
                } else if(fopen&&foc==W_CODE){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_CODE].minimized=1;
                    else code_key(evt.key.code);
                } else if(fopen&&foc==W_WORD){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_WORD].minimized=1;
                    else word_key(evt.key.code);
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
                if(fopen&&foc==W_CODE)code_text(ch);
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
        if(_sleeping){ draw_screensaver(win); }
        else if(dirty){ redraw(win);dirty=0; }
        else __asm__ volatile("pause");
    }
    cmd_reboot();
}
