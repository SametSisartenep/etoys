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
	PCAux,
	NCOLORS
};

typedef struct Rectangle2 Rectangle2;
typedef struct Container Container;

/* min = bottom-left, max = top-right */
struct Rectangle2
{
	Point2 min, max;
};

struct Container
{
	RFrame;
	Rectangle2 bbox;	/* defined wrt worldrf */
};

RFrame worldrf;
Image *pal[NCOLORS];
Container containers[4];
Point2 thepoint;

void resized(void);

Rectangle2
Rpt2(Point2 min, Point2 max)
{
	return (Rectangle2){min, max};
}

Rectangle2
Rect2(double minx, double miny, double maxx, double maxy)
{
	/* assumed that w is always 1 */
	return Rpt2(Pt2(minx,miny,1), Pt2(maxx,maxy,1));
}

Rectangle2
rectaddpt2(Rectangle2 r, Point2 p)
{
	return Rpt2(addpt2(r.min, p), addpt2(r.max, p));
}

int
ptinrect2(Point2 p, Rectangle2 r)
{
	return p.x >= r.min.x && p.x < r.max.x &&
		p.y >= r.min.y && p.y < r.max.y;
}

void
initcontainers(Rectangle parent)
{
	int w, h, i;

	w = Dx(parent);
	h = Dy(parent);

	containers[0].bbox = Rpt2(Pt2(0,0,1), Pt2(w/2,h/2,1));
	containers[1].bbox = rectaddpt2(containers[0].bbox, Vec2(w/2,0));
	containers[2].bbox = rectaddpt2(containers[0].bbox, Vec2(0,h/2));
	containers[3].bbox = rectaddpt2(containers[0].bbox, Vec2(w/2,h/2));

	for(i = 0; i < nelem(containers); i++)
		containers[i].p = addpt2(containers[i].bbox.min, Vec2(w/4,h/4));

	containers[0].bx = Vec2(0,  1);
	containers[1].bx = Vec2(2,  0);
	containers[2].bx = Vec2(0.5,0);
	containers[3].bx = Vec2(1,  0);

	containers[0].by = Vec2(1,0);
	containers[1].by = Vec2(0,2);
	containers[2].by = Vec2(0,1);
	containers[3].by = Vec2(0,1);
}

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
	pal[PCBg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DBlack);
	pal[PCFg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DWhite);
	pal[PCAux] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
}

void
drawcontainers(void)
{
	static Rectangle UR = {0,0,1,1};	/* unit rectangle */
	char buf[128];
	int i;

	line(screen, addpt(screen->r.min, Pt(Dx(screen->r)/2,0)), subpt(screen->r.max, Pt(Dx(screen->r)/2, 0)), Endsquare, Endsquare, 0, pal[PCFg], ZP);
	line(screen, addpt(screen->r.min, Pt(0,Dy(screen->r)/2)), subpt(screen->r.max, Pt(0,Dy(screen->r)/2)), Endsquare, Endsquare, 0, pal[PCFg], ZP);

	for(i = 0; i < nelem(containers); i++){
		/* TODO: replace for an axis. */
		draw(screen, rectaddpt(UR, toscreen(containers[i].p)), pal[PCAux], nil, ZP);

		if(ptinrect2(invrframexform(thepoint, containers[i]), containers[i].bbox))
			fillellipse(screen, toscreen(invrframexform(thepoint, containers[i])), 2, 2, pal[PCAux], ZP);

		snprint(buf, sizeof buf, "bx %v", containers[i].bx);
		stringn(screen, toscreen(addpt2(containers[i].bbox.min, Vec2(10,2.5*font->height))), pal[PCFg], ZP, font, buf, sizeof buf);

		snprint(buf, sizeof buf, "by %v", containers[i].by);
		stringn(screen, toscreen(addpt2(containers[i].bbox.min, Vec2(10,1.5*font->height))), pal[PCFg], ZP, font, buf, sizeof buf);
	}
}

void
redraw(void)
{
	lockdisplay(display);
	draw(screen, screen->r, pal[PCBg], nil, ZP);
	drawcontainers();
	flushimage(display, 1);
	unlockdisplay(display);
}

void
rmb(Mousectl *mc, Keyboardctl *kc)
{
	Point2 mpos, nbx, nby;
	char buf[128], *s;
	int i;

	mpos = fromscreen(mc->xy);
	for(i = 0; i < nelem(containers); i++)
		if(ptinrect2(mpos, containers[i].bbox)){
			nbx = containers[i].bx;
			snprint(buf, sizeof buf, "%g %g", nbx.x, nbx.y);
			if(enter("bx", buf, sizeof buf, mc, kc, nil) <= 0)
				return;
			nbx.x = strtod(buf, &s);
			if(*s != ' ')
				return;
			nbx.y = strtod(s, &s);
			if(*s != 0)
				return;

			nby = containers[i].by;
			snprint(buf, sizeof buf, "%g %g", nby.x, nby.y);
			if(enter("by", buf, sizeof buf, mc, kc, nil) <= 0)
				return;
			nby.x = strtod(buf, &s);
			if(*s != ' ')
				return;
			nby.y = strtod(s, &s);
			if(*s != 0)
				return;

			containers[i].bx = nbx;
			containers[i].by = nby;
		}
}

void
lmb(Mousectl *mc, Keyboardctl *)
{
	Point2 mpos;
	int i;

	mpos = fromscreen(mc->xy);
	for(i = 0; i < nelem(containers); i++)
		if(ptinrect2(mpos, containers[i].bbox))
			thepoint = rframexform(mpos, containers[i]);
}

void
mouse(Mousectl *mc, Keyboardctl *kc)
{
	if((mc->buttons&1) != 0)
		lmb(mc, kc);
	if((mc->buttons&4) != 0)
		rmb(mc, kc);
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
	initcontainers(screen->r);
	worldrf.p = Pt2(screen->r.min.x,screen->r.max.y,1);
	worldrf.bx = Vec2(1, 0);
	worldrf.by = Vec2(0,-1);
	thepoint = Pt2(0,0,1);

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
			mouse(mc, kc);
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
	worldrf.p = Pt2(screen->r.min.x,screen->r.max.y,1);
	initcontainers(screen->r);
	redraw();
}
