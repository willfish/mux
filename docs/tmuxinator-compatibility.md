# Tmuxinator Compatibility

This matrix tracks mux against the
[upstream tmuxinator README](https://github.com/tmuxinator/tmuxinator#readme).
The upstream README documents project hooks, `pre_window`, tmux options and
command overrides, startup window/pane, attach control, pane titles, window
roots, layouts, named panes, `focused_pane`, ERB templates, and config
locations.

## Supported

| Area | Upstream key or shape | mux support | Coverage |
| --- | --- | --- | --- |
| Project identity | `name`, `root` (`.` and `:` sanitised to `_` for tmux) | Supported; missing `name` or `windows` fails validation | `sample.yml`, config & integration tests |
| Socket and tmux command | `socket_name`, `tmux_options`, `tmux_command` | Supported; `socket_path` is also accepted | `socket.yml`, `sample_wemux.yml`, script tests |
| Lifecycle hooks | `on_project_start`, `on_project_first_start`, `on_project_restart`, `on_project_exit`, `on_project_stop` | Supported as scalar or sequence; run from project root, without strict mode so unset variables cannot abort the script | `hooks.yml`, `hooks.commands`, live tmux tests |
| Shared pane setup | `pre_window`, `--no-pre-window` | Supported as scalar or sequence (`&&` joined) | `sample.yml`, `sample.commands`, script tests |
| Startup selection | `startup_window` (name or index), `startup_pane` | Supported; out-of-range values fall back to `0` | `startup.yml`, script & live tmux tests |
| Attach control | `attach: false`/`no`, `--attach`, `--no-attach` | Supported across tmux and herdr | `detach.yml`, script & CLI tests |
| Pane titles | `enable_pane_titles`, `pane_title_position`, `pane_title_format`, named panes | Window-scoped options with `#{pane_index}: #{pane_title}` default | `pane_titles.yml`, `pane_titles.commands`, live tmux tests |
| Windows | scalar command windows, mapping windows, null/nameless windows, duplicate/dotted/numeric names | Targeted by `base_index` | `sample.yml`, `nameless_window.yml`, live tmux tests |
| Window roots | absolute, `~` (including spaces), and relative window `root` | Relative window roots join onto project root | `window_root.yml`, `window_root.commands`, live tmux tests |
| Layouts | window-level `layout` | Passed through to tmux | `sample.commands`, `pane_titles.commands` |
| Panes | scalar panes, named panes, empty/`~`/`null` panes, sequence commands | Supported; defaults focus to pane `0` | `sample.yml`, `pane_titles.yml`, `focused_pane.yml` |
| Pane synchronization | `synchronize: before`, `after`, `true`/`yes`, `false`/`no` | Supported | `synchronize.yml`, script tests |
| Focus pane | `focused_pane` by index or named pane | Supported; out-of-range falls back to `0` | `focused_pane.yml`, `focused_pane.commands`, live tmux tests |
| Append mode | `-a`, `--append` | Supported on tmux backend; appends to the current tmux session (`display-message -p '#S'`), errors `Cannot append to a session that does not exist` outside one, and never attaches or switches (upstream `attach? && !append?`) | `test_script`, live tmux tests |
| Local projects | `.tmuxinator.yml`, `.tmuxinator.yaml`, `new --local`, `start`/`stop`/`debug` fallback | Supported | `test_path`, `test_app` |
| Config discovery | `$TMUXINATOR_CONFIG`, `$XDG_CONFIG_HOME/tmuxinator`, `~/.config/tmuxinator`, `~/.tmuxinator`, `.yml`/`.yaml`, subdirectories | Multi-directory fallback, sorted listing | `test_path`, `test_app` |
| Templates | `<%= @settings["k"] %>`, `@settings['k']`, `@args[N]`, `ENV["VAR"]` | Always evaluated, even with no CLI args | `test_template`, `test_config`, `test_app` |
| Deprecated aliases | `project_name`, `project_root`, `tabs`, `cli_args` | Supported | `sample_deprecations.yml`, `sample_deprecations.commands` |
| Herdr backends | `--backend herdr`, `--backend herdr-app`, `MUX_BACKEND` | Maps project to workspace/space, windows to tabs, panes to splits | `test_script`, `test_basic.sh` |

## Partial Or Unsupported

| Area | Status | Notes |
| --- | --- | --- |
| Arbitrary Ruby ERB | Partial | mux supports `@settings["k"]`/`@settings['k']`, `@args[N]`, and `ENV["VAR"]`/`ENV['VAR']`. Arbitrary Ruby expressions and `<% ... %>` control flow are not evaluated. |
| Deprecated `pre` / `post` top-level keys | Unsupported | Upstream marks these as deprecated in favour of project hooks. mux ignores them rather than emulating legacy hook order. |
| Interpreter-manager keys | Unsupported | Legacy `rbenv` and `rvm` keys are ignored. Use `pre_window`, matching upstream guidance. |
| Recording from an existing tmux session | Unsupported | `tmuxinator new [project] [session]` is not implemented. |
| Herdr layout fidelity | Partial | The Herdr backends approximate tmux `layout:` values with split directions and ratios because Herdr does not accept tmux layout strings. |

## Fixture Policy

Compatibility changes should add parser-level coverage, normalized script
coverage when the generated tmux command sequence changes, and live tmux
verification in `tests/integration/test_basic.sh` for runtime tmux behaviour.
Add a `*.yml` fixture under `tests/fixtures/`, a matching `*.commands`
expectation, and register it in `tests/test_script_regressions.c`.
