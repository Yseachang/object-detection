CC=/home/meetyoo/t113/Tina-Linux/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-gcc
CFLAGS = -Wall -I/home/meetyoo/t113/Tina-Linux/out/t113-bingpi_m2/staging_dir/target/usr/include
LIBS = -L/home/meetyoo/t113/Tina-Linux/out/t113-bingpi_m2/staging_dir/target/usr/lib -static -lcurl -lssl -lcrypto -lpthread

all: detect

detect: detect.o
	$(CC) $(CFLAGS) -o detect detect.o $(LIBS)

detect.o: detect.c
	$(CC) $(CFLAGS) -c detect.c

clean:
	rm -f exe detect.o