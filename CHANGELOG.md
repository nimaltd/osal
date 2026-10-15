# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-10-15

### Added

- Tasks: `osal_task_create()`, `osal_task_delete()`, `osal_task_suspend()`
  and `osal_task_resume()`, with `osal_task_t` and the task function type
  `osal_task_func_t`. The stack size is in bytes on every RTOS, and the
  stack and control block come from the RTOS's heap. The priority runs from
  0, the lowest, to `OSAL_TASK_PRIORITY_MAX`, which comes from the RTOS:
  `configMAX_PRIORITIES - 1` with CMSIS-RTOS v1, `osPriorityRealtime7` with
  CMSIS-RTOS v2, `TX_MAX_PRIORITIES - 1` with ThreadX, whose order osal
  turns round. `OSAL_TASK_STACK_MIN` is the RTOS's smallest stack. Bare
  metal has no task, and says so with `OSAL_ERR_TASK`.
- Every task behaves the same on every port:
  - A task whose function returns stops for good, where FreeRTOS would
    stop the CPU. A task that deletes itself stops the same way. Either
    keeps its memory until another task deletes it, since ThreadX cannot
    free a task from inside it.
  - A second `osal_task_create()` on a task that exists is refused, so the
    task is never lost.
  - Tasks of the same priority take turns every tick, as FreeRTOS's do: on
    ThreadX each has a one tick time slice and no preemption threshold.
  - With FreeRTOS the scheduler is held while a task is created, so a new
    task of higher priority finds its `osal_task_t` filled in.
- A critical section: `osal_critical_enter()` and `osal_critical_exit()`,
  the RTOS's own. It works in a task, in an interrupt and before the RTOS
  starts, and nests. Before the start FreeRTOS's own would leave the
  interrupts masked until the RTOS starts, so osal keeps and puts back the
  mask there, as every other port does.
- A heap: `osal_malloc()` and `osal_free()`. FreeRTOS's heap, the first
  ThreadX byte pool created, which in a CubeMX project is its own, or the C
  library's on bare metal. 0 bytes gives `NULL`, and nothing waits for
  memory.
- `OSAL_ERR_TASK`.
- With FreeRTOS, osal stops the build unless `configSUPPORT_DYNAMIC_ALLOCATION`
  is 1, CubeMX's default. Mutexes always needed it.
- A test build of CMSIS-RTOS v1 as CubeMX most often sets it up, with no
  static allocation and 7 priorities.

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
- Every port behaves the same where the RTOSes differ:
  - A thread may take a mutex it already holds, and gives it back as often.
    With FreeRTOS the mutex is now its recursive one, which needs
    `configUSE_RECURSIVE_MUTEXES 1`, CubeMX's default. osal stops the build
    where it is off. ThreadX mutexes always were recursive.
  - From an interrupt, creating or taking a mutex returns `OSAL_ERR_MUTEX`,
    giving one back does nothing, and no delay, pause or yield waits, bare
    metal included. FreeRTOS's CMSIS-RTOS v1 layer took a mutex in an
    interrupt and slept in one.
  - A second `osal_mutex_create()` on the same mutex gives a new mutex.
    ThreadX refused it, and now the old one is deleted first.

### Fixed

- With CMSIS-RTOS v1, a mutex another thread kept for the whole wait was
  reported as `OSAL_ERR_MUTEX`. ST's layer says `osErrorOS` for it, which osal
  read as a refusal. It is `OSAL_ERR_TIMEOUT` now, as on every other port.

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
