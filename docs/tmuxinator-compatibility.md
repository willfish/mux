# Tmuxinator Compatibility

This matrix tracks mux against the
[upstream tmuxinator README](https://github.com/tmuxinator/tmuxinator#readme) as
of 2026-06-09. The upstream README documents project hooks, `pre_window`, tmux
options and command overrides, startup window/pane, attach control, pane titles,
window roots, layouts, named panes, `focused_pane`, ERB templates, and config
locations.

## Supported

| Area | Upstream key or shape | mux support | Coverage |
| --- | --- | --- | --- |
| Project identity | `name`, `root` | Supported | `sample.yml`, config tests |
| Socket and tmux command | `socket_name`, `tmux_options`, `tmux_command` | Supported; `socket_path` is also accepted | `socket.yml`, `sample_wemux.yml`, script tests |
| Lifecycle hooks | `on_project_start`, `on_project_first_start`, `on_project_restart`, `on_project_exit`, `on_project_stop` | Supported as scalar or sequence | `hooks.yml`, `hooks.commands` |
| Shared pane setup | `pre_window` | Supported as scalar or sequence | `sample.yml`, `sample.commands` |
| Startup selection | `startup_window`, `startup_pane` | Supported | `startup.yml`, script tests |
| Attach control | `attach: false` | Supported | `detach.yml`, script tests |
| Pane titles | `enable_pane_titles`, `pane_title_position`, `pane_title_format`, named panes | Supported | `pane_titles.yml`, `pane_titles.commands` |
| Windows | scalar command windows, mapping windows, null/nameless windows | Supported | `sample.yml`, `nameless_window.yml` |
| Window roots | window-level `root` | Supported | `window_root.yml`, `window_root.commands` |
| Layouts | window-level `layout` | Passed through to tmux | `sample.commands`, `pane_titles.commands` |
| Panes | scalar panes, named panes, empty panes, sequence commands | Supported | `sample.yml`, `pane_titles.yml`, `focused_pane.yml` |
| Pane synchronization | `synchronize: before`, `after`, `true`, `false` | Supported | `synchronize.yml`, script tests |
| Focus pane | `focused_pane` by index or named pane | Supported | `focused_pane.yml`, `focused_pane.commands` |
| Deprecated aliases | `project_name`, `project_root`, `tabs`, `cli_args` | Supported | `sample_deprecations.yml`, `sample_deprecations.commands` |
| Experimental Herdr backend | `--backend herdr`, `MUX_BACKEND=herdr` | Maps project to workspace, windows to tabs, panes to splits | `test_script`, live Herdr validation |

## Partial Or Unsupported

| Area | Status | Notes |
| --- | --- | --- |
| Arbitrary ERB | Partial | mux supports `<%= @settings["key"] %>` substitutions only. Ruby expressions, `ENV[...]`, and `@args` array templates are not evaluated. |
| Deprecated `pre` / `post` top-level keys | Unsupported | Upstream marks these as deprecated in favour of project hooks. mux ignores them rather than emulating legacy hook order. |
| Interpreter-manager keys | Unsupported | Legacy `rbenv` and `rvm` keys are ignored. Use `pre_window`, matching upstream guidance. |
| `append` start mode | Unsupported | The CLI parses `--append`, but start script generation still creates/selects a named session rather than appending windows to the current session. |
| Recording from an existing tmux session | Unsupported | `tmuxinator new [project] [session]` is not implemented. |
| Local project creation | Unsupported | `mux local` starts `./.tmuxinator.yml`, but `new --local` style creation is not implemented. |
| Herdr layout fidelity | Partial | The Herdr backend approximates tmux `layout:` values with Herdr split directions and ratios because Herdr does not accept tmux layout strings. |

## Fixture Policy

Compatibility changes should add both parser-level coverage and normalized script
coverage when the generated tmux command sequence changes. Add a `*.yml` fixture
under `tests/fixtures/`, a matching `*.commands` expectation, and register it in
`tests/test_script_regressions.c`.
