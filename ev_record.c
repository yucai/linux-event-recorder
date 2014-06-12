#include "ev_recorder.h"

static int debug_level = 3;

static struct pollfd *ufds;
/* pollfd { */
/*  int fd; */
/*  short int events;//types of events poller cares about */
/*  short int revents;//types of events that actually occurred */
/* } */
static char **device_names;
static int nfds;
static FILE *f_event = NULL;

static const char *device_dir = "/dev/input";
static const char* dev_white_list[] = {
	"/dev/input/event",
};
#define N_DEV_WLIST sizeof(dev_white_list)/sizeof(dev_white_list[0])

static int open_device(const char *device)
{
	int fd;
	struct pollfd *new_ufds;
	char **new_device_names;
	char name[80];
	char location[80];
	char idstr[80];
	struct input_id id;

	fd = open(device, O_RDWR);
	if(fd < 0) {
		LOG(LOG_ERR, "could not open %s, %s\n", device, strerror(errno));
		return -1;
	}
    
	name[sizeof(name) - 1] = '\0';
	if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
		name[0] = '\0';
	}

	LOG(LOG_INFO, "%s: %s", device, name);
	
	if (f_event)
		fprintf(f_event, "%s: %s\n", device, name);
	
	new_ufds = realloc(ufds, sizeof(ufds[0]) * (nfds + 1));
	if(new_ufds == NULL) {
		LOG(LOG_ERR, "out of memory\n");
		return -1;
	}
	ufds = new_ufds;
	
	new_device_names = realloc(device_names, sizeof(device_names[0]) * (nfds + 1));
	if(new_device_names == NULL) {
		LOG(LOG_ERR, "out of memory\n");
		return -1;
	}
	device_names = new_device_names;

	ufds[nfds].fd = fd;
	ufds[nfds].events = POLLIN;
	device_names[nfds] = strdup(device);
	nfds++;

	return 0;
}

int close_device(const char *device)
{
	for(int i = 1; i < nfds; i++) {
		if (strcmp(device_names[i], device) == 0) {
			LOG(LOG_INFO, "remove device %d: %s\n", i, device);
			int count = nfds - i - 1;
			free(device_names[i]);
			memmove(device_names + i, device_names + i + 1, sizeof(device_names[0]) * count);
			memmove(ufds + i, ufds + i + 1, sizeof(ufds[0]) * count);
			nfds--;
			return 0;
		}
	}
		
	LOG(LOG_ERR, "remote device: %s not found\n", device);
	return -1;
}

static int read_notify(const char *dirname, int nfd)
{
	int ret;
	char devname[PATH_MAX];
	char *filename;
	char event_buf[512];
	int event_size;
	int event_pos = 0;
	struct inotify_event *event;

	ret = read(nfd, event_buf, sizeof(event_buf));
	if(ret < (int)sizeof(*event)) {
		if(errno == EINTR)
			return 0;
		LOG(LOG_ERR, "could not get event, %s\n", strerror(errno));
		return 1;
	}
	LOG(LOG_DBG, "got %d bytes of event information\n", ret);

	strcpy(devname, dirname);
	filename = devname + strlen(devname);
	*filename++ = '/';
	while(ret >= (int)sizeof(*event)) {
		event = (struct inotify_event *)(event_buf + event_pos);
		LOG(LOG_DBG, "%d: %08x \"%s\"\n", event->wd, event->mask, event->len ? event->name : "");
		if(event->len) {
			strcpy(filename, event->name);
			if(event->mask & IN_CREATE) {
				open_device(devname);
			}
			else {
				close_device(devname);
			}
		}
		event_size = sizeof(*event) + event->len;
		ret -= event_size;
		event_pos += event_size;
	}

	return 0;
}

