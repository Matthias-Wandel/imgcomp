#--------------------------------
# imgcomp makefile for Linux (raspberry pi)
#--------------------------------
OBJ=.
SRC=.
CFLAGS:= $(CFLAGS) -O3 -Wall

all: imgcomp

objs = $(OBJ)/djpeg.o $(OBJ)/compare.o $(OBJ)/jpeg2mem.o 

$(OBJ)/%.o:$(SRC)/%.c imgcomp.h
	${CC} $(CFLAGS) -c $< -o $@

imgcomp: $(objs) libjpeg/libjpeg.a jconfig.h
	${CC} -o imgcomp $(objs) libjpeg/libjpeg.a

clean:
	rm -f $(objs) imgcomp
