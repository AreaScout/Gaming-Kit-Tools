#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <poll.h>

#define USERSWITCH "/sys/class/gpio/gpio24/value"
#define PIN 24

std::string falling_arg;
std::string rising_arg;
bool exec_falling = false;
bool exec_rising = false;
bool running = true;

void setup_gpio_pin()
{
	FILE *fd = NULL;

	if((fd = fopen ("/sys/class/gpio/export", "w")) == NULL) {
		fprintf (stderr, "Unable to open GPIO export interface: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	fprintf (fd, "%d\n", PIN);
	fclose (fd);

	if((fd = fopen ("/sys/class/gpio/gpio24/direction", "w")) == NULL) {
		fprintf (stderr, "Unable to open GPIO direction interface for pin %d: %s\n", PIN, strerror(errno));
		exit(EXIT_FAILURE);
	}

	fprintf (fd, "in\n"); 
	fclose (fd);

	if((fd = fopen ("/sys/class/gpio/gpio24/edge", "w")) == NULL) {
		fprintf (stderr, "Unable to open GPIO edge interface for pin %d: %s\n", PIN, strerror(errno));
		exit(EXIT_FAILURE);
	}

	// falling, rising, both
	fprintf (fd, "both\n");

	fclose (fd);
}

void intHandler(int param) 
{
	running = false;
}

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
	char ** it = std::find(begin, end, option);
	if(it != end && ++it != end) {
		return *it;
	}
	return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

void cleanUp(pid_t pid)
{
	int stat_loc;
	int child = 1;
	while (child > 0) {
		child = (int)waitpid(pid, &stat_loc, 0);
		if(child < 0) {
			if(errno != ECHILD) {
				printf("wait for child failed: %s\n", strerror(errno));
			}
		}
	}
}

void forkForCmd(std::string *cmd)
{
	pid_t pid = fork();
	if( pid < 0 )
		printf("fork failed, cannot execute command %s\n", cmd->c_str());
	else if (pid > 0)
	{
		// remove zombie child processes if any
		cleanUp(pid);
	} else {
		if (execl( "/bin/sh", "/bin/sh", "-c", cmd->c_str(), NULL ) < 0)
			printf("cannot execute /bin/sh");
		abort();
	}
}

int main(int argc, char *argv[])
{
	int ret, fd_dt, count;
	uint8_t c = 0;
	pid_t pid;
	struct pollfd polls;

	if(cmdOptionExists(argv, argv+argc, "-d")) {
		// Run as deamon
		pid = fork();
		if(pid < 0)
			exit(EXIT_FAILURE);

		// Let the parent terminate
		if(pid > 0)
			exit(EXIT_SUCCESS);

		// The child process becomes session leader
		if(setsid() < 0)
			exit(EXIT_FAILURE);

		signal(SIGCHLD, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
	}

	signal(SIGINT, intHandler);

	// Configure the gpio pin device tree entries on the first run 
	if(access(USERSWITCH, F_OK) != -1) {
		if(getuid() == 0) {
			printf("Device tree entry is already set - exiting\n");
			exit(EXIT_SUCCESS);
		}
	} else {
		if(getuid() != 0) {
			printf("User has to be root to create gpio tree\n");
			exit(EXIT_FAILURE);
		}
		setup_gpio_pin();
		printf("All device tree entries created successfully - exiting\n");
		exit(EXIT_SUCCESS);
	}

	char * fargv = getCmdOption(argv, argv+argc, "-f");
	char * rargv = getCmdOption(argv, argv+argc, "-r");

	if(fargv) {
		exec_falling = true;
		falling_arg = std::string(fargv);
	}

	if (rargv) {
		exec_rising = true;
		rising_arg = std::string(rargv);
	}

	if (!fargv && !rargv) {
		printf("Usage: %s [-f] <cmd> [-r] <cmd> [-d]\n\t[-f] <cmd> command to execute when button is pressed\n\t[-r] <cmd> command to execute when button is released\n\t[-d] run as daemon\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Open device tree gpio entry
	if((fd_dt = open(USERSWITCH, O_RDONLY)) < 0) {
		printf("Unable to open gpio device tree entry\n");
		exit(EXIT_FAILURE);
	}

	// Clear any initial pending interrupt
	ioctl(fd_dt, FIONREAD, &count);
	for(int i = 0 ; i < count ; ++i)
		read(fd_dt, &c, 1);

	// Setup poll structure
	polls.fd     = fd_dt;
	polls.events = POLLPRI;

	while(running) {
		int x =  poll(&polls, 1, -1);

		(void)read(fd_dt, &c, 1);
		lseek(fd_dt, 0, SEEK_SET);

		if(c == 0x30 || c == 0x0a && exec_falling) {
			forkForCmd(&falling_arg); // Button pressed
		} else if(c == 0x31 && exec_rising) {
			forkForCmd(&rising_arg);  // Button released
		}
	}

	close(fd_dt);

	return 0;
}
