/*
 * osd2fb Copyright (C) 2018 Daniel Mehrwald
 *
 * Displays on screen messages on the OGST Gaming Kit LCD
 *
 * This program is free software and comes with ABSOLUTELY NO WARRANTY;
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 *
 */

#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define PXLENGTH 153600

TTF_Font *font = NULL;

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
	int fontsize = 15;
	int dim = 128;
	SDL_Surface *temp = NULL, *osd_surface = NULL;
	unsigned char buffer[PXLENGTH] = {0};
	int x = 0, y = 0;
	std::string osd_text;
	const char *opt = NULL;

	uint32_t rmask = 0x0000f800, gmask = 0x000007e0, bmask = 0x0000001f, amask = 0x00000000;
	uint32_t rmask32 = 0x0000ff00, gmask32 = 0x00ff0000, bmask32 = 0xff000000, amask32 = 0x000000ff;

	if(cmdOptionExists(argv, argv+argc, "-f")) {
		filename = std::string(getCmdOption(argv, argv+argc, "-f")).c_str();
	}
	
	if(cmdOptionExists(argv, argv+argc, "-h") || cmdOptionExists(argv, argv+argc, "-help") || filename.empty()) {
		fprintf(stderr, "Usage: %s [-f] <font> [-i] <msg> [-s] [-x] [-y] [-c] [-w] [-o]"
		"\n\t[-f] <file> your TTF font to load"
		"\n\t[-i] <msg> string to display"
		"\n\t[-s] outline the string"
		"\n\t[-x] <num> x offset for the string, usually 0-320"
		"\n\t[-y] <num> y offset for the string, usually 0-240"
		"\n\t[-c] center the string"
		"\n\t[-w] <num> weight/size of the font (default 15)"
		"\n\t[-o] <num> draw overlay with transparency 0-255\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(opt = getCmdOption(argv, argv+argc, "-w")) {
		std::istringstream(std::string(opt)) >> fontsize;
	}

	if ((fd = open("/dev/fb1", O_RDWR)) < 0) {
		perror("can't open device");
		abort();
	}
	
	uint8_t *fbp = (uint8_t*)mmap(0, sizeof(buffer), PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0);	
	// copy the current framebuffer contains
	for(int i = 0; i < sizeof(buffer); i++) {
		buffer[i] = fbp[i];
	}

	TTF_Init();
	font = TTF_OpenFont(filename.c_str(), fontsize);
	if (font == NULL) {
		fprintf(stderr, "Error: open the font\n");
		exit(EXIT_FAILURE);
	}

	SDL_Surface *image = SDL_CreateRGBSurfaceFrom(buffer, 320, 240, 16, 640, rmask, gmask, bmask, amask);

	if(opt = getCmdOption(argv, argv+argc, "-o")) {
		int tmp = 0;
		std::istringstream(std::string(opt)) >> tmp;
		if(tmp) {
			dim = tmp;
		}
		SDL_Surface *overlay = SDL_CreateRGBSurface(0, image->w, image->h, 32, rmask32, gmask32, bmask32, amask32);
		SDL_FillRect(overlay, NULL, SDL_MapRGBA(overlay->format, 0, 0, 0, dim));
		// Darken the BG Image
		SDL_BlitSurface(overlay, NULL, image, NULL);
	}	

	if(cmdOptionExists(argv, argv+argc, "-i")) {
		osd_text = std::string(getCmdOption(argv, argv+argc, "-i"));
		if(osd_text.empty()) {
			fprintf(stderr, "error: no string given\n");
			exit(EXIT_FAILURE);			
		}
		SDL_Color white = {0xee, 0xfb, 0xff};
		SDL_Color black = {0x00, 0x00, 0x00};
		temp = TTF_RenderText_Blended(font, osd_text.c_str(), white);
		if(cmdOptionExists(argv, argv+argc, "-s")) {
			osd_surface = TTF_RenderText_Blended(font, osd_text.c_str(), black);
			SDL_Rect rect = {1, 1, temp->w, temp->h};
			// blit text on background surface
			SDL_SetSurfaceBlendMode(osd_surface, SDL_BLENDMODE_BLEND);
			SDL_BlitSurface(temp, NULL, osd_surface, &rect);
		} else {
			osd_surface = temp;
		}		
	} else {
		fprintf(stderr, "error: no string given\n");
		exit(EXIT_FAILURE);
	}

	if(opt = getCmdOption(argv, argv+argc, "-x")) {
		std::istringstream(std::string(opt)) >> x;
	}

	if(opt = getCmdOption(argv, argv+argc, "-y")) {
		std::istringstream(std::string(opt)) >> y;
	}

	if(cmdOptionExists(argv, argv+argc, "-c")) {
		x = (320 - osd_surface->w) / 2;
	}	

	dest.w = image->clip_rect.w;
	dest.h = image->clip_rect.h;
	dest.x = x;
	dest.y = y;

	// finaly blit the OSD
	SDL_BlitSurface(osd_surface, NULL, image, &dest);

	int pitch = image->pitch;
	int pxlength = pitch * image->h;
	unsigned char * pixels = (unsigned char*)image->pixels;

	for (int it = 0; it < pxlength; it++) {
		fbp[it] = pixels[it];
	}

	close(fd);
	SDL_FreeSurface(image);
	SDL_FreeSurface(osd_surface);

	return 0;
}
