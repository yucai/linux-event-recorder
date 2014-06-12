#include "ev_recorder.h"

static int debug_level = 3;

static const char* dev_black_list[] = {
	"vmware",
};
#define N_DEV_BLIST sizeof(dev_black_list)/sizeof(dev_black_list[0])

static const char* dev_white_list[] = {
	"mouse",
	"keyboard",
	"synaptics_rmi4_touchkey",
	"synaptics_rmi4",
	"egalax inc. egalaxtouch exc7200-7368v1.010",
	"eGalax inc. usb touchcontroller",
	"synps/2 synaptics touchpad",
};
#define N_DEV_WLIST sizeof(dev_white_list)/sizeof(dev_white_list[0])

static int fd_open[N_DEV_WLIST]={0};

static const char* extract_file_name(const char* fname) {
	const char * t = fname;
	while (t = strchr(t, '/')) {
		t += 1;
		fname = t;
	}
	return fname;
}

char *trimwhitespace(char *str)
{
	char *end;

	// Trim leading space
	while(isspace(*str)) str++;

	if(*str == 0)  // All spaces?
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) end--;

	// Write new null terminator
	*(end+1) = 0;

	return str;
}

static int scan_dir(const char *dirname)
{
	char devname[40];
	int fd;
	char name[80];
	char *filename;
	DIR *dir;
	struct dirent *de;

	dir = opendir(dirname);
	if(dir == NULL)
		return -1;

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
		LOG(LOG_DBG, "open device %s", devname);
		fd = open(devname,O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "could not open %s, %s\n", devname, strerror(errno));
			continue;
		}

		if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1),&name) < 1) {
			name[0]='\0';
			continue;
		}

		char *s = name;
		while (*s != '\0') {
			*s=tolower(*s);
			s++;
		}
		LOG(LOG_DBG, "dev_info: '%s'", name);

		int black_match = 0;
		for (int i=0; i<N_DEV_BLIST; i++) {
			if (strstr(trimwhitespace(name), dev_black_list[i])) {
				black_match = 1;
				LOG(LOG_DBG, "close device %s", devname);
				close(fd);
				break;
			}
		}
		if (black_match)
			continue;

		int match = 0;
		for (int i=0; i<N_DEV_WLIST; i++) {
			if ((i<=1 && strstr(trimwhitespace(name), dev_white_list[i]) && !fd_open[i]) || \
			    (i>1 && !strcmp(trimwhitespace(name), dev_white_list[i]) && !fd_open[i])) {
				fd_open[i] = fd;
				match = 1;
				LOG(LOG_DBG, "match! i: %d, id: %d, type: %s", \
				    i, i, dev_white_list[i]);
				break;				
			}
		}
		if (!match) {
			close(fd);
			LOG(LOG_DBG, "close device %s", devname);
		}
	}

	closedir(dir);
	return 0;
}

static void usage(int argc, char *argv[])
{
	printf("Usage: %s [-d] debug_level event_file\n", argv[0]);
}

int main(int argc, char *argv[])
{
	int c;
	int i;
	int ret;
	int version;
	const char *event_fname = NULL;
	char *processed_event_fname = NULL;
	struct input_event event;
	memset(&event, 0, sizeof(event));

	do {
		c = getopt(argc, argv, "d:h");
		if (c == EOF)
			break;
		switch (c) {
		case 'd':
			debug_level = atoi(optarg);
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

	struct stat st;
	if (stat(event_fname, &st) != 0) {
		fprintf(stderr, "could not find the event file: %s!!\n", event_fname);
		return -1;
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int delay = 0;

	FILE *fh_events = fopen(event_fname,"r");
	if (read = getline(&line, &len,fh_events) != -1){
		const char *processed = "#processed";
		if ( 0 != strncmp(line,processed,strlen(processed))) {
			asprintf(&processed_event_fname, "/tmp/%s.processed", \
	         		extract_file_name(event_fname));
			char *event_proc_cmd = NULL;
			asprintf(&event_proc_cmd, "%s %s > %s", \
	         		"/usr/bin/process_event", event_fname, processed_event_fname);
			LOG(LOG_DBG, "event_proc_cmd: %s", event_proc_cmd);

			ret = system(event_proc_cmd);
			if (ret != 0) {
				LOG(LOG_ERR, "fail to execute %s", event_proc_cmd);
				return -1;
			}
			fclose(fh_events);
			fh_events = fopen(processed_event_fname, "r");
			if (NULL == fh_events) {
				LOG(LOG_ERR, "could not open %s\n", processed_event_fname);
				return -1;
			}
		}
	}


	const char *device_path = "/dev/input";
	ret = scan_dir(device_path);
	if (ret < 0) {
		fprintf(stderr, "scan dir failed for %s\n",device_path);
		return -1;
	}

	for (int i = 0; i < N_DEV_WLIST; ++i) {
		if (0 == fd_open[i])
			LOG(LOG_WARN, "could not open '%s' device!", dev_white_list[i]);
	}

	while ((read = getline(&line, &len, fh_events)) != -1) {
		if (line[0] == '#')
			continue;

		sscanf(line,"%d %x %x %x %d", &i, \
		       &event.type, &event.code, &event.value, \
		       &delay);
		if (fd_open[i]) {
			LOG(LOG_DBG, "write into %d, type: %04x, code: %04x, value: %08x, delay: %d", \
			    i, event.type, event.code, event.value, delay);
			gettimeofday(&event.time, NULL);

			ret = write(fd_open[i], &event, sizeof(event));
			if (ret < sizeof(event)) {
				fprintf(stderr, "write event failed, %s\n", strerror(errno));
				return -1;
			}
		}

		usleep(delay);
	}

	for (i=0; i<N_DEV_WLIST; i++) {
		close(fd_open[i]);
	}
	fclose(fh_events);

	return 0;
}
