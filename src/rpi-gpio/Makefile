.PHONY: all clean

all:
	cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=qnx/build/aarch64-qnx.cmake
	cmake --build build/

clean:
	rm -rf build
