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

### Changed

- Milliseconds become ticks the RTOS's own way. Through CMSIS-RTOS v2 that
  is FreeRTOS's `pdMS_TO_TICKS()`, so one a project defines in
  `FreeRTOSConfig.h` is used, and `osal_cmsis_v2.h` now includes
  `FreeRTOS.h`. ThreadX has no such macro, so osal does the same sum with
  `TX_TIMER_TICKS_PER_SECOND`. What is left of a tick is dropped, where 1.0.0
  rounded up: on a 100 Hz tick 25 ms now waits 20, not 30. On the usual 1 kHz
  tick nothing changes, and a time under one tick still waits one. CMSIS-RTOS
  v1 is given milliseconds as before.
- `osal_delay_ms(HAL_MAX_DELAY)` waits for ever, as `HAL_MAX_DELAY` does for
  a mutex, and never returns. In 1.0.0 it ended after about 49 days.

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
