#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>

enum {
	PCBg,
	PCFg,
	PCPoly,
	PCPolydark,
	PCAux,
	NCOLORS
};

typedef struct Triangle Triangle;

struct Triangle
{
	Point2 p0, p1, p2;
};

Rectangle UR = {0,0,1,1};	/* unit rectangle */
RFrame worldrf;
Image *pal[NCOLORS];
Triangle thetriangle;
Point2 thepoint;
int isinside;

void resized(void);

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

/*
 * comparison of a point p with an edge [e0 e1]
 * p to the right: +
 * p to the left: -
 * p on the edge: 0
 */
//static int
//edgeptcmp(Point2 e0, Point2 e1, Point2 p)
//{
//	Point3 e0p, e01, r;
//
//	p = subpt2(p, e0);
//	e1 = subpt2(e1, e0);
//	e0p = Vec3(p.x,p.y,0);
//	e01 = Vec3(e1.x,e1.y,0);
//	r = crossvec3(e0p, e01);
//
//	/* clamp to avoid overflow */
//	return fclamp(r.z, -1, 1); /* e0.x*e1.y - e0.y*e1.x */
//}

int
ptintriangle(Point2 p, Triangle t)
{
	/* counter-clockwise check */
	return edgeptcmp(t.p0, t.p2, p) <= 0 &&
		edgeptcmp(t.p2, t.p1, p) <= 0 &&
		edgeptcmp(t.p1, t.p0, p) <= 0; 
}

Triangle
Trianpt(Point2 p0, Point2 p1, Point2 p2)
{
	return (Triangle){p0, p1, p2};
};

void
triangle(Image *dst, Triangle t, int thick, Image *src, Point sp)
{
	Point pl[4];

	pl[0] = toscreen(t.p0);
	pl[1] = toscreen(t.p1);
	pl[2] = toscreen(t.p2);
	pl[3] = pl[0];

	poly(dst, pl, nelem(pl), Endsquare, Endsquare, thick, src, sp);
}

void
filltriangle(Image *dst, Triangle t, Image *src, Point sp)
{
	Point pl[3];

	pl[0] = toscreen(t.p0);
	pl[1] = toscreen(t.p1);
	pl[2] = toscreen(t.p2);

	fillpoly(dst, pl, nelem(pl), 0, src, sp);
}

void
initpalette(void)
{
	pal[PCBg] = allocimage(display, UR, screen->chan, 1, DWhite);
	pal[PCFg] = allocimage(display, UR, screen->chan, 1, DBlack);
	pal[PCPoly] = allocimage(display, UR, screen->chan, 1, DPalebluegreen);
	pal[PCPolydark] = allocimage(display, UR, screen->chan, 1, DDarkblue);
	pal[PCAux] = allocimage(display, UR, screen->chan, 1, DRed);
}

void
drawbanner(void)
{
	static char *msgs[] = {
		"THE POINT IS OUTSIDE",
		"THE POINT IS INSIDE"
	}, *s;

	s = msgs[isinside];
	string(screen, toscreen(Pt2(Dx(screen->r)/2 - strlen(s)/2*font->width,10,1)), pal[PCFg], ZP, font, s);
}

void
redraw(void)
{
	lockdisplay(display);
	draw(screen, screen->r, pal[PCBg], nil, ZP);
	filltriangle(screen, thetriangle, pal[PCPoly], ZP);
	triangle(screen, thetriangle, 1, pal[PCPolydark], ZP);
	fillellipse(screen, toscreen(thetriangle.p0), 2, 2, pal[PCPolydark], ZP);
	fillellipse(screen, toscreen(thetriangle.p1), 2, 2, pal[PCPolydark], ZP);
	fillellipse(screen, toscreen(thetriangle.p2), 2, 2, pal[PCPolydark], ZP);
	fillellipse(screen, toscreen(thepoint), 2, 2, pal[PCAux], ZP);
	drawbanner();
	flushimage(display, 1);
	unlockdisplay(display);
}

void
rmb(Mousectl *mc)
{
	thepoint = fromscreen(mc->xy);
	isinside = ptintriangle(thepoint, thetriangle);
}

void
lmb(Mousectl *mc)
{
	USED(mc);
}

void
mouse(Mousectl *mc)
{
	if((mc->buttons&1) != 0)
		lmb(mc);
	if((mc->buttons&4) != 0)
		rmb(mc);
}

void
key(Rune r)
{
	switch(r){
	case Kdel:
	case 'q':
		threadexitsall(nil);
	}
}

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	Mousectl *mc;
	Keyboardctl *kc;
	Rune r;
	Point2 tmp;

	GEOMfmtinstall();
	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc > 0)
		usage();

	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	initpalette();

	worldrf.p = Pt2(screen->r.min.x,screen->r.min.y,1);
	worldrf.bx = Vec2(1,0);
	worldrf.by = Vec2(0,1);
	tmp = Pt2(Dx(screen->r)/2,Dy(screen->r)/2,1);
	thetriangle = Trianpt(tmp, addpt2(tmp, Vec2(200,0)), addpt2(tmp, Vec2(0,-160)));

	display->locking = 1;
	unlockdisplay(display);
	redraw();

	for(;;){
		enum { MOUSE, RESIZE, KEYBOARD };
		Alt a[] = {
			{mc->c, &mc->Mouse, CHANRCV},
			{mc->resizec, nil, CHANRCV},
			{kc->c, &r, CHANRCV},
			{nil, nil, CHANEND}
		};

		switch(alt(a)){
		case MOUSE:
			mouse(mc);
			break;
		case RESIZE:
			resized();
			break;
		case KEYBOARD:
			key(r);
			break;
		}

		redraw();
	}
}

void
resized(void)
{
	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("couldn't resize");
	unlockdisplay(display);
	worldrf.p = Pt2(screen->r.min.x,screen->r.min.y,1);
	redraw();
}
