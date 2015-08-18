
APP_NAME = raycast

OBJS = ctrl.o disp.o game.o main.o

LIBS = -L../../build/libs -luser -lmath

INCLUDES = -I../../giet_libs -I. -I../..

LIB_DEPS = ../../build/libs/libuser.a ../../build/libs/libmath.a

appli.elf: $(OBJS) $(APP_NAME).ld $(LIBS_DEPS)
	$(LD) -o $@ -T $(APP_NAME).ld $(OBJS) $(LIBS)
	$(DU) -D $@ > $@.txt

%.o: %.c
	$(CC)  $(INCLUDES) $(CFLAGS) -c -o  $@ $<

clean:
	rm -f *.o *.elf *.elf.txt *.pyc
