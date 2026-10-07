# 🧩 osal

[![CI](https://github.com/nimaltd/osal/actions/workflows/ci.yml/badge.svg)](https://github.com/nimaltd/osal/actions/workflows/ci.yml)
[![Stars](https://img.shields.io/github/stars/nimaltd/osal?style=social)](https://github.com/nimaltd/osal)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE.md)

OS abstraction layer for STM32: the same mutex and delay on bare metal, FreeRTOS and ThreadX.

Code written against osal does not care which RTOS the project runs, or whether it runs one at all. You choose that with one line in `osal_config.h`. The NimaLTD libraries use it for exactly this, so a driver works the same in a bare metal project and in an RTOS one.

The library is headers only, so there is nothing to compile. You include `osal.h`, and it includes the one port header for your RTOS, so the others are never built.

---

## ✨ What you get

- `osal_mutex_create()`, `osal_mutex_lock()`, `osal_mutex_unlock()` and `osal_delay_ms()`, the same on every RTOS
- A delay that lets other threads run with an RTOS, and is `HAL_Delay()` without one
- Mutexes with priority inheritance
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

Run the same command again. Every file is replaced, and your setting in `osal_config.h` is kept: whatever is between `USER CODE BEGIN OSAL_CONFIGURATION` and `USER CODE END OSAL_CONFIGURATION`. Anything you changed outside those lines is saved to a `.bak` file first. This needs stm32-installer 1.7.1 or newer, and an older one says to update.

By default you get the newest code on `main`. To hold a project on one release, add `--ref` with a tag, a branch or a commit:

```bash
stm32-installer nimaltd/osal --ref v1.0.0
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

### With an RTOS, start it first

With an RTOS, call osal, and every library built on it, only from a thread, once the RTOS has started. Not in `main()` before `osKernelStart()` or `MX_ThreadX_Init()`: the RTOS cannot lock or sleep yet, and FreeRTOS can crash if asked to.

So work you would do once at power up, such as reading settings from an EEPROM, goes at the start of your first thread instead of in `main()`:

```c
void StartDefaultTask(void *argument)
{
    ee24_init(&eeprom, &hi2c1, EE24_ADDRESS_DEFAULT, 256, NULL, 0);
    ee24_read(&eeprom, 0, (uint8_t *)&settings, sizeof(settings), 100);

    for (;;)
    {
        /* ... */
    }
}
```

Bare metal has no such rule: with `OSAL_RTOS_NONE`, call it from anywhere but an interrupt.

### Things to know

- **Not from an interrupt.** A mutex cannot be taken there, and a delay would block it.
- **Do not take a mutex twice from one thread.** FreeRTOS then waits for ever, while ThreadX counts it, so code that relies on either does not move to the other.
- **A timeout of `HAL_MAX_DELAY` waits for ever.** Any other timeout is rounded up to whole RTOS ticks, so a short wait never becomes no wait at all.
- **With CMSIS-RTOS the mutex comes from the FreeRTOS heap.** `OSAL_ERR_MUTEX` from `osal_mutex_create()` almost always means the heap is too small. With ThreadX it lives inside `osal_mutex_t`, so make that static or global.

---

## 🧰 API

| Function | What it does |
|---|---|
| `osal_err_t osal_mutex_create(osal_mutex_t *mutex)` | Create a mutex. Call it once per mutex |
| `osal_err_t osal_mutex_lock(osal_mutex_t *mutex, uint32_t timeout_ms)` | Take it, waiting up to `timeout_ms` for another thread to give it back |
| `void osal_mutex_unlock(osal_mutex_t *mutex)` | Give it back |
| `void osal_delay_ms(uint32_t ms)` | Wait at least `ms` milliseconds. 0 returns at once |

| Error | Means |
|---|---|
| `OSAL_ERR_NONE` | Done |
| `OSAL_ERR_INVALID` | A `NULL` mutex |
| `OSAL_ERR_TIMEOUT` | Another thread kept the mutex for the whole wait |
| `OSAL_ERR_MUTEX` | The RTOS could not create or take the mutex, such as for lack of heap or from an interrupt |

---

## 🧪 Running the tests

The tests run on your PC, not on hardware. With an RTOS, osal's calls land on a fake kernel the tests control, and time is faked, so every wait is tested instantly. You need cmake and any C compiler, nothing else: [Unity](https://github.com/ThrowTheSwitch/Unity) is vendored into `test/unity/`, so there is nothing to install.

One command does everything:

```bash
python test/run_tests.py
```

It configures, builds and runs the suite, then tells you plainly whether it passed. The suite is built four times, once with the shipped `osal_config.h` and once for each RTOS. Add `--clean` to start from an empty build folder.

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
