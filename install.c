// SPDX-License-Identifier: AGPL-3.0-only
/*
 * install.c - Flat's Packages installer.
 *
 * Author: Breno Rodrigues Alves
 *
 * Usage: install [--user|--system|--prefix=PATH]
 *
 * Prefix resolution order:
 *   --prefix=PATH -> --user/--system -> $PREFIX
 *   -> /usr/local (if writable) -> $HOME/.local
 *
 * Installs the binary plus, if present in the source tree,
 * the man page and the shell completions (bash, zsh, fish).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PRG       "flatspkg"
#define PRG_NAME  "Flat's Packages"
#define fBlue     "\033[38;2;28;57;187m"    /* #1C39BB */
#define fBlueLt   "\033[38;2;92;123;255m"   /* #5C7BFF */
#define fRed      "\033[31;1m"
#define fGreen    "\033[32;1m"
#define fYellow   "\033[33;1m"
#define fEnd      "\033[m"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct optional_file {
	const char *src;
	const char *dst_rel;
	mode_t mode;
	const char *label;
};

static const struct optional_file OPTIONAL_FILES[] = {
	{ "man/flatspkg.1",
	  "share/man/man1/flatspkg.1", 0644, "manpage" },
	{ "completions/flatspkg.bash",
	  "share/bash-completion/completions/flatspkg", 0644, "bash-completion" },
	{ "completions/flatspkg.zsh",
	  "share/zsh/site-functions/_flatspkg", 0644, "zsh-completion" },
	{ "completions/flatspkg.fish",
	  "share/fish/vendor_completions.d/flatspkg.fish", 0644, "fish-completion" },
};

static int path_installable(const char *path)
{
	struct stat st;
	char parent[PATH_MAX];
	size_t plen;
	const char *slash;

	if (stat(path, &st) == 0)
		return S_ISDIR(st.st_mode) && access(path, W_OK) == 0;

	slash = strrchr(path, '/');
	if (!slash || slash == path)
		return access("/", W_OK) == 0;

	plen = (size_t)(slash - path);
	if (plen >= sizeof(parent))
		return 0;
	memcpy(parent, path, plen);
	parent[plen] = '\0';
	return path_installable(parent);
}

static int make_dir_p(const char *path)
{
	char tmp[PATH_MAX];
	size_t len;
	char *p;

	len = strlen(path);
	if (!len || len >= sizeof(tmp))
		return -1;
	memcpy(tmp, path, len + 1);
	if (tmp[len - 1] == '/') {
		if (len == 1)
			return 0;
		tmp[len - 1] = '\0';
	}
	for (p = tmp + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(tmp, 0755) < 0 && errno != EEXIST)
			return -1;
		*p = '/';
	}
	if (mkdir(tmp, 0755) < 0 && errno != EEXIST)
		return -1;
	return 0;
}

static int copy_file(const char *src, const char *dst, mode_t mode)
{
	int in, out;
	char buf[8192];
	ssize_t n;

	in = open(src, O_RDONLY);
	if (in < 0) {
		perror(src);
		return -1;
	}
	out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (out < 0) {
		perror(dst);
		close(in);
		return -1;
	}
	while ((n = read(in, buf, sizeof(buf))) > 0) {
		ssize_t off = 0;

		while (off < n) {
			ssize_t w = write(out, buf + off, (size_t)(n - off));

			if (w < 0) {
				perror(dst);
				close(in);
				close(out);
				return -1;
			}
			off += w;
		}
	}
	if (n < 0)
		perror(src);
	close(in);
	if (close(out) < 0) {
		perror(dst);
		return -1;
	}
	return chmod(dst, mode) < 0 ? -1 : 0;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS]\n"
		"\n"
		"Installs %s system-wide or per-user.\n"
		"\n"
		"Options:\n"
		"  --user          Install to $HOME/.local\n"
		"  --system        Install to /usr/local (requires root)\n"
		"  --prefix=PATH   Install under PATH/bin\n"
		"  -h, --help      Show this help\n"
		"\n"
		"Without options, the prefix is auto-detected:\n"
		"  /usr/local when writable, otherwise $HOME/.local.\n"
		"\n"
		"If man/flatspkg.1 and completions/* are present in the source\n"
		"tree, they are installed under PREFIX/share as well.\n",
		prog, PRG_NAME);
}

