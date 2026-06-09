# Benchmarks

`benchmark_mux` is a lightweight performance baseline for the paths that make
`mux` feel fast in daily use:

- parsing regular configs
- parsing configs with template settings
- generating tmux scripts
- listing projects from a synthetic directory with 1000 config files

Run it locally with:

```sh
nix develop --command meson test -C build --benchmark --verbose --print-errorlogs
```

CI runs the same command so each pull request records current benchmark numbers
in the GitHub Actions log. Treat the numbers as a trend signal rather than a
hard pass/fail budget.
