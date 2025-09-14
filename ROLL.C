/********************************************************************
	回転ルーチン　アルゴリズム編　　　　　　　　　　　by Makken
********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <conio.h>
#include <egb.h>
#include <snd.h>
#include <dos.h>


/********************************************************************
	コプロセッサなしの機種でも動作可能にするための宣言
********************************************************************/
#pragma Off(387)
#pragma Off(Floating_point)
#pragma Off(486)

/*
　　　　　パッド読み取り用の定数
*/
#define UP	0xfe
#define DOWN	0xfd
#define LEFT	0xfb
#define RIGHT	0xf7
#define TRIGA	0xef
#define TRIGB	0xdf
#define RUN	0xbf
#define SELECT	0x7f

/********************************************************************
	描画画面の大きさ
********************************************************************/
#define DispX 256
#define DispY 240

char *gwk;
char *Head;
short *Map;
short *CT;
short *ST;

unsigned int vram_page;

union fixed_pack {
	short xy[2]; // xy[0] = x, xy[1] = y
	int pack;
};

/********************************************************************
	拡大縮小回転ルーチン
********************************************************************/
void roll(char offset_x, char offset_y, char deg, short scale) {
	union fixed_pack sxy, dxy, txy;
	int x, y;
	_Far short *VRAM;
	register int temp_s, temp_d;

	dxy.xy[0] = CT[(-deg)&0xff];
	dxy.xy[1] = ST[(-deg)&0xff];
	scale = _max(1,scale);
	dxy.xy[0] = (dxy.xy[0] * scale) >> 8;
	dxy.xy[1] = (dxy.xy[1] * scale) >> 8;
	sxy.xy[0] = (offset_x << 8) + dxy.xy[0] * (-DispX / 2) - dxy.xy[1] * (-DispY / 2);
	sxy.xy[1] = (offset_y << 8) + dxy.xy[1] * (-DispX / 2) + dxy.xy[0] * (-DispY / 2);
	txy.pack = sxy.pack;

	_FP_SEG(VRAM) = 0x104;
	_FP_OFF(VRAM) = vram_page*2;

	temp_d = dxy.pack;
	y = 0;
	do {
		temp_s = txy.pack;
		x = 0;
		do {
			VRAM[x] = Map[(temp_s >> 16 & 0xff00) + (temp_s >> 8 & 0xff)];
			temp_s += temp_d;
			x++;
		}while(x < DispX);
		VRAM += 512;
		txy.xy[0] -= dxy.xy[1];
		txy.xy[1] += dxy.xy[0];
		y++;
	} while(y < DispY);
}

/********************************************************************
	メインルーチン
********************************************************************/
void main(int argc, char *argv[]) {
	int i, j;
	int TiffX, TiffY;
	FILE *fp;
	char deg = 0;
	char x = DispX / 2;
	char y = DispY / 2;
	short s = 0x100;
	int pad;

	/*******************************************************
		TIFFファイルの読み込み
	*******************************************************/
	if(argc!=2) {
		puts("run386 roll -----.tif");
		exit(1);
	}
	if((fp=fopen(argv[1],"rb"))==NULL) {
		puts("TIFFファイルがオープンできません");
		exit(1);
	}
	Head = (char *)malloc(512);
	fread(Head, 1, 512, fp);
	/*  TIFFヘッダの解読  */
	if(*(int *)(Head+0)!=0x002a4949 ||
		*(int *)(Head+0x36)!=16 ||
		*(int *)(Head+0x42)!=1) {
		puts("32768色の非圧縮TIFFファイルを指定してください");
		free(Head);
		exit(1);
	}
	TiffX=*(int *)(Head+0x1e);
	TiffY=*(int *)(Head+0x2a);

	/* Map[]にTIFF画像を読みこむ */
	Map = (short *)malloc(131072);
	if(TiffX < 256) {
		for(i = 0; i < _min(256,TiffY); i++) {
			fread(&Map[i<<8], 2, TiffX, fp);
			for(j = TiffX; j < 256; j++) {
				Map[(i << 8) + j]  = 0;
			}
		}
	} else {
		for(i = 0; i < _min(256,TiffY); i++) {
			fread(&Map[i<<8], 2, TiffX, fp);
			fseek(fp,(TiffX - 256) * 2, SEEK_CUR);
		}
	}
	while(i<256) {
		j = 0;
		do {
			Map[(i << 8) + j]  = 0;
			j++;
		} while(j < 256);
		i++;
	}
	fclose(fp);

	/*******************************************************
		画面設定
	*******************************************************/
	gwk = (char *)malloc(1536);
	EGB_resolution(gwk, 0, 10);
	EGB_resolution(gwk, 1, 3);
	EGB_displayPage(gwk, 1, 3);
	EGB_writePage(gwk, 1);
	EGB_color(gwk, 1, 0);
	EGB_clearScreen(gwk);
	EGB_writePage(gwk, 0);
	EGB_displayStart(gwk, 0, 64, 0);
	EGB_displayStart(gwk, 1, 0, 0);
	EGB_displayStart(gwk, 2, 2, 2);
	EGB_displayStart(gwk, 3, 256, 240);
	EGB_color(gwk, 1, 0);
	EGB_clearScreen(gwk);

	/*******************************************************
		COS,SINテーブル作成
	*******************************************************/
	CT = (short *)malloc(512);
	ST = (short *)malloc(512);
	i = 0;
	do {
		CT[i] = cos((float)i*_PI/128)*256;
		ST[i] = sin((float)i*_PI/128)*256;
		i++;
	} while(i < 256);

	/*******************************************************
		初期画面表示
	*******************************************************/
	vram_page = 0;
	roll(x, y, deg, s);
	vram_page = 256 - vram_page;
	roll(x, y, deg, s);
	vram_page = 256 - vram_page;

	/*******************************************************
		パッド操作で画面を回転
	*******************************************************/
	do {
		// SND_joy_in_2はRUN・SELECTの同時押しに対応していない
		SND_joy_in_1(0, &pad);
		pad &= 0xff;
		
		if(((pad&~LEFT) == 0) && ((pad&~RIGHT) == 0))
		{
			if(((pad&~UP) == 0) && ((pad&~DOWN) == 0)) {
				deg=0;
				x = DispX / 2;
				y = DispY / 2;
			} else {
				x+=8;
			}
		} else if(((pad&~UP) == 0) && ((pad&~DOWN) == 0)) {
			x-=8;
		} else {
			if((pad&~RIGHT) == 0)
			{
				deg--;
			} else if((pad&~LEFT) == 0) {
				deg++;
			}

			if((pad&~UP) == 0)
			{
				y-=8;
			} else if((pad&~DOWN) == 0) {
				y+=8;
			}
		}

		if((pad&~TRIGA) == 0) {
			if((pad&~TRIGB) == 0) {
				s = 0x100;
			} else {
				s = _max(0x40,s-8);
			}
		} else if((pad&~TRIGB) == 0) {
			s = _min(0x500,s + 8);
		}

		if(pad != 0xff) {
			_outb( 0x0440, 17 );
			_outw( 0x0442, vram_page / 2);
			vram_page = 256 - vram_page;
			roll(x, y, deg, s);
		}
	} while(_kbhit() == 0);

	EGB_init(gwk, 1536);
	free(gwk);
	free(Head);
	free(Map);
	free(CT);
	free(ST);
}