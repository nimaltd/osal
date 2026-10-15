# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-10-15

### Added

- `osal_yield()`, the first function in every port: with an RTOS the RTOS's
  own yield, so the threads of the same priority get a turn in a loop that
  keeps the CPU busy, and without one nothing.
- `osal_pause()`, for a loop that waits on hardware: with an RTOS it sleeps
  one tick, so every other thread runs whatever its priority, and without
  one it returns at once, since nothing else could run and a sleep would
  only make the wait end later.

### Fixed

- With CMSIS-RTOS v1, a delay or a mutex wait could be shorter than asked
  on a tick slower than 1 ms. FreeRTOS's v1 layer divides by the tick period
  and drops the rest, so 15 ms on a 10 ms tick slept 10. osal now rounds up
  to whole ticks first, as the other ports always did. On the usual 1 kHz
  tick nothing changes.

## [1.0.0] - 2026-10-01

### Added

- The first release: a mutex and a delay, the same on bare metal, FreeRTOS
  through CMSIS-RTOS v1 or v2, and ThreadX, chosen by `OSAL_RTOS` in
  `osal_config.h`. The library is headers only: `osal.h` checks the
  setting and includes the one port header for that RTOS, `osal_none.h`,
  `osal_cmsis_v1.h`, `osal_cmsis_v2.h` or `osal_threadx.h`.
- `osal_mutex_create()`, `osal_mutex_lock()` and `osal_mutex_unlock()`, with
  priority inheritance, and `osal_delay_ms()`.
- With an RTOS, it is called only from a thread once the RTOS has started,
  and the README says how to move power up work into the first thread.
- Host unit tests, run with `python test/run_tests.py`.
- `osal_config.h` is installed like the other files, and stm32-installer keeps
  what is between its `USER CODE` lines.
- CMake build, and an `installer.yml` for installing with stm32-installer 1.9.0
  or newer.
