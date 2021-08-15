CC = gcc
CFLAGS = -g -std=gnu99 -Wall

OBJS = smallsh.o

SRCS = smallsh.c

HEADERS = 

TARGET = smallsh

${TARGET}: ${OBJS} ${HEADERS}
	${CC} ${OBJS} -o ${TARGET}

${OBJS}: ${SRCS}
	${CC} ${CFLAGS} -c $(@:.o=.c)

clean:
	$(RM) ${TARGET} ${OBJS}