// SPDX-License-Identifier: AGPL-3.0-only
/*
 * flatspkg - Flatpak manager for the terminal.
 *
 * Author: Breno Rodrigues Alves
 *
 * Self-contained translations (no gettext, no .mo files).
 * Deps: flatpak, fzf
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <termios.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PRG               "flatspkg"
#define PRG_NAME          "Flat's Packages"
#define VERSION           "1.0"
#define CACHE_FILE_SUFFIX ".cache/" PRG "-flathub.cache"
#define CACHE_TIME        (24 * 60 * 60)

/* Persian Blue palette (24-bit truecolor) */
#define fBlue             "\033[38;2;28;57;187m"    /* #1C39BB */
#define fBlueLight        "\033[38;2;92;123;255m"   /* #5C7BFF */
#define fRed              "\033[31;1m"
#define fGreen            "\033[32;1m"
#define fEnd              "\033[m"

#define FZF_COLORS \
	"--color=fg:#D0D0D0,fg+:#FFFFFF," \
	"hl:#5C7BFF,hl+:#FFFFFF," \
	"prompt:#5C7BFF,pointer:#5C7BFF,marker:#5C7BFF," \
	"spinner:#5C7BFF,header:#5C7BFF,border:#1C39BB," \
	"info:#5C7BFF,query:#FFFFFF"

#define ARRAY_SIZE(a)     (sizeof(a) / sizeof((a)[0]))

/* ---- translations (compiled in) ---- */

struct translation {
	const char *key;
	const char *pt_BR;
	const char *es;
	const char *fr;
	const char *de;
};

static const struct translation translations[] = {
	{ "Generating cache file, please wait...",
	  "Gerando o arquivo de cache, aguarde...",
	  "Generando el archivo de caché, espere...",
	  "Génération du fichier cache, veuillez patienter...",
	  "Cache-Datei wird erstellt, bitte warten..." },

	{ "Failed to generate cache.",
	  "Falha ao gerar o cache.",
	  "Error al generar el caché.",
	  "Échec de la génération du cache.",
	  "Cache konnte nicht erstellt werden." },

	{ "Press any key to continue...",
	  "Pressione qualquer tecla para continuar...",
	  "Presione cualquier tecla para continuar...",
	  "Appuyez sur une touche pour continuer...",
	  "Beliebige Taste drücken zum Fortfahren..." },

	{ "Select applications to INSTALL <TAB selects multiple>.",
	  "Selecione os aplicativos para INSTALAR <TAB marca múltiplos>.",
	  "Seleccione las aplicaciones para INSTALAR <TAB marca varias>.",
	  "Sélectionnez les applications à INSTALLER "
	  "<TAB en sélectionne plusieurs>.",
	  "Anwendungen zum INSTALLIEREN auswählen "
	  "<TAB wählt mehrere>." },

	{ "is already installed",
	  "já está instalado",
	  "ya está instalado",
	  "est déjà installé",
	  "ist bereits installiert" },

	{ "Installing",
	  "Instalando",
	  "Instalando",
	  "Installation de",
	  "Installation von" },

	{ "Select applications to REMOVE (TAB marks).",
	  "Selecione os aplicativos para REMOVER (TAB marca).",
	  "Seleccione las aplicaciones para ELIMINAR (TAB marca).",
	  "Sélectionnez les applications à SUPPRIMER (TAB marque).",
	  "Anwendungen zum ENTFERNEN auswählen (TAB markiert)." },

	{ "Remove",
	  "Remover",
	  "Eliminar",
	  "Supprimer",
	  "Entfernen" },

	{ "Removing",
	  "Removendo",
	  "Eliminando",
	  "Suppression de",
	  "Entferne" },

	{ "is not installed",
	  "não está instalado",
	  "no está instalado",
	  "n'est pas installé",
	  "ist nicht installiert" },

	{ "Updating Flatpaks...",
	  "Atualizando Flatpaks...",
	  "Actualizando Flatpaks...",
	  "Mise à jour des Flatpaks...",
	  "Flatpaks werden aktualisiert..." },

	{ "What would you like to do?",
	  "O que você deseja fazer?",
	  "¿Qué desea hacer?",
	  "Que souhaitez-vous faire ?",
	  "Was möchten Sie tun?" },

	{ "Install",
	  "Instalar",
	  "Instalar",
	  "Installer",
	  "Installieren" },

	{ "Update",
	  "Atualizar",
	  "Actualizar",
	  "Mettre à jour",
	  "Aktualisieren" },

	{ "Exit",
	  "Sair",
	  "Salir",
	  "Quitter",
	  "Beenden" },

	{ "Usage:",
	  "Uso:",
	  "Uso:",
	  "Utilisation :",
	  "Verwendung:" },

	{ "Error: utility is not installed:",
	  "Erro: utilitário não instalado:",
	  "Error: utilidad no instalada:",
	  "Erreur : utilitaire non installé :",
	  "Fehler: Hilfsprogramm nicht installiert:" },
	{ "Search packages (empty for all):",
	  "Pesquisar pacotes (vazio para todos):",
	  "Buscar paquetes (vacío para todos):",
	  "Rechercher des paquets (vide pour tous) :",
	  "Pakete suchen (leer für alle):" },
};

