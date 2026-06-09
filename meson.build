project('mux', 'c',
  version: '0.2.0',
  default_options: ['c_std=c11', 'warning_level=3', 'werror=true'])

add_project_arguments(
  '-D_GNU_SOURCE',
  '-Wno-unused-parameter',
  '-Wno-error=cpp',
  language: 'c',
)

libyaml = dependency('yaml-0.1')

common_src = files(
  'src/cli.c',
  'src/config.c',
  'src/project.c',
  'src/script.c',
  'src/path.c',
  'src/doctor.c',
  'src/completion.c',
  'src/shell.c',
  'src/str.c',
  'src/arena.c',
  'src/template.c',
  'src/tmux.c',
)

executable('mux', files('src/main.c') + common_src, dependencies: [libyaml], install: true)

mux_lib = static_library('mux_lib', common_src, dependencies: [libyaml])

benchmark('benchmark_mux',
  executable('benchmark_mux', 'benchmarks/benchmark_mux.c',
    link_with: mux_lib,
    dependencies: [libyaml],
    include_directories: include_directories('src')),
  workdir: meson.project_source_root())

test_names = [
  'test_str',
  'test_arena',
  'test_shell',
  'test_path',
  'test_config',
  'test_script',
  'test_cli',
  'test_template',
  'test_tmux',
  'test_script_regressions',
]

foreach t : test_names
  test(t,
    executable(t, 'tests' / t + '.c',
      link_with: mux_lib,
      dependencies: [libyaml],
      include_directories: include_directories('src')),
    workdir: meson.project_source_root())
endforeach
