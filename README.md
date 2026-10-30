# Flat's Packages

**Flat's Packages** (`flatspkg`) is an interactive terminal manager for Flatpak applications, written in C. It uses `fzf` to browse and select Flathub applications and invokes the official `flatpak` utility to list, install, remove, and update applications.

- **Version in source:** 1.0
- **Author:** Breno Rodrigues Alves
- **License:** AGPL-3.0-only (see [`LICENSE`](LICENSE))

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Build and test](#build-and-test)
- [Installation](#installation)
- [Usage](#usage)
- [Command-line options](#command-line-options)
- [Languages](#languages)
- [Cache and local data](#cache-and-local-data)
- [Installed files and uninstalling](#installed-files-and-uninstalling)
- [Repository layout](#repository-layout)
- [Limitations and troubleshooting](#limitations-and-troubleshooting)

## Features

- Interactive browsing of the Flathub application catalog, powered by `fzf`.
- Install one or more selected applications; already-installed applications are detected and skipped.
- Remove installed applications and update all installed Flatpaks.
- A local catalog cache, created when needed and refreshed automatically if missing, empty, or more than 24 hours old.
- Interface translations for Brazilian Portuguese, Spanish, French, and German, as well as English by default; translations are embedded in the executable.
- An installer supporting per-user, system-wide, or custom-prefix installation, and installing the man page and available shell completions as well.

## Requirements

### Runtime

- `flatpak`, available in `PATH`.
- `fzf`, available in `PATH`.
- An interactive terminal for the menus. The initial installation search prompt is shown when standard input is a terminal.
- The Flatpak `flathub` remote configured to query the catalog and install applications from it.

The program checks for the `flatpak` and `fzf` executables when starting the normal application flow. `--help` and `--version` are handled before these checks and can be used without those dependencies. The C standard library and POSIX facilities used by the project are also required; these are normally available on Linux systems.

### Build and test

- A C compiler supported by the system (for example, `cc` or `gcc`).
- `make`.
- `bash` to run the test suite.

## Build and test

From the repository root, run:

```sh
make
```

The default target builds two executables in the current directory:

- `flatspkg`: the application manager.
- `install`: the installer for `flatspkg` itself.

Run the automated tests with:

```sh
make check
```

The suite tests basic options, version reporting, language selection through the environment and command line, rejection of invalid languages, `--version` without `HOME`, and the missing-dependency error. The current code has **17 checks**. These tests do not need network access and do not install or remove applications; they are not integration tests of the interactive Flathub workflow.

Remove generated executables with:

```sh
make clean
```

Build options can be passed to `make`, for example:

```sh
make CC=gcc CFLAGS='-O2 -Wall -Wextra'
```

## Installation

After building, invoke the installer directly:

```sh
./install --user
```

This installs under `$HOME/.local` and does not require administrative privileges, provided the destination is writable. Ensure that `$HOME/.local/bin` is in `PATH` to run `flatspkg` by name.

For a system-wide installation:

```sh
sudo ./install --system
```

The system prefix is `/usr/local`; the process must have permission to write there. You can also choose an explicit prefix:

```sh
./install --prefix=/opt/flatspkg
```

The executable is installed as `PREFIX/bin/flatspkg`. The man page and shell completions are installed under `PREFIX/share` when the corresponding files exist in the source tree.

Without an explicit prefix option, resolution follows this order: `--prefix=PATH` (or `--prefix PATH`), `--user`, `--system`, the `PREFIX` environment variable, and finally automatic detection. Automatic detection chooses `/usr/local` if `/usr/local/bin` is writable; otherwise, it chooses `$HOME/.local`. The installer reports an error if no destination can be determined or written. Avoid combining conflicting options; if both `--user` and `--system` are supplied, the code gives precedence to `--user`.

The Makefile also provides these shortcuts:

```sh
make install-user
sudo make install-system
make install-prefix PREFIX=/opt/flatspkg
```

By default, `make install` invokes `./install` without extra options. Pass options through `INSTALL_ARGS`, for example:

```sh
make install INSTALL_ARGS='--user'
```

## Usage

Start the menu with:

```sh
flatspkg
```

The main menu offers **Install**, **Remove**, **Update**, and **Exit** (labels are translated according to the active language). Use the arrow keys to navigate and `Enter` to select; `Esc` or making no selection exits the menu.

### Install applications

The installation list comes from the Flathub remote's application catalog, stored in the local cache. In an interactive run, the program first offers a search prompt; leave it empty to show all entries, or enter a term to seed the `fzf` filter. In the selector, press `Tab` to mark multiple applications and `Enter` to confirm. `Ctrl+A` selects all and `Ctrl+D` clears all selections.

For each selection, the program checks the installed application list. If the application is not already installed, it runs the equivalent of:

```sh
flatpak install flathub APPLICATION_ID -y
```

The `-y` flag automatically confirms installation prompts. Removal, by contrast, does not add `-y` and may prompt for confirmation according to Flatpak's behavior.

### Remove applications

**Remove** lists installed applications, displaying each application's name and ID. Mark one or more items with `Tab` and press `Enter`; `Ctrl+A` and `Ctrl+D` are also available. The program runs `flatpak uninstall APPLICATION_ID` for each selection.

### Update

**Update** runs:

```sh
flatpak update -y
```

This updates Flatpaks according to the behavior of the Flatpak command. When it returns, the program waits for a key press before returning to the menu.

## Command-line options

```text
flatspkg [-u|--update-cache] [-v|--version] [-l|--lang LANG] [-h|--help]
```

| Option | Description |
|---|---|
| `-h`, `--help` | Print a short usage summary and exit. |
| `-v`, `--version` | Print the program name and version (`1.0`) and exit. |
| `-u`, `--update-cache` | Regenerate the cache before opening the menu. |
| `-l LANG`, `--lang LANG`, `--lang=LANG` | Force the interface language. Accepted values: `pt`, `pt_BR`, `pt-BR`, `es`, `fr`, `de`, and `en` (case-insensitive). |

Unknown arguments, a missing argument to `-l`/`--lang`, or an unsupported language result in exit status `1`. `--update-cache` is not a standalone command: after attempting the refresh, the program proceeds to the menu. If the cache cannot be refreshed, an error is displayed, but the program still attempts to open the menu; a usable previous cache may remain available.

Examples:

```sh
flatspkg --help
flatspkg --version
flatspkg --update-cache
flatspkg -l pt_BR
flatspkg --lang=fr
```

## Languages

Translations are compiled into the program; `.mo` files, GNU gettext, and separate language packs are not required. Choose a language with `--lang` or let the program detect it from the environment.

An explicit command-line selection takes precedence over the environment. Without `--lang`, `LANGUAGE` takes precedence over `LANG`; if `LANGUAGE` contains a colon-separated list, only its first entry is considered. Encoding suffixes (such as `.UTF-8`) and modifier suffixes (such as `@euro`) are stripped during detection. The `pt`, `es`, `fr`, and `de` language families select Brazilian Portuguese, Spanish, French, and German, respectively; other languages fall back to English. English is also the default when these variables are unset.

```sh
LANG=pt_BR.UTF-8 flatspkg
LANG=es_ES.UTF-8 flatspkg
LANG=fr_FR.UTF-8 flatspkg
LANG=de_DE.UTF-8 flatspkg
LANGUAGE=fr:en flatspkg
flatspkg --lang=en
```

## Cache and local data

The catalog is stored at:

```text
$HOME/.cache/flatspkg-flathub.cache
```

The cache is regenerated if it is missing, empty, or more than 24 hours old. `--update-cache` forces a refresh. To populate it, the program queries the Flathub remote for application name, version, description, and application ID. The displayed column layout adapts to the detected terminal width; some values may be truncated to fit.

The query requires Flatpak to be installed and the `flathub` remote to be configured and reachable. If cache generation fails, the program reports the failure and attempts to proceed to the menu. `HOME` is required to determine the cache path; without it, the normal application flow exits with an error. `--help` and `--version` exit before `HOME` is read.

## Installed files and uninstalling

The installer copies:

- `flatspkg` to `PREFIX/bin/flatspkg`;
- `man/flatspkg.1`, if present, to `PREFIX/share/man/man1/flatspkg.1`;
- Bash completion to `PREFIX/share/bash-completion/completions/flatspkg`;
- Zsh completion to `PREFIX/share/zsh/site-functions/_flatspkg`;
- Fish completion to `PREFIX/share/fish/vendor_completions.d/flatspkg.fish`.

Auxiliary files are optional: the installer skips any that are absent from the source tree. To remove an installation, use the same prefix. For an installation under `/usr/local`, for example:

```sh
make uninstall PREFIX=/usr/local
```

The `uninstall` target removes the executable, man page, and three completion files under the prefix supplied to Make (default `/usr/local`). It does not remove the cache at `$HOME/.cache/flatspkg-flathub.cache`, installed Flatpak applications, empty directories, or build executables in the project directory. Supply the appropriate prefix to `make` to uninstall from a different location.

## Repository layout

```text
.
├── flatspkg.c                 # Manager: CLI, interface, cache, Flatpak/fzf integration
├── install.c                  # Executable and auxiliary-file installer
├── Makefile                   # Build, tests, installation, uninstall, and clean targets
├── man/flatspkg.1             # Manual page
├── completions/               # Bash, Fish, and Zsh completions
├── tests/test.sh              # CLI and language-selection tests
├── LICENSE                    # AGPL-3.0-only license text
└── README.md                  # This document
```

## Limitations and troubleshooting

- **`utility is not installed` / missing utility:** install `flatpak` and `fzf`, and make sure both are available in `PATH`.
- **Cache-generation failure or empty catalog:** check network access, the `flathub` remote configuration (`flatpak remotes`), and whether `flatpak remote-ls flathub --app` works in the terminal. The cache may not be replaced if the query fails.
- **`Cannot determine HOME directory`:** set `HOME` to the user's home directory before starting the interactive flow.
- **`fzf` does not open correctly:** run the program in a compatible terminal and check your local `fzf` version and configuration.
- **Command not found after a per-user install:** add `$HOME/.local/bin` to `PATH`, then open a new shell or reload your shell configuration.
- **Permission denied for the prefix:** choose `--user` or a writable `--prefix`; use appropriate privileges for `/usr/local`.

The automated suite covers the command-line interface and localization messages, but does not provide end-to-end validation of `fzf` interaction, network access, or actual application installation, removal, or updating.
