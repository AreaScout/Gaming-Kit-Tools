#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/fb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "font.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define PXLENGTH 153600

bool running = true;
static char temp_buffer[256] = {0};
static char info_string[16][128] = {0};
static char ip_buffer[256] = {0};
int cpu[4], freq[8], gpu1, gpu_clock;
int usage[9] = {0};
int prev_total[9] = {0};
int prev_idle[9] = {0};
std::string gpu_string;
std::string connect_string;
std::string hdd_string;
std::string mem_string;
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

int get_ip (char *buf)
{
	struct  ifaddrs *ifa;
	int     str_cnt;

	getifaddrs(&ifa);

	while(ifa)  {
		if(ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)    {
			struct sockaddr_in *pAddr = (struct sockaddr_in *)ifa->ifa_addr;

			if(strncmp(ifa->ifa_name, "eth", 3) == 0) {
				str_cnt = sprintf(buf, "%s", inet_ntoa(pAddr->sin_addr));
			}
		}
		ifa = ifa->ifa_next;
	}
	freeifaddrs(ifa);

	return str_cnt;
}

void intHandler(int param) 
{
	running = false;
}

int calUsage(int cpu_idx, int user, int nice, int system, int idle, int wait, int irq, int srq)
{
	long total = 0;
	long cusage = 0;
	int diff_idle, diff_total;

	total = user + nice + system + idle + wait + irq + srq;

	diff_idle  = idle - prev_idle[cpu_idx];
	diff_total = total - prev_total[cpu_idx];
	if(diff_total < 0)
		diff_total = 0;

	if (total != 0)
		// https://github.com/Leo-G/DevopsWiki/wiki/How-Linux-CPU-Usage-Time-and-Percentage-is-calculated
		cusage = (1000 * (diff_total - diff_idle) / diff_total + 5) / 10;

	prev_total[cpu_idx] = total;
	prev_idle[cpu_idx] = idle;

	return cusage;
}

SDL_Surface* CreateOSDandShadow(char *string)
{
	SDL_Color white = {0xee, 0xfb, 0xff};
	SDL_Color black = {0x00, 0x00, 0x00};
	SDL_Surface *bg_surface = TTF_RenderText_Blended(font, string, black);
	SDL_Surface *fg_surface = TTF_RenderText_Blended(font, string, white);
	SDL_Rect rect = {1, 1, fg_surface->w, fg_surface->h};

	// blit text on background surface 
	SDL_SetSurfaceBlendMode(fg_surface, SDL_BLENDMODE_BLEND);
	SDL_BlitSurface(fg_surface, NULL, bg_surface, &rect);
	SDL_FreeSurface(fg_surface);

	return bg_surface;
}

SDL_Surface* blitOSD(SDL_Surface *image)
{
	SDL_Rect dest = {0};
	
	for(int i = 0;i < 16; i++) {
		dest.w = image->clip_rect.w;
		dest.h = image->clip_rect.h;
		dest.x = 0;
		SDL_Surface *temp = CreateOSDandShadow(info_string[i]);
		SDL_BlitSurface(temp, NULL, image, &dest);
		SDL_FreeSurface(temp);
		dest.y += 15;
	}

	return image;
}

