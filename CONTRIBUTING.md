# Contributing

Thanks for wanting to help. Bug reports, fixes and new features are all welcome.
There is no contributor agreement to sign: under section 5 of the Apache Licence
2.0, anything you submit for inclusion is covered by the project's own licence.

## Reporting a bug

Open an issue and include the STM32 family you are using, your `OSAL_RTOS`
setting, the RTOS and its version, the error value you got back, and the
smallest piece of code that shows the problem. A failing test is even better,
see below.

## Making a change

1. Add or update a test in `test/test_osal.c` that fails before your change and
   passes after it. With an RTOS, osal's calls land on a fake kernel in the same
   file, so most behaviour can be tested without hardware. If a change cannot
   be covered by a test, say why in the pull request.
2. Run the tests:

   ```bash
   python test/run_tests.py
   ```

   This builds and runs the suite four times, once with the shipped
   `osal_config.h` and once for each RTOS.

3. Match the existing code style. The short version: 4 spaces and no tabs, Allman
   braces, `snake_case`, every name prefixed with `osal_`, a short Doxygen block
   on every function and a comment on each important step inside it, section
   banners at 103 columns, and comments in plain 7-bit ASCII with no em dashes.
   A `.clang-format` in the project root handles the mechanical parts:

   ```bash
   clang-format -i src/*.h
   ```

4. Keep every header in `src/` valid C++ as well as C, since C++ code includes
   them too.

5. A change to what a function does goes into all four ports, so the API stays
   the same on every RTOS. A new RTOS is a new port header beside the others, a
   new `OSAL_RTOS_` value in `osal.h` and `osal_config.h`, one line in the port
   choice at the bottom of `osal.h`, and its own build in the tests and CI.

## What CI checks

Every pull request runs two jobs, and both must be green:

- the full test suite on Ubuntu, in all four builds, with `-Wall -Wextra -Wpedantic -Werror`
- a cross compile for Cortex-M4 with the same warning settings, once per build

Warnings are errors, so a build that warns will not merge.

## Questions

Open an issue, or reach me at nima.askari@gmail.com.
