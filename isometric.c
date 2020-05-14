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
	NCOLOR
};

enum {
	TW = 16,
	TH = 8
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
	{ .name = "filled", .id = 'f' }
};
RFrame worldrf;
char *map[] = {
	"eeeee",
	"eefee",
	"efefe",
	"eefee",
	"eefee"
};
Point mpos;

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
	}
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
redraw(void)
{
	Point2 dp;
	int i, j;
	char *row;

	draw(screen, screen->r, pal[Cbg], nil, ZP);
	for(i = 0; i < nelem(map); i++)
		for(row = map[i]; *row; row++){
			dp = Pt2((row-map[i]-i)*TW/2,(i+row-map[i])*TH/2,1);
			for(j = 0; j < nelem(tiles); j++)
				if(tiles[j].id == *row)
					draw(screen, Rpt(toscreen(dp),addpt(toscreen(dp), Pt(TW,TH))), tiles[j].img, nil, ZP);
		}
	drawstats();
	flushimage(display, 1);
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
	worldrf.p = Pt2(screen->r.min.x,screen->r.min.y,1);
	worldrf.p = addpt2(worldrf.p, Vec2(Dx(screen->r)/2,Dy(screen->r)/3));
	worldrf.bx = Vec2(1,0);
	worldrf.by = Vec2(0,1);
	einit(Emouse|Ekeyboard);
	redraw();
	for(;;)
		switch(event(&e)){
		case Emouse:
			mpos = e.mouse.xy;
			if((e.mouse.buttons&1) != 0)
				worldrf.p = Pt2(e.mouse.xy.x,e.mouse.xy.y,1);
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
	worldrf.p = Pt2(screen->r.min.x,screen->r.min.y,1);
	worldrf.p = addpt2(worldrf.p, Vec2(Dx(screen->r)/2,Dy(screen->r)/3));
	redraw();
}
