#include <u.h>
#include <libc.h>
#include <draw.h>

int
max(int a, int b)
{
	return a > b? a: b;
}

/* 
 * A Fast 2D Point-On-Line Test
 * by Alan Paeth
 * from "Graphics Gems", Academic Press, 1990
*/
int
ptinline(Point p, Point q, Point t)
{
	/*
	 * given a line through q and t
	 * return 0 if p is not on the line through      <--q--t-->
	 *        1 if p is on the open ray ending at q: <--q
	 *        2 if p is on the closed interior along:   q--t
	 *        3 if p is on the open ray beginning at t:    t-->
	 */
	if(q.x == t.x && q.y == t.y){
		if(p.x == q.x && p.y == q.y)
			return 2;
		return 0;
	}

	if(abs((t.y - q.y)*(p.x - q.x) - (p.y - q.y)*(t.x - q.x)) >=
	    max(abs(t.x - q.x), abs(t.y - q.y)))
		return 0;
	if((t.x < q.x && q.x < p.x) || (t.y < q.y && q.y < p.y))
		return 1;
	if((p.x < q.x && q.x < t.x) || (p.y < q.y && q.y < t.y))
		return 1;
	if((q.x < t.x && t.x < p.x) || (q.y < t.y && t.y < p.y))
		return 3;
	if((p.x<t.x && t.x<q.x) || (p.y<t.y && t.y<q.y))
		return 3;
	return 2;
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
	Point pts[3];
	char buf[256], *p;
	int n;

	fmtinstall('P', Pfmt);
	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc > 0)
		usage();

	pts[0] = Pt(0,0);
	pts[1] = Pt(2,2);

	while((n = read(0, buf, sizeof(buf)-1)) > 0){
		buf[n-1] = buf[n] = 0;

		pts[2].x = strtol(buf, &p, 10);
		if(p == nil || *p == 0){
			fprint(2, "wrong x coordinate\n");
			continue;
		}
		pts[2].y = strtol(p, &p, 10);
		if(p == nil || *p != 0){
			fprint(2, "wrong y coordinate\n");
			continue;
		}

		switch(ptinline(pts[2], pts[0], pts[1])){
		case 0:
			print("%P is not on %P%P\n", pts[2], pts[0], pts[1]);
			break;
		case 1:
			print("%P in (∞,%P)\n", pts[2], pts[0]);
			break;
		case 2:
			print("%P in [%P,%P]\n", pts[2], pts[0], pts[1]);
			break;
		case 3:
			print("%P in (%P,∞)\n", pts[2], pts[1]);
			break;
		}
	}

	exits(nil);
}
