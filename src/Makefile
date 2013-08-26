SRC := print3d.c logger.cpp serial.cpp
OBJ := print3d.o logger.o serial.o
HDR := print3d.h
EXEC := print3d
CFLAGS += -Wall

ifndef ($(OPENWRT_BUILD))
	CC := g++
endif

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(EXEC)
	rm -f $(OBJ)