static const char *current_lang = "en";

/* ---- language ---- */

static const char *normalize_lang(const char *lang)
{
	if (!lang || !*lang)
		return NULL;
	if (!strcasecmp(lang, "pt") || !strcasecmp(lang, "pt_br") ||
	    !strcasecmp(lang, "pt-br"))
		return "pt_BR";
	if (!strcasecmp(lang, "es"))
		return "es";
	if (!strcasecmp(lang, "fr"))
		return "fr";
	if (!strcasecmp(lang, "de"))
		return "de";
	if (!strcasecmp(lang, "en"))
		return "en";
	return NULL;
}

static const char *detect_language(void)
{
	static char buf[16];
	const char *env;
	char *cut;
	size_t n;

	env = getenv("LANGUAGE");
	if (!env || !*env)
		env = getenv("LANG");
	if (!env || !*env)
		return "en";

	cut = (char *)strchr(env, ':');
	n = cut ? (size_t)(cut - env) : strlen(env);
	if (n >= sizeof(buf))
		n = sizeof(buf) - 1;
	memcpy(buf, env, n);
	buf[n] = '\0';

	cut = strchr(buf, '.');
	if (cut)
		*cut = '\0';
	cut = strchr(buf, '@');
	if (cut)
		*cut = '\0';

	if (!strncmp(buf, "pt", 2)) return "pt_BR";
	if (!strncmp(buf, "es", 2)) return "es";
	if (!strncmp(buf, "fr", 2)) return "fr";
	if (!strncmp(buf, "de", 2)) return "de";
	return "en";
}

static const char *tr(const char *key)
{
	size_t i;

	if (!current_lang || !strcmp(current_lang, "en"))
		return key;

	for (i = 0; i < ARRAY_SIZE(translations); i++) {
		const struct translation *t = &translations[i];

		if (strcmp(t->key, key))
			continue;
		if (!strcmp(current_lang, "pt_BR") && t->pt_BR && *t->pt_BR)
			return t->pt_BR;
		if (!strcmp(current_lang, "es") && t->es && *t->es)
			return t->es;
		if (!strcmp(current_lang, "fr") && t->fr && *t->fr)
			return t->fr;
		if (!strcmp(current_lang, "de") && t->de && *t->de)
			return t->de;
		break;
	}
	return key;
}

#define _(s) tr(s)

/* ---- generic ---- */

static char cache_file[PATH_MAX];

struct string_list {
	char **items;
	size_t count;
	size_t capacity;
};

struct fzf_opts {
	int multi;
	const char *height;
	const char *border;
	const char *prompt;
	const char *header;
	const char *info;
	const char *bind;
	const char *pointer;
	const char *marker;
	const char *query;
};

static void string_list_free(struct string_list *l)
{
	size_t i;

	for (i = 0; i < l->count; i++)
		free(l->items[i]);
	free(l->items);
	l->items = NULL;
	l->count = l->capacity = 0;
}

