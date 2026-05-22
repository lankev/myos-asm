#!/usr/bin/env python3
"""Patch kmain.c: 800x600 screen + dynamic VFS + nano command."""
import re

PATH = r"c:\Users\towoga\Downloads\myos_asm (1)\myos_asm\kernel\kmain.c"

with open(PATH, 'r', encoding='utf-8') as f:
    src = f.read()

# ──────────────────────────────────────────────────────────────────────
# 1. Add SCR_W / SCR_H / TB_Y defines right after the #includes block
# ──────────────────────────────────────────────────────────────────────
DEFINES = """
/* ============================================================
 * Resolution ecran
 * ============================================================ */
#define SCR_W  800
#define SCR_H  600
#define TB_Y   (SCR_H - 31)   /* y de la taskbar */
"""

# Insert after #include "../net/net.h"
src = src.replace(
    '#include "../net/net.h"\n',
    '#include "../net/net.h"\n' + DEFINES
)

# ──────────────────────────────────────────────────────────────────────
# 2. draw_splash  – use win->width / win->height for full clear + center
# ──────────────────────────────────────────────────────────────────────
OLD_SPLASH = '''static void draw_splash(sfcml_Window* win){
    sfcml_Color bk=SFCML_BLACK,rd=sfcml_rgb(220,20,20);
    sfcml_Color gr=sfcml_rgb(150,150,150),wh=SFCML_WHITE;
    sfcml_fillRect(win,sfcml_rect(0,0,640,480),bk);
    draw_big_text(win,"EPITECH",208,158,4,rd,bk);
    sfcml_fillRect(win,sfcml_rect(208,200,224,3),rd);
    sfcml_drawText(win,"L\'expertise informatique - epitech.eu",165,218,gr,bk);
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
}'''

NEW_SPLASH = '''static void draw_splash(sfcml_Window* win){
    int sw=(int)win->width,sh=(int)win->height;
    int cx=sw/2,cy=sh/2;
    sfcml_Color bk=SFCML_BLACK,rd=sfcml_rgb(220,20,20);
    sfcml_Color gr=sfcml_rgb(150,150,150),wh=SFCML_WHITE;
    sfcml_fillRect(win,sfcml_rect(0,0,sw,sh),bk);
    draw_big_text(win,"EPITECH",cx-112,cy-122,4,rd,bk);
    sfcml_fillRect(win,sfcml_rect(cx-112,cy-82,224,3),rd);
    sfcml_drawText(win,"L\'expertise informatique - epitech.eu",cx-148,cy-62,gr,bk);
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
}'''

src = src.replace(OLD_SPLASH, NEW_SPLASH)

# ──────────────────────────────────────────────────────────────────────
# 3. Full-screen window sizes (Paint, Code, Word, Browser, Settings)
# ──────────────────────────────────────────────────────────────────────
src = src.replace(
    '{1,  1,  638,447,0,0,"Epitech Paint"},',
    '{1,  1,  SCR_W-2,SCR_H-33,0,0,"Epitech Paint"},')
src = src.replace(
    '{1,  1,  638,447,0,0,"Epitech Code Editor"},',
    '{1,  1,  SCR_W-2,SCR_H-33,0,0,"Epitech Code Editor"},')
src = src.replace(
    '{1,  1,  638,447,0,0,"Epitech Word"},',
    '{1,  1,  SCR_W-2,SCR_H-33,0,0,"Epitech Word"},')
src = src.replace(
    '{45, 0,  595,449,0,0,"MyBrowser"},',
    '{1,  0,  SCR_W-2,SCR_H-31,0,0,"MyBrowser"},')
src = src.replace(
    '{40, 20, 560,410,0,0,"Parametres"},',
    '{40, 20, SCR_W-80,SCR_H-70,0,0,"Parametres"},')

# ──────────────────────────────────────────────────────────────────────
# 4. SM_Y  (#define using runtime constant → convert to computed)
# ──────────────────────────────────────────────────────────────────────
src = src.replace(
    '#define SM_Y   (450-SM_H)',
    '#define SM_Y   (TB_Y-SM_H)')

