
CHECK      = check

EXE       = event_messaged
EXE_OBJS  = $(EXE).o message.o event_messaged_sdbus.o

OBJS_CHECK = test.o message.o

CFLAGS += -Wall -Werror
CPPFLAGS  += -g -fpic -Wall -Werror -std=c++11

DEPPKGS = libsystemd
INCLUDES += $(shell pkg-config --cflags $(DEPPKGS)) 
LIBS += $(shell pkg-config --libs $(DEPPKGS))



all: $(EXE)

%.o : %.c 
	$(CC) -c $< $(CFLAGS) $(INCLUDES) -o $@

%.o : %.C
	$(CXX) -c $< $(CPPFLAGS) -o $@


$(EXE): $(EXE_OBJS)
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@


$(CHECK): $(OBJS_CHECK)
	$(CXX) $^ $(LDFLAGS) -o $@
	./$(CHECK)

clean:
	rm -f $(CHECK) *.o *.so $(EXE)
