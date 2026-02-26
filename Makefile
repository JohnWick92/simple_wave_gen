# Makefile
CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS = -lrt -pthread
GTKMMLIBS = `pkg-config --cflags --libs gtkmm-4.0`

# Diretórios
INCDIR = include
SRCDIR = src
BINDIR = bin

# Arquivos fonte
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
TARGETS = generator controller viewer

all: $(TARGETS)

generator: $(SRCDIR)/generator.cpp
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $< $(LDFLAGS)

controller: $(SRCDIR)/controller.cpp
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $<

viewer: $(SRCDIR)/gtkview.cpp
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -o $(BINDIR)/$@ $< $(LDFLAGS) $(GTKMMLIBS)

clean:
	rm -f $(BINDIR)/*
	rm -f /tmp/sine_comandos
	rm -f /dev/shm/sine_buffer

.PHONY: all clean
