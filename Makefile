CXX     ?= g++
CC      ?= cc
LIB      = lib
WARN      = -Wall -Wextra
CXXFLAGS  = -O3 -march=native -flto=auto -Wno-stringop-overflow -Dunix -I$(LIB)
CFLAGS    = -O3 -flto=auto -DZ7_ST
TARGET    = zpq
LZMA_C    = Alloc CpuArch LzFind LzFindOpt LzmaDec LzmaEnc LzmaLib
LZMA_O    = $(LZMA_C:=.o)
LIB_H     = $(wildcard $(LIB)/*.h $(LIB)/liblzma/*.h)

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.PHONY: all clean install uninstall test

all: $(TARGET)

$(TARGET): zpq.cpp libzpaq.o libsais.o $(LZMA_O) $(LIB_H)
	$(CXX) $(CXXFLAGS) $(WARN) -o $@ zpq.cpp libzpaq.o libsais.o $(LZMA_O)

libzpaq.o: $(LIB)/libzpaq.cpp $(LIB)/libzpaq.h $(LIB)/libsais.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

libsais.o: $(LIB)/libsais.c $(LIB)/libsais.h
	$(CC) $(CFLAGS) -I$(LIB) -c $< -o $@

%.o: $(LIB)/liblzma/%.c $(LIB_H)
	$(CC) $(CFLAGS) -c $< -o $@

# Round trip: compress, decompress, compare with the original
test: $(TARGET)
	@[ "$$(echo test | ./$(TARGET) -c | ./$(TARGET) -d)" = test ] && echo OK || { echo FAILED; exit 1; }

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET) libzpaq.o libsais.o $(LZMA_O)
