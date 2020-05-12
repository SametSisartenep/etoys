#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

typedef struct Point2 Point2;
typedef struct Bspline Bspline;

struct Point2
{
	double x, y, w;
};

struct Bspline
{
	Point2 *pts;
	ulong npt;
};

enum {
	Cbg,
	Cctl,
	Cbez,
	NCOLOR
};

Image *pal[NCOLOR];
Bspline *bsplines;
Point2 *pts;
ulong nbspline, npt;

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
	Point *bpts;
	int i, j;

	draw(screen, screen->r, pal[Cbg], nil, ZP);
	for(i = 0; i < nbspline; i++){
		bpts = malloc(bsplines[i].npt*sizeof(Point));
		for(j = 0; j < bsplines[i].npt; j++)
			bpts[j] = toscreen(bsplines[i].pts[j]);
		bezspline(screen, bpts, bsplines[i].npt, Endsquare, Endsquare, 0, pal[Cbez], ZP);
		free(bpts);
	}
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
	Bspline b;

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
				pts = realloc(pts, ++npt*sizeof(Point2));
				pts[npt-1] = fromscreen(e.mouse.xy);
				redraw();
			}
			if((e.mouse.buttons&4) != 0){
				if(npt > 1){
					b.pts = pts;
					b.npt = npt;
					bsplines = realloc(bsplines, ++nbspline*sizeof(Bspline));
					bsplines[nbspline-1] = b;
					pts = nil;
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