# ──────────────────────────────────────────────────────────────────────
# 5. draw_bg – 640 → SCR_W, 448 → (SCR_H-32)
# ──────────────────────────────────────────────────────────────────────
OLD_DRAWBG = '''static void draw_bg(sfcml_Window* win){
    for(int i=0;i<9;i++){
        int y=i*50,h=(i<8)?50:49;
        sfcml_fillRect(win,sfcml_rect(0,y,640,h),sfcml_rgb(
            (uint8_t)((int)C_DK.r*(i+1)/9),
            (uint8_t)((int)C_DK.g*(i+1)/9),
            (uint8_t)((int)C_DK.b*(i+1)/9)));
    }
    for(int y=0;y<448;y+=20)
        for(int x=44;x<640;x+=20)
            sfcml_drawPixel(win,x,y,sfcml_rgb(
                (uint8_t)((int)C_DK.r/4+20<255?(int)C_DK.r/4+20:255),
                (uint8_t)((int)C_DK.g/4+20<255?(int)C_DK.g/4+20:255),
                (uint8_t)((int)C_DK.b/4+20<255?(int)C_DK.b/4+20:255)));
}'''

NEW_DRAWBG = '''static void draw_bg(sfcml_Window* win){
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
}'''

src = src.replace(OLD_DRAWBG, NEW_DRAWBG)

# ──────────────────────────────────────────────────────────────────────
# 6. draw_taskbar – replace all hardcoded pixel positions
# ──────────────────────────────────────────────────────────────────────
OLD_TB = '''    for(int i=0;i<31;i++){
        int t=i*100/31;
        sfcml_Color gc=sfcml_rgb(
            (uint8_t)((int)C_TB.r*(100-t*30/100)/100),
            (uint8_t)((int)C_TB.g*(100-t*30/100)/100),
            (uint8_t)((int)C_TB.b*(100-t*30/100)/100));
        sfcml_drawHLine(win,0,449+i,640,gc);
    }
    sfcml_drawHLine(win,0,449,640,sfcml_rgb(0,90,170));
    sfcml_Color sc=_start_open?sfcml_rgb(160,5,5):sfcml_rgb(215,20,20);
    sfcml_fillRect(win,sfcml_rect(2,451,84,26),sc);
    sfcml_drawRect(win,sfcml_rect(2,451,84,26),sfcml_rgb(120,4,4));
    sfcml_fillRect(win,sfcml_rect(6,455,7,7),sfcml_rgb(255,80,80));
    sfcml_fillRect(win,sfcml_rect(15,455,7,7),sfcml_rgb(80,200,80));
    sfcml_fillRect(win,sfcml_rect(6,464,7,7),sfcml_rgb(80,80,255));
    sfcml_fillRect(win,sfcml_rect(15,464,7,7),sfcml_rgb(255,200,0));
    sfcml_drawText(win,"Start",26,460,SFCML_WHITE,sc);'''

NEW_TB = '''    for(int i=0;i<31;i++){
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
    sfcml_drawText(win,"Start",26,TB_Y+11,SFCML_WHITE,sc);'''

src = src.replace(OLD_TB, NEW_TB)

OLD_TB2 = '''        sfcml_fillRect(win,sfcml_rect(bx,452,TB_BW,24),bc);
        sfcml_drawRect(win,sfcml_rect(bx,452,TB_BW,24),sfcml_rgb(80,80,110));
        if(foc)sfcml_drawHLine(win,bx,452,TB_BW,sfcml_rgb(100,180,255));
        sfcml_Color tc=_wins[i].minimized?sfcml_rgb(140,140,160):SFCML_WHITE;
        sfcml_drawText(win,_wins[i].title,bx+4,460,tc,bc);'''
NEW_TB2 = '''        sfcml_fillRect(win,sfcml_rect(bx,TB_Y+3,TB_BW,24),bc);
        sfcml_drawRect(win,sfcml_rect(bx,TB_Y+3,TB_BW,24),sfcml_rgb(80,80,110));
        if(foc)sfcml_drawHLine(win,bx,TB_Y+3,TB_BW,sfcml_rgb(100,180,255));
        sfcml_Color tc=_wins[i].minimized?sfcml_rgb(140,140,160):SFCML_WHITE;
        sfcml_drawText(win,_wins[i].title,bx+4,TB_Y+11,tc,bc);'''
src = src.replace(OLD_TB2, NEW_TB2)

