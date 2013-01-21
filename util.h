void error(char *str)
{
	perror(str);
	exit(EXIT_FAILURE);
}

void fatal(char *str)
{
	fprintf(stderr, "%s\n", str);
	exit(EXIT_FAILURE);
}

int eopen(const char *path, int flag)
{
	int fd;

	if ((fd = open(path, flag)) < 0) {
		fprintf(stderr, "cannot open \"%s\"\n", path);
		error("open");
	}
	return fd;
}

void eclose(int fd)
{
	if (close(fd) < 0)
		error("close");
}

FILE *efopen(const char *path, char *mode)
{
	FILE *fp;

	if ((fp = fopen(path, mode)) == NULL) {
		fprintf(stderr, "cannot open \"%s\"\n", path);
		error("fopen");
	}
	return fp;
}

void efclose(FILE *fp)
{
	if (fclose(fp) < 0)
		error("fclose");
}

void *emalloc(size_t size)
{
	void *p;

	if ((p = calloc(1, size)) == NULL)
		error("calloc");
	return p;
}

void eselect(int max_fd, fd_set *rd, fd_set *wr, fd_set *err, struct timeval *tv)
{
	if (select(max_fd, rd, wr, err, tv) < 0) {
		if (errno == EINTR)
			eselect(max_fd, rd, wr, err, tv);
		else
			error("select");
	}
}

void ewrite(int fd, const void *buf, int size)
{
	int ret;

	if ((ret = write(fd, buf, size)) < 0)
		error("write");
	else if (ret < size)
		ewrite(fd, (char *) buf + ret, size - ret);
}

void eexecvp(const char *file, char *const arg[])
{
	if (execvp(file, arg) < 0)
		error("execl");
}

pid_t eforkpty(int *master, char *name, struct termios *termp, struct winsize *winp)
{
	/* name must be null */
	int slave;
	char *sname;
	pid_t pid;

	if (((*master = posix_openpt(O_RDWR | O_NOCTTY)) < 0)
		|| (grantpt(*master) < 0)
		|| (unlockpt(*master) < 0)
		|| ((sname = ptsname(*master)) == NULL))
		error("forkpty");

	slave = eopen(sname, O_RDWR | O_NOCTTY);

	if (termp)
		tcsetattr(slave, TCSAFLUSH, termp);

	if (winp)
		ioctl(*master, TIOCSWINSZ, winp);

	pid = fork();
	if (pid < 0)
		error("fork");
	else if (pid == 0) { /* child */
		dup2(slave, STDIN_FILENO);
		dup2(slave, STDOUT_FILENO);
		dup2(slave, STDERR_FILENO);
		setsid();
		ioctl(slave, TIOCSCTTY, NULL); /* ioctl may fail in Mac OS X */
		close(slave);
		close(*master);
	}
	else /* parent */
		close(slave);

	return pid;
}
