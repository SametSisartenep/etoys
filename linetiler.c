#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>

#define CLAMP(n, min, max)	((n)<(min)?(min):(n)>(max)?(max):(n))

enum {
	CLIPL = 1,
	CLIPR = 2,
	CLIPT = 4,
	CLIPB = 8,
};

enum {
	PCBg0,
	PCBg1,
	PCFg0,
	PCFg1,
	PCAux,
	NCOLORS
};

typedef struct Line Line;

struct Line
{
	Point2 pts[2];
};

Rectangle UR = {0,0,1,1};	/* unit rectangle */
RFrame worldrf;
Image *pal[NCOLORS];
Rectangle *tiles;
int ntiles;
Line *lines;
int nlines;
Line theline;
Point2 thepoint;
Point2 pts[2];
int npts;
uint paloff;
int thickness;
int showtheline;

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

static void
swapi(int *a, int *b)
{
	int t;

	t = *a;
	*a = *b;
	*b = t;
}

static void
swappt(Point *a, Point *b)
{
	Point t;

	t = *a;
	*a = *b;
	*b = t;
}

static int
ptisinside(int code)
{
	return !code;
}

static int
lineisinside(int code0, int code1)
{
	return !(code0|code1);
}

static int
lineisoutside(int code0, int code1)
{
	return code0 & code1;
}

static int
outcode(Point p, Rectangle r)
{
	int code;

	code = 0;
	if(p.x < r.min.x) code |= CLIPL;
	if(p.x > r.max.x) code |= CLIPR;
	if(p.y < r.min.y) code |= CLIPT;
	if(p.y > r.max.y) code |= CLIPB;
	return code;
}

/*
 * Cohen-Sutherland rectangle-line clipping
 */
int
rectclipline(Rectangle r, Point *p0, Point *p1)
{
	int code0, code1;
	int Δx, Δy;
	double m;

	Δx = p1->x - p0->x;
	Δy = p1->y - p0->y;
	m = Δx == 0? 0: (double)Δy/Δx;

	for(;;){
		code0 = outcode(*p0, r);
		code1 = outcode(*p1, r);

		if(lineisinside(code0, code1))
			return 0;
		else if(lineisoutside(code0, code1))
			return -1;

		if(ptisinside(code0)){
			swappt(p0, p1);
			swapi(&code0, &code1);
		}

		if(code0 & CLIPL){
			p0->y += (r.min.x - p0->x)*m;
			p0->x = r.min.x;
		}else if(code0 & CLIPR){
			p0->y += (r.max.x - p0->x)*m;
			p0->x = r.max.x;
		}else if(code0 & CLIPT){
			if(p0->x != p1->x && m != 0)
				p0->x += (r.min.y - p0->y)/m;
			p0->y = r.min.y;
		}else if(code0 & CLIPB){
			if(p0->x != p1->x && m != 0)
				p0->x += (r.max.y - p0->y)/m;
			p0->y = r.max.y;
		}
	}
}

void
addline(Line l)
{
	lines = realloc(lines, ++nlines*sizeof(*lines));
	lines[nlines-1] = l;
}

void
initpalette(void)
{
	pal[PCBg0] = allocimage(display, UR, screen->chan, 1, DWhite);
	pal[PCBg1] = allocimage(display, UR, screen->chan, 1, 0x333333FF);
	pal[PCFg0] = pal[PCBg1];
	pal[PCFg1] = pal[PCBg0];
	pal[PCAux] = allocimage(display, UR, screen->chan, 1, DRed);
}

void
redraw(void)
{
	int i;

	lockdisplay(display);
	for(i = 0; i < ntiles; i++)
		draw(screen, rectaddpt(tiles[i], screen->r.min), pal[PCBg0+(i%2)], nil, ZP);
	for(i = 0; i < nlines; i++)
		line(screen, toscreen(lines[i].pts[0]), toscreen(lines[i].pts[1]), 0, 0, thickness, pal[PCFg0+(i+paloff)%2], ZP);
	if(showtheline)
		line(screen, toscreen(theline.pts[0]), toscreen(theline.pts[1]), 0, 0, 0, pal[PCAux], ZP);
	fillellipse(screen, toscreen(thepoint), 2, 2, pal[PCAux], ZP);
	flushimage(display, 1);
	unlockdisplay(display);
}

void
lmb(Mousectl *mc)
{
	Line l;
	Point p0, p1;
	int i;

	pts[npts] = thepoint = fromscreen(mc->xy);
	if(++npts > 2-1){
		theline.pts[0] = pts[0];
		theline.pts[1] = pts[1];
		npts = 0;

		if(lines != nil){
			free(lines);
			lines = nil;
			nlines = 0;
		}

		for(i = 0; i < ntiles; i++){
			p0 = Pt(theline.pts[0].x,theline.pts[0].y);
			p1 = Pt(theline.pts[1].x,theline.pts[1].y);
			if(ptinrect(p0, tiles[i]))
				paloff = i;
//fprint(2, "%P, %P ∩ %R = ", p0, p1, tiles[i]);
			if(rectclipline(tiles[i], &p0, &p1) < 0)
				continue;
//fprint(2, "%P, %P\n", p0, p1);
			l.pts[0] = Pt2(p0.x,p0.y,1);
			l.pts[1] = Pt2(p1.x,p1.y,1);
			addline(l);
		}
	}
}

void
rmb(Mousectl *)
{
	showtheline ^= 1;
}

void
mouse(Mousectl *mc)
{
	static Mouse om;

	if((om.buttons^mc->buttons) == 0)
		return;
	if((mc->buttons&1) != 0)
		lmb(mc);
	if((mc->buttons&4) != 0)
		rmb(mc);
	if((mc->buttons&8) != 0)
		thickness = CLAMP(++thickness, 0, 10);
	if((mc->buttons&16) != 0)
		thickness = CLAMP(--thickness, 0, 10);
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
	fprint(2, "usage: %s tiles\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	Mousectl *mc;
	Keyboardctl *kc;
	Rune r;
	int i, Δx, Δy;

	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc != 1)
		usage();

	ntiles = strtoul(argv[0], nil, 10);
	tiles = malloc(ntiles*sizeof(*tiles));

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

	Δx = Dx(screen->r);
	Δy = Dy(screen->r)/ntiles;
	for(i = 0; i < ntiles; i++)
		tiles[i] = Rect(0,i*Δy,Δx,i == ntiles-1? Dy(screen->r): (i+1)*Δy);

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