OLD_TB3 = '''    int tx=480;
    sfcml_Color nicol=net_ok?sfcml_rgb(80,200,80):sfcml_rgb(160,160,160);
    sfcml_fillRect(win,sfcml_rect(tx,455,18,16),C_TB);
    sfcml_fillRect(win,sfcml_rect(tx+2,464,14,4),nicol);
    sfcml_fillRect(win,sfcml_rect(tx+5,459,8,6),nicol);
    sfcml_fillRect(win,sfcml_rect(tx+8,455,2,5),nicol);
    tx+=22;
    sfcml_fillRect(win,sfcml_rect(tx,457,6,12),C_TB);
    sfcml_fillRect(win,sfcml_rect(tx,461,4,4),sfcml_rgb(180,180,200));
    sfcml_fillTriangle(win,tx+4,457,tx+4,469,tx+10,463,sfcml_rgb(180,180,200));
    tx+=18;
    char clk[9];
    clk[0]='0'+_rtc.h/10;clk[1]='0'+_rtc.h%10;clk[2]=':';
    clk[3]='0'+_rtc.m/10;clk[4]='0'+_rtc.m%10;clk[5]=':';
    clk[6]='0'+_rtc.s/10;clk[7]='0'+_rtc.s%10;clk[8]='\\0';
    char dat[11];
    dat[0]='0'+_rtc.day/10;dat[1]='0'+_rtc.day%10;dat[2]='/';
    dat[3]='0'+_rtc.mon/10;dat[4]='0'+_rtc.mon%10;dat[5]='/';
    dat[6]='0'+(_rtc.year/1000)%10;dat[7]='0'+(_rtc.year/100)%10;
    dat[8]='0'+(_rtc.year/10)%10;dat[9]='0'+_rtc.year%10;dat[10]='\\0';
    sfcml_drawText(win,dat,tx+4,453,sfcml_rgb(180,180,210),C_TB);
    sfcml_drawText(win,clk,tx+8,463,SFCML_WHITE,C_TB);
}'''
NEW_TB3 = '''    int tx=SCR_W-160;
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
    clk[6]='0'+_rtc.s/10;clk[7]='0'+_rtc.s%10;clk[8]='\\0';
    char dat[11];
    dat[0]='0'+_rtc.day/10;dat[1]='0'+_rtc.day%10;dat[2]='/';
    dat[3]='0'+_rtc.mon/10;dat[4]='0'+_rtc.mon%10;dat[5]='/';
    dat[6]='0'+(_rtc.year/1000)%10;dat[7]='0'+(_rtc.year/100)%10;
    dat[8]='0'+(_rtc.year/10)%10;dat[9]='0'+_rtc.year%10;dat[10]='\\0';
    sfcml_drawText(win,dat,tx+4,TB_Y+4,sfcml_rgb(180,180,210),C_TB);
    sfcml_drawText(win,clk,tx+8,TB_Y+14,SFCML_WHITE,C_TB);
}'''
src = src.replace(OLD_TB3, NEW_TB3)

# ──────────────────────────────────────────────────────────────────────
# 7. Right-click menu bounds
# ──────────────────────────────────────────────────────────────────────
src = src.replace(
    'if(x0+RC_W>640)x0=640-RC_W;\n    if(y0+mh>450)y0=450-mh;\n    sfcml_fillRect',
    'if(x0+RC_W>SCR_W)x0=SCR_W-RC_W;\n    if(y0+mh>TB_Y)y0=TB_Y-mh;\n    sfcml_fillRect')
src = src.replace(
    'if(x0+RC_W>640)x0=640-RC_W;\n    if(y0+mh>450)y0=450-mh;\n    _rcopen=0;',
    'if(x0+RC_W>SCR_W)x0=SCR_W-RC_W;\n    if(y0+mh>TB_Y)y0=TB_Y-mh;\n    _rcopen=0;')

# ──────────────────────────────────────────────────────────────────────
# 8. Screensavers
# ──────────────────────────────────────────────────────────────────────
# Clock SS
src = src.replace(
    'static int _clk_x=220,_clk_y=170,_clk_vx=1,_clk_vy=1;',
    'static int _clk_x=260,_clk_y=200,_clk_vx=1,_clk_vy=1;')
