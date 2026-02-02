# mux

A fast, dependency-free tmuxinator replacement written in C.

## Why

tmuxinator requires Ruby. mux is a single binary with no runtime dependencies beyond tmux itself.

## Install

### Nix (recommended)

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
mux start <project>       Start a tmux session
mux stop <project>        Stop a tmux session
mux new <project>         Create a new project config
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
mux s <project>           Short alias for start
mux ls                    Short alias for list
```

### Flags

```
-n, --name NAME           Override session name
-p, --project-config P    Specify config file path
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
