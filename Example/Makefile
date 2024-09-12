defalut: build

cmake:
	cmake -S  . -B Build \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1

build:
	@if [ ! -d build ]; then \
		$(MAKE) cmake; \
	fi
	cd Build && make

run:
	./Build/Tinyecs_Example

clean:
	make -C build clean

.PHONY: build