src = src.replace(
    '    sfcml_fillRect(win,sfcml_rect(0,0,640,480),SFCML_BLACK);\n    char hm[6];',
    '    sfcml_fillRect(win,sfcml_rect(0,0,SCR_W,SCR_H),SFCML_BLACK);\n    char hm[6];')
src = src.replace(
    '    if(_clk_x<0||_clk_x>440)_clk_vx=-_clk_vx; /* 200px wide=5chars*8*5 */',
    '    if(_clk_x<0||_clk_x>SCR_W-200)_clk_vx=-_clk_vx; /* 200px wide=5chars*8*5 */')
src = src.replace(
    '    if(_clk_y<0||_clk_y>440)_clk_vy=-_clk_vy;',
    '    if(_clk_y<0||_clk_y>SCR_H-40)_clk_vy=-_clk_vy;')

# Tree SS
src = src.replace(
    '    sfcml_fillRect(win,sfcml_rect(0,0,640,480),sfcml_rgb(0,4,2));\n    sfcml_fillRect(win,sfcml_rect(0,458,640,22),sfcml_rgb(5,18,5));',
    '    sfcml_fillRect(win,sfcml_rect(0,0,SCR_W,SCR_H),sfcml_rgb(0,4,2));\n    sfcml_fillRect(win,sfcml_rect(0,SCR_H-22,SCR_W,22),sfcml_rgb(5,18,5));')
src = src.replace(
    '    if(x2<0)x2=0;if(x2>639)x2=639;\n    if(y2<0)y2=0;if(y2>479)y2=479;',
    '    if(x2<0)x2=0;if(x2>SCR_W-1)x2=SCR_W-1;\n    if(y2<0)y2=0;if(y2>SCR_H-1)y2=SCR_H-1;')
src = src.replace(
    '    _branch(win,320,458,90,78,8); /* 8 niveaux de recursion */',
    '    _branch(win,SCR_W/2,SCR_H-22,90,78,8); /* 8 niveaux de recursion */')

# Mandelbrot SS
src = src.replace(
    'static float _mb_cx=-0.5f,_mb_cy=0.0f,_mb_sc=3.5f/480.0f;',
    'static float _mb_cx=-0.5f,_mb_cy=0.0f,_mb_sc=3.5f/SCR_H;')
src = src.replace(
    '    if(_mb_row>=480){\n        /* Zoom vers l\'elephant valley: (-0.7436, 0.1319) */',
    '    if(_mb_row>=SCR_H){\n        /* Zoom vers l\'elephant valley: (-0.7436, 0.1319) */')
src = src.replace(
    '        _mb_cy=_mb_cy*0.85f+( 0.1319f)*0.15f;\n        _mb_sc*=0.78f;\n        _mb_zc++;\n        if(_mb_zc>18){_mb_cx=-0.5f;_mb_cy=0.0f;_mb_sc=3.5f/480.0f;_mb_zc=0;}',
    '        _mb_cy=_mb_cy*0.85f+( 0.1319f)*0.15f;\n        _mb_sc*=0.78f;\n        _mb_zc++;\n        if(_mb_zc>18){_mb_cx=-0.5f;_mb_cy=0.0f;_mb_sc=3.5f/SCR_H;_mb_zc=0;}')
src = src.replace(
    '    /* Rend 24 lignes par appel */\n    int rend=_mb_row+24; if(rend>480)rend=480;\n    for(int row=_mb_row;row<rend;row++){\n        for(int col=0;col<640;col++){',
    '    /* Rend 24 lignes par appel */\n    int rend=_mb_row+24; if(rend>SCR_H)rend=SCR_H;\n    for(int row=_mb_row;row<rend;row++){\n        for(int col=0;col<SCR_W;col++){')
src = src.replace(
    '            float x0=_mb_cx+(col-320)*_mb_sc;\n            float y0=_mb_cy+(row-240)*_mb_sc;',
    '            float x0=_mb_cx+(col-SCR_W/2)*_mb_sc;\n            float y0=_mb_cy+(row-SCR_H/2)*_mb_sc;')

# Stars SS
src = src.replace(
    '    sfcml_fillRect(win,sfcml_rect(0,0,640,480),SFCML_BLACK);\n    if(!_ss_si){',
    '    sfcml_fillRect(win,sfcml_rect(0,0,SCR_W,SCR_H),SFCML_BLACK);\n    if(!_ss_si){')
