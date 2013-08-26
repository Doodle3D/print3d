SRC := print3d.c logger.c serial.c
OBJ := print3d.o logger.o serial.o
HDR := print3d.h
EXEC := print3d
CFLAGS += -Wall

ifndef ($(OPENWRT_BUILD))
	CC := gcc
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
