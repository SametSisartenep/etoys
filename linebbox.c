#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <memdraw.h>
#include <mouse.h>
#include <keyboard.h>

enum {
	PCBg,
	PCFg,
	PCAux,
	NCOLORS,

	MAXTHICK = 30,

	TechOld = 0,
	TechNew,
};

typedef struct LBoundary LBoundary;

struct LBoundary
{
	Rectangle r;
	Point dir;
	int repeat;
};

Rectangle UR = {0,0,1,1};	/* unit rectangle */
Image *pal[NCOLORS];
Point pts[2];
int npts;
LBoundary lbound;
Rectangle bbox;
int thickness;
int endtype;
int technique;

void resized(void);

void
initpalette(void)
{
	pal[PCBg] = allocimage(display, UR, screen->chan, 1, DWhite);
	pal[PCFg] = allocimage(display, UR, screen->chan, 1, 0x333333FF);
	pal[PCAux] = allocimage(display, UR, screen->chan, 1, DRed);
}

LBoundary
mklineboundary(Point pts[2], int radius)
{
	LBoundary b;
	int len, len2, c, s;
	int extra;

	extra = memlineendsize(endtype);
	b.r = Rect(0, 0, 5*radius/2+1+extra, 5*radius/2+1+extra);
	b.r = rectsubpt(b.r, Pt(Dx(b.r)/2, Dy(b.r)/2));
	b.dir = subpt(pts[1], pts[0]);
	len = hypot(b.dir.x*ICOSSCALE, b.dir.y*ICOSSCALE);
	icossin2(b.dir.x, b.dir.y, &c, &s);
	b.dir.x = c*Dx(b.r)/2;
	b.dir.y = s*Dy(b.r)/2;
	len2 = hypot(b.dir.x, b.dir.y);
	b.repeat = len2 > 0? len/len2: 0;
	b.repeat += 2;
	return b;
}

void
updatebounds(void)
{
	bbox = memlinebbox(pts[0], pts[1], endtype, endtype, thickness);
	lbound = mklineboundary(pts, thickness);
}

void
drawinfo(void)
{
	static Point sp = {10,10};
	char buf[64];

	snprint(buf, sizeof buf, "nboxes %d", lbound.repeat);
	stringbg(screen, addpt(screen->r.min, sp), pal[PCBg], ZP, font, buf, pal[PCFg], ZP);
}

void
redraw(void)
{
	Point rp;
	int i;

	draw(screen, screen->r, pal[PCBg], nil, ZP);
	if(npts == 2){
		line(screen, addpt(screen->r.min, pts[0]), addpt(screen->r.min, pts[1]),
			endtype, endtype, thickness, pal[PCFg], ZP);
		if(technique == TechOld)
			border(screen, rectaddpt(bbox, screen->r.min), 1, pal[PCAux], ZP);
		else{
			rp = addpt(screen->r.min, pts[0]);
			rp = mulpt(rp, ICOSSCALE);
			for(i = 0; i < lbound.repeat; i++){
				border(screen, rectaddpt(lbound.r, divpt(rp, ICOSSCALE)), 1, pal[PCAux], ZP);
				rp = addpt(rp, lbound.dir);
			}
		}
	}else
		fillellipse(screen, addpt(screen->r.min, pts[0]), thickness, thickness, pal[PCAux], ZP);
	drawinfo();
	flushimage(display, 1);
}

void
lmb(Mousectl *mc)
{
	if(npts == 2)
		npts = 0;
	pts[npts++] = subpt(mc->xy, screen->r.min);
	if(npts == 2)
		updatebounds();
}

static char *
mmbmenugen(int idx)
{
	if(idx == 0)
		return technique == TechOld?
			"use new technique":
			"use old technique";
	return nil;
}

void
mmb(Mousectl *mc)
{
	static Menu menu = { .gen = mmbmenugen };

	if(menuhit(2, mc, &menu, _screen) == 0)
		technique ^= 1;
}

void
rmb(Mousectl *mc)
{
	static char *items[] = {
		"Endsquare",
		"Enddisc",
		"Endarrow",
		nil,
	};
	static Menu menu = { .item = items };

	switch(menuhit(3, mc, &menu, _screen)){
	case 0:
		endtype = Endsquare;
		break;
	case 1:
		endtype = Enddisc;
		break;
	case 2:
		endtype = Endarrow;
		break;
	}
}

void
mouse(Mousectl *mc)
{
	static Mouse om;

	if((om.buttons^mc->buttons) == 0)
		return;
	if((mc->buttons&1) != 0)
		lmb(mc);
	if((mc->buttons&2) != 0)
		mmb(mc);
	if((mc->buttons&4) != 0)
		rmb(mc);
	if((mc->buttons&(8|16)) != 0){
		if((mc->buttons&8) != 0)
			thickness = ++thickness > MAXTHICK? MAXTHICK: thickness;
		if((mc->buttons&16) != 0)
			thickness = --thickness < 0? 0: thickness;
		if(npts == 2)
			updatebounds();
	}
	om = mc->Mouse;
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

	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc != 0)
		usage();

	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	initpalette();

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
	if(getwindow(display, Refnone) < 0)
		sysfatal("couldn't resize");
	redraw();
}