src = src.replace(
    '    _ssx[i]=(int16_t)((ss_rand()%640)-320);\n    _ssy[i]=(int16_t)((ss_rand()%480)-240);',
    '    _ssx[i]=(int16_t)((ss_rand()%SCR_W)-(SCR_W/2));\n    _ssy[i]=(int16_t)((ss_rand()%SCR_H)-(SCR_H/2));')
src = src.replace(
    '        int sx=_ssx[i]*200/_ssz[i]+320;\n        int sy=_ssy[i]*200/_ssz[i]+240;\n        if(sx<0||sx>=640||sy<0||sy>=480)',
    '        int sx=_ssx[i]*200/_ssz[i]+SCR_W/2;\n        int sy=_ssy[i]*200/_ssz[i]+SCR_H/2;\n        if(sx<0||sx>=SCR_W||sy<0||sy>=SCR_H)')

# Matrix SS
src = src.replace(
    '    if(!_mc_i){\n        for(int i=0;i<SS_MC;i++){\n            _mcy[i]=(uint8_t)(mc_rand()%60);\n            _mcs[i]=(uint8_t)(1+mc_rand()%4);\n            _mcf[i]=0;\n        }\n        _mc_i=1;\n        sfcml_fillRect(win,sfcml_rect(0,0,640,480),SFCML_BLACK);\n    }',
    '    if(!_mc_i){\n        for(int i=0;i<SS_MC;i++){\n            _mcy[i]=(uint8_t)(mc_rand()%(SCR_H/8));\n            _mcs[i]=(uint8_t)(1+mc_rand()%4);\n            _mcf[i]=0;\n        }\n        _mc_i=1;\n        sfcml_fillRect(win,sfcml_rect(0,0,SCR_W,SCR_H),SFCML_BLACK);\n    }')
src = src.replace(
    '        int x=c*8,y=(int)_mcy[c]*8;\n        if(y<480){',
    '        int x=c*8,y=(int)_mcy[c]*8;\n        if(y<SCR_H){')
src = src.replace(
    '        _mcy[c]++;if(_mcy[c]>=60)_mcy[c]=0;',
    '        _mcy[c]++;if(_mcy[c]>=(uint8_t)(SCR_H/8))_mcy[c]=0;')

# ──────────────────────────────────────────────────────────────────────
# 9. Drag bounds in SFCML_EVT_MOUSE_MOVED
# ──────────────────────────────────────────────────────────────────────
src = src.replace(
    '                    if(w->x<0)w->x=0;if(w->x+w->w>640)w->x=640-w->w;\n                    if(w->y<0)w->y=0;if(w->y+w->h>448)w->y=448-w->h;',
    '                    if(w->x<0)w->x=0;if(w->x+w->w>SCR_W)w->x=SCR_W-w->w;\n                    if(w->y<0)w->y=0;if(w->y+w->h>TB_Y)w->y=TB_Y-w->h;')

# ──────────────────────────────────────────────────────────────────────
# 10. on_press – taskbar area check
# ──────────────────────────────────────────────────────────────────────
src = src.replace(
    '    if(mx>=2&&mx<86&&my>=451&&my<477){_start_open=!_start_open;return;}',
    '    if(mx>=2&&mx<86&&my>=TB_Y+2&&my<TB_Y+28){_start_open=!_start_open;return;}')
src = src.replace(
    '    if(my>=451&&my<477){',
    '    if(my>=TB_Y+2&&my<TB_Y+28){')

