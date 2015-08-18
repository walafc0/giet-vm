APP_NAME=pgcd

USE+= stdio.o

USES=$(patsubst %,$(BUILD_PATH)/$(LIB_NAME)/%,$(USE))

OBJS= main.o 

all: $(APP_NAME).elf

BIN_NAME_PATH=$(BUILD_PATH)$(APP_NAME).elf

$(APP_NAME).elf: $(OBJS) $(APP_NAME).ld
	$(LD) -o $(BIN_NAME_PATH) -T $(APP_NAME).ld $(OBJS) $(USES)
	$(DU) -D $(BIN_NAME_PATH) > $@.txt

%.o: %.c 
	$(CC)  $(INCLUDE) $(CFLAGS) $($*.o_CFLAGS) -c -o  $@ $<
	$(DU) -D  $@ >  $@.txt


clean:
	rm -f *.o *.elf *.txt core *~ 2>$(TRASH)
	rm $(BIN_NAME_PATH) 2>$(TRASH)
