#-------------------------------------------------------------
# imgcomp makefile for Linux (compile on raspberry pi)
# Matthias Wandel 2015
#-------------------------------------------------------------
OBJ=obj
SRC=src

# with debug:
#CFLAGS:= $(CFLAGS) -O3 -Wall -g

CFLAGS:= $(CFLAGS) -O3 -Wall

all: objdir imgcomp blink_camera_led run_stepper

objdir:
	@mkdir -p obj

objs = $(OBJ)/main.o $(OBJ)/config.o $(OBJ)/compare.o $(OBJ)/jpeg2mem.o \
	$(OBJ)/jpgfile.o $(OBJ)/exif.o $(OBJ)/start_raspistill.o $(OBJ)/util.o $(OBJ)/send_udp.o

$(OBJ)/jpgfile.o $(OBJ)/exif.o $(OBJ)/start_raspistill.o: $(SRC)/jhead.h
$(OBJ)/main.o $(OBJ)/config.o: $(SRC)/config.h

$(OBJ)/%.o:$(SRC)/%.c $(SRC)/imgcomp.h
	${CC} $(CFLAGS) -c $< -o $@

imgcomp: $(objs)
	${CC} -lm -o imgcomp $(objs) -ljpeg

$(SRC)/pi_model.h:
	$(SRC)/identify_pi >> $(SRC)/pi_model.h

blink_camera_led: $(SRC)/blink_camera_led.c $(SRC)/pi_model.h
	$(CC) -o blink_camera_led $(SRC)/blink_camera_led.c

run_stepper: $(SRC)/run_stepper.c $(SRC)/stepper_ip.c $(SRC)/pi_model.h
	$(CC) -lm -o run_stepper $(SRC)/run_stepper.c $(SRC)/stepper_ip.c

clean:
	rm -f $(objs) imgcomp