# ──────────────────────────────────────────────────────────────────────
# 11. DYNAMIC VFS (DVFS) – insert after the static VFS definitions
# ──────────────────────────────────────────────────────────────────────
DVFS_CODE = '''
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
            strncpy(_dvfs[i].path,p,DVFS_PLEN-1);_dvfs[i].path[DVFS_PLEN-1]=\'\\0\';
            _dvfs[i].content[0]=\'\\0\';_dvfs[i].is_dir=is_dir;_dvfs[i].used=1;
            return &_dvfs[i];
        }
    }
    return 0;
}
static void _dvfs_remove(const char* p){DVFSEntry*e=_dvfs_find(p);if(e)e->used=0;}
static void _dvfs_fullpath(const char* name,char* out,int olen){
    if(name[0]==\'/\'){strncpy(out,name,olen-1);out[olen-1]=\'\\0\';return;}
    int cl=(int)strlen(_cwd);strncpy(out,_cwd,olen-1);out[olen-1]=\'\\0\';
    if(out[cl-1]!=\'/\'&&cl<olen-2){out[cl]=\'/\';out[cl+1]=\'\\0\';cl++;}
    strncat(out,name,(size_t)(olen-cl-1));
}
static int _dvfs_in_dir(const DVFSEntry* e,int dir_idx){
    const char* dir=_sh_dirs[dir_idx];
    int dl=(int)strlen(dir),el=(int)strlen(e->path);
    if(el<=dl)return 0;
    if(strncmp(e->path,dir,(size_t)dl)!=0)return 0;
    if(dir[dl-1]==\'/\')return !strchr(e->path+dl,\'/\');
    if(e->path[dl]!=\'/\')return 0;
    return !strchr(e->path+dl+1,\'/\');
}
static const char* _dvfs_basename2(const DVFSEntry* e,int dir_idx){
    const char* dir=_sh_dirs[dir_idx];
    int dl=(int)strlen(dir);
    if(dir[dl-1]==\'/\')return e->path+dl;
    return e->path+dl+1;
}

/* Path du fichier ouvert dans nano */
static char _nano_path[DVFS_PLEN]="";

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
        if(i<_word_nl-1)e->content[o++]=\'\\n\';
    }
    e->content[o]=\'\\0\';
}

/* Ouvre nano avec un fichier du VFS */
static void sh_nano(const char* a){
    if(!a[0]){t_print("Usage: nano <fichier>\\n");return;}
    char fp[DVFS_PLEN];_dvfs_fullpath(a,fp,DVFS_PLEN);
    DVFSEntry* e=_dvfs_create(fp,0);
    strncpy(_nano_path,fp,DVFS_PLEN-1);_nano_path[DVFS_PLEN-1]=\'\\0\';
    _word_nl=0;_word_cl=0;_word_cc=0;_word_sc=0;
    if(e&&e->content[0]){
        const char* c=e->content;
        while(*c&&_word_nl<WORD_ROWS){
            int li=0;
            while(*c&&*c!=\'\\n\'&&li<WORD_COLS)_word[_word_nl][li++]=*c++;
            _word[_word_nl][li]=\'\\0\';_word_nl++;
            if(*c==\'\\n\')c++;
        }
    }
    if(_word_nl==0){_word[0][0]=\'\\0\';_word_nl=1;}
    _word_inited=1;
    win_open(W_WORD);
    t_print("nano: ");t_print(fp);t_print(" (Echap pour fermer)\\n");
}

'''

# Insert DVFS code after _cwd definition
src = src.replace(
    'static int  _cwd_idx=0;\nstatic char _cwd[64]="/";',
    'static int  _cwd_idx=0;\nstatic char _cwd[64]="/";\n' + DVFS_CODE)

# ──────────────────────────────────────────────────────────────────────
# 12. Auto-save in word_text and word_key (when _nano_path is set)
# ──────────────────────────────────────────────────────────────────────
# After word_text appends a character, save
src = src.replace(
    '''static void word_text(char ch){
    if(ch<32)return;
    char* cur=_word[_word_cl];int len=(int)strlen(cur);
    if(len>=WORD_COLS)return;
    memmove(cur+_word_cc+1,cur+_word_cc,len-_word_cc+1);
    cur[_word_cc++]=ch;
}''',
    '''static void word_text(char ch){
    if(ch<32)return;
    char* cur=_word[_word_cl];int len=(int)strlen(cur);
    if(len>=WORD_COLS)return;
    memmove(cur+_word_cc+1,cur+_word_cc,len-_word_cc+1);
    cur[_word_cc++]=ch;
    _nano_save();
}''')

# ──────────────────────────────────────────────────────────────────────
# 13. sh_ls – also show DVFS entries
# ──────────────────────────────────────────────────────────────────────
OLD_SHLS = '''static void sh_ls(const char*a){
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
        t_print(fn);t_print("\\n");
    }
}'''

