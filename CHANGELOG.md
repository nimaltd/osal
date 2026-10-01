# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-10-01

### Added

- The first release: a mutex and a delay, the same on bare metal, FreeRTOS
  through CMSIS-RTOS v1 or v2, and ThreadX, chosen by `OSAL_RTOS` in
  `osal_config.h`. The whole library is one header.
- `osal_mutex_create()`, `osal_mutex_lock()` and `osal_mutex_unlock()`, with
  priority inheritance, and `osal_delay_ms()`.
- With an RTOS, it is called only from a thread once the RTOS has started,
  and the README says how to move power up work into the first thread.
- Host unit tests, run with `python test/run_tests.py`.
- CMake build, and a `library.yml` for installing with stm32-installer.
