default: build

install:
	conan install . --output-folder=Build --build=missing -s compiler.cppstd=20 -s build_type=Release

cmake-build:
	@if [ ! -d Build ]; then \
		$(MAKE) install; \
	fi
	cd Build && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1

build: cmake-build
	@if [ ! -d Build ]; then \
		$(MAKE) cmake; \
	fi
	cd Build && make

run: build
	ctest --test-dir ./Build --output-on-failure

clean:
	make -C build clean

.PHONY: build
