#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

double
flerp(double a, double b, double t)
{
	return a + (b - a)*t;
}

double
fclamp(double n, double min, double max)
{
	return n < min? min: n > max? max: n;
}

Memimage*
lerpimages(Memimage *i0, Memimage *i1, double blendf)
{
	Memimage *ri;
	Point p;
	uchar *s0, *s1, *d;
	int bpp;

	assert(i0->chan == i1->chan);
	assert(Dx(i0->r) == Dx(i1->r) && Dy(i0->r) == Dy(i1->r));

	ri = allocmemimage(i0->r, i0->chan);
	if(ri == nil)
		sysfatal("allocmemimage: %r");
	bpp = (i0->depth+7)/8;

	for(p.y = 0; p.y < i0->r.max.y; p.y++)
		for(p.x = 0; p.x < i0->r.max.x; p.x++){
			s0 = byteaddr(i0, addpt(i0->r.min, p));
			s1 = byteaddr(i1, addpt(i1->r.min, p));
			d = byteaddr(ri, addpt(ri->r.min, p));
			switch(bpp){
			case 4:
				d[3] = flerp(s0[3], s1[3], blendf);
			case 3:
				d[2] = flerp(s0[2], s1[2], blendf);
			case 2:
				d[1] = flerp(s0[1], s1[1], blendf);
			case 1:
				d[0] = flerp(s0[0], s1[0], blendf);
			}
		}
	return ri;
}

void
usage(void)
{
	fprint(2, "usage: %s [-f factor] image0 image1\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Memimage *m[2], *r;
	char *s;
	int fd;
	double f;

	f = 0.5;
	ARGBEGIN{
	case 'f':
		f = strtod(EARGF(usage()), &s);
		if(*s != 0)
			sysfatal("wrong blending factor");
		break;
	default: usage();
	}ARGEND;
	if(argc != 2)
		usage();

	if(memimageinit() != 0)
		sysfatal("memimageinit: %r");
	fd = open(argv[0], OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	if((m[0] = readmemimage(fd)) == nil)
		sysfatal("readmemimage: %r");
	close(fd);
	fd = open(argv[1], OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	if((m[1] = readmemimage(fd)) == nil)
		sysfatal("readmemimage: %r");
	close(fd);
	r = lerpimages(m[0], m[1], fclamp(f, 0, 1));
	if(writememimage(1, r) < 0)
		sysfatal("writememimage: %r");
	exits(nil);
}

