# 🧩 osal

[![CI](https://github.com/nimaltd/osal/actions/workflows/ci.yml/badge.svg)](https://github.com/nimaltd/osal/actions/workflows/ci.yml)
[![Stars](https://img.shields.io/github/stars/nimaltd/osal?style=social)](https://github.com/nimaltd/osal)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE.md)

OS abstraction layer for STM32: the same tasks, mutexes, critical sections, delays and heap on bare metal, FreeRTOS and ThreadX.

Code written against osal does not care which RTOS the project runs, or whether it runs one at all. You choose that with one line in `osal_config.h`. The NimaLTD libraries use it for exactly this, so a driver works the same in a bare metal project and in an RTOS one.

The library is headers only, so there is nothing to compile. You include `osal.h`, and it includes the one port header for your RTOS, so the others are never built.

---

## ✨ What you get

- Tasks, mutexes, critical sections, delays and a heap, the same on every RTOS
- Tasks you create, delete, suspend and resume, with one priority scale: 0 is the lowest, and `OSAL_TASK_PRIORITY_MAX` the highest your RTOS has
- A critical section that works in a task, in an interrupt and before the RTOS starts, and nests
- `osal_malloc()` and `osal_free()` on the RTOS's own heap: FreeRTOS's, ThreadX's byte pool, or the C library's on bare metal
- A pause for a loop that waits on hardware: one tick's sleep with an RTOS, so every other thread runs, and nothing at all without one
- A yield for a loop that keeps the CPU busy itself: the RTOS's own, and nothing without one
- A delay that lets other threads run with an RTOS, and is `HAL_Delay()` without one
- Mutexes with priority inheritance, recursive on every RTOS
- The same behaviour on every port, down to a mutex created twice, taken twice, or asked for from an interrupt
- Unit tested on every commit, once for each RTOS setting

---

## 📁 Layout

```
src/    osal.h            the one you include: it checks your setting and picks the port
        osal_config.h     your setting
        osal_none.h       the port for bare metal
        osal_cmsis_v1.h   the port for FreeRTOS through CMSIS-RTOS v1
        osal_cmsis_v2.h   the port for FreeRTOS through CMSIS-RTOS v2
        osal_threadx.h    the port for ThreadX
test/   host unit tests, run on a PC
```

Each port holds the whole code for one RTOS, with no `#if` inside it, so reading
the one your project uses tells you everything it does. A new RTOS is a new port
header, a new `OSAL_RTOS_` value and one line in `osal.h`.

Installed into a project, the code keeps its `src/` folder, with your `osal_config.h`
beside `osal.h`, and the README, changelog and licence files around it. There is no
`test/`: the section below about the tests refers to this repository, not to an
installed copy.

---

## ⚙️ Installing it

[stm32-installer](https://github.com/nimaltd/stm32-installer) copies the library into your project, `osal_config.h` included, and adds it to your CMake, STM32CubeIDE, Keil, IAR or Makefile project for you. Your project file is backed up first.

Install it once per machine:

```bash
pip install stm32-installer
```

Then, from the root of your STM32 project:

```bash
stm32-installer nimaltd/osal
```

### From a downloaded zip

Downloaded this repository with **Code**, **Download ZIP**? Give the installer the zip in place of `nimaltd/osal`, with no need to unpack it:

```bash
stm32-installer D:/Downloads/osal-main.zip
```

Only the files the library needs are copied into your project, and the zip is left alone. An unpacked folder works the same way. [stm32-installer's README](https://github.com/nimaltd/stm32-installer#installing-a-library) has every option, and how to install on a machine with no internet at all.

### Updating, and pinning a version

Run the same command again. Every file is replaced, and your setting in `osal_config.h` is kept: whatever is between `USER CODE BEGIN OSAL_CONFIGURATION` and `USER CODE END OSAL_CONFIGURATION`. Anything you changed outside those lines is saved to a `.bak` file first. This needs stm32-installer 1.9.0 or newer. An older one cannot find the library: update it with `pip install --upgrade stm32-installer`.

By default you get the newest code on `main`. To hold a project on one release, add `--ref` with a tag, a branch or a commit:

```bash
stm32-installer nimaltd/osal --ref v1.2.0
```

### Or copy the files in by hand

1. Copy `src/osal.h` and the four port headers, `osal_none.h`, `osal_cmsis_v1.h`, `osal_cmsis_v2.h` and `osal_threadx.h`, into your project's `Core/Inc`
2. Copy `src/osal_config.h` into `Core/Inc`

Only the port your setting names is ever compiled, but keep all four, so changing the setting later needs no other file. There is no `.c` file. Keep your setting between the `USER CODE BEGIN` and `USER CODE END` lines of `osal_config.h`. Installing later with stm32-installer keeps what is between them and replaces the rest.

### Or add the whole repository to a CMake build

If you keep this repository as a submodule rather than installing it:

```cmake
add_subdirectory(osal)
target_link_libraries(${CMAKE_PROJECT_NAME} nimaltd::osal)
```

The target is INTERFACE, since there is nothing to compile, so `osal.h` sees your application's include paths and finds `main.h` and the RTOS headers on its own. There is no `PRIVATE` on purpose: CubeMX links your application without one, and CMake refuses to mix the two forms on one target. The settings come from `osal/src/osal_config.h`, beside `osal.h`.

---

## 🔧 Configuration

Everything lives in your `osal_config.h`, between its `USER CODE` lines, which every install keeps. There is one setting, the RTOS your project runs:

```c
#define OSAL_RTOS           OSAL_RTOS_NONE
```

| Value | For | Port |
|---|---|---|
| `OSAL_RTOS_NONE` | Bare metal, no RTOS | `osal_none.h` |
| `OSAL_RTOS_CMSIS_V1` | FreeRTOS through CMSIS-RTOS v1, `cmsis_os.h` | `osal_cmsis_v1.h` |
| `OSAL_RTOS_CMSIS_V2` | FreeRTOS through CMSIS-RTOS v2, `cmsis_os2.h` | `osal_cmsis_v2.h` |
| `OSAL_RTOS_THREADX` | ThreadX, `tx_api.h` | `osal_threadx.h` |

Always include `osal.h`, never a port header. A port included on its own, or one that is not the port your setting names, stops the build with an error that says so.

In CubeMX, FreeRTOS asks which CMSIS-RTOS interface to use when you enable it. Pick the same one here.

---

## 🚀 Getting started

```c
#include "osal.h"

osal_mutex_t uart_mutex;

void app_init(void)
{
    osal_mutex_create(&uart_mutex);
}

void log_line(const char *text)
{
    if (osal_mutex_lock(&uart_mutex, 100) == OSAL_ERR_NONE)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)text, strlen(text), 100);
        osal_mutex_unlock(&uart_mutex);
    }

    osal_delay_ms(10);
}
```

With an RTOS, two threads calling `log_line()` take turns at the UART, and the delay lets other threads run. Without one, the mutex costs nothing and the delay is `HAL_Delay()`.

### Loops: osal_pause() and osal_yield()

A loop that asks a chip or a DMA again and again until it has finished calls `osal_pause()` between the questions:

```c
while (!chip_ready())
{
    osal_pause();
}
```

With an RTOS it sleeps one tick, 1 ms at the usual 1 kHz, so every other thread runs meanwhile, whatever its priority. Without one there is nothing else to run, so it returns at once and the loop asks again straight away: a 1 ms sleep there would only make a page that takes 3.2 ms to store finish at 4 ms instead.

A loop that keeps the CPU busy itself, such as a read in pieces by polling, calls `osal_yield()` between the pieces. With an RTOS that is the RTOS's own yield: the threads of the same priority get a turn, and when none is waiting it costs a few microseconds, where a pause would add half a tick to every piece. It never lets a thread of lower priority run, which is why a loop that waits on hardware pauses instead: yielding through a chip erase of minutes would starve every thread below it. Without an RTOS it does nothing. The NimaLTD drivers use both this way.

### Tasks

```c
static osal_task_t led_task;

static void led_blink(void *argument)
{
    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        osal_delay_ms(500);
    }
}

void app_start(void)
{
    osal_task_create(&led_task, "led", led_blink, NULL, 512, 1);
}
```

The stack size is in bytes on every RTOS, and the stack and control block come from the RTOS's heap. The priority is a number from 0, the lowest, to `OSAL_TASK_PRIORITY_MAX`, the highest your RTOS has: 55 with CMSIS-RTOS v1 and CubeMX's 56 priorities, 6 with 7, 54 with CMSIS-RTOS v2, and 31 with ThreadX's 32. ThreadX counts the other way, 0 its highest, and osal turns the number round for you. `OSAL_TASK_STACK_MIN` is the smallest stack the RTOS takes: 512 bytes with CubeMX's FreeRTOS, 200 with ThreadX.

Keep the `osal_task_t` static or global: it lasts as long as the task does. A task's function may return, and the task then stops for good. `osal_task_suspend()` and `osal_task_resume()` pause a task and let it go on, and a task may suspend itself. `osal_task_delete()` from another task ends the task at once and gives its memory back.

Bare metal has no task to run, so `osal_task_create()` returns `OSAL_ERR_TASK` there.

### Critical sections

```c
uint32_t state = osal_critical_enter();
counter++;
osal_critical_exit(state);
```

Nothing else runs between the two, not even an interrupt that touches `counter`. Give `osal_critical_exit()` what `osal_critical_enter()` returned: sections nest, and each exit puts back what its enter found. It works in a task, in an interrupt and before the RTOS starts. Keep it short, since every interrupt waits for it.

It is the RTOS's own: FreeRTOS's `taskENTER_CRITICAL()`, ThreadX's `tx_interrupt_control()`, and `PRIMASK` on bare metal. FreeRTOS masks the interrupts up to `configMAX_SYSCALL_INTERRUPT_PRIORITY`, as it always does: those above it never call the RTOS, and never call osal either.

### The heap

`osal_malloc()` and `osal_free()` take memory from the RTOS's heap and give it back. With FreeRTOS that is its own heap, `configTOTAL_HEAP_SIZE` bytes. ThreadX has no heap, only byte pools, so osal uses the first pool created, which in a CubeMX project is CubeMX's own Tx App memory pool of `TX_APP_MEM_POOL_SIZE` bytes. Make it big enough for the stacks of osal's tasks too. On bare metal it is the C library's `malloc()`. `osal_malloc(0)` gives `NULL` everywhere, and nothing waits for memory: when there is not enough, it gives `NULL` at once.

### With an RTOS, start it first

With an RTOS, call osal, and every library built on it, only from a task, once the RTOS has started. Not in `main()` before `osKernelStart()` or `MX_ThreadX_Init()`: the RTOS cannot lock or sleep yet, and FreeRTOS can crash if asked to. The exceptions are `osal_task_create()`, `osal_malloc()`, `osal_free()` and the critical section, which also work where the RTOS creates its first tasks: in `MX_FREERTOS_Init()`, or in `App_ThreadX_Init()`.

So work you would do once at power up, such as reading settings from an EEPROM, goes at the start of your first thread instead of in `main()`:

```c
void StartDefaultTask(void *argument)
{
    ee24_init(&eeprom, &hi2c1, EE24_ADDRESS_DEFAULT, 256, NULL, 0);
    ee24_read(&eeprom, 0, (uint8_t *)&settings, sizeof(settings));

    for (;;)
    {
        /* ... */
    }
}
```

Bare metal has no such rule: with `OSAL_RTOS_NONE` there is no RTOS to start first.

### Things to know

- **Every port behaves the same.** Where the RTOSes differ, osal decides, so code moves between them, and to bare metal, unchanged.
- **From an interrupt nothing waits.** `osal_mutex_create()` and `osal_mutex_lock()` return `OSAL_ERR_MUTEX`, `osal_mutex_unlock()` does nothing, and `osal_delay_ms()`, `osal_pause()` and `osal_yield()` return at once. Every task call returns `OSAL_ERR_TASK`, `osal_malloc()` gives `NULL` and `osal_free()` does nothing, so give memory back from a task. On bare metal too, where a mutex taken in an interrupt would protect nothing. A critical section works in an interrupt on every port.
- **Inside a critical section, call only `osal_critical_enter()` and `osal_critical_exit()`.** osal tells an interrupt from the CPU's exception state alone, as ST's CMSIS-RTOS v2 layer does, and a task inside a critical section does not set that. So a mutex lock or a delay there is not refused, and cannot work either: with interrupts masked no task can switch and the tick stops. A delay returns at once and the task only sleeps when the critical section ends, a lock on a mutex another task holds can hang the CPU, and bare metal's `HAL_Delay()` never ends. Take the mutex and wait outside the critical section.
- **A task that deletes itself, or returns, stops for good and keeps its memory.** Another task's `osal_task_delete()` gives it back, and the same `osal_task_t` can then be created again. FreeRTOS would free it later on its own, but ThreadX cannot free a task from inside it, so on every port it waits for another task. `osal_task_create()` on an `osal_task_t` that still has a task returns `OSAL_ERR_TASK`, since the task it has would be lost.
- **Never delete a task that holds a mutex.** ThreadX gives the mutex back, and FreeRTOS keeps it taken for ever.
- **Suspend a task where it waits for nothing, or let it suspend itself.** A task suspended while it waits in `osal_delay_ms()` or for a mutex is where the RTOSes differ: FreeRTOS suspends it at once and ends the delay when it is resumed, and ThreadX lets the wait finish first.
- **Tasks of the same priority take turns every tick**, as FreeRTOS's do by default. With ThreadX osal gives each task a time slice of one tick and no preemption threshold, which FreeRTOS does not have.
- **A thread may take a mutex it already holds.** Every port counts it, and the thread gives it back once for each time it took it. With FreeRTOS that needs `configUSE_RECURSIVE_MUTEXES 1`, CubeMX's default, and osal stops the build with a message where it is off.
- **A second `osal_mutex_create()` on the same mutex gives a new one.** With FreeRTOS the first one's memory is not given back, so call it once per mutex.
- **`HAL_MAX_DELAY` waits for ever.** A mutex lock with it waits until the mutex is free, and `osal_delay_ms(HAL_MAX_DELAY)` never returns.
- **Every other time becomes ticks the RTOS's own way.** CMSIS-RTOS v1 takes milliseconds and turns them into ticks itself. Through CMSIS-RTOS v2, osal uses FreeRTOS's `pdMS_TO_TICKS()`, so one your `FreeRTOSConfig.h` defines its own way is the one used. ThreadX has no such macro, only `TX_TIMER_TICKS_PER_SECOND`, so osal does the same sum with it. Each drops what is left of a tick: on a 100 Hz tick, 25 ms waits 20. On the usual 1 kHz tick nothing changes, and a time under one tick still waits one, never none.
- **With CMSIS-RTOS a mutex and a task come from the FreeRTOS heap.** `OSAL_ERR_MUTEX` from `osal_mutex_create()`, or `OSAL_ERR_TASK` from `osal_task_create()`, almost always means the heap is too small. osal needs `configSUPPORT_DYNAMIC_ALLOCATION 1`, CubeMX's default, and stops the build with a message where it is off. With ThreadX a mutex and a task's control block live inside `osal_mutex_t` and `osal_task_t`, so make those static or global, and a task's stack comes from the byte pool.

---

## 🧰 API

| Function | What it does |
|---|---|
| `void osal_yield(void)` | Let the tasks of the same priority run, back at once if none waits. Nothing without an RTOS |
| `void osal_pause(void)` | Pause a loop that waits on hardware: one tick's sleep with an RTOS, nothing without one |
| `uint32_t osal_critical_enter(void)` | Mask the interrupts. Returns what to give `osal_critical_exit()` |
| `void osal_critical_exit(uint32_t state)` | Put the interrupts back as `osal_critical_enter()` found them |
| `osal_err_t osal_mutex_create(osal_mutex_t *mutex)` | Create a mutex. Call it once per mutex |
| `osal_err_t osal_mutex_lock(osal_mutex_t *mutex, uint32_t timeout_ms)` | Take it, waiting up to `timeout_ms` for another task to give it back |
| `void osal_mutex_unlock(osal_mutex_t *mutex)` | Give it back |
| `void osal_delay_ms(uint32_t ms)` | Wait `ms` milliseconds. 0 returns at once, and `HAL_MAX_DELAY` never returns |
| `void *osal_malloc(size_t size)` | Take `size` bytes from the RTOS's heap. `NULL` when there is not enough |
| `void osal_free(void *memory)` | Give memory from `osal_malloc()` back. `NULL` does nothing |
| `osal_err_t osal_task_create(osal_task_t *task, const char *name, osal_task_func_t function, void *argument, uint32_t stack_size, uint32_t priority)` | Create a task running `function(argument)`, and start it. `stack_size` in bytes, `priority` from 0 to `OSAL_TASK_PRIORITY_MAX` |
| `osal_err_t osal_task_delete(osal_task_t *task)` | Delete a task. One that deletes itself stops, and keeps its memory until another task deletes it |
| `osal_err_t osal_task_suspend(osal_task_t *task)` | Suspend a task until it is resumed. A task may suspend itself |
| `osal_err_t osal_task_resume(osal_task_t *task)` | Resume a suspended task. Any other is left as it is |

| Macro | What it is |
|---|---|
| `OSAL_TASK_PRIORITY_MAX` | The highest task priority, from the RTOS. 0 is the lowest on every RTOS |
| `OSAL_TASK_STACK_MIN` | The smallest stack in bytes the RTOS takes |

| Error | Means |
|---|---|
| `OSAL_ERR_NONE` | Done |
| `OSAL_ERR_INVALID` | A `NULL` mutex or task, a task with no function, a priority above `OSAL_TASK_PRIORITY_MAX` or a stack below `OSAL_TASK_STACK_MIN` |
| `OSAL_ERR_TIMEOUT` | Another task kept the mutex for the whole wait |
| `OSAL_ERR_MUTEX` | The RTOS could not create or take the mutex, such as for lack of heap or from an interrupt |
| `OSAL_ERR_TASK` | The RTOS could not create the task or act on it, such as for lack of heap, from an interrupt, a task that does not exist, or bare metal |

---

## 🧪 Running the tests

The tests run on your PC, not on hardware. With an RTOS, osal's calls land on a fake kernel the tests control, and time is faked, so every wait is tested instantly. You need cmake and any C compiler, nothing else: [Unity](https://github.com/ThrowTheSwitch/Unity) is vendored into `test/unity/`, so there is nothing to install.

One command does everything:

```bash
python test/run_tests.py
```

It configures, builds and runs the suite, then tells you plainly whether it passed. The suite is built five times: once with the shipped `osal_config.h`, once for each RTOS, and once more for CMSIS-RTOS v1 as CubeMX most often sets it up, with no static allocation and 7 priorities. Add `--clean` to start from an empty build folder.

If you prefer doing it by hand:

```bash
cmake -S . -B build -DOSAL_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 🤝 Contributing

Bug reports and pull requests are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for the style rules and how to run the tests. Nothing to sign, just open a pull request.

---

## 💖 Support

I write these libraries in my own time and give them away, because good tools should be easy to get. If this one saved you an afternoon, there are two things that genuinely help:

**⭐ Star the repo.** It costs you one click, it helps other engineers find the library, and it is the main reason I keep going.

**☕ [Buy me a coffee on Ko-fi](https://ko-fi.com/nimaltd).** Any amount is a real motivation to keep writing, documenting and maintaining this work.

[![GitHub](https://img.shields.io/badge/GitHub-Follow-black?style=for-the-badge&logo=github)](https://github.com/NimaLTD)
[![YouTube](https://img.shields.io/badge/YouTube-Subscribe-red?style=for-the-badge&logo=youtube)](https://youtube.com/@nimaltd)
[![Instagram](https://img.shields.io/badge/Instagram-Follow-purple?style=for-the-badge&logo=instagram)](https://instagram.com/github.nimaltd)
[![LinkedIn](https://img.shields.io/badge/LinkedIn-Connect-blue?style=for-the-badge&logo=linkedin)](https://linkedin.com/in/nimaltd)
[![Email](https://img.shields.io/badge/Email-Contact-red?style=for-the-badge&logo=gmail)](mailto:nima.askari@gmail.com)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-Support-orange?style=for-the-badge&logo=ko-fi)](https://ko-fi.com/nimaltd)

---

## 📜 License

Apache License 2.0. See [LICENSE.md](LICENSE.md).

You are free to use this in commercial and closed source products. What the license asks in return is that you keep the copyright notice and pass along the [NOTICE](NOTICE) file, so the credit travels with the code.

The test folder vendors [Unity](https://github.com/ThrowTheSwitch/Unity) under its own MIT license, kept in [test/unity/LICENSE.txt](test/unity/LICENSE.txt). It is only used for testing and is not part of what you flash to a device.
