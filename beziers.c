#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

typedef struct Point2 Point2;
typedef struct Bézier Bézier;

struct Point2
{
	double x, y, w;
};

struct Bézier
{
	Point2 p0, p1, p2, p3;
};

enum {
	Cbg,
	Cctl,
	Cbez,
	NCOLOR
};

Image *pal[NCOLOR];
Bézier *bezs;
Point2 pts[4];
ulong nbez, npt;

Point2
Pt2(double x, double y, double w)
{
	return (Point2){x,y,w};
}

Point
toscreen(Point2 p)
{
	return addpt(screen->r.min, Pt(p.x,p.y));
}

Point2
fromscreen(Point p)
{
	p = subpt(p, screen->r.min);
	return Pt2(p.x,p.y,1);
}

void
initpalette(void)
{
	pal[Cbg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000ff);
	pal[Cctl] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x00ff00ff);
	pal[Cbez] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xff0000ff);
}

void
redraw(void)
{
	int i;

	draw(screen, screen->r, pal[Cbg], nil, ZP);
	for(i = 0; i < nbez; i++)
		bezier(screen, toscreen(bezs[i].p0), toscreen(bezs[i].p1), toscreen(bezs[i].p2), toscreen(bezs[i].p3), Endsquare, Endsquare, 0, pal[Cbez], ZP);
	for(i = 0; i < npt; i++)
		fillellipse(screen, toscreen(pts[i]), 2, 2, pal[Cctl], ZP);
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
	Bézier b;

	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc > 0)
		usage();
	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	initpalette();
	einit(Emouse|Ekeyboard);
	redraw();
	for(;;)
		switch(event(&e)){
		case Emouse:
			if((e.mouse.buttons&1) != 0){
				pts[npt++] = fromscreen(e.mouse.xy);
				if(npt >= nelem(pts)){
					b.p0 = pts[0];
					b.p1 = pts[1];
					b.p2 = pts[2];
					b.p3 = pts[3];
					bezs = realloc(bezs, ++nbez*sizeof(Bézier));
					bezs[nbez-1] = b;
					npt = 0;
				}
				redraw();
			}
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
	redraw();
}
