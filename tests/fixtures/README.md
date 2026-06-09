# Script Regression Fixtures

Each `*.commands` file is the normalized command sequence expected from the
matching `*.yml` fixture when passed through `script_generate_start`.

The normalization used by `tests/test_script_regressions.c` removes blank lines,
comments, and leading/trailing whitespace. It keeps shell control flow, hooks,
and generated tmux commands so regressions are visible as whole-command diffs
rather than loose substring checks.

To add a fixture:

1. Add or update `tests/fixtures/<name>.yml`.
2. Generate the current normalized output with:

   ```sh
   ./build/mux debug -p tests/fixtures/<name>.yml ignored \
     | sed 's/^[[:space:]]*//;s/[[:space:]]*$//' \
     | sed '/^$/d;/^#/d' > tests/fixtures/<name>.commands
   ```

3. Add `RUN_TEST` coverage for `<name>` in `tests/test_script_regressions.c`.
4. Review the `*.commands` file as the behavioural contract before committing.
