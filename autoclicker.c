#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DEF_CPS 10
#define MIN_CPS 1
#define MAX_CPS 100

static char dir[256], pid_path[300], speed_path[300];
static volatile sig_atomic_t reload, quit;

static void init_paths(void)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (rt)
		snprintf(dir, sizeof dir, "%s/autoclicker", rt);
	else
		snprintf(dir, sizeof dir, "/tmp/autoclicker-%d", (int)getuid());
	snprintf(pid_path, sizeof pid_path, "%s/pid", dir);
	snprintf(speed_path, sizeof speed_path, "%s/speed", dir);
	mkdir(dir, 0700);
}

static long read_num(const char *path)
{
	FILE *f = fopen(path, "r");
	long v = -1;
	if (!f)
		return -1;
	if (fscanf(f, "%ld", &v) != 1)
		v = -1;
	fclose(f);
	return v;
}

static void write_num(const char *path, long v)
{
	char tmp[320];
	FILE *f;
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	f = fopen(tmp, "w");
	if (!f) {
		perror("write");
		exit(1);
	}
	fprintf(f, "%ld\n", v);
	fclose(f);
	rename(tmp, path);
}

static long clamp(long v)
{
	return v < MIN_CPS ? MIN_CPS : v > MAX_CPS ? MAX_CPS : v;
}

static long get_speed(void)
{
	long v = read_num(speed_path);
	return v < 0 ? DEF_CPS : clamp(v);
}

static pid_t running_pid(void)
{
	long p = read_num(pid_path);
	if (p > 0 && (kill((pid_t)p, 0) == 0 || errno == EPERM))
		return (pid_t)p;
	return 0;
}

static void emit(int fd, int type, int code, int val)
{
	struct input_event ev = {0};
	ev.type = type;
	ev.code = code;
	ev.value = val;
	if (write(fd, &ev, sizeof ev) < 0)
		quit = 1;
}

static void on_usr1(int s) { (void)s; reload = 1; }
static void on_term(int s) { (void)s; quit = 1; }

static void add_ns(struct timespec *t, long ns)
{
	t->tv_nsec += ns;
	while (t->tv_nsec >= 1000000000L) {
		t->tv_nsec -= 1000000000L;
		t->tv_sec++;
	}
}

static int daemon_loop(void)
{
	struct uinput_setup us = {0};
	struct timespec next, hold = {0, 1000000L};
	long cps = get_speed();
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

	if (fd < 0) {
		fprintf(stderr, "autoclicker: cannot open /dev/uinput: %s\n"
			"Needs a udev rule (GROUP=input, MODE=0660) and membership in the input group.\n",
			strerror(errno));
		return 1;
	}
	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
	us.id.bustype = BUS_VIRTUAL;
	us.id.vendor = 0x1234;
	us.id.product = 0x5678;
	strcpy(us.name, "autoclicker");
	ioctl(fd, UI_DEV_SETUP, &us);
	ioctl(fd, UI_DEV_CREATE);

	signal(SIGUSR1, on_usr1);
	signal(SIGTERM, on_term);
	signal(SIGINT, on_term);

	/* Give the compositor time to pick up the new device. */
	nanosleep(&(struct timespec){0, 300000000L}, NULL);
	clock_gettime(CLOCK_MONOTONIC, &next);

	while (!quit) {
		if (reload) {
			reload = 0;
			cps = get_speed();
		}
		emit(fd, EV_KEY, BTN_LEFT, 1);
		emit(fd, EV_SYN, SYN_REPORT, 0);
		nanosleep(&hold, NULL);
		emit(fd, EV_KEY, BTN_LEFT, 0);
		emit(fd, EV_SYN, SYN_REPORT, 0);

		add_ns(&next, 1000000000L / cps);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
	}

	ioctl(fd, UI_DEV_DESTROY);
	close(fd);
	unlink(pid_path);
	return 0;
}

static int cmd_start(void)
{
	pid_t p = running_pid();
	if (p) {
		printf("already running (pid %d)\n", (int)p);
		return 1;
	}
	if (read_num(speed_path) < 0)
		write_num(speed_path, DEF_CPS);

	p = fork();
	if (p < 0) {
		perror("fork");
		return 1;
	}
	if (p > 0) {
		/* Wait briefly so a uinput failure is reported to the terminal. */
		nanosleep(&(struct timespec){0, 100000000L}, NULL);
		if (!running_pid()) {
			return 1;
		}
		printf("started at %ld CPS\n", get_speed());
		return 0;
	}
	setsid();
	write_num(pid_path, getpid());
	int rc = daemon_loop();
	if (rc)
		unlink(pid_path);
	_exit(rc);
}

static int cmd_end(void)
{
	pid_t p = running_pid();
	if (!p) {
		printf("not running\n");
		unlink(pid_path);
		return 1;
	}
	kill(p, SIGTERM);
	printf("stopped\n");
	return 0;
}

static int cmd_speed(const char *arg, int add)
{
	char *end;
	long n = strtol(arg, &end, 10), v;
	pid_t p;

	if (*arg == '\0' || *end != '\0') {
		fprintf(stderr, "autoclicker: invalid number '%s'\n", arg);
		return 1;
	}
	v = clamp(add ? get_speed() + n : n);
	write_num(speed_path, v);
	p = running_pid();
	if (p)
		kill(p, SIGUSR1);
	printf("speed: %ld CPS%s\n", v, p ? "" : " (not running)");
	return 0;
}

static int cmd_status(void)
{
	pid_t p = running_pid();
	if (p)
		printf("running (pid %d), %ld CPS\n", (int)p, get_speed());
	else
		printf("stopped, %ld CPS\n", get_speed());
	return 0;
}

static int usage(void)
{
	fprintf(stderr, "usage: autoclicker start | end | status | speed-add <n> | speed-set <n>\n");
	return 1;
}

int main(int argc, char **argv)
{
	const char *c = argc > 1 ? argv[1] : "";
	init_paths();

	if (!strcmp(c, "start"))
		return cmd_start();
	if (!strcmp(c, "end"))
		return cmd_end();
	if (!strcmp(c, "status"))
		return cmd_status();
	if (!strcmp(c, "speed-add") && argc == 3)
		return cmd_speed(argv[2], 1);
	if (!strcmp(c, "speed-set") && argc == 3)
		return cmd_speed(argv[2], 0);
	return usage();
}