static int scan_dir(const char *dirname)
{
	char devname[PATH_MAX];
	char *filename;
	DIR *dir;
	struct dirent *de;
	
	dir = opendir(dirname);
	if (dir == NULL) {
		LOG(LOG_ERR, "could not open dir %s", dirname);
		return -1;
	}
	
	strcpy(devname, dirname);
	filename = devname + strlen(devname);
	*filename++ = '/';
	while((de = readdir(dir))) {
		if(de->d_name[0] == '.' &&
		   (de->d_name[1] == '\0' ||
		    (de->d_name[1] == '.' && de->d_name[2] == '\0')))
			continue;
		
		if (de->d_type == DT_DIR)
			continue;

		strcpy(filename, de->d_name);

		int match = 0;
		for (int i=0; i<N_DEV_WLIST; i++) {
			if (strstr(devname, dev_white_list[i])) {
				match = 1;
				break;
			}
		}
		if (!match) {
				LOG(LOG_INFO, "skip %s", devname);
				continue;
		}
		
		open_device(devname);
	}
	
	closedir(dir);
	return 0;
}

static void usage(int argc, char *argv[])
{
	printf("Usage: %s [-d] debug_level [-n] device_node event_file\n", argv[0]);
}

int main(int argc, char *argv[])
{
	int c;
	int ret;
	int pollret;
	struct input_event event;
	const char *device_node = NULL;
	char *event_details = NULL;
	const char *event_fname = NULL;
	
	do {
		c = getopt(argc, argv, "d:hn:");
		if (c == EOF)
			break;
		switch (c) {
		case 'd':
			debug_level = atoi(optarg);
			break;
		case 'n':
			device_node = strdup(optarg);
			break;
		case '?':
			LOG(LOG_ERR, "%s: invalid option -%c\n",
			        argv[0], optopt);
		case 'h':
			usage(argc, argv);
			exit(1);
		}
	} while (1);

	event_fname = argv[optind];
	optind++;
 
	if (optind != argc) {
		printf("incorrect parameters, optind: %d, argc: %d\n", optind, argc);
		usage(argc, argv);
		exit(1);
	}

	if (event_fname) {
		f_event = fopen(event_fname,"w+");
		if (NULL == f_event){
			LOG(LOG_ERR, "cannot open %s file", event_fname);
			return 0;
		}
	}
	
	nfds = 1;
	ufds = calloc(1, sizeof(ufds[0]));
	ufds[0].fd = inotify_init();
	ufds[0].events = POLLIN;
	
	if (device_node) {
		ret = open_device(device_node);
		if(ret < 0) {
			LOG(LOG_ERR, "could not open %s", device_node);
			return 1;
		}
	}
	else {
		ret = inotify_add_watch(ufds[0].fd, device_dir, IN_DELETE | IN_CREATE);
		if (ret < 0) {
			LOG(LOG_ERR, "could not add watch for %s, %s\n", \
			    device_dir, strerror(errno));
			return 1;
		}
		
		ret = scan_dir(device_dir);
		if (ret < 0) {
			LOG(LOG_ERR, "scan dir failed for %s\n", device_dir);
			return 1;
		}
	}

	while (1) {
		pollret = poll(ufds, nfds, -1);
		// LOG(LOG_INFO, "poll %d, returned %d\n", nfds, pollret);
		if (ufds[0].revents & POLLIN) {
			read_notify(device_dir, ufds[0].fd);
		}
		for (int i = 1; i < nfds; i++) {
			if (ufds[i].revents) {
				if (ufds[i].revents & POLLIN) {
					ret = read(ufds[i].fd, &event, sizeof(event));
					if (ret < (int)sizeof(event)) {
						LOG(LOG_ERR, "could not get event\n");
						return 1;
					}
					if (f_event) {
						asprintf(&event_details, "[%8ld.%06ld], %s, %04x %04x %08x\n", \
						         event.time.tv_sec, event.time.tv_usec, \
						         device_names[i],     \
						         event.type, event.code, event.value);
						LOG(LOG_DBG, "[%8ld.%06ld], %s, %04x %04x %08x", \
						    event.time.tv_sec, event.time.tv_usec, \
						    device_names[i], \
						    event.type, event.code, event.value);
						fprintf(f_event, "%s", event_details);
						fflush(f_event);
					}
				}
			}
		}
	}

	return 0;
}
