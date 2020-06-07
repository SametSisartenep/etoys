#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>

enum {
	MAXMAGNITUDE = 40
};

enum {
	Cbg,
	Cfg,
	NCOLOR
};

typedef struct Strut Strut;

struct Strut
{
	Point2 v;
	Strut *next;
};

RFrame worldrf;
Image *pal[NCOLOR];
Strut *struts;
Point2 origin;

Strut*
newstrut(Point2 v)
{
	Strut *s;

	s = malloc(sizeof(Strut));
	if(s == nil)
		sysfatal("malloc: %r");
	setmalloctag(s, getcallerpc(&v));
	s->v = v;
	s->next = nil;
	return s;
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
	pal[Cbg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000ff);
	pal[Cfg] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xffffffff);
}

void
redraw(void)
{
	Strut *s;
	Point2 p0, p1;

	p0 = origin;

	lockdisplay(display);
	draw(screen, screen->r, pal[Cbg], nil, ZP);
	for(s = struts; s != nil; s = s->next){
		p1 = addpt2(p0, s->v);
		line(screen, toscreen(p0), toscreen(p1), Endsquare, Endarrow, 0, pal[Cfg], ZP);
		p0 = p1;
	}
	flushimage(display, 1);
	unlockdisplay(display);
}

void
resized(void)
{
	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	unlockdisplay(display);
	worldrf.p = Pt2(screen->r.min.x,screen->r.max.y,1);
	origin = Pt2(Dx(screen->r)/2,Dy(screen->r)/2,1);
	redraw();
}

void
mouse(Mouse m)
{
	Strut *s;
	Point2 mpos, v;
	double θ;

	if((m.buttons & 1) != 0){
		mpos = subpt2(fromscreen(m.xy), origin);
		θ = atan2(mpos.y, mpos.x);
		//v = Vec2(cos(frand() * 2*PI),sin(frand() * 2*PI));
		v = Vec2(cos(θ),sin(θ));
		v = mulpt2(v, frand()*MAXMAGNITUDE);
		s = newstrut(v);
		if(struts == nil)
			struts = s;
		else{
			s->next = struts;
			struts = s;
		}
	}
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
	worldrf.p = Pt2(screen->r.min.x,screen->r.max.y,1);
	worldrf.bx = Vec2(1, 0);
	worldrf.by = Vec2(0,-1);
	origin = Pt2(Dx(screen->r)/2,Dy(screen->r)/2,1);

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
			mouse(mc->Mouse);
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