NEW_SHLS = '''static void sh_ls(const char*a){
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
        t_print(fn);t_print("\\n");
    }
    /* Fichiers dynamiques */
    for(int i=0;i<DVFS_MAX;i++){
        if(!_dvfs[i].used)continue;
        if(!_dvfs_in_dir(&_dvfs[i],dir))continue;
        const char* bn=_dvfs_basename2(&_dvfs[i],dir);
        if(!bn||!bn[0])continue;
        t_print(_dvfs[i].is_dir?"drwxr-xr-x ":"-rw-r--r-- ");
        t_print(bn);t_print("\\n");
    }
}'''

src = src.replace(OLD_SHLS, NEW_SHLS)

# ──────────────────────────────────────────────────────────────────────
# 14. sh_cat – check DVFS first
# ──────────────────────────────────────────────────────────────────────
OLD_SHCAT_END = '''    /* fallback: search vfs */
    for(int i=0;i<SH_NDIRS;i++)
        for(int j=0;j<8;j++)
            if(_sh_files[i][j].n[0]&&!strcmp(_sh_files[i][j].n,a)){
                t_print("(fichier binaire)\\n");return;}
    t_print(a);t_print(": Aucun fichier ou repertoire\\n");
}'''

NEW_SHCAT_END = '''    /* Fichier dynamique VFS */
    {char fp[DVFS_PLEN];_dvfs_fullpath(a,fp,DVFS_PLEN);
    DVFSEntry*e=_dvfs_find(fp);
    if(!e)e=_dvfs_find(a);
    if(e&&!e->is_dir){
        if(e->content[0]){t_print(e->content);t_print("\\n");}
        else t_print("(fichier vide)\\n");
        return;
    }}
    /* fallback: search vfs */
    for(int i=0;i<SH_NDIRS;i++)
        for(int j=0;j<8;j++)
            if(_sh_files[i][j].n[0]&&!strcmp(_sh_files[i][j].n,a)){
                t_print("(fichier binaire)\\n");return;}
    t_print(a);t_print(": Aucun fichier ou repertoire\\n");
}'''

src = src.replace(OLD_SHCAT_END, NEW_SHCAT_END)

# ──────────────────────────────────────────────────────────────────────
# 15. handle_command – touch / mkdir / rm use DVFS, add nano, echo >
# ──────────────────────────────────────────────────────────────────────
# touch
src = src.replace(
    '    else if(!strcmp(inp,"touch")||_sh_sw(inp,"touch "))\n        {if(arg[0]){t_print("touch: ");t_print(arg);t_print(": ok\\n");}else t_print("Usage: touch <nom>\\n");}',
    '    else if(!strcmp(inp,"touch")||_sh_sw(inp,"touch "))\n        {if(arg[0]){char fp[DVFS_PLEN];_dvfs_fullpath(arg,fp,DVFS_PLEN);_dvfs_create(fp,0);t_print("touch: ");t_print(arg);t_print(": cree\\n");}else t_print("Usage: touch <nom>\\n");}')

# mkdir
src = src.replace(
    '    else if(!strcmp(inp,"mkdir")||_sh_sw(inp,"mkdir "))\n        {if(arg[0]){t_print("mkdir: ");t_print(arg);t_print(": cree\\n");}else t_print("Usage: mkdir <nom>\\n");}',
    '    else if(!strcmp(inp,"mkdir")||_sh_sw(inp,"mkdir "))\n        {if(arg[0]){char fp[DVFS_PLEN];_dvfs_fullpath(arg,fp,DVFS_PLEN);_dvfs_create(fp,1);t_print("mkdir: ");t_print(arg);t_print(": cree\\n");}else t_print("Usage: mkdir <nom>\\n");}')

# rm
src = src.replace(
    '    else if(!strcmp(inp,"rm")||_sh_sw(inp,"rm ")||\n            !strcmp(inp,"rmdir")||_sh_sw(inp,"rmdir "))\n        {if(arg[0]){t_print("rm: ");t_print(arg);t_print(": supprime\\n");}else t_print("Usage: rm <nom>\\n");}',
    '    else if(!strcmp(inp,"rm")||_sh_sw(inp,"rm ")||\n            !strcmp(inp,"rmdir")||_sh_sw(inp,"rmdir "))\n        {if(arg[0]){char fp[DVFS_PLEN];_dvfs_fullpath(arg,fp,DVFS_PLEN);_dvfs_remove(fp);_dvfs_remove(arg);t_print("rm: ");t_print(arg);t_print(": supprime\\n");}else t_print("Usage: rm <nom>\\n");}')