static const char *resolve_prefix(const char *cli_prefix, int force_user,
				  int force_system)
{
	static char user_prefix[PATH_MAX];
	const char *env, *home;

	if (cli_prefix && *cli_prefix)
		return cli_prefix;
	if (force_user) {
		home = getenv("HOME");
		if (!home || !*home)
			return NULL;
		snprintf(user_prefix, sizeof(user_prefix), "%s/.local", home);
		return user_prefix;
	}
	if (force_system)
		return "/usr/local";
	env = getenv("PREFIX");
	if (env && *env)
		return env;
	if (path_installable("/usr/local/bin"))
		return "/usr/local";
	home = getenv("HOME");
	if (home && *home) {
		snprintf(user_prefix, sizeof(user_prefix), "%s/.local", home);
		return user_prefix;
	}
	return NULL;
}

static int install_optional(const char *prefix)
{
	size_t i;
	char target_dir[PATH_MAX + 512];
	char target[PATH_MAX + 512];
	char *slash;
	struct stat st;

	for (i = 0; i < ARRAY_SIZE(OPTIONAL_FILES); i++) {
		const struct optional_file *f = &OPTIONAL_FILES[i];

		if (stat(f->src, &st) < 0)
			continue;

		snprintf(target, sizeof(target), "%s/%s", prefix, f->dst_rel);
		snprintf(target_dir, sizeof(target_dir), "%s", target);
		slash = strrchr(target_dir, '/');
		if (slash)
			*slash = '\0';

		if (make_dir_p(target_dir) < 0) {
			perror(target_dir);
			return -1;
		}
		if (copy_file(f->src, target, f->mode) < 0)
			return -1;

		printf("%s+%s %s\n", fBlueLt, fEnd, f->label);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	const char *cli_prefix = NULL;
	const char *prefix;
	char bindir[PATH_MAX + 64], dst[PATH_MAX + 512];
	int force_user = 0, force_system = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--prefix=", 9) == 0)
			cli_prefix = argv[i] + 9;
		else if (!strcmp(argv[i], "--prefix") && i + 1 < argc)
			cli_prefix = argv[++i];
		else if (!strcmp(argv[i], "--user"))
			force_user = 1;
		else if (!strcmp(argv[i], "--system"))
			force_system = 1;
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage(argv[0]);
			return 0;
		} else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			usage(argv[0]);
			return 1;
		}
	}

	prefix = resolve_prefix(cli_prefix, force_user, force_system);
	if (!prefix) {
		fprintf(stderr,
			"Cannot determine install prefix.\n"
			"Set $PREFIX, use --prefix=PATH, or ensure $HOME is set.\n");
		return 1;
	}

	snprintf(bindir, sizeof(bindir), "%s/bin", prefix);

	printf("%s>%s Installing %s under %s\n",
	       fBlueLt, fEnd, PRG_NAME, prefix);
	printf("  %s/%s\n", bindir, PRG);

	if (make_dir_p(bindir) < 0) {
		perror(bindir);
		fprintf(stderr,
			"%sHint: use --user or --prefix to pick another location.%s\n",
			fYellow, fEnd);
		return 1;
	}
	snprintf(dst, sizeof(dst), "%s/%s", bindir, PRG);
	if (copy_file(PRG, dst, 0755) < 0)
		return 1;

	if (install_optional(prefix) < 0)
		return 1;

	printf("%s+%s %s installed successfully.\n", fGreen, fEnd, PRG_NAME);

	if (strstr(bindir, "/.local/bin"))
		printf("%sHint:%s make sure %s is in your PATH.\n",
		       fYellow, fEnd, bindir);

	return 0;
}