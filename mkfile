</$objtype/mkfile

BIN=/$objtype/bin/etoys
TARG=\
	triangles\
	beziers\
	bsplines\
	grid\
	isometric\
	ptinline\
	chain\
	imagelerp\
	rframeviz\
	ptinpoly\
	ptintriangle\

HFILES=\
	libgeometry/geometry.h\

LIB=\
	libgeometry/libgeometry.a$O\

CFLAGS=$CFLAGS -Ilibgeometry

</sys/src/cmd/mkmany

libgeometry/libgeometry.a$O:
	cd libgeometry
	mk install

clean nuke:V:
	rm -f *.[$OS] [$OS].??* $TARG
	@{cd libgeometry; mk $target}
