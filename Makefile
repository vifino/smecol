# Makefile for SMeCoL

PROJECT := smecol
VERSION=0.1

FLAGS := -pedantic -Wall -O2

SO_FILE ?="lib${PROJECT}.so.${VERSION}"
OBJS := src/${PROJECT}.o src/alloc.o src/chain.o src/ctx.o src/modules.o

all: ${SO_FILE}

# Generic rules
.c.o:
	${CC} -c -fPIC ${CPPFLAGS} ${LDFLAGS} ${FLAGS} ${CFLAGS} -o $@ $<

${SO_FILE}: ${OBJS}
	${CC} -shared -o ${SO_FILE} ${OBJS}


# Cleaning
clean:
	rm -f ${SO_FILE} ${OBJS}