static int string_list_append(struct string_list *l, const char *s)
{
	if (l->count == l->capacity) {
		size_t n = l->capacity ? l->capacity * 2 : 16;
		char **p = realloc(l->items, n * sizeof(char *));

		if (!p)
			return -1;
		l->items = p;
		l->capacity = n;
	}
	l->items[l->count] = strdup(s);
	if (!l->items[l->count])
		return -1;
	l->count++;
	return 0;
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

static int command_exists(const char *cmd)
{
	char path[PATH_MAX];
	const char *env = getenv("PATH");
	char *copy, *save = NULL, *dir;
	int found = 0;

	if (!env)
		return 0;
	copy = strdup(env);
	if (!copy)
		return 0;
	for (dir = strtok_r(copy, ":", &save); dir;
	     dir = strtok_r(NULL, ":", &save)) {
		snprintf(path, sizeof(path), "%s/%s", dir, cmd);
		if (access(path, X_OK) == 0) {
			found = 1;
			break;
		}
	}
	free(copy);
	return found;
}

static int run_command(char *const argv[])
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return -1;
	}
	if (pid == 0) {
		execvp(argv[0], argv);
		perror(argv[0]);
		_exit(127);
	}
	if (waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
		return -1;
	}
	return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static void press_any_key(const char *prompt)
{
	struct termios old_t, new_t;
	int has_tty;

	printf("%s", prompt);
	fflush(stdout);
	has_tty = tcgetattr(STDIN_FILENO, &old_t) == 0;
	if (has_tty) {
		new_t = old_t;
		new_t.c_lflag &= ~(ICANON | ECHO);
		new_t.c_cc[VMIN] = 1;
		new_t.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
	}
	getchar();
	if (has_tty)
		tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
}

static char *last_field(char *line)
{
	char *last = line;
	char *p;

	for (p = line; *p; p++) {
		if (*p == ' ' || *p == '\t') {
			char *n = p + 1;

			while (*n == ' ' || *n == '\t')
				n++;
			if (*n)
				last = n;
			p = n - 1;
		}
	}
	return last;
}

/* Detect the terminal width (stdin, stdout or stderr must be a tty). */
static int terminal_width(void)
{
	struct winsize ws;
	const char *env;
	int n;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		return ws.ws_col;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		return ws.ws_col;
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		return ws.ws_col;

	env = getenv("COLUMNS");
	if (env && *env) {
		n = atoi(env);
		if (n > 0)
			return n;
	}
	return 80;
}

/* ---- cache ---- */

static int init_cache_file(void)
{
	const char *home = getenv("HOME");

	if (!home)
		return -1;
	snprintf(cache_file, sizeof(cache_file), "%s/" CACHE_FILE_SUFFIX, home);
	return 0;
}

static int check_cache_update(void)
{
	struct stat st;

	if (stat(cache_file, &st) < 0 || st.st_size == 0)
		return 1;
	return (time(NULL) - st.st_mtime) > CACHE_TIME;
}

