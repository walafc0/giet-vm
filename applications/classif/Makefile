
APP_NAME = classif

OBJS= main.o 

LIBS= -L../../build/libs -luser

INCLUDES = -I.  -I../..  -I../../giet_libs  -I../../giet_xml  

LIB_DEPS = ../../build/libs/libuser.a

appli.elf: $(OBJS) $(APP_NAME).ld $(LIBS_DEPS) 
	$(LD) -o $@ -T $(APP_NAME).ld $(OBJS) $(LIBS)
	$(DU) -D $@ > $@.txt

%.o: %.c 
	$(CC)  $(INCLUDES) $(CFLAGS) -c -o  $@ $<

clean:
	rm -f *.o *.elf *.txt *.pyc core *~
