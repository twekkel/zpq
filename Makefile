CXX      = g++
CC       = gcc
# lib/ is vendored: libzpaq from ../zpaqf (with divsufsort replaced by libsais),
# liblzma byte-identical to ../7zip/C, libsais from ../libsais
LIB      = lib
WARN      = -Wall -Wextra
CXXFLAGS  = -O3 -march=native -flto -Wno-stringop-overflow -Dunix -I$(LIB)
CFLAGS    = -O3 -flto -DZ7_ST
TARGET    = zpq
LZMA_C    = Alloc CpuArch LzFind LzFindOpt LzmaDec LzmaEnc LzmaLib
LZMA_O    = $(LZMA_C:=.o)

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): zpq.cpp libzpaq.o libsais.o $(LZMA_O)
	$(CXX) $(CXXFLAGS) $(WARN) -o $@ zpq.cpp libzpaq.o libsais.o $(LZMA_O)

# libzpaq is third-party: build it without -Wall/-Wextra
libzpaq.o: $(LIB)/libzpaq.cpp $(LIB)/libzpaq.h $(LIB)/libsais.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

libsais.o: $(LIB)/libsais.c $(LIB)/libsais.h
	$(CC) $(CFLAGS) -I$(LIB) -c $< -o $@

%.o: $(LIB)/liblzma/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Round trip: compress, decompress, compare with the original
test: $(TARGET)
	@[ "$$(echo test | ./$(TARGET) -c | ./$(TARGET) -d)" = test ] && echo OK || { echo FAILED; exit 1; }

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET) libzpaq.o libsais.o $(LZMA_O)
