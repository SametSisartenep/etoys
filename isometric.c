#include <u.h>
#include <libc.h>
#include <draw.h>
#include <geometry.h>
#include <event.h>
#include <keyboard.h>

enum {
	Cbg,
	Cfg,
	Ctxtbg,
	Cgrid0,
	Cgrid1,
	NCOLOR
};

enum {
	TW = 32,
	TH = 16
};

typedef struct Tile Tile;
struct Tile
{
	char *name;
	char id;
	Image *img;
};

RFrame screenrf;
RFrame worldrf;
RFrame tilerf;
Image *pal[NCOLOR];
Tile tiles[] = {
	{ .name = "empty", .id = 'e' },
	{ .name = "filled", .id = 'f' },
	{ .name = "building", .id = 'b' },
	{ .name = "focus", .id = 'F' }
};
Tile *tfocus;
char *map[] = {
	"eeeee",
	"eefee",
	"efbfe",
	"eefee",
	"eefee",
};
Point mpos;
Point spacegrid[10][10];
int showgrid;

Point
fromworld(Point2 p)
{
	p = invrframexform(p, worldrf);
	return Pt(p.x,p.y);
}

Point2
toworld(Point p)
{
	return rframexform(Pt2(p.x,p.y,1), worldrf);
}

Point
fromtile(Point2 p)
{
	p = invrframexform(p, tilerf);
	return Pt(p.x,p.y);
}

Point2
totile(Point p)
{
	return rframexform(Pt2(p.x,p.y,1), tilerf);
}

void
initpalette(void)
{
	pal[Cbg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DBlack);
	pal[Cfg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DDarkblue);
	pal[Ctxtbg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPaleyellow);
	pal[Cgrid0] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
	pal[Cgrid1] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DBlue);
}

void
inittiles(void)
{
	char path[256];
	int fd, i;

	for(i = 0; i < nelem(tiles); i++){
		snprint(path, sizeof path, "asset/tile/%s.pic", tiles[i].name);
		fd = open(path, OREAD);
		tiles[i].img = readimage(display, fd, 0);
		close(fd);
		if(tiles[i].id == 'F')
			tfocus = &tiles[i];
	}
}

void
initgrid(void)
{
	int i, j;

	for(i = 0; i < nelem(spacegrid); i++)
		for(j = 0; j < nelem(spacegrid[i]); j++)
			spacegrid[i][j] = fromworld(Pt2(j, i, 1));
}

void
drawstats(void)
{
	Point2 mp, p;
	char s[256];

	mp = toworld(mpos);
	snprint(s, sizeof s, "Global %v", mp);
	stringbg(screen, addpt(screen->r.min, Pt(20,20)), pal[Cfg], ZP, font, s, pal[Ctxtbg], ZP);
	p = Pt2(fmod(mp.x,1),fmod(mp.y,1),1);
	snprint(s, sizeof s, "Local %v", p);
	stringbg(screen, addpt(screen->r.min, Pt(20,20+font->height)), pal[Cfg], ZP, font, s, pal[Ctxtbg], ZP);
	p = Pt2((int)mp.x,(int)mp.y,1);
	snprint(s, sizeof s, "Cell %v", p);
	stringbg(screen, addpt(screen->r.min, Pt(20,20+font->height*2)), pal[Cfg], ZP, font, s, pal[Ctxtbg], ZP);
	p = totile(mpos);
	snprint(s, sizeof s, "Tile %v", p);
	stringbg(screen, addpt(screen->r.min, Pt(20,20+font->height*3)), pal[Cfg], ZP, font, s, pal[Ctxtbg], ZP);
}

void
drawgrid(void)
{
	int i, j;

	for(i = 0; i < nelem(spacegrid); i++)
		line(screen, spacegrid[i][0], spacegrid[i][nelem(spacegrid[i])-1], Endsquare, Endsquare, 0, pal[Cgrid0], ZP);
	for(j = 0; j < nelem(spacegrid[0]); j++)
		line(screen, spacegrid[0][j], spacegrid[nelem(spacegrid)-1][j], Endsquare, Endsquare, 0, pal[Cgrid1], ZP);
}

