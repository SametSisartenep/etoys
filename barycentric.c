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

typedef struct Polygon Polygon;

struct Polygon
{
	Point2 *pts;
	Point *scrpts;
	int npts;

	void (*push)(Polygon*, Point2);
};

Rectangle UR = {0,0,1,1};	/* unit rectangle */
RFrame worldrf;
Image *pal[NCOLORS];
Polygon *thepoly;
Point2 thepoint;

void resized(void);
Point toscreen(Point2);

void
polygonpush(Polygon *poly, Point2 p)
{
	poly->pts = realloc(poly->pts, ++poly->npts*sizeof(Point2));
	poly->pts[poly->npts-1] = p;
	poly->scrpts = realloc(poly->scrpts, (poly->npts+1)*sizeof(Point));
	poly->scrpts[poly->npts-1] = toscreen(p);
	poly->scrpts[poly->npts] = poly->scrpts[0]; /* close the polygon */
}

Polygon*
newpolygon(Point2 *pts, int npts)
{
	Polygon *poly;
	int i;

	poly = malloc(sizeof(Polygon));
	if(pts == nil){
		poly->pts = nil;
		poly->scrpts = nil;
		poly->npts = 0;
	}else{
		poly->pts = pts;
		poly->scrpts = malloc((npts+1)*sizeof(Point));
		for(i = 0; i < npts; i++)
			poly->scrpts[i] = toscreen(poly->pts[i]);
		poly->scrpts[npts] = poly->scrpts[0]; /* close the polygon */
		poly->npts = npts;
	}
	poly->push = polygonpush;
	return poly;
}

void
delpolygon(Polygon *poly)
{
	poly->push = nil;
	poly->npts = 0;
	free(poly->scrpts);
	free(poly->pts);
	free(poly);
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
	pal[PCBg] = allocimage(display, UR, screen->chan, 1, DWhite);
	pal[PCFg] = allocimage(display, UR, screen->chan, 1, DBlack);
	pal[PCPoly] = allocimage(display, UR, screen->chan, 1, DPalebluegreen);
	pal[PCPolydark] = allocimage(display, UR, screen->chan, 1, DDarkblue);
	pal[PCAux] = allocimage(display, UR, screen->chan, 1, DRed);
}

void
drawinfo(void)
{
	Triangle2 t;
	Point3 barypt;
	Point2 *nextv, *prevv, in0, in1, labelpt;
	char buf[128];
	int i;

	if(thepoly->npts < 3)
		return;

	t.p0 = thepoly->pts[0];
	t.p1 = thepoly->pts[1];
	t.p2 = thepoly->pts[2];
	barypt = barycoords(t, thepoint);

	snprint(buf, sizeof buf, "homo %v", thepoint);
	string(screen, toscreen(Pt2(Dx(screen->r)/2 - strlen(buf)/2*font->width,10,1)), pal[PCFg], ZP, font, buf);

	snprint(buf, sizeof buf, "bary %V", barypt);
	string(screen, toscreen(Pt2(Dx(screen->r)/2 - strlen(buf)/2*font->width,10+font->height+2,1)), pal[PCFg], ZP, font, buf);

	/* paint the vertex labels so that they are always visible */
	for(i = 0; i < thepoly->npts; i++){
		prevv = &thepoly->pts[(i-1+thepoly->npts) % thepoly->npts];
		nextv = &thepoly->pts[(i+1+thepoly->npts) % thepoly->npts];
		in0 = normvec2(subpt2(thepoly->pts[i], *prevv));
		in1 = normvec2(subpt2(thepoly->pts[i], *nextv));
		labelpt = addpt2(thepoly->pts[i], mulpt2(lerp2(in0, in1, 0.5), 30));

		snprint(buf, sizeof buf, "p%d", i);
		string(screen, toscreen(labelpt), pal[PCFg], ZP, font, buf);
	}
}

void
redraw(void)
{
	int i;

	lockdisplay(display);
	draw(screen, screen->r, pal[PCBg], nil, ZP);
	fillpoly(screen, thepoly->scrpts, thepoly->npts, 1, pal[PCPoly], ZP);
	poly(screen, thepoly->scrpts, thepoly->npts > 0? thepoly->npts+1: 0, Enddisc, Enddisc, 1, pal[PCPolydark], ZP);
	for(i = 0; thepoly->npts > 0 && i < thepoly->npts+1; i++)
		fillellipse(screen, thepoly->scrpts[i], 2, 2, pal[PCPolydark], ZP);
	fillellipse(screen, toscreen(thepoint), 2, 2, pal[PCAux], ZP);
	drawinfo();
	flushimage(display, 1);
	unlockdisplay(display);
}

void
rmb(Mousectl *mc)
{
	thepoint = fromscreen(mc->xy);
}

void
lmb(Mousectl *mc)
{
	if(thepoly->npts == 3){
		delpolygon(thepoly);
		thepoly = newpolygon(nil, 0);
	}
	thepoly->push(thepoly, fromscreen(mc->xy));
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
	thepoly = newpolygon(nil, 0);

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
