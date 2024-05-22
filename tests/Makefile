default: build

install:
	conan install . --output-folder=build --build=missing -s compiler.cppstd=20 -s build_type=Release

cmake-build:
	cd build && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1

build: cmake-build
	cd build && make

run: build
	ctest --test-dir ./build --output-on-failure

clean:
	make -C build clean

.PHONY: build