void fillInfoString()
{
	get_ip(ip_buffer);
	connect_string.erase(std::remove(connect_string.begin(), connect_string.end(), '\n'), connect_string.end());

	sprintf(info_string[0], "GPU : %s, %d\xBA""C, %dMHz", gpu_string.c_str(), gpu1, gpu_clock);
	sprintf(info_string[1], "%s", connect_string.c_str());
	sprintf(info_string[2], "Current IP : %s", ip_buffer);
	sprintf(info_string[3], "%s", hdd_string.c_str());
	sprintf(info_string[4], "%s", mem_string.c_str());
	sprintf(info_string[5], "Overall CPU usage : %s%s%2d%%", (usage[0] >= 100) ? "" : " ", (usage[0] >= 10) ? "" : " ", usage[0]);
	sprintf(info_string[6], "Cortex-A15:");
	sprintf(info_string[7], "CPU1 : %s%s%2d%%  %2d\xBA""C  %.2fGHz", (usage[5] >= 100) ? "" : " ", (usage[5] >= 10) ? "" : " ", usage[5], cpu[0], (float)freq[4] / 1000000.f);
	sprintf(info_string[8], "CPU2 : %s%s%2d%%  %2d\xBA""C  %.2fGHz", (usage[6] >= 100) ? "" : " ", (usage[6] >= 10) ? "" : " ", usage[6], cpu[1], (float)freq[5] / 1000000.f);
	sprintf(info_string[9], "CPU3 : %s%s%2d%%  %2d\xBA""C  %.2fGHz", (usage[7] >= 100) ? "" : " ", (usage[7] >= 10) ? "" : " ", usage[7], cpu[2], (float)freq[6] / 1000000.f);
	sprintf(info_string[10], "CPU4 : %s%s%2d%%  %2d\xBA""C  %.2fGHz", (usage[8] >= 100) ? "" : " ", (usage[8] >= 10) ? "" : " ", usage[8], cpu[3], (float)freq[7] / 1000000.f);
	sprintf(info_string[11], "Cortex-A7:");
	sprintf(info_string[12], "CPU1 : %s%s%2d%%  %.2fGHz", (usage[1] >= 100) ? "" : " ", (usage[1] >= 10) ? "" : " ", usage[1], (float)freq[0] / 1000000.f);
	sprintf(info_string[13], "CPU2 : %s%s%2d%%  %.2fGHz", (usage[2] >= 100) ? "" : " ", (usage[2] >= 10) ? "" : " ", usage[2], (float)freq[1] / 1000000.f);
	sprintf(info_string[14], "CPU3 : %s%s%2d%%  %.2fGHz", (usage[3] >= 100) ? "" : " ", (usage[3] >= 10) ? "" : " ", usage[3], (float)freq[2] / 1000000.f);
	sprintf(info_string[15], "CPU4 : %s%s%2d%%  %.2fGHz", (usage[4] >= 100) ? "" : " ", (usage[4] >= 10) ? "" : " ", usage[4], (float)freq[3] / 1000000.f);
}

