#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	struct sockaddr_un addr;
	const char *env = getenv("SERAPHIM_SOCKET");
	const char *rt = getenv("XDG_RUNTIME_DIR");
	std::string path, line, reply;
	char buf[4096];
	ssize_t n;
	int fd, i;

	if (argc < 2) {
		fprintf(stderr, "usage: seraphimctl <command> [arguments]   (seraphimctl help lists them)\n");
		return 2;
	}

	if (env && *env)
		path = env;
	else if (rt && *rt)
		path = std::string(rt) + "/seraphim.sock";
	else
		path = "/tmp/seraphim-" + std::to_string((int)getuid()) + ".sock";

	for (i = 1; i < argc; i++) {
		if (i > 1)
			line += ' ';
		line += argv[i];
	}
	line += '\n';

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (path.size() >= sizeof(addr.sun_path)) {
		fprintf(stderr, "seraphimctl: socket path too long\n");
		return 2;
	}
	strcpy(addr.sun_path, path.c_str());

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "seraphimctl: cannot reach seraphim at %s (is it running?)\n", path.c_str());
		return 2;
	}
	if (write(fd, line.data(), line.size()) != (ssize_t)line.size()) {
		fprintf(stderr, "seraphimctl: write failed\n");
		return 2;
	}
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		reply.append(buf, (size_t)n);
	close(fd);

	fputs(reply.c_str(), stdout);
	return reply.compare(0, 6, "error:") == 0 ? 1 : 0;
}
