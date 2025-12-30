
SRCS := $(wildcard *.c) $(wildcard helpers/*/*.c)
OBJS := $(SRCS:.c=.o)
HDRS := $(wildcard *.h) $(wildcard helpers/*/*.h)

CFLAGS := -g `pkg-config fuse --cflags` -I. -Ihelpers -Ihelpers/bitmap -Ihelpers/blocks -Ihelpers/storage
LDLIBS := `pkg-config fuse --libs`

CC := gcc
CXX := g++
CXXFLAGS := $(CFLAGS)

nufs: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

nufs_cpp: nufs_cpp.o $(filter-out nufs.o,$(OBJS))
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean: unmount
	rm -f nufs nufs_cpp *.o test.log filesystem-data.bin
	rmdir mnt || true

mount: nufs
	mkdir -p mnt || true
	./nufs -s -f mnt filesystem-data.bin

unmount:
	fusermount -u mnt || true

test: nufs
	perl test.pl

gdb: nufs
	mkdir -p mnt || true
	gdb --args ./nufs -s -f mnt filesystem-data.bin

.PHONY: clean mount unmount gdb

