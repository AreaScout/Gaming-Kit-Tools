#LCD Tools Makefile
CC = g++
CPPFLAGS = -mfloat-abi=hard -marm -mtune=cortex-a15.cortex-a7 -mcpu=cortex-a15

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

all: gpio_button img2fb show_info osd2fb

gpio_button: gpio_button.cpp
	$(CC) -o gpio_button gpio_button.cpp
img2fb: img2fb.cpp
	$(CC) -o img2fb img2fb.cpp -lSDL2 -lSDL2_image
show_info: show_info.cpp
	$(CC) -o show_info show_info.cpp -lSDL2 -lSDL2_ttf $(CPPFLAGS)
osd2fb: osd2fb.cpp
	$(CC) -o osd2fb osd2fb.cpp -lSDL2 -lSDL2_ttf
clean:
	rm -f gpio_button img2fb show_info osd2fb

install:
	install -d $(PREFIX)/bin/
	install -m 4755 gpio_button $(PREFIX)/bin/
	install -m 4755 img2fb $(PREFIX)/bin/
	install -m 4755 show_info $(PREFIX)/bin/
	install -m 4755 osd2fb $(PREFIX)/bin/
