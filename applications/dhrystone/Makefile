
APP_NAME = dhrystone

OBJS = dhry_1.o \
       dhry_2.o

LIBS = -L../../build/libs -luser

INCLUDES = -I../../giet_libs -I. -I../..

LIB_DEPS = ../../build/libs/libuser.a

appli.elf: $(OBJS) $(APP_NAME).ld $(LIBS_DEPS)
	$(LD) -o $@ -T $(APP_NAME).ld $(OBJS) $(LIBS)
	$(DU) -D $@ > $@.txt

%.o: %.c 
	$(CC)  $(INCLUDES) $(CFLAGS) -c -o  $@ $<

clean:
	rm -f *.o *.elf *.txt core *~