void
drawtile(Tile *t, Point2 cell)
{
	Point p;

	p = fromtile(cell);
	p.y -= Dy(t->img->r) - TH; /* XXX hack to draw overheight tile sprites */
	draw(screen, Rpt(p,addpt(p, Pt(TW,Dy(t->img->r)))), t->img, nil, ZP);
}

void
redraw(void)
{
	Point2 dp;
	int i, j;
	char *row;

	draw(screen, screen->r, pal[Cbg], nil, ZP);
	for(i = 0; i < nelem(map); i++)
		for(row = map[i]; *row; row++){
			dp = Pt2(row-map[i],nelem(map)-i-1,1);
			for(j = 0; j < nelem(tiles); j++)
				if(tiles[j].id == *row)
					drawtile(&tiles[j], dp);
		}
	dp = toworld(mpos);
	dp.x = (int)dp.x;
	dp.y = (int)dp.y;
	drawtile(tfocus, dp);
	if(showgrid)
		drawgrid();
	drawstats();
	flushimage(display, 1);
}

void
lmb(Mouse *m)
{
	Point2 mp;
	Point cell;
	char buf[2];

	mp = toworld(mpos);
	if(mp.x < 0 || mp.y < 0)
		return;
	cell.x = mp.x;
	cell.y = mp.y;
	if(cell.y >= nelem(map) || cell.x >= strlen(map[cell.y]))
		return;
	snprint(buf, sizeof buf, "%c", map[nelem(map)-cell.y-1][cell.x]);
	if(eenter("tile id", buf, sizeof buf, m) > 0)
		map[nelem(map)-cell.y-1][cell.x] = buf[0];
}

void
mmb(Mouse *m)
{
	enum {
		SHOWGRID,
		CHGBASIS
	};
	static char *items[] = {
	 [SHOWGRID]	"toggle grid",
	 [CHGBASIS]	"change basis",
		nil
	};
	static Menu menu = { .item = items };
	char buf[256], *p;

	switch(emenuhit(2, m, &menu)){
	case SHOWGRID:
		showgrid ^= 1;
		break;
	case CHGBASIS:
		snprint(buf, sizeof buf, "%g %g", worldrf.bx.x, worldrf.bx.y);
		eenter("bx", buf, sizeof buf, m);
		worldrf.bx.x = strtod(buf, &p);
		worldrf.bx.y = strtod(p, nil);
		snprint(buf, sizeof buf, "%g %g", worldrf.by.x, worldrf.by.y);
		eenter("by", buf, sizeof buf, m);
		worldrf.by.x = strtod(buf, &p);
		worldrf.by.y = strtod(p, nil);
		break;
	}
}

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Event e;

	GEOMfmtinstall();
	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc > 0)
		usage();
	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	initpalette();
	inittiles();
	worldrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.min.y+Dy(screen->r)/3,1);
	worldrf.bx = Vec2(TW/2,TH/2);
	worldrf.by = Vec2(TW/2,-TH/2);
	tilerf.p = subpt2(worldrf.p, Vec2(0,TH/2));
	tilerf.bx = worldrf.bx;
	tilerf.by = worldrf.by;

	initgrid();
	einit(Emouse|Ekeyboard);
	redraw();
	for(;;)
		switch(event(&e)){
		case Emouse:
			mpos = e.mouse.xy;
			if((e.mouse.buttons&1) != 0)
				lmb(&e.mouse);
			if((e.mouse.buttons&2) != 0)
				mmb(&e.mouse);
			redraw();
			break;
		case Ekeyboard:
			switch(e.kbdc){
			case 'q':
			case Kdel:
				exits(0);
			}
			break;
		}
}

void
eresized(int)
{
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	worldrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.min.y+Dy(screen->r)/3,1);
	tilerf.p = subpt2(worldrf.p, Vec2(0,TH/2));
	redraw();
}
