#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/input.h>
#include <time.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

#define EVIOCGNAME(len) _IOC(_IOC_READ, 'E', 0x06, len) /* get device name */

#define LOG_ERR  (1)
#define LOG_WARN (2)
#define LOG_INFO (3)
#define LOG_DBG  (4)
#define LOG(level, ...) do {  \
		if (level <= debug_level) { \
			if (level >= LOG_DBG) \
				printf("[%s:%d] ", __FILE__, __LINE__); \
			printf(__VA_ARGS__); \
			printf("\n"); \
		} \
	} while (0)
