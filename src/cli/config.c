/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * proot.yml config loader
 *
 * Reads /usr/local/.config/proot.yml and applies settings
 * before CLI argument parsing, so proot works with zero flags.
 *
 * Supported keys:
 *   rootfs:     /path/to/rootfs        → -r
 *   cwd:        /root                  → -w
 *   bindings:                          → -b  (list, one per line with leading "- ")
 *     - /dev
 *     - /proc
 *     - /sys
 *     - /host/path:/guest/path
 *   fake_root:  true                   → -0
 *   kill_on_exit: true                 → --kill-on-exit
 *   verbose:    0                      → -v
 *   command:    /bin/bash              → default command (if none given on CLI)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <linux/limits.h>

#include "cli/cli.h"
#include "cli/config.h"
#include "cli/note.h"
#include "path/binding.h"

/* Config file path — override via env PROOT_CONFIG */
#define DEFAULT_CONFIG_PATH "/usr/local/.config/proot.yml"

/* ------------------------------------------------------------------ */
/* Tiny YAML helpers — no external lib needed for this simple format   */
/* ------------------------------------------------------------------ */

static char *trim(char *s)
{
	/* left trim */
	while (*s && isspace((unsigned char)*s))
		s++;
	/* right trim */
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)*(end - 1)))
		end--;
	*end = '\0';
	return s;
}

/* Return pointer to value after "key: " prefix, or NULL if key doesn't match */
static const char *match_key(const char *line, const char *key)
{
	size_t klen = strlen(key);
	if (strncmp(line, key, klen) != 0)
		return NULL;
	const char *p = line + klen;
	/* skip optional whitespace then colon */
	while (*p == ' ' || *p == '\t') p++;
	if (*p != ':') return NULL;
	p++;
	while (*p == ' ' || *p == '\t') p++;
	return p;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/**
 * load_yml_config - parse proot.yml and fill *cfg.
 *
 * Returns 0 on success, -1 if file not found (non-fatal),
 * -2 on parse error.
 */
int load_yml_config(ProotConfig *cfg)
{
	const char *path = getenv("PROOT_CONFIG");
	if (!path || path[0] == '\0')
		path = DEFAULT_CONFIG_PATH;

	FILE *f = fopen(path, "r");
	if (!f)
		return -1; /* not found — silently skip */

	memset(cfg, 0, sizeof(*cfg));
	cfg->verbose = -1; /* sentinel: not set */

	char line[PATH_MAX + 64];
	bool in_bindings = false;

	while (fgets(line, sizeof(line), f)) {
		/* strip newline */
		char *p = trim(line);

		/* skip blanks and comments */
		if (p[0] == '\0' || p[0] == '#')
			continue;

		/* binding list item */
		if (in_bindings) {
			if (p[0] == '-' && (p[1] == ' ' || p[1] == '\t')) {
				char *val = trim(p + 2);
				if (cfg->nb_bindings < PROOT_CONFIG_MAX_BINDINGS)
					cfg->bindings[cfg->nb_bindings++] = strdup(val);
				continue;
			} else {
				in_bindings = false;
				/* fall through to parse current line */
			}
		}

		const char *v;

		if ((v = match_key(p, "rootfs"))) {
			strncpy(cfg->rootfs, v, PATH_MAX - 1);
		} else if ((v = match_key(p, "cwd"))) {
			strncpy(cfg->cwd, v, PATH_MAX - 1);
		} else if ((v = match_key(p, "command"))) {
			strncpy(cfg->command, v, PATH_MAX - 1);
		} else if ((v = match_key(p, "fake_root"))) {
			cfg->fake_root = (strncmp(v, "true", 4) == 0);
		} else if ((v = match_key(p, "kill_on_exit"))) {
			cfg->kill_on_exit = (strncmp(v, "true", 4) == 0);
		} else if ((v = match_key(p, "verbose"))) {
			cfg->verbose = atoi(v);
		} else if (strncmp(p, "bindings:", 9) == 0) {
			in_bindings = true;
		}
		/* unknown keys are silently ignored */
	}

	fclose(f);
	return 0;
}

/**
 * apply_yml_config - push config values into the tracee using the same
 * handler functions that CLI parsing uses, so behaviour is identical.
 *
 * Must be called BEFORE parse_config() so that explicit CLI flags can
 * still override anything set here.
 */
void apply_yml_config(Tracee *tracee, const Cli *cli, const ProotConfig *cfg)
{
	int i;

	if (cfg->rootfs[0])
		handle_config_r(tracee, cli, cfg->rootfs);

	if (cfg->cwd[0])
		handle_config_w(tracee, cli, cfg->cwd);

	for (i = 0; i < cfg->nb_bindings; i++)
		handle_config_b(tracee, cli, cfg->bindings[i]);

	if (cfg->fake_root)
		handle_config_0(tracee, cli);

	if (cfg->kill_on_exit)
		handle_config_kill_on_exit(tracee, cli);

	if (cfg->verbose >= 0)
		handle_config_v(tracee, cli, cfg->verbose);

	/* cfg->command is consumed by the caller (main) if argv has no cmd */
}
