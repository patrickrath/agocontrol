LD = $(CXX)
INCLUDES ?= -I../../shared -I.

%.o : %.cpp
	$(CXX) -c $(CFLAGS) $(INCLUDES) -o $@ $<

all: agoblinkm

agoblinkm: agoblinkm.o
	$(LD) -o $@ $(LDFLAGS) $^ -lagoclient 

clean:
	rm -f agoblinkm agoblinkm.o

install:
	$(INSTALL_DIR) $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) agoblinkm $(DESTDIR)$(BINDIR)
