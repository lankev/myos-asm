#!/usr/bin/env python3
# Ajoute Snake, R-Type et Pong dans kmain.c

with open('kernel/kmain.c', 'r', encoding='utf-8') as f:
    src = f.read()

# ----------------------------------------------------------------
# 1. Declarations anticipees (juste apres sh_nano)
# ----------------------------------------------------------------
src = src.replace(
    'static void win_open(int idx);\nstatic void sh_nano(const char* a);',
    '''static void win_open(int idx);
static void sh_nano(const char* a);
#define W_SNAKE    11
#define W_RTYPE    12
#define W_PONG     13
static void snake_reset(void);
static void draw_snake(sfcml_Window* win, int wi);
static void rtype_reset(void);
static void draw_rtype(sfcml_Window* win, int wi);
static void pong_reset(void);
static void draw_pong(sfcml_Window* win, int wi);''', 1)

# ----------------------------------------------------------------
# 2. NW 11 -> 14
# ----------------------------------------------------------------
src = src.replace('#define NW         11', '#define NW         14', 1)

# ----------------------------------------------------------------
# 3. Ajouter 3 fenetres dans _wins
# ----------------------------------------------------------------
src = src.replace(
    '    {30, 25, 580,395,0,0,"Reseau - MyOS"},\n};',
    '''    {30, 25, 580,395,0,0,"Reseau - MyOS"},
    {80, 50, 660,490,0,0,"Snake"},
    {1,  1,  SCR_W-2,SCR_H-33,0,0,"R-Type"},
    {150,50, 500,430,0,0,"Pong"},
};''', 1)

# ----------------------------------------------------------------
# 4. Tableau Z-order (11 entrees -> 14)
# ----------------------------------------------------------------
src = src.replace(
    'static int _z[NW]={0,1,2,3,4,5,6,7,8,9,10};',
    'static int _z[NW]={0,1,2,3,4,5,6,7,8,9,10,11,12,13};', 1)

# ----------------------------------------------------------------
# 5. Icones : 9 -> 12, espacement 45px, libelles
# ----------------------------------------------------------------
src = src.replace('#define NICONS 9', '#define NICONS 12', 1)
src = src.replace(
    'static const int   IC_Y[NICONS]={8,58,108,158,208,258,308,358,408};',
    'static const int   IC_Y[NICONS]={8,53,98,143,188,233,278,323,368,413,458,503};', 1)
src = src.replace(
    'static const char* IC_LBL[NICONS]={"Terminal","Paint","Code","Word","Creeper","A propos","Reboot","Params","Browser"};',
    'static const char* IC_LBL[NICONS]={"Terminal","Paint","Code","Word","Creeper","A propos","Reboot","Params","Browser","Snake","R-Type","Pong"};', 1)

# ----------------------------------------------------------------
# 6. Rendu : dispatch dessin jeux
# ----------------------------------------------------------------
src = src.replace(
    '        else if(i==W_NETMGR)    draw_netmgr(win,i);\n    }',
    '''        else if(i==W_NETMGR)    draw_netmgr(win,i);
        else if(i==W_SNAKE)     draw_snake(win,i);
        else if(i==W_RTYPE)     draw_rtype(win,i);
        else if(i==W_PONG)      draw_pong(win,i);
    }''', 1)

# ----------------------------------------------------------------
# 7. Boucle principale : tick jeux
# ----------------------------------------------------------------
src = src.replace(
    '        if(_sleeping){ draw_screensaver(win); }',
    '''        if(!_sleeping){
            if(_wins[W_SNAKE].visible&&!_wins[W_SNAKE].minimized){snake_step();dirty=1;}
            if(_wins[W_RTYPE].visible&&!_wins[W_RTYPE].minimized){rtype_step();dirty=1;}
            if(_wins[W_PONG].visible&&!_wins[W_PONG].minimized){pong_step();dirty=1;}
        }
        if(_sleeping){ draw_screensaver(win); }''', 1)

# ----------------------------------------------------------------
# 8. KEY_PRESSED : ajout gestion jeux
# ----------------------------------------------------------------
src = src.replace(
    '''                } else if(fopen&&foc==W_WORD){
                    if(evt.key.code==SFCML_KEY_ESCAPE)_wins[W_WORD].minimized=1;
                    else word_key(evt.key.code);
                } else {''',
    '''                } else if(fopen&&foc==W_WORD){
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
                } else {''', 1)

# ----------------------------------------------------------------
# 9. KEY_RELEASED : nouveau case pour R-Type et Pong
# ----------------------------------------------------------------
src = src.replace(
    '            case SFCML_EVT_MOUSE_RELEASED:\n                _drag_win=-1;_painting=0;\n                break;',
    '''            case SFCML_EVT_MOUSE_RELEASED:
                _drag_win=-1;_painting=0;
                break;

            case SFCML_EVT_KEY_RELEASED:{
                int foc=_focus;
                int fopen=(foc>=0&&_wins[foc].visible&&!_wins[foc].minimized);
                if(fopen&&foc==W_RTYPE)rtype_key_release(evt.key.code);
                else if(fopen&&foc==W_PONG)pong_key_release(evt.key.code);
                break;
            }''', 1)

# ----------------------------------------------------------------
# 10. Commandes terminal : snake, rtype, pong
# ----------------------------------------------------------------
src = src.replace(
    '    else if(!strcmp(inp,"clear")||!strcmp(inp,"cls"))',
    '''    else if(!strcmp(inp,"snake")){win_open(W_SNAKE);snake_reset();}
    else if(!strcmp(inp,"rtype")){win_open(W_RTYPE);rtype_reset();}
    else if(!strcmp(inp,"pong")){win_open(W_PONG);pong_reset();}
    else if(!strcmp(inp,"clear")||!strcmp(inp,"cls"))''', 1)

# ----------------------------------------------------------------
# 11. Double-clic icone : Snake(9), R-Type(10), Pong(11)
# ----------------------------------------------------------------
src = src.replace(
    '            else{win_open(W_BROWSER);_bnav(0);}',
    '''            else if(found==9){win_open(W_SNAKE);snake_reset();}
            else if(found==10){win_open(W_RTYPE);rtype_reset();}
            else if(found==11){win_open(W_PONG);pong_reset();}
            else{win_open(W_BROWSER);_bnav(0);}''', 1)

# ----------------------------------------------------------------
# 12. Code des jeux (insere avant kmain)
# ----------------------------------------------------------------
GAME_CODE = r"""
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

"""

ANCHOR = '/* ============================================================\n * Point d\'entree\n * ============================================================ */\nvoid kmain(void){'
src = src.replace(ANCHOR, GAME_CODE + ANCHOR, 1)

with open('kernel/kmain.c', 'w', encoding='utf-8') as f:
    f.write(src)

print("Jeux ajoutes avec succes!")
