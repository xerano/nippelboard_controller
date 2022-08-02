CC=arm-linux-gnueabihf-gcc
CXX=arm-linux-gnueabihf-g++

HEADER_DIR=/usr/local/include/RF24
LIB_DIR=/usr/local/lib
LIB=rf24

CFLAGS=-marm -march=armv6zk -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -Ofast -Wall -pthread -g
LDFLAGS += -lpthread
LDFLAGS += -lmpdclient

PROGRAMS = nippelboard_controller

BINARY_PREFIX = rf24
SOURCES = $(PROGRAMS:=.cpp)

LIBS=-l$(LIB)

all: nippelboard_controller

nippelboard_controller: nippelboard_controller.o bcm2835.o
	@echo "[Linking]"
	$(CXX) -L$(LIB_DIR) -L. -o $@ $^ $(LIBS)  $(LDFLAGS)

nippelboard_controller.o: nippelboard_controller.cpp
	$(CXX) -fPIC $(CFLAGS) -I$(HEADER_DIR)/.. -c $^

bcm2835.o: bcm2835.c
	$(CC) -fPIC $(CFLAGS) -c $^

i2clcd.o: i2clcd.c
	$(CC) -fPIC $(CFLAGS) -c $^

clean:
	@echo "[Cleaning]"
	rm -rf $(PROGRAMS)
	rm -rf *.o

install: install-bin

install-bin:
	@echo "[Installing to /opt/rf24_gateway]"
	@install rf24_gateway /opt/rf24_gateway/ 

.PHONY: install upload
