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
	PCLine,
	PCLineseg,
	PCCin,
	PCCout,
	PCPt,
	NCOLORS
};

Rectangle UR = {0,0,1,1};	/* unit rectangle */
RFrame worldrf;
Image *pal[NCOLORS];
Point2 theline[2], thecircle[2];
int npts;
Point2 Xpts[2];
int nXpts;

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

double
sgn(double n)
{
	return n < 0? -1: 1;
}

int
lineXcircle(Point2 *r)
{
	Point2 p0, p1, dp;
	double dr, D, cr, Δ;

	p0 = subpt2(theline[0], thecircle[0]);
	p1 = subpt2(theline[1], thecircle[0]);
	dp = subpt2(p1, p0);
	dr = vec2len(dp);
	D = p0.x*p1.y - p0.y*p1.x;
	cr = vec2len(subpt2(thecircle[1], thecircle[0]));
	Δ = cr*cr*dr*dr - D*D;

	if(Δ < 0)		/* imaginary */
		return 0;
	else if(Δ == 0){	/* tangent */
		r[0].x = (D*dp.y + sgn(dp.y)*dp.x*sqrt(Δ))/(dr*dr);
		r[0].y = (-D*dp.x + fabs(dp.y)*sqrt(Δ))/(dr*dr);
		r[0] = addpt2(r[0], thecircle[0]);
		return 1;
	}else{			/* secant */
		r[0].x = (D*dp.y + sgn(dp.y)*dp.x*sqrt(Δ))/(dr*dr);
		r[0].y = (-D*dp.x + fabs(dp.y)*sqrt(Δ))/(dr*dr);
		r[0] = addpt2(r[0], thecircle[0]);
		r[1].x = (D*dp.y - sgn(dp.y)*dp.x*sqrt(Δ))/(dr*dr);
		r[1].y = (-D*dp.x - fabs(dp.y)*sqrt(Δ))/(dr*dr);
		r[1] = addpt2(r[1], thecircle[0]);
		return 2;
	}
}

void
initpalette(void)
{
	pal[PCBg] = allocimage(display, UR, screen->chan, 1, DWhite);
	pal[PCFg] = allocimage(display, UR, screen->chan, 1, DBlack);
	pal[PCLine] = allocimage(display, UR, screen->chan, 1, 0xEEEEEEFF);
	pal[PCLineseg] = allocimage(display, UR, screen->chan, 1, DDarkblue);
	pal[PCCin] = allocimage(display, UR, screen->chan, 1, DPalebluegreen);
	pal[PCCout] = allocimage(display, UR, screen->chan, 1, DDarkyellow);
	pal[PCPt] = allocimage(display, UR, screen->chan, 1, DRed);
}

void
drawline(void)
{
	Point pts[2], dp;
	int i;

	for(i = 0; i < nelem(pts); i++) pts[i] = toscreen(theline[i]);
	dp = subpt(pts[1], pts[0]);

	line(screen, pts[0], pts[1], 0, 0, 1, pal[PCLineseg], ZP);
	for(i = 0; i < nelem(pts); i++){
		line(screen, pts[i], addpt(mulpt(dp, i == 0? -10: 10), pts[i]), 0, 0, 0, pal[PCLine], ZP);
		fillellipse(screen, pts[i], 2, 2, pal[PCPt], ZP);
	}
}

void
drawcircle(void)
{
	Point p;
	double r;

	p = toscreen(thecircle[0]);
	r = vec2len(subpt2(thecircle[1], thecircle[0]));

	fillellipse(screen, p, r, r, pal[PCCin], ZP);
	ellipse(screen, p, r, r, 0, pal[PCCout], ZP);
	fillellipse(screen, p, 2, 2, pal[PCPt], ZP);
}

void
drawX(void)
{
	int i;

	if(nXpts < 1)
		return;

	for(i = 0; i < nelem(Xpts); i++)
		fillellipse(screen, toscreen(Xpts[i]), 2, 2, pal[PCPt], ZP);
}

void
redraw(void)
{
	lockdisplay(display);
	draw(screen, screen->r, pal[PCBg], nil, ZP);
	if(npts >= 4){
		drawcircle();
		drawX();
	}
	if(npts >= 2)
		drawline();
	flushimage(display, 1);
	unlockdisplay(display);
}

void
rmb(Mousectl *mc)
{
	USED(mc);
}

void
lmb(Mousectl *mc)
{
	if(npts >= 4)
		npts = 0;
	if(npts >= 0 && npts < 2)
		theline[npts++] = fromscreen(mc->xy);
	else if(npts >= 2 && npts < 4)
		thecircle[npts++ & 1] = fromscreen(mc->xy);
	if(npts >= 4)
		nXpts = lineXcircle(Xpts);
}

void
mouse(Mousectl *mc)
{
	static Mouse om;

	if(mc->buttons == om.buttons)
		return;

	if((mc->buttons&1) != 0)
		lmb(mc);
	if((mc->buttons&4) != 0)
		rmb(mc);
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
