/* proot.yml config loader — header */
#ifndef PROOT_CONFIG_H
#define PROOT_CONFIG_H

#include <stdbool.h>
#include <linux/limits.h>
#include "tracee/tracee.h"
#include "cli/cli.h"

#define PROOT_CONFIG_MAX_BINDINGS 64

typedef struct {
	char  rootfs[PATH_MAX];
	char  cwd[PATH_MAX];
	char  command[PATH_MAX];
	char *bindings[PROOT_CONFIG_MAX_BINDINGS];
	int   nb_bindings;
	bool  fake_root;
	bool  kill_on_exit;
	int   verbose;          /* -1 = not set */
} ProotConfig;

int  load_yml_config(ProotConfig *cfg);
void apply_yml_config(Tracee *tracee, const Cli *cli, const ProotConfig *cfg);

/* Thin wrappers so config.c can call the same logic as the CLI handlers */
int handle_config_r(Tracee *tracee, const Cli *cli, const char *value);
int handle_config_b(Tracee *tracee, const Cli *cli, const char *value);
int handle_config_w(Tracee *tracee, const Cli *cli, const char *value);
int handle_config_0(Tracee *tracee, const Cli *cli);
int handle_config_kill_on_exit(Tracee *tracee, const Cli *cli);
int handle_config_v(Tracee *tracee, const Cli *cli, int level);

#endif /* PROOT_CONFIG_H */