void readSensors()
{
	FILE *fd;
	int lSize;
	char buf[256] = {0};
	char cpuid[9] = {0};
	int user = 0, system = 0, nice = 0, idle = 0, wait = 0, irq = 0, srq = 0;
	int cpu_index = 0;
	char line[256] = {0};
	char hdd_info[5][15] = {0};
	char mem_info[9][30] = {0};
	char cmd[]="df /home -h";
	int count = 0;

	if ((fd = fopen ("/sys/devices/10060000.tmu/temp", "r")) == NULL) {
		fprintf (stderr, "Unable to open sysfs for temperature reading: %s\n", strerror (errno));
		return;
	}

	fseek (fd , 0 , SEEK_END);
	lSize = ftell (fd);
	rewind (fd);

	size_t result = fread(temp_buffer, 1, lSize, fd);

	std::string input(temp_buffer);

	std::stringstream stream(input.substr(10, 2));
	stream >> cpu[0];
	stream.str("");
	stream.clear();
	stream << input.substr(26, 2);
	stream >> cpu[1];
	stream.str("");
	stream.clear();
	stream << input.substr(42, 2);
	stream >> cpu[2];
	stream.str("");
	stream.clear();
	stream << input.substr(58, 2);
	stream >> cpu[3];
	stream.str("");
	stream.clear();
	stream << input.substr(74, 2);
	stream >> gpu1;
	stream.str("");
	stream.clear();

	fclose(fd);
	
	fd = fopen("/proc/stat", "r");
	if (fd == NULL) {
		fprintf (stderr, "Unable to open proc file system: %s\n", strerror (errno));
		return;
	}

	while (fgets(buf, 256, fd)) {
		if (!strncmp(buf, "cpu", 3)) {
			sscanf(buf, "%s %d %d %d %d", cpuid, &user, &nice, &system, &idle, &wait, &irq, &srq);
			// CPU Usage integer array
			usage[cpu_index] = calUsage(cpu_index, user, nice, system, idle, wait, irq, srq);
			cpu_index++;
		}
		if (!strncmp(buf, "intr", 4))
			break;
	}
	fclose(fd);

	for (int i = 0; i < 8; i++) {
		char freq_path[60] = {0};
		sprintf(freq_path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
		fd = fopen(freq_path, "r");
		if (fd == NULL) {
			fprintf (stderr, "Unable to open proc file system: %s\n", strerror (errno));
			return;
		}
		fgets(line, 256 , fd);
		std::string _freq(line);
		std::stringstream ss_freq(_freq);
		// CPU Clock integer
		ss_freq >> freq[i];
		memset(line, 0, 256);
		fclose(fd);
	}

	fd = fopen("/proc/meminfo", "r");
	if (fd == NULL) {
		fprintf (stderr, "Unable to open proc file system: %s\n", strerror (errno));
		return;
	}

	std::stringstream mem_tmp;
	while (fgets(line, 256 , fd)) {
		mem_tmp << line;
		count++;
		if(count > 3)
			break;
	}
	memset(line, 0, 256);
	fclose(fd);

	mem_string = std::string(mem_tmp.str());

	char mem_str[256] = {0};
	sscanf(mem_string.c_str(), "%s %s %s %s %s %s %s %s %s", mem_info[0], mem_info[1], mem_info[2], mem_info[3], mem_info[4], mem_info[5], mem_info[6], mem_info[7], mem_info[8]);
	sprintf(mem_str, "Mem: %.1f/%.1f MiB (%.1f%)", atof(mem_info[4])/1000.f, atof(mem_info[1])/1000.f, (atof(mem_info[4])/1000.f) * 100.f / (atof(mem_info[1])/1000.f));
	// Memory Info string
	mem_string = std::string(mem_str);

	fd = fopen("/sys/devices/11800000.mali/clock", "r");
	if (fd == NULL) {
		fprintf (stderr, "Unable to open proc file system: %s\n", strerror (errno));
		return;
	}

	fgets(line, 256 , fd);

	std::string gpuclock(line);
	std::stringstream gpustream(gpuclock);

	// GPU clock frequency integer
	gpustream >> gpu_clock;
	memset(line, 0, 256);
	fclose(fd);

	fd = fopen("/sys/devices/11800000.mali/gpuinfo", "r");
	if (fd == NULL) {
        fprintf (stderr, "Unable to open sys file system: %s\n", strerror (errno));
		return;
	}

	fgets(line, 256 , fd);

	// GPU Info string
	gpu_string = std::string(line).substr(0, 13);
	memset(line, 0, 256);
	fclose(fd);

	fd = fopen("/sys/devices/platform/exynos-drm/drm/card0/card0-HDMI-A-1/status", "r");
	if (fd == NULL) {
		fprintf (stderr, "Unable to open sys file system: %s\n", strerror (errno));
		return;
	}

	fgets(line, 256 , fd);

	// HDMI connect string 
	connect_string = "HDMI: " + std::string(line);
	memset(line, 0, 256);
	fclose(fd);

	count = 0;
	FILE* apipe = popen(cmd, "r");
	while (!feof(apipe)) {
		line[count] = fgetc(apipe);
		count++;
	}

	hdd_string = std::string(line).substr(48, std::string(line).size() - 48);
	sscanf(hdd_string.c_str(), "%s %s %s %s %s", hdd_info[0], hdd_info[1], hdd_info[2], hdd_info[3], hdd_info[4]); 
	char hdd_str[256] = {0};
	sprintf(hdd_str, "HDD: %siB (%s used) %s", hdd_info[1], hdd_info[4], hdd_info[0]);

	// HDD info string
	hdd_string = std::string(hdd_str);

	pclose(apipe);

	fillInfoString();
}

int main(int argc, char *argv[])
{
	int fd;
	SDL_Renderer *renderer = NULL;
	SDL_Window *window = NULL;
	SDL_Event event;
	SDL_Surface *surface = NULL;
	SDL_Surface *_surface = NULL;
	unsigned char *pixels = NULL;
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;
	unsigned char buffer[PXLENGTH];
	const char *opt = NULL;
	int dim = 128; 

	uint32_t rmask = 0x0000f800, gmask = 0x000007e0, bmask = 0x0000001f, amask = 0x00000000;
	uint32_t rmask32 = 0x0000ff00, gmask32 = 0x00ff0000, bmask32 = 0xff000000, amask32 = 0x000000ff;

	if(cmdOptionExists(argv, argv+argc, "-h") || cmdOptionExists(argv, argv+argc, "--help") || argc < 2) {
		fprintf(stderr, "Usage: %s [-d] <num>\n\tDim amount for OSD overlay (0 - 255, default 128)\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(opt = getCmdOption(argv, argv+argc, "-d")) {
		std::istringstream(std::string(opt)) >> dim;
		if(dim < 0 || dim > 255) {
			fprintf(stderr, "Dim value to high\n");
			exit(EXIT_FAILURE);
		}
	}

	signal(SIGINT, intHandler);

	TTF_Init();
	font = TTF_OpenFontRW(SDL_RWFromMem(rawData, sizeof(rawData)), 1, 15);
	if (font == NULL) {
		fprintf(stderr, "error: open the font\n");
		exit(EXIT_FAILURE);
	}

	if ((fd = open("/dev/fb1", O_RDWR)) < 0) {
		perror("can't open device");
		abort();
	}

	uint8_t *fbp = (uint8_t*)mmap(0, sizeof(buffer), PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0);	
	
	for(int i = 0; i < sizeof(buffer); i++) {
		buffer[i] = fbp[i];
	}

	SDL_Surface *image = SDL_CreateRGBSurfaceFrom(buffer, 320, 240, 16, 640, rmask, gmask, bmask, amask);

	SDL_Surface *dark_surface = SDL_CreateRGBSurface(0, image->w, image->h, 32, rmask32, gmask32, bmask32, amask32);
	SDL_FillRect(dark_surface, NULL, SDL_MapRGBA(dark_surface->format, 0, 0, 0, dim));

	SDL_Surface *image_orig = SDL_CreateRGBSurface(image->flags, image->w, image->h, image->format->BytesPerPixel * 8, rmask, gmask, bmask, amask);

	// Copy the image
	SDL_BlitSurface(image, NULL, image_orig, NULL);	

	// Darken the BG Image
	SDL_BlitSurface(dark_surface, NULL, image, NULL);

	// Prepare sensor data for OSD
	readSensors();

	int pitch = image->pitch;
	int pxlength = pitch * image->h;
	unsigned char *pixels_orig = (unsigned char*)image_orig->pixels;

	while(running) {
		// Blit/Update the OSD
		readSensors();
		SDL_FreeSurface(surface);
		SDL_FreeSurface(_surface);
		surface = SDL_CreateRGBSurface(image->flags, image->w, image->h, image->format->BytesPerPixel * 8, rmask, gmask, bmask, amask);
		_surface = SDL_CreateRGBSurface(image->flags, image->w, image->h, image->format->BytesPerPixel * 8, rmask, gmask, bmask, amask);
		SDL_BlitSurface(image, NULL, surface, NULL);
		SDL_BlitSurface(blitOSD(surface), NULL, _surface, NULL);
		pixels = (unsigned char*)_surface->pixels;

		for (int i = 0; i < pxlength; i++) {
			fbp[i] = pixels[i];
		}
		usleep(100000);
	}
	close(fd);

	for (int i = 0; i < pxlength; i++) {
		fbp[i] = pixels_orig[i];
	}

    return 0;
}
