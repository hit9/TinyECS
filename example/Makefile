defalut: build

cmake:
	cmake -S  . -B build \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1

build:
	@if [ ! -d build ]; then \
		$(MAKE) cmake; \
	fi
	cd build && make

run:
	./build/Tinyecs_Example

clean:
	make -C build clean

.PHONY: build