static int update_cache(void)
{
	FILE *fp, *out;
	char tmp[PATH_MAX + 64], line[8192], dir[PATH_MAX];
	struct stat st;
	char *slash;
	int fd;
	int tw;

	printf("%s>%s %s\n", fBlueLight, fEnd,
	       _("Generating cache file, please wait..."));

	snprintf(dir, sizeof(dir), "%s", cache_file);
	slash = strrchr(dir, '/');
	if (slash) {
		*slash = '\0';
		if (make_dir_p(dir) < 0) {
			perror(dir);
			return -1;
		}
	}

	snprintf(tmp, sizeof(tmp), "%s.XXXXXX", cache_file);
	fd = mkstemp(tmp);
	if (fd < 0) {
		perror("mkstemp");
		return -1;
	}

	fp = popen("flatpak remote-ls flathub --app "
		   "--columns=name,version,description,application", "r");
	if (!fp) {
		perror("popen");
		close(fd);
		unlink(tmp);
		return -1;
	}
	out = fdopen(fd, "w");
	if (!out) {
		perror("fdopen");
		pclose(fp);
		close(fd);
		unlink(tmp);
		return -1;
	}

	tw = terminal_width();
	while (fgets(line, sizeof(line), fp)) {
		char *nl, *f1, *f2, *f3, *f4, *extra;

		if ((nl = strchr(line, '\n')))
			*nl = '\0';
		f1 = line;
		f2 = strchr(f1, '\t');
		if (!f2) continue;
		*f2++ = '\0';
		f3 = strchr(f2, '\t');
		if (!f3) continue;
		*f3++ = '\0';
		f4 = strchr(f3, '\t');
		if (!f4) continue;
		*f4++ = '\0';
		if ((extra = strchr(f4, '\t')))
			*extra = '\0';

		if (tw < 100)
			fprintf(out, "%-30.30s %-15.15s %s\n",
				f1, f3, f4);
		else if (tw < 140)
			fprintf(out, "%-35.35s %-10.10s %-20.20s %s\n",
				f1, f2, f3, f4);
		else
			fprintf(out, "%-45.45s %-12.12s %-45.45s %s\n",
				f1, f2, f3, f4);
	}
	pclose(fp);

	if (fclose(out) != 0) {
		perror("fclose");
		unlink(tmp);
		return -1;
	}
	if (stat(tmp, &st) < 0 || st.st_size == 0) {
		unlink(tmp);
		printf("%s!%s %s\n", fRed, fEnd,
		       _("Failed to generate cache."));
		press_any_key(_("Press any key to continue..."));
		return -1;
	}
	if (rename(tmp, cache_file) < 0) {
		perror("rename");
		unlink(tmp);
		return -1;
	}
	return 0;
}

/* ---- fzf ---- */

static int run_fzf(FILE *input, const struct fzf_opts *opts,
		   struct string_list *results)
{
	int pipefd[2];
	pid_t pid;
	int status;
	FILE *fp;
	char line[4096];
	char height_arg[64], border_arg[64], info_arg[64], bind_arg[192];
	char query_arg[512];
	char *args[24];
	int i = 0;

	snprintf(height_arg, sizeof(height_arg), "--height=%s", opts->height);
	snprintf(info_arg, sizeof(info_arg), "--info=%s", opts->info);
	if (opts->bind)
		snprintf(bind_arg, sizeof(bind_arg), "--bind=%s", opts->bind);
	if (opts->border)
		snprintf(border_arg, sizeof(border_arg), "--border=%s",
			 opts->border);
	if (opts->query && *opts->query)
		snprintf(query_arg, sizeof(query_arg), "--query=%s",
			 opts->query);

	args[i++] = "fzf";
	args[i++] = opts->multi ? "--multi" : "--no-multi";
	args[i++] = height_arg;
	args[i++] = "--layout=reverse";
	args[i++] = "--preview";
	args[i++] = "";
	args[i++] = opts->border ? border_arg : "--border";
	args[i++] = "--margin=1";
	args[i++] = "--padding=1";
	args[i++] = "--prompt";
	args[i++] = (char *)opts->prompt;
	args[i++] = "--pointer";
	args[i++] = (char *)opts->pointer;
	args[i++] = "--marker";
	args[i++] = (char *)opts->marker;
	args[i++] = "--header";
	args[i++] = (char *)opts->header;
	args[i++] = info_arg;
	args[i++] = FZF_COLORS;
	args[i++] = "--cycle";
	if (opts->bind)
		args[i++] = bind_arg;
	if (opts->query && *opts->query)
		args[i++] = query_arg;
	args[i++] = NULL;

	if (pipe(pipefd) < 0)
		return -1;

	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}
	if (pid == 0) {
		dup2(fileno(input), STDIN_FILENO);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);
		execvp("fzf", args);
		perror("fzf");
		_exit(127);
	}
	close(pipefd[1]);
	fp = fdopen(pipefd[0], "r");
	if (!fp) {
		close(pipefd[0]);
		waitpid(pid, &status, 0);
		return -1;
	}
	while (fgets(line, sizeof(line), fp)) {
		char *nl = strchr(line, '\n');

		if (nl)
			*nl = '\0';
		if (*line)
			string_list_append(results, last_field(line));
	}
	fclose(fp);
	waitpid(pid, &status, 0);
	return 0;
}