# Add nano before "find" command
src = src.replace(
    '    else if(!strcmp(inp,"find")||_sh_sw(inp,"find "))sh_find(arg);',
    '    else if(!strcmp(inp,"nano")||_sh_sw(inp,"nano "))sh_nano(arg);\n    else if(!strcmp(inp,"vi")||_sh_sw(inp,"vi ")||!strcmp(inp,"vim")||_sh_sw(inp,"vim "))sh_nano(arg);\n    else if(!strcmp(inp,"find")||_sh_sw(inp,"find "))sh_find(arg);')

# echo with redirect '>'
src = src.replace(
    '    else if(!strcmp(inp,"echo"))t_print("\\n");\n    else if(_sh_sw(inp,"echo "))t_print(arg),t_print("\\n");',
    '''    else if(!strcmp(inp,"echo"))t_print("\\n");
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
                if(clen>1&&(content[0]=='"'||content[0]=='\\'')){content++;clen-=2;}
                if(clen<0)clen=0;
                if(clen>DVFS_CLEN-2)clen=DVFS_CLEN-2;
                memcpy(e->content,content,(size_t)clen);
                e->content[clen]='\\n';e->content[clen+1]='\\0';
            }
            t_print("Ecrit dans: ");t_print(fn);t_print("\\n");
        }else{t_print(arg);t_print("\\n");}
    }''')

# help text: add nano
src = src.replace(
    '        t_print(" FS      : ls ll cd pwd cat less mkdir touch\\n");',
    '        t_print(" FS      : ls ll cd pwd cat less mkdir touch\\n");\n        t_print("           nano vi vim  (editeur de fichiers)\\n");')

# ──────────────────────────────────────────────────────────────────────
# 16. nano: word_key saves after RETURN and BACKSPACE
# ──────────────────────────────────────────────────────────────────────
# After RETURN case in word_key
src = src.replace(
    '''    case SFCML_KEY_RETURN:
        if(_word_nl<WORD_ROWS){
            for(int i=_word_nl;i>_word_cl+1;i--)memcpy(_word[i],_word[i-1],WORD_COLS+1);
            memcpy(_word[_word_cl+1],cur+_word_cc,len-_word_cc+1);
            cur[_word_cc]=\'\\0\';_word_cl++;_word_cc=0;_word_nl++;
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
static void word_text''',
    '''    case SFCML_KEY_RETURN:
        if(_word_nl<WORD_ROWS){
            for(int i=_word_nl;i>_word_cl+1;i--)memcpy(_word[i],_word[i-1],WORD_COLS+1);
            memcpy(_word[_word_cl+1],cur+_word_cc,len-_word_cc+1);
            cur[_word_cc]=\'\\0\';_word_cl++;_word_cc=0;_word_nl++;
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
static void word_text''')

# ──────────────────────────────────────────────────────────────────────
# 17. Word processor title — show nano filename when editing
# ──────────────────────────────────────────────────────────────────────
# In draw_word, replace the static "Epitech Word" toolbar title
src = src.replace(
    '    sfcml_drawText(win,"Epitech Word",sx+6,sy+10,rd,tbg);',
    '    sfcml_drawText(win,_nano_path[0]?_nano_path:"Epitech Word",sx+6,sy+10,rd,tbg);')

# ──────────────────────────────────────────────────────────────────────
# 18. When closing W_WORD, clear _nano_path
# ──────────────────────────────────────────────────────────────────────
src = src.replace(
    '        if(hit_x(i,mx,my)){w->visible=0;return;}',
    '        if(hit_x(i,mx,my)){w->visible=0;if(i==W_WORD)_nano_path[0]=\'\\0\';return;}')

# ──────────────────────────────────────────────────────────────────────
# Done – write back
# ──────────────────────────────────────────────────────────────────────
with open(PATH, 'w', encoding='utf-8') as f:
    f.write(src)

print("[OK] kmain.c patched successfully")
