# mux

A fast, dependency-free tmuxinator replacement for tmux and Herdr, written in C.

[![Backends: tmux + Herdr](https://img.shields.io/badge/backends-tmux%20%2B%20Herdr-0f766e?style=for-the-badge)](#multiplexer-backends)
[![Language: C](https://img.shields.io/badge/language-C-555?style=for-the-badge)](https://en.wikipedia.org/wiki/C_%28programming_language%29)

![mux demo](demo.gif)

## Why

tmuxinator requires Ruby. mux is a single binary that launches the same
tmuxinator-style project files into either classic tmux sessions or Herdr
workspaces.

### Size comparison

| Tool | Install size |
|------|-------------|
| **mux** | 64 KB |
| tmuxinator | 85 MB |

That's **~1300x smaller**. The tmuxinator closure includes Ruby, RubyGems, and all gem dependencies. mux just needs libyaml and libc.

## Multiplexer backends

**One config. Two multiplexers.** mux turns tmuxinator-compatible YAML into
repeatable terminal workspaces for both:

- **tmux** - the default backend, with full tmuxinator-style session support
- **Herdr** - a workspace/tab/pane backend for AI-agent-friendly terminal layouts

```sh
mux start work              # tmux backend
mux start work --backend herdr
MUX_BACKEND=herdr mux start work
```

## Install

### Quick install (Linux/macOS)

```sh
curl -fsSL https://raw.githubusercontent.com/willfish/mux/master/install.sh | sh
```

This downloads a pre-built binary for your platform. Supports:
- Linux x86_64 / aarch64
- macOS aarch64 (Apple Silicon)

Intel Mac users: build from source (see below).

### Nix flake

Add mux as a flake input and include it in your packages via an overlay:

```nix
# flake.nix
{
  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    mux = {
      url = "github:willfish/mux";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { nixpkgs, home-manager, mux, ... }:
    let
      system = "x86_64-linux";
      overlay = final: prev: {
        mux = mux.packages.${system}.default;
      };
      pkgs = import nixpkgs {
        inherit system;
        overlays = [ overlay ];
      };
    in {
      homeConfigurations.yourname = home-manager.lib.homeManagerConfiguration {
        inherit pkgs;
        modules = [ ./home ];
      };
    };
}
```

Then add `mux` to your Home Manager packages:

```nix
# home/packages.nix
{ pkgs, ... }:
{
  home.packages = with pkgs; [
    mux
  ];
}
```

### nix profile

```sh
nix profile install github:willfish/mux
```

### From source

Requires: C11 compiler, meson, ninja, libyaml

```sh
meson setup build
meson compile -C build
meson install -C build
```

### Development

```sh
nix develop
meson setup builddir
meson compile -C builddir
meson test -C builddir
```

## Usage

```
mux start <project>       Start a tmux session or Herdr workspace
mux stop <project>        Stop a tmux session or Herdr workspace
mux new <project>         Create a new project config
mux edit <project>        Edit a project config in $EDITOR
mux copy <src> <dst>      Copy a project config
mux delete <project>      Delete a project config
mux list                  List available projects
mux debug <project>       Print generated tmux script
mux local                 Start from ./.tmuxinator.yml
mux doctor                Check dependencies
mux completions <shell>   Print shell completion script (bash/zsh/fish)
```

### Shortcuts

```
mux <project>             Same as mux start <project>
mux s                     start
mux e, open, o            edit
mux cp                    copy
mux rm                    delete
mux ls, l                 list
```

### Flags

```
-n, --name NAME           Override session name
-p, --project-config P    Specify config file path
-b, --backend NAME        Backend to use: tmux or herdr
-a, --append              Add windows to existing session
-A, --active              Only list active sessions
```

### Template variables

Configs can use `<%= @settings["key"] %>` placeholders, filled from CLI args:

```sh
mux start myproject host=localhost port=3000
```

## Configuration

mux reads tmuxinator-compatible YAML configs from:

1. `$TMUXINATOR_CONFIG/`
2. `$XDG_CONFIG_HOME/tmuxinator/`
3. `~/.config/tmuxinator/`
4. `~/.tmuxinator/`
5. `./.tmuxinator.yml` (local, via `mux local`)

### Example config

```yaml
name: myproject
root: ~/code/myproject

on_project_start: echo "Starting myproject"

windows:
  - editor:
      layout: main-vertical
      panes:
        - vim
        - guard
  - server: bundle exec rails s
  - logs: tail -f log/development.log
```

### tmux and Herdr backends

mux launches tmuxinator layouts into tmux by default, and can launch the same
layouts into Herdr workspaces:

```sh
mux start work --backend herdr
MUX_BACKEND=herdr mux start work
```

The Herdr backend starts the Herdr server when needed, maps one tmuxinator
project to one Herdr workspace, maps tmuxinator windows to Herdr tabs, and maps
panes to Herdr pane splits. Pane names are applied with `herdr pane rename`,
and pane commands are sent to the pane shell just like the tmux backend sends
keys to tmux panes.

tmux remains the default backend unless `--backend herdr` or
`MUX_BACKEND=herdr` is set.
Known limitations:

- `python3` is required at runtime to parse Herdr's JSON CLI output.
- tmux `layout:` strings are approximated with Herdr split directions and
  ratios; Herdr does not currently replay tmux layout strings directly.
- tmux-specific options such as sockets, tmux command overrides, and tmux pane
  synchronization do not have Herdr equivalents.

## Compatibility

mux aims for full compatibility with tmuxinator YAML configs. Your existing configs should work unchanged.

Supported features:
- All window/pane layouts
- Project hooks (`on_project_start`, `on_project_stop`, etc.)
- `pre_window` commands
- Window-specific root directories
- Pane synchronisation
- Pane titles
- Socket name/path
- Custom tmux command
- Deprecated field aliases (`project_name`, `project_root`, `tabs`, `cli_args`)

## Shell completions

```sh
# bash — add to ~/.bashrc
eval "$(mux completions bash)"

# zsh — add to ~/.zshrc
eval "$(mux completions zsh)"

# fish — add to ~/.config/fish/config.fish
mux completions fish | source
```

## License

MIT