/* ---- flatpak ---- */

static int is_installed(const char *pkg)
{
	FILE *fp;
	char line[4096];
	int found = 0;

	fp = popen("flatpak list --app --columns=application", "r");
	if (!fp)
		return 0;
	while (fgets(line, sizeof(line), fp)) {
		size_t n = strlen(line);

		while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
			line[--n] = '\0';
		if (!strcmp(line, pkg)) {
			found = 1;
			break;
		}
	}
	pclose(fp);
	return found;
}

/* Prompts for an optional search term. Returns NULL on empty input
 * (equivalent to "no filter"). */
static char *prompt_search_term(void)
{
	static char buf[256];
	char *p;

	printf("%s>%s %s ", fBlueLight, fEnd,
	       _("Search packages (empty for all):"));
	fflush(stdout);

	if (!fgets(buf, sizeof(buf), stdin))
		return NULL;

	p = strchr(buf, '\n');
	if (p)
		*p = '\0';
	p = strchr(buf, '\r');
	if (p)
		*p = '\0';

	return *buf ? buf : NULL;
}

static void install_flatpak(void)
{
	struct string_list sel = {0};
	struct fzf_opts opts = {
		.multi   = 1,
		.height  = "80%",
		.prompt  = "Apps > ",
		.header  = _("Select applications to INSTALL "
			     "<TAB selects multiple>."),
		.info    = "inline",
		.bind    = "ctrl-a:select-all,ctrl-d:deselect-all",
		.pointer = ">",
		.marker  = "*",
	};
	FILE *input;
	size_t i;

	input = fopen(cache_file, "r");
	if (!input) {
		perror(cache_file);
		return;
	}

	if (isatty(STDIN_FILENO)) {
		char *term = prompt_search_term();

		if (term)
			opts.query = term;
	}
	run_fzf(input, &opts, &sel);
	fclose(input);

	for (i = 0; i < sel.count; i++) {
		const char *pkg = sel.items[i];
		char *args[] = { "flatpak", "install", "flathub",
				 (char *)pkg, "-y", NULL };

		if (is_installed(pkg)) {
			printf("%s+%s %s %s\n", fGreen, fEnd, pkg,
			       _("is already installed"));
			sleep(1);
		} else {
			printf("%s>%s %s %s...\n", fBlueLight, fEnd,
			       _("Installing"), pkg);
			run_command(args);
		}
	}
	string_list_free(&sel);
}

static void remove_flatpak(void)
{
	struct string_list sel = {0};
	struct fzf_opts opts = {
		.multi   = 1,
		.height  = "80%",
		.header  = _("Select applications to REMOVE (TAB marks)."),
		.info    = "inline",
		.bind    = "ctrl-a:select-all,ctrl-d:deselect-all",
		.pointer = ">",
		.marker  = "*",
	};
	char prompt_buf[128];
	FILE *input, *fp;
	size_t i;

	snprintf(prompt_buf, sizeof(prompt_buf), "%s > ", _("Remove"));
	opts.prompt = prompt_buf;

	input = tmpfile();
	if (!input) {
		perror("tmpfile");
		return;
	}
	fp = popen("flatpak list --app --columns=name,application", "r");
	if (fp) {
		char line[4096];

		while (fgets(line, sizeof(line), fp)) {
			char *nl, *tab;

			if ((nl = strchr(line, '\n')))
				*nl = '\0';
			tab = strchr(line, '\t');
			if (!tab)
				continue;
			*tab++ = '\0';
			fprintf(input, "%-45s %s\n", line, tab);
		}
		pclose(fp);
	}
	rewind(input);
	run_fzf(input, &opts, &sel);
	fclose(input);

	for (i = 0; i < sel.count; i++) {
		const char *pkg = sel.items[i];
		char *args[] = { "flatpak", "uninstall", (char *)pkg, NULL };

		if (is_installed(pkg)) {
			printf("%s>%s %s %s...\n", fBlueLight, fEnd,
			       _("Removing"), pkg);
			run_command(args);
		} else {
			printf("%s!%s %s %s\n", fRed, fEnd, pkg,
			       _("is not installed"));
			press_any_key(_("Press any key to continue..."));
		}
	}
	string_list_free(&sel);
}

