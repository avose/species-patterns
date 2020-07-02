#
# Main configuration
#

CFLAGS= -Wall -O2 #-Wall -g
INC= -I/usr/include
LIB= -L/usr/lib -lpthread -lssl
CC= gcc

#
# Build targets
#

PROGS= governor client arbiter 

all: ${PROGS}

#

arbiter.o: arbiter.c arbiter.h protocol.h makefile
	${CC} ${CFLAGS} ${INC} -c arbiter.c

arbiter: arbiter.o protocol.o util.o makefile
	${CC} ${CFLAGS} ${LIB} arbiter.o protocol.o util.o -o arbiter

#

governor.o: governor.c governor.h protocol.h makefile
	${CC} ${CFLAGS} ${INC} -c governor.c

governor: governor.o protocol.o util.o makefile
	${CC} ${CFLAGS} ${LIB} governor.o protocol.o util.o -o governor

#

client.o: client.c client.h protocol.h makefile
	${CC} ${CFLAGS} ${INC} -c client.c

client: client.o protocol.o util.o makefile
	${CC} ${CFLAGS} ${LIB} client.o protocol.o util.o -o client

#

protocol.o: protocol.c protocol.h makefile
	${CC} ${CFLAGS} ${INC} -c protocol.c

util.o: util.c util.h makefile
	${CC} ${CFLAGS} ${INC} -c util.c

#

clean:
	rm -f ${PROGS} *.o *~

strip: clean
