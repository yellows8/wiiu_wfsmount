OBJS = wiiu_usbstoragemount.o
PACKAGES = fuse
CFLAGS = $(shell pkg-config --cflags $(PACKAGES)) -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=25 -g
LDFLAGS = $(shell pkg-config --libs $(PACKAGES))
LIBS = -lcrypto
OUTPUT = wiiu_usbstoragemount
main: $(OBJS)
	gcc $(CFLAGS) -o $(OUTPUT) $(OBJS) $(LIBS) $(LDFLAGS)
clean:
	rm -f $(OUTPUT) $(OBJS)