static void update_flatpak(void)
{
	char *args[] = { "flatpak", "update", "-y", NULL };

	printf("%s>%s %s\n", fBlueLight, fEnd, _("Updating Flatpaks..."));
	run_command(args);
	press_any_key(_("Press any key to continue..."));
}

/* ---- menu ---- */

static int main_menu(void)
{
	char header_buf[256];

	for (;;) {
		struct string_list sel = {0};
		struct fzf_opts opts = {
			.multi   = 0,
			.height  = "40%",
			.border  = "rounded",
			.prompt  = "[" PRG_NAME " v" VERSION "] > ",
			.header  = NULL,
			.info    = "hidden",
			.pointer = ">",
			.marker  = "*",
		};
		const char *opt_install = _("Install");
		const char *opt_remove  = _("Remove");
		const char *opt_update  = _("Update");
		const char *opt_exit    = _("Exit");
		FILE *input;

		snprintf(header_buf, sizeof(header_buf), "%s  [%s]",
			 _("What would you like to do?"), current_lang);
		opts.header = header_buf;

		input = tmpfile();
		if (!input)
			return -1;
		fprintf(input, "%s\n%s\n%s\n%s\n",
			opt_install, opt_remove, opt_update, opt_exit);
		rewind(input);
		run_fzf(input, &opts, &sel);
		fclose(input);

		if (sel.count == 0) {
			string_list_free(&sel);
			return 0;
		}

		if (!strcasecmp(sel.items[0], opt_install))
			install_flatpak();
		else if (!strcasecmp(sel.items[0], opt_remove))
			remove_flatpak();
		else if (!strcasecmp(sel.items[0], opt_update))
			update_flatpak();
		else if (!strcasecmp(sel.items[0], opt_exit)) {
			string_list_free(&sel);
			return 0;
		}
		string_list_free(&sel);
	}
}

/* ---- main ---- */

int main(int argc, char *argv[])
{
	const char *deps[] = { "flatpak", "fzf", NULL };
	const char *forced_lang = NULL;
	int do_update = 0, do_help = 0, do_version = 0;
	int i;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (!strcmp(a, "-h") || !strcmp(a, "--help"))
			do_help = 1;
		else if (!strcmp(a, "-v") || !strcmp(a, "--version"))
			do_version = 1;
		else if (!strcmp(a, "-u") || !strcmp(a, "--update-cache"))
			do_update = 1;
		else if (!strcmp(a, "-l") || !strcmp(a, "--lang")) {
			if (i + 1 >= argc) {
				fprintf(stderr,
					"Option %s requires an argument\n", a);
				return 1;
			}
			forced_lang = argv[++i];
		} else if (!strncmp(a, "--lang=", 7)) {
			forced_lang = a + 7;
		} else {
			fprintf(stderr, "Unknown option: %s\n", a);
			return 1;
		}
	}

	if (forced_lang) {
		const char *norm = normalize_lang(forced_lang);

		if (!norm) {
			fprintf(stderr, "Unknown language: %s\n", forced_lang);
			return 1;
		}
		current_lang = norm;
	} else {
		current_lang = detect_language();
	}

	if (do_help) {
		printf("%s %s [-u|--update-cache] [-v|--version] "
		       "[-l|--lang=LANG] [-h|--help]\n",
		       _("Usage:"), PRG_NAME);
		return 0;
	}
	if (do_version) {
		printf("%s %s\n", PRG_NAME, VERSION);
		return 0;
	}

	if (init_cache_file() < 0) {
		fprintf(stderr, "Cannot determine HOME directory\n");
		return 1;
	}

	for (i = 0; deps[i]; i++) {
		if (!command_exists(deps[i])) {
			fprintf(stderr, "%s!%s %s %s\n", fRed, fEnd,
				_("Error: utility is not installed:"),
				deps[i]);
			return 1;
		}
	}

	if (do_update)
		update_cache();
	else if (check_cache_update())
		update_cache();

	return main_menu();
}