CC=/home/meetyoo/t113/Tina-Linux/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-gcc
CXX=/home/meetyoo/t113/Tina-Linux/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-g++
CFLAGS = -Wall -I/home/meetyoo/t113/Tina-Linux/out/t113-bingpi_m2/staging_dir/target/usr/include
LIBS = -L/home/meetyoo/t113/Tina-Linux/out/t113-bingpi_m2/staging_dir/target/usr/lib  -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs  -lcurl -lssl -lcrypto -lpthread

all: display

display: display.o
	$(CXX) $(CFLAGS) -o display display.o $(LIBS)

display.o: display.cpp
	$(CXX) $(CFLAGS) -c display.cpp

clean:
	rm -f display display.o