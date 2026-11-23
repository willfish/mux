#ifndef MUX_APP_H
#define MUX_APP_H

/* Execute the mux CLI entrypoint with the given arguments.
 * Returns process exit status (0 on success, non-zero on error). */
int mux_run(int argc, char **argv);

#endif
