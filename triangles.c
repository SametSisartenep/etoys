#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

typedef struct Point2 Point2;
typedef struct Triangle Triangle;
typedef struct Circle Circle;
typedef struct Object Object;

struct Point2
{
	double x, y, w;
};

struct Triangle
{
	Point2 p0, p1, p2;
};

struct Circle
{
	Point2 c;
	double r;
};

struct Object
{
	Triangle t;
	Circle c;
};

enum {
	Cbg,
	Ctri,
	Ccir,
	NCOLOR
};

Image *pal[NCOLOR];
Object *tris;
ulong ntri;
Point2 pts[3];
int npt;

Point2
Pt2(double x, double y, double w)
{
	return (Point2){x,y,w};
}

Point2
addpt2(Point2 p, Point2 q)
{
	return (Point2){p.x+q.x,p.y+q.y,p.w+q.w};
}

Point2
subpt2(Point2 p, Point2 q)
{
	return (Point2){p.x-q.x,p.y-q.y,p.w-q.w};
}

Point2
mulpt2(Point2 p, double s)
{
	return (Point2){p.x*s,p.y*s,p.w*s};
}

Point2
divpt2(Point2 p, double s)
{
	return (Point2){p.x/s,p.y/s,p.w/s};
}

double
veclen(Point2 p)
{
	return sqrt(p.x*p.x + p.y*p.y);
};

Point2
crossvec(Point2 p, Point2 q)
{
	return (Point2){
		p.y*q.w - p.w*q.y,
		p.w*q.x - p.x*q.w,
		p.x*q.y - p.y*q.x
	};
}

void
addobj(Triangle t, Circle c)
{
	tris = realloc(tris, ++ntri*sizeof(Object));
	tris[ntri-1].t = t;
	tris[ntri-1].c = c;
}

double
triperi(Triangle t)
{
	return veclen(subpt2(t.p0, t.p1))
		+ veclen(subpt2(t.p1, t.p2))
		+ veclen(subpt2(t.p2, t.p0));
};

double
triarea(Triangle t)
{
	Point2 p;
	double r;

	p = crossvec(t.p0, t.p1);
	p = addpt2(p, crossvec(t.p1, t.p2));
	p = addpt2(p, crossvec(t.p2, t.p0));
	r = sqrt(p.x*p.x + p.y*p.y + p.w*p.w);
	return r/2;
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
	return (Point2){p.x,p.y,0};
}

void
triangle(Triangle t)
{
	Point p[4];

	p[0] = toscreen(t.p0);
	p[1] = toscreen(t.p1);
	p[2] = toscreen(t.p2);
	p[3] = p[0];
	poly(screen, p, nelem(p), Endsquare, Endsquare, 0, pal[Ctri], ZP);
}

void
circle(Circle c)
{
	ellipse(screen, toscreen(c.c), c.r, c.r, 0, pal[Ccir], ZP);
}

void
initpalette(void)
{
	pal[Cbg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000ff);
	pal[Ctri] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x00ff00ff);
	pal[Ccir] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xff0000ff);
}

void
redraw(void)
{
	int i;

	draw(screen, screen->r, pal[Cbg], nil, ZP);
	for(i = 0; i < ntri; i++){
		triangle(tris[i].t);
		circle(tris[i].c);
	}
	for(i = 0; i < npt; i++)
		fillellipse(screen, toscreen(pts[i]), 2, 2, pal[Ctri], ZP);
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
	Triangle t;
	Circle c;

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
					t.p0 = pts[0];
					t.p1 = pts[1];
					t.p2 = pts[2];
					c.c = divpt2(addpt2(addpt2(mulpt2(t.p0, veclen(subpt2(t.p1, t.p2))),
						mulpt2(t.p1, veclen(subpt2(t.p2, t.p0)))),
						mulpt2(t.p2, veclen(subpt2(t.p0, t.p1)))),
					triperi(t));
					c.r = 2*triarea(t)/triperi(t);
					addobj(t, c);
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
