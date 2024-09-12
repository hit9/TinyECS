defalut: build

cmake-build:
	cmake -S  . -B build \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_STANDARD=20 \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=1

build: cmake-build
	cd build && make

clean:
	make -C build clean

lint:
	cppcheck . --enable=warning,style,performance,portability\
		--inline-suppr --language=c++ --std=c++20 -i main.cpp -i build -i tests

.PHONY: build
