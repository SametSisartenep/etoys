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

Image *pal[NCOLOR];
Tile tiles[] = {
	{ .name = "empty", .id = 'e' },
	{ .name = "filled", .id = 'f' },
	{ .name = "building", .id = 'b' },
	{ .name = "focus", .id = 'F' }
};
Tile *tfocus;
RFrame worldrf;
char *map[] = {
	"eeeee",
	"eefee",
	"efbfe",
	"eefee",
	"eefee"
};
Point mpos;
Point2 spacegrid[10][10];
int showgrid;

Point
toscreen(Point2 p)
{
	p = invrframexform(p, worldrf);
	return Pt(p.x,p.y);
}

Point2
fromscreen(Point p)
{
	return rframexform(Pt2(p.x,p.y,1), worldrf);
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
			spacegrid[i][j] = Pt2(j*TW, i*TH, 1);
}

void
drawstats(void)
{
	Point2 mp, p;
	char s[256];

	mp = fromscreen(mpos);
	snprint(s, sizeof s, "Global %v", mp);
	stringbg(screen, addpt(screen->r.min, Pt(20,20)), pal[Cfg], ZP, font, s, pal[Ctxtbg], ZP);
	p = Pt2(fmod(mp.x, TW),fmod(mp.y, TH),1);
	snprint(s, sizeof s, "Local %v", p);
	stringbg(screen, addpt(screen->r.min, Pt(20,20+font->height)), pal[Cfg], ZP, font, s, pal[Ctxtbg], ZP);
	p = Pt2((int)mp.x/TW,(int)mp.y/TH,1);
	snprint(s, sizeof s, "Cell %v", p);
	stringbg(screen, addpt(screen->r.min, Pt(20,20+font->height*2)), pal[Cfg], ZP, font, s, pal[Ctxtbg], ZP);
}

void
drawgrid(void)
{
	int i, j;

	for(i = 0; i < nelem(spacegrid); i++)
		line(screen, toscreen(spacegrid[i][0]), toscreen(spacegrid[i][nelem(spacegrid[i])-1]), Endsquare, Endsquare, 0, pal[Cgrid0], ZP);
	for(j = 0; j < nelem(spacegrid[0]); j++)
		line(screen, toscreen(spacegrid[0][j]), toscreen(spacegrid[nelem(spacegrid)-1][j]), Endsquare, Endsquare, 0, pal[Cgrid1], ZP);
}

void
drawtile(Tile *t, Point2 cell)
{
	Point p;

	cell.x *= TW;
	cell.y *= TH;
	p = toscreen(cell);
	p.x -= TW/2;
	p.y -= Dy(t->img->r)-TH;
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
			dp = Pt2(row-map[i],i,1);
			for(j = 0; j < nelem(tiles); j++)
				if(tiles[j].id == *row)
					drawtile(&tiles[j], dp);
		}
	dp = fromscreen(mpos);
	dp.x = (int)dp.x/TW;
	dp.y = (int)dp.y/TH;
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

	mp = fromscreen(mpos);
	if(mp.x < 0 || mp.y < 0)
		return;
	cell.x = mp.x/TW;
	cell.y = mp.y/TH;
	if(cell.y >= nelem(map) || cell.x >= strlen(map[cell.y]))
		return;
	snprint(buf, sizeof buf, "%c", map[cell.y][cell.x]);
	if(eenter("tile id", buf, sizeof buf, m) > 0)
		map[cell.y][cell.x] = buf[0];
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
	initgrid();
	worldrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.min.y+Dy(screen->r)/3,1);
	worldrf.bx = Vec2(1,2);
	worldrf.by = Vec2(-0.5,1);
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
	redraw();
}
