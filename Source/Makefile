defalut: build

cmake-build:
	cmake -S  . -B Build \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_STANDARD=20 \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=1

build: cmake-build
	cd Build && make

clean:
	make -C Build clean

lint:
	cppcheck . --enable=warning,style,performance,portability\
		--inline-suppr --language=c++ --std=c++20 -i main.cpp -i Build -i Tests

.PHONY: build
