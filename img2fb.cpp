#include <algorithm>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
	char ** it = std::find(begin, end, option);
	if (it != end && ++it != end) {
		return *it;
	}
	static std::string empty_string("");
	return (char*)empty_string.c_str();
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

int main(int argc, char *argv[])
{
	int fd;
	SDL_Rect dest = {0};
	std::string filename = "";
	uint32_t rmask = 0x0000f800, gmask = 0x000007e0, bmask = 0x0000001f, amask = 0x00000000;

	if(cmdOptionExists(argv, argv+argc, "-i")) {
		filename = std::string(getCmdOption(argv, argv+argc, "-i")).c_str();
	}

	if(cmdOptionExists(argv, argv+argc, "-h") || cmdOptionExists(argv, argv+argc, "-help") || argc < 2 || filename.empty()) {
		fprintf(stderr, "Usage: %s [-i] <file> [-d]\n\t[-i] <file> valid image file types are JPG, PNG, TIF, WEBP\n\t[-d] disable aspect ratio\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if ((fd = open("/dev/fb1", O_RDWR)) < 0) {
		perror("can't open device");
		abort();
	}

	SDL_Surface *image = IMG_Load(filename.c_str());
	if(image == NULL) {
		fprintf(stderr, "Can't open file\n");
		exit(EXIT_FAILURE);
	}

	SDL_Surface *rgb565_surface = SDL_CreateRGBSurface(0, image->w, image->h, 16, rmask, gmask, bmask, amask);
	SDL_Surface *surface = SDL_CreateRGBSurface(0, 320, 240, 16, rmask, gmask, bmask, amask);

	if(!cmdOptionExists(argv, argv+argc, "-d")) {
		dest.w = surface->w;
		dest.h = (int)(float)image->h / (float)image->w * 320;
		if(image->w < image->h) {
			dest.w = (int)(float)image->w / (float)image->h * 320;
		}
		dest.x = (320 - dest.w) / 2;
		dest.y = (240 - dest.h) / 2;		
		SDL_BlitSurface(image, NULL, rgb565_surface, NULL);
		SDL_BlitScaled(rgb565_surface, NULL, surface, &dest);
	} else {
		SDL_BlitSurface(image, NULL, rgb565_surface, NULL);
		SDL_BlitScaled(rgb565_surface, NULL, surface, NULL);
	}

	int pitch = surface->pitch;
	int pxlength = pitch * surface->h;
	unsigned char * pixels = (unsigned char*)surface->pixels;

	uint8_t *fbp = (uint8_t*)mmap(0, pxlength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0);

	for (int it = 0; it < pxlength; it++) {
		fbp[it] = pixels[it];
	}

	close(fd);
	SDL_FreeSurface(image);
	SDL_FreeSurface(rgb565_surface);
	SDL_FreeSurface(surface);
	IMG_Quit();

	return 0;
}
