#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>

enum {
	NSAMP	= 1323,		/* 30ms */
};

static int debug;

static Point2 ZP2 = {0,0,1};
RFrame beatrf;
Image *screenb;
Channel *drawc;

Point2
p2(Point p)
{
	return Pt2(p.x, p.y, 1);
}

Point
p2p(Point2 p)
{
	return Pt(p.x, p.y);
}

Point
toscreen(Point2 p)
{
	return p2p(invrframexform(p, beatrf));
}

void
redraw(void)
{
	lockdisplay(display);
	draw(screen, screen->r, screenb, nil, screenb->r.min);
	flushimage(display, 1);
	unlockdisplay(display);
}

void
drawproc(void *arg)
{
	Channel *c;

	threadsetname("drawproc");

	c = arg;

	for(;;){
		recv(c, nil);
		redraw();
	}
}

void
beatproc(void *arg)
{
	Point2 v;
	Point poly[NSAMP];
	u32int ss[NSAMP];	/* stereo samples */
	s32int as[NSAMP], s;	/* avg'd samples */
	u16int min, max;
	int fd, i, ns;
	double θ;
	uvlong frame, t0, t1;
	char buf[32];

	threadsetname("beatproc");

	fd = *(int*)arg;
	ns = frame = 0;

	t0 = nsec();
	while(readn(fd, ss, 2*2*NSAMP) > 0){
		min = (1<<15) - 1;
		max = 0;
		for(i = 0; i < nelem(ss); i++){
			s = (s16int)(ss[i] & 0xFFFF) + (s16int)(ss[i]>>16 & 0xFFFF);
			/* [-2¹⁶, 2¹⁶) → [0, 2¹⁵) */
			s = (s/2 + (1<<15))/2;
			min = s < min? s: min;
			max = s > max? s: max;
			as[i] = s;
		}
		lockdisplay(display);
		for(i = 0; i < nelem(as); i++){
			θ = 2*PI * ((double)(ns++ % NSAMP)/NSAMP);
			v = Vec2(cos(θ), sin(θ));
			s = as[i];
			poly[i] = toscreen(addpt2(ZP2, mulpt2(v, 4*(s-min)*100/max)));
//			line(screenb, toscreen(ZP2), toscreen(addpt2(ZP2, mulpt2(v, 400))), 0, 0, 0, display->black, ZP);
//			line(screenb, toscreen(ZP2), toscreen(addpt2(ZP2, mulpt2(v, 4*(s-min)*100/max))), 0, 0, 0, display->white, ZP);
		}
		draw(screenb, screenb->r, display->black, nil, ZP);
		fillpoly(screenb, poly, nelem(poly), 1, display->white, ZP);

		if(debug){
			t1 = nsec();
			snprint(buf, sizeof(buf), "%llud (%3.0fHz)", ++frame, 1e9/(t1-t0));
			t0 = t1;
			stringbg(screenb, addpt(screen->r.min, Pt(20, 20)), display->white, ZP, font, buf, display->black, ZP);
		}
		unlockdisplay(display);
		nbsend(drawc, nil);
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
resized(void)
{
	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	freeimage(screenb);
	screenb = allocimage(display, screen->r, screen->chan, 0, DBlack);
	beatrf.p = p2(divpt(addpt(screen->r.min, screen->r.max), 2));
	unlockdisplay(display);
	nbsend(drawc, nil);
}

void
usage(void)
{
	fprint(2, "usage: %s /dev/audio\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	Mousectl *mc;
	Keyboardctl *kc;
	Rune r;
	int fd;

	ARGBEGIN{
	case 'd': debug++; break;
	default: usage();
	}ARGEND;
	if(argc != 1)
		usage();

	fd = open(argv[0], OREAD);
	if(fd < 0)
		sysfatal("open: %r");

	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	beatrf.p = p2(divpt(addpt(screen->r.min, screen->r.max), 2));
	beatrf.bx = Vec2(1, 0);
	beatrf.by = Vec2(0,-1);
	screenb = allocimage(display, screen->r, screen->chan, 0, DBlack);
	if(screenb == nil)
		sysfatal("allocimage: %r");

	drawc = chancreate(sizeof(void*), 1);
	proccreate(drawproc, drawc, 8*1024);
	proccreate(beatproc, &fd, 32*1024);

	display->locking = 1;
	unlockdisplay(display);
	nbsend(drawc, nil);

	for(;;){
		enum { MOUSE, RESIZE, KEYBOARD };
		Alt a[] = {
			{mc->c, &mc->Mouse, CHANRCV},
			{mc->resizec, nil, CHANRCV},
			{kc->c, &r, CHANRCV},
			{nil, nil, CHANEND}
		};

		switch(alt(a)){
		case RESIZE:
			resized();
			break;
		case KEYBOARD:
			key(r);
			break;
		}
	}
}
