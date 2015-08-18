SRCS += $(wildcard src/*.cpp)
SRCS += $(wildcard src/libelfpp/*)
SRCS += main.cpp

HDRS += $(wildcard include/*)
HDRS += $(wildcard include/libelfpp/elfpp/*)
HDRS += $(wildcard include/libelfpp/dpp/*)

INCLUDES = -Iinclude/libelfpp -Iinclude/libelfpp/elfpp -Iinclude/ -I../xml
memo.x: $(SRCS) $(HDRS)
	g++ -Wall -g -DDISTRIBUTED_SCHEDULERS   $(SRCS) $(INCLUDES) -o memo.x

debug: $(SRCS) $(HDRS)
	g++ -Wall -DMOVER_DEBUG -g  $(SRCS) $(INCLUDES) -o debug.x

check:
	test/check.sh

clean:
	rm -f *.x
