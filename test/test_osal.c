/**
 * @file        test_osal.c
 * @brief       Host unit tests for the osal library, built on Unity.
 * @version     1.2.0
 *
 * @author      Nima Askari (NimaLTD)
 * @email       nima.askari@gmail.com
 * @github      https://www.github.com/nimaltd
 * @linkedin    https://www.linkedin.com/in/nimaltd
 * @youtube     https://www.youtube.com/@nimaltd
 * @instagram   https://instagram.com/github.nimaltd
 *
 * @copyright   (c) 2026 Nima Askari (NimaLTD)
 *              SPDX-License-Identifier: Apache-2.0
 *              See LICENSE.md in the project root for the full license text.
 *
 * @note        Runs on a PC, not on hardware. The suite is built once per RTOS
 *              setting, and with an RTOS its calls land on a fake kernel the
 *              tests control. Time is a clock the tests move, so every wait is
 *              checked instantly. A task's function is run by the tests, the
 *              way the RTOS would start it.
 */

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "osal.h"

/*
 * ****************************************************************************************************
 * Macros
 * ****************************************************************************************************
*/

/* A task's lowest and highest priority, and a stack of b bytes, each in the
   RTOS's own terms, as the fake kernel records them. */
#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
#define NATIVE_LOWEST       0U
#define NATIVE_HIGHEST      ((uint32_t)configMAX_PRIORITIES - 1U)
#define NATIVE_STACK(b)     ((b) / 4U)
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
#define NATIVE_LOWEST       ((uint32_t)osPriorityIdle)
#define NATIVE_HIGHEST      ((uint32_t)osPriorityRealtime7)
#define NATIVE_STACK(b)     (b)
#elif OSAL_RTOS == OSAL_RTOS_THREADX
#define NATIVE_LOWEST       ((uint32_t)TX_MAX_PRIORITIES - 1U)
#define NATIVE_HIGHEST      0U
#define NATIVE_STACK(b)     (b)
#endif

/*
 * ****************************************************************************************************
 * Types
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief What the fake RTOS answers when osal asks for the mutex.
 */
typedef enum
{
    TAKE_OK      = 0, /**< Taken.                                     */
    TAKE_TIMEOUT = 1, /**< Another thread held it for the whole wait. */
    TAKE_BUSY    = 2, /**< Held elsewhere, and no wait was asked for. */
    TAKE_REFUSED = 3, /**< Refused outright, as from an interrupt.    */

} take_t;

/*****************************************************************************************************/
/**
 * @brief What the fake RTOS answers when osal deletes, suspends or resumes a task.
 */
typedef enum
{
    ANSWER_OK            = 0, /**< Done.                                              */
    ANSWER_REFUSED       = 1, /**< Refused, as for a task that does not exist.        */
    ANSWER_ENDED         = 2, /**< ThreadX: suspend of a task that has ended.         */
    ANSWER_NOT_SUSPENDED = 3, /**< ThreadX: resume of a task that was not suspended.  */
    ANSWER_LIFTED        = 4, /**< ThreadX: resume called off a suspend still to come. */

} answer_t;

/*
 * ****************************************************************************************************
 * Global variables
 * ****************************************************************************************************
*/

/* The clock, and what has been spent waiting on it. */
static uint32_t now_ms      = 0U;
static int      hal_delays  = 0;
static int      rtos_sleeps = 0;
static int      rtos_yields = 0;
static uint32_t last_sleep  = 0U;

/* A delay of HAL_MAX_DELAY never returns, so a test gets out of it here, after
   the number of sleeps it chose. 0 lets every sleep return. A ThreadX task
   that ends itself never returns either, and gets out the same way. */
static jmp_buf  escape;
static int      sleeps_to_escape = 0;

/* Everything the fake kernel caught osal doing wrong. */
static int        faults = 0;
static const char *fault = "none";

/* The exception the CPU is in, 0 in thread mode. */
static uint32_t ipsr = 0U;

/* The fake CPU's interrupt mask, 1 while masked. */
static uint32_t irq_mask = 0U;

/* The fake kernel. */
static uint32_t tick_hz            = 1000U;
static take_t   take_result        = TAKE_OK;
static bool     mutex_create_fails = false;
static int      mutex_creates      = 0;
static int      mutex_deletes      = 0;
static int      mutex_takes        = 0;
static int      mutex_gives        = 0;
static int      mutex_held         = 0;
static uint32_t last_wait          = 0U;

/* Whether the RTOS has started, FreeRTOS's count of how deep its own critical
   section is, and how often a task's critical section was entered. */
static bool     kernel_started = true;
static int      critical_depth = 0;
static int      counted_enters = 0;

/* The fake heap. */
static bool     heap_fails     = false;
static int      heap_takes     = 0;
static int      heap_gives     = 0;
static int      heap_held      = 0;
static size_t   last_heap_size = 0U;

/* The fake kernel's tasks. */
static bool        task_create_fails = false;
static answer_t    task_answer       = ANSWER_OK;
static int         task_creates      = 0;
static int         task_deletes      = 0;
static int         task_suspends     = 0;
static int         task_resumes      = 0;
static int         self_ends         = 0;
static int         scheduler_holds   = 0;
static bool        filled_at_create  = false;
static bool        held_at_create    = false;
static const char  *created_name     = NULL;
static uint32_t    created_stack     = 0U;
static uint32_t    created_priority  = 0U;
static void        *created_argument = NULL;

#if OSAL_RTOS != OSAL_RTOS_NONE
/* The task the CPU runs, the caller of osal. */
static void        *running          = NULL;
#endif

/* What a task's function saw. */
static int  body_runs     = 0;
static void *body_argument = NULL;
static int  marker        = 0;

static osal_mutex_t mutex;
static osal_task_t  task;

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V1) || (OSAL_RTOS == OSAL_RTOS_CMSIS_V2)
/* What a CMSIS mutex or thread handle points at. */
struct test_os_mutex
{
    int unused;
};
static struct test_os_mutex test_mutex_object;

struct test_os_thread
{
    int unused;
};
static struct test_os_thread test_thread_object;
static struct test_os_thread other_thread_object;
#endif

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
static os_pthread     created_entry = NULL;
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
static osThreadFunc_t created_entry = NULL;
#elif OSAL_RTOS == OSAL_RTOS_THREADX
/* ThreadX's list of byte pools starts here. */
TX_BYTE_POOL *_tx_byte_pool_created_ptr = NULL;

static TX_BYTE_POOL test_pool;
static TX_THREAD    other_thread;
static TX_THREAD    *created_thread = NULL;
#endif

/*
 * ****************************************************************************************************
 * Private function prototypes
 * ****************************************************************************************************
*/

static int    sleeps(void);
static void   escape_if_due(void);
static void   task_body(void *argument);
#if OSAL_RTOS != OSAL_RTOS_NONE
static void   note_fault(const char *what);
static take_t mutex_take(bool right_mutex, uint32_t wait);
static void   mutex_give(bool right_mutex);
static void   rtos_sleep(uint32_t ticks, uint32_t ms);
static void   *heap_take(size_t size);
static void   heap_give(void *memory);
static void   check_create(const osal_task_t *made);
static void   check_task(bool right_task);
static void   be_the_task(void);
static void   be_another_task(void);
static void   run_created_task(void);
static bool   stopped_for_good(void);
#endif

/*
 * ****************************************************************************************************
 * Public function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief The tick, from the tests' clock.
 *
 * @return Milliseconds since the clock was reset.
 */
uint32_t HAL_GetTick(void)
{
    return now_ms;
}

/*****************************************************************************************************/
/**
 * @brief The exception the tests say the CPU is in.
 *
 * @return 0 in thread mode, an exception number in an interrupt.
 */
uint32_t __get_IPSR(void)
{
    return ipsr;
}

/*****************************************************************************************************/
/**
 * @brief PRIMASK on the fake CPU.
 *
 * @return 1 while every interrupt is masked.
 */
uint32_t __get_PRIMASK(void)
{
    return irq_mask;
}

/*****************************************************************************************************/
/**
 * @brief Set PRIMASK on the fake CPU.
 *
 * @param[in] priMask  1 masks every interrupt, 0 lets them in.
 */
void __set_PRIMASK(uint32_t priMask)
{
    irq_mask = priMask;
}

/*****************************************************************************************************/
/**
 * @brief Mask every interrupt on the fake CPU.
 */
void __disable_irq(void)
{
    irq_mask = 1U;
}

/*****************************************************************************************************/
/**
 * @brief Wait, by moving the clock on.
 *
 * @param[in] Delay  Milliseconds.
 */
void HAL_Delay(uint32_t Delay)
{
#if OSAL_RTOS != OSAL_RTOS_NONE
    /* With an RTOS, spinning here would starve every other thread. */
    note_fault("spun in HAL_Delay() instead of sleeping on the RTOS");
#endif

    hal_delays++;
    last_sleep = Delay;
    now_ms    += Delay;
    escape_if_due();
}

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V1) || (OSAL_RTOS == OSAL_RTOS_CMSIS_V2)
/*****************************************************************************************************/
/**
 * @brief FreeRTOS's critical section for a task: masks, and counts how deep it is.
 */
void test_enter_critical(void)
{
    if (ipsr != 0U)
    {
        note_fault("FreeRTOS's task critical section was used in an interrupt, where it asserts");
    }

    counted_enters++;
    critical_depth++;
    irq_mask = 1U;
}

/*****************************************************************************************************/
/**
 * @brief Leave it. Unmasks at a count of 0, and never before the RTOS starts, as FreeRTOS does.
 */
void test_exit_critical(void)
{
    if (critical_depth == 0)
    {
        note_fault("left FreeRTOS's critical section without entering it");
    }
    else
    {
        critical_depth--;
    }

    if ((critical_depth == 0) && kernel_started)
    {
        irq_mask = 0U;
    }
}

/*****************************************************************************************************/
/**
 * @brief FreeRTOS's critical section for an interrupt: masks, and returns what was masked before.
 *
 * @return The mask as it was.
 */
UBaseType_t test_enter_critical_from_isr(void)
{
    UBaseType_t state = irq_mask;

    irq_mask = 1U;

    return state;
}

/*****************************************************************************************************/
/**
 * @brief Leave it, putting back what test_enter_critical_from_isr() returned.
 *
 * @param[in] state  The mask as it was.
 */
void test_exit_critical_from_isr(UBaseType_t state)
{
    irq_mask = (uint32_t)state;
}

/*****************************************************************************************************/
/**
 * @brief Take memory from the fake FreeRTOS heap.
 */
void *pvPortMalloc(size_t xWantedSize)
{
    return heap_take(xWantedSize);
}

/*****************************************************************************************************/
/**
 * @brief Give memory back to the fake FreeRTOS heap.
 */
void vPortFree(void *pv)
{
    heap_give(pv);
}
#endif

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
/*****************************************************************************************************/
/**
 * @brief Create the fake recursive mutex, unless the test says the heap is full.
 */
osMutexId osRecursiveMutexCreate(const osMutexDef_t *mutex_def)
{
    osMutexId id = NULL;

    (void)mutex_def;
    mutex_creates++;

    if (!mutex_create_fails)
    {
        id = &test_mutex_object;
    }

    return id;
}

/*****************************************************************************************************/
/**
 * @brief Take the fake recursive mutex, answering as the test chose, with the values ST's layer gives.
 *
 * ST's osRecursiveMutexWait() says osErrorOS for a mutex held the whole wait,
 * and osErrorParameter for one never created.
 */
osStatus osRecursiveMutexWait(osMutexId mutex_id, uint32_t millisec)
{
    osStatus status = osErrorOS;

    switch (mutex_take(mutex_id == &test_mutex_object, millisec))
    {
        case TAKE_OK:
            status = osOK;
            break;

        case TAKE_TIMEOUT:
        case TAKE_BUSY:
            status = osErrorOS;
            break;

        default:
            status = osErrorParameter;
            break;
    }

    return status;
}

/*****************************************************************************************************/
/**
 * @brief Give the fake recursive mutex back.
 */
osStatus osRecursiveMutexRelease(osMutexId mutex_id)
{
    mutex_give(mutex_id == &test_mutex_object);

    return osOK;
}

/*****************************************************************************************************/
/**
 * @brief Let the threads of the same priority run: counted, and no time passes.
 */
osStatus osThreadYield(void)
{
    rtos_yields++;

    return osOK;
}

/*****************************************************************************************************/
/**
 * @brief Sleep, in milliseconds as CMSIS-RTOS v1 counts, turned into ticks as FreeRTOS's layer does.
 *
 * The milliseconds are divided by the tick period, the rest dropped, and a
 * sleep of any length is at least one tick.
 */
osStatus osDelay(uint32_t millisec)
{
    uint32_t period = 1000U / tick_hz;
    uint32_t ticks  = millisec / period;

    if ((ticks == 0U) && (millisec > 0U))
    {
        ticks = 1U;
    }

    rtos_sleep(millisec, ticks * period);

    return osOK;
}

/*****************************************************************************************************/
/**
 * @brief Whether the fake kernel runs, as the test chose.
 */
int32_t osKernelRunning(void)
{
    return kernel_started ? 1 : 0;
}

/*****************************************************************************************************/
/**
 * @brief Create the fake thread, recording what ST's layer would hand FreeRTOS.
 */
osThreadId osThreadCreate(const osThreadDef_t *thread_def, void *argument)
{
    osThreadId id = NULL;

    task_creates++;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    if ((thread_def->buffer != NULL) || (thread_def->controlblock != NULL))
    {
        note_fault("memory was given, so the task did not come from the FreeRTOS heap");
    }
#endif

    check_create((const osal_task_t *)argument);

    /* ST's makeFreeRtosPriority(), and stacksize as it is. */
    created_entry    = thread_def->pthread;
    created_argument = argument;
    created_name     = thread_def->name;
    created_stack    = thread_def->stacksize;
    created_priority = (uint32_t)((int32_t)thread_def->tpriority - (int32_t)osPriorityIdle);

    if (!task_create_fails)
    {
        id = &test_thread_object;
    }

    return id;
}

/*****************************************************************************************************/
/**
 * @brief The task the tests say is running.
 */
osThreadId osThreadGetId(void)
{
    return (osThreadId)running;
}

/*****************************************************************************************************/
/**
 * @brief Delete the fake thread, answering as the test chose.
 */
osStatus osThreadTerminate(osThreadId thread_id)
{
    task_deletes++;
    check_task(thread_id == &test_thread_object);

    if (thread_id == (osThreadId)running)
    {
        note_fault("a task deleted itself through FreeRTOS, which frees it later, where ThreadX cannot");
    }

    return (task_answer == ANSWER_OK) ? osOK : osErrorOS;
}

/*****************************************************************************************************/
/**
 * @brief Suspend the fake thread, answering as the test chose.
 */
osStatus osThreadSuspend(osThreadId thread_id)
{
    task_suspends++;
    check_task(thread_id == &test_thread_object);

    return (task_answer == ANSWER_OK) ? osOK : osErrorResource;
}

/*****************************************************************************************************/
/**
 * @brief Resume the fake thread, answering as the test chose.
 */
osStatus osThreadResume(osThreadId thread_id)
{
    task_resumes++;
    check_task(thread_id == &test_thread_object);

    return (task_answer == ANSWER_OK) ? osOK : osErrorResource;
}

/*****************************************************************************************************/
/**
 * @brief Hold the fake scheduler. FreeRTOS counts how often.
 */
osStatus osThreadSuspendAll(void)
{
    scheduler_holds++;

    return osOK;
}

/*****************************************************************************************************/
/**
 * @brief Let the fake scheduler go once.
 */
osStatus osThreadResumeAll(void)
{
    if (scheduler_holds == 0)
    {
        note_fault("let go a scheduler that was not held");
    }
    else
    {
        scheduler_holds--;
    }

    return osOK;
}
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
/*****************************************************************************************************/
/**
 * @brief The fake kernel's tick rate, FreeRTOS's configTICK_RATE_HZ, which the tests can change.
 */
uint32_t test_tick_rate(void)
{
    return tick_hz;
}

/*****************************************************************************************************/
/**
 * @brief Create the fake mutex, unless the test says the heap is full.
 */
osMutexId_t osMutexNew(const osMutexAttr_t *attr)
{
    osMutexId_t id = NULL;

    mutex_creates++;

    if ((attr == NULL) || ((attr->attr_bits & osMutexPrioInherit) == 0U))
    {
        note_fault("the mutex was made without priority inheritance");
    }

    if ((attr == NULL) || ((attr->attr_bits & osMutexRecursive) == 0U))
    {
        note_fault("the mutex was made without recursion, so it would not behave as ThreadX's");
    }

    if (!mutex_create_fails)
    {
        id = &test_mutex_object;
    }

    return id;
}

/*****************************************************************************************************/
/**
 * @brief Take the fake mutex, answering as the test chose.
 */
osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout)
{
    osStatus_t status = osError;

    switch (mutex_take(mutex_id == &test_mutex_object, timeout))
    {
        case TAKE_OK:
            status = osOK;
            break;

        case TAKE_TIMEOUT:
            status = osErrorTimeout;
            break;

        case TAKE_BUSY:
            status = osErrorResource;
            break;

        default:
            status = osErrorISR;
            break;
    }

    return status;
}

/*****************************************************************************************************/
/**
 * @brief Give the fake mutex back.
 */
osStatus_t osMutexRelease(osMutexId_t mutex_id)
{
    mutex_give(mutex_id == &test_mutex_object);

    return osOK;
}

/*****************************************************************************************************/
/**
 * @brief Let the threads of the same priority run: counted, and no time passes.
 */
osStatus_t osThreadYield(void)
{
    rtos_yields++;

    return osOK;
}

/*****************************************************************************************************/
/**
 * @brief Sleep, in ticks as CMSIS-RTOS v2 counts.
 */
osStatus_t osDelay(uint32_t ticks)
{
    rtos_sleep(ticks, (uint32_t)(((uint64_t)ticks * 1000U) / tick_hz));

    return osOK;
}

/*****************************************************************************************************/
/**
 * @brief The fake kernel's state: inactive before the start, locked while the scheduler is held.
 */
osKernelState_t osKernelGetState(void)
{
    osKernelState_t state = osKernelInactive;

    if (kernel_started)
    {
        state = (scheduler_holds > 0) ? osKernelLocked : osKernelRunning;
    }

    return state;
}

/*****************************************************************************************************/
/**
 * @brief Hold the fake scheduler as ST's layer does, which holds it only when it was not already.
 */
int32_t osKernelLock(void)
{
    int32_t lock = (int32_t)osError;

    if (kernel_started && (scheduler_holds > 0))
    {
        lock = 1;
    }
    else if (kernel_started)
    {
        scheduler_holds++;
        lock = 0;
    }
    else
    {
        /* Before the start there is nothing to hold. */
    }

    return lock;
}

/*****************************************************************************************************/
/**
 * @brief Let the fake scheduler go once, as ST's layer does.
 */
int32_t osKernelUnlock(void)
{
    int32_t lock = (int32_t)osError;

    if (kernel_started && (scheduler_holds > 0))
    {
        scheduler_holds--;
        lock = 1;
    }
    else if (kernel_started)
    {
        lock = 0;
    }
    else
    {
        /* Before the start there is nothing to let go. */
    }

    return lock;
}

/*****************************************************************************************************/
/**
 * @brief Create the fake thread, recording what ST's layer would hand FreeRTOS.
 */
osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr)
{
    osThreadId_t id = NULL;

    task_creates++;

    if ((attr->cb_mem != NULL) || (attr->cb_size != 0U) || (attr->stack_mem != NULL))
    {
        note_fault("memory was given, so the task did not come from the FreeRTOS heap");
    }

    if ((attr->attr_bits != 0U) || (attr->reserved != 0U))
    {
        note_fault("attributes ST's layer refuses, such as joinable");
    }

    if ((attr->priority < osPriorityIdle) || (attr->priority > osPriorityRealtime7))
    {
        note_fault("a priority ST's layer refuses, or keeps for itself");
    }

    check_create((const osal_task_t *)argument);

    /* ST's layer hands FreeRTOS the priority as it is. */
    created_entry    = func;
    created_argument = argument;
    created_name     = attr->name;
    created_stack    = attr->stack_size;
    created_priority = (uint32_t)attr->priority;

    if (!task_create_fails)
    {
        id = &test_thread_object;
    }

    return id;
}

/*****************************************************************************************************/
/**
 * @brief The task the tests say is running.
 */
osThreadId_t osThreadGetId(void)
{
    return running;
}

/*****************************************************************************************************/
/**
 * @brief Delete the fake thread, answering as the test chose.
 */
osStatus_t osThreadTerminate(osThreadId_t thread_id)
{
    task_deletes++;
    check_task(thread_id == &test_thread_object);

    if (thread_id == running)
    {
        note_fault("a task deleted itself through FreeRTOS, which frees it later, where ThreadX cannot");
    }

    return (task_answer == ANSWER_OK) ? osOK : osErrorResource;
}

/*****************************************************************************************************/
/**
 * @brief Suspend the fake thread, answering as the test chose.
 */
osStatus_t osThreadSuspend(osThreadId_t thread_id)
{
    task_suspends++;
    check_task(thread_id == &test_thread_object);

    return (task_answer == ANSWER_OK) ? osOK : osErrorParameter;
}

/*****************************************************************************************************/
/**
 * @brief Resume the fake thread, answering as the test chose.
 */
osStatus_t osThreadResume(osThreadId_t thread_id)
{
    task_resumes++;
    check_task(thread_id == &test_thread_object);

    return (task_answer == ANSWER_OK) ? osOK : osErrorParameter;
}
#elif OSAL_RTOS == OSAL_RTOS_THREADX
/*****************************************************************************************************/
/**
 * @brief The fake kernel's tick rate, ThreadX's TX_TIMER_TICKS_PER_SECOND, which the tests can change.
 */
unsigned long test_tick_rate(void)
{
    return tick_hz;
}

/*****************************************************************************************************/
/**
 * @brief Create the fake mutex, unless the test says it cannot be made.
 */
UINT tx_mutex_create(TX_MUTEX *mutex_ptr, CHAR *name_ptr, UINT inherit)
{
    UINT status = TX_MUTEX_ERROR;

    (void)name_ptr;
    mutex_creates++;

    if (inherit != TX_INHERIT)
    {
        note_fault("the mutex was made without priority inheritance");
    }

    if (mutex_ptr->tx_mutex_id != 0U)
    {
        /* Already created, which ThreadX refuses with the same error. */
        status = TX_MUTEX_ERROR;
    }
    else if (!mutex_create_fails)
    {
        mutex_ptr->tx_mutex_id    = 1U;
        mutex_ptr->tx_mutex_count = 0U;
        status                    = TX_SUCCESS;
    }
    else
    {
        /* The test says it cannot be made. */
    }

    return status;
}

/*****************************************************************************************************/
/**
 * @brief Delete the fake mutex.
 */
UINT tx_mutex_delete(TX_MUTEX *mutex_ptr)
{
    mutex_deletes++;
    mutex_ptr->tx_mutex_id = 0U;

    return TX_SUCCESS;
}

/*****************************************************************************************************/
/**
 * @brief Take the fake mutex, answering as the test chose.
 */
UINT tx_mutex_get(TX_MUTEX *mutex_ptr, ULONG wait_option)
{
    UINT status = TX_WAIT_ERROR;

    switch (mutex_take(mutex_ptr == &mutex.mutex, (uint32_t)wait_option))
    {
        case TAKE_OK:
            status = TX_SUCCESS;
            break;

        case TAKE_TIMEOUT:
        case TAKE_BUSY:
            status = TX_NOT_AVAILABLE;
            break;

        default:
            status = TX_WAIT_ERROR;
            break;
    }

    return status;
}

/*****************************************************************************************************/
/**
 * @brief Give the fake mutex back.
 */
UINT tx_mutex_put(TX_MUTEX *mutex_ptr)
{
    mutex_give(mutex_ptr == &mutex.mutex);

    return TX_SUCCESS;
}

/*****************************************************************************************************/
/**
 * @brief Let the threads of the same priority run: counted, and no time passes.
 */
VOID tx_thread_relinquish(VOID)
{
    rtos_yields++;
}

/*****************************************************************************************************/
/**
 * @brief Sleep, in ThreadX ticks.
 */
UINT tx_thread_sleep(ULONG timer_ticks)
{
    rtos_sleep((uint32_t)timer_ticks,
               (uint32_t)(((uint64_t)timer_ticks * 1000U) / TX_TIMER_TICKS_PER_SECOND));

    return TX_SUCCESS;
}

/*****************************************************************************************************/
/**
 * @brief Set the fake CPU's mask, and return what it was.
 */
UINT tx_interrupt_control(UINT new_posture)
{
    UINT old = (UINT)irq_mask;

    irq_mask = (uint32_t)new_posture;

    return old;
}

/*****************************************************************************************************/
/**
 * @brief Create the fake thread, recording what osal gave ThreadX.
 */
UINT tx_thread_create(TX_THREAD *thread_ptr, CHAR *name_ptr, VOID (*entry_function)(ULONG entry_input),
                      ULONG entry_input, VOID *stack_start, ULONG stack_size, UINT priority,
                      UINT preempt_threshold, ULONG time_slice, UINT auto_start)
{
    UINT status = TX_THREAD_ERROR;

    task_creates++;
    check_create((const osal_task_t *)thread_ptr);

    if (((const osal_task_t *)thread_ptr)->stack != stack_start)
    {
        note_fault("the stack ThreadX got is not the one osal_task_t keeps to give back");
    }

    if (thread_ptr->tx_thread_id != 0U)
    {
        /* Already created, which ThreadX refuses with the same error. */
        status = TX_THREAD_ERROR;
    }
    else if (!task_create_fails)
    {
        thread_ptr->tx_thread_id = 1U;
        thread_ptr->name         = name_ptr;
        thread_ptr->entry        = entry_function;
        thread_ptr->input        = entry_input;
        thread_ptr->stack        = stack_start;
        thread_ptr->stack_size   = stack_size;
        thread_ptr->priority     = priority;
        thread_ptr->threshold    = preempt_threshold;
        thread_ptr->time_slice   = time_slice;
        thread_ptr->auto_start   = auto_start;
        thread_ptr->ended        = 0U;
        status                   = TX_SUCCESS;
    }
    else
    {
        /* The test says it cannot be made. */
    }

    created_thread   = thread_ptr;
    created_argument = thread_ptr;
    created_name     = name_ptr;
    created_stack    = (uint32_t)stack_size;
    created_priority = priority;

    return status;
}

/*****************************************************************************************************/
/**
 * @brief End the fake thread. One that ends itself never comes back.
 */
UINT tx_thread_terminate(TX_THREAD *thread_ptr)
{
    if (thread_ptr == (TX_THREAD *)running)
    {
        self_ends++;
        escape_if_due();

        if (sleeps_to_escape == 0)
        {
            note_fault("a thread that ended itself came back");
        }
    }

    check_task(thread_ptr == &task.thread);

    if (task_answer == ANSWER_OK)
    {
        thread_ptr->ended = 1U;
    }

    return (task_answer == ANSWER_OK) ? TX_SUCCESS : TX_THREAD_ERROR;
}

/*****************************************************************************************************/
/**
 * @brief Delete the fake thread, which ThreadX does only for one that has ended.
 */
UINT tx_thread_delete(TX_THREAD *thread_ptr)
{
    UINT status = TX_DELETE_ERROR;

    task_deletes++;
    check_task(thread_ptr == &task.thread);

    if (thread_ptr == (TX_THREAD *)running)
    {
        note_fault("a thread deleted itself, which ThreadX refuses");
    }

    if (thread_ptr->ended != 0U)
    {
        thread_ptr->tx_thread_id = 0U;
        status                   = TX_SUCCESS;
    }

    return status;
}

/*****************************************************************************************************/
/**
 * @brief Suspend the fake thread, answering as the test chose, with ThreadX's values.
 */
UINT tx_thread_suspend(TX_THREAD *thread_ptr)
{
    UINT status = TX_THREAD_ERROR;

    task_suspends++;
    check_task(thread_ptr == &task.thread);

    if (task_answer == ANSWER_OK)
    {
        status = TX_SUCCESS;
    }
    else if (task_answer == ANSWER_ENDED)
    {
        status = TX_SUSPEND_ERROR;
    }
    else
    {
        status = TX_THREAD_ERROR;
    }

    return status;
}

/*****************************************************************************************************/
/**
 * @brief Resume the fake thread, answering as the test chose, with ThreadX's values.
 */
UINT tx_thread_resume(TX_THREAD *thread_ptr)
{
    UINT status = TX_THREAD_ERROR;

    task_resumes++;
    check_task(thread_ptr == &task.thread);

    if (task_answer == ANSWER_OK)
    {
        status = TX_SUCCESS;
    }
    else if (task_answer == ANSWER_NOT_SUSPENDED)
    {
        status = TX_RESUME_ERROR;
    }
    else if (task_answer == ANSWER_LIFTED)
    {
        status = TX_SUSPEND_LIFTED;
    }
    else
    {
        status = TX_THREAD_ERROR;
    }

    return status;
}

/*****************************************************************************************************/
/**
 * @brief The thread the tests say is running.
 */
TX_THREAD *tx_thread_identify(VOID)
{
    return (TX_THREAD *)running;
}

/*****************************************************************************************************/
/**
 * @brief Take memory from the fake byte pool, which must be the first one created.
 */
UINT tx_byte_allocate(TX_BYTE_POOL *pool_ptr, VOID **memory_ptr, ULONG memory_size, ULONG wait_option)
{
    if (pool_ptr != &test_pool)
    {
        note_fault("memory was taken from a pool that is not the first one created");
    }

    if (wait_option != TX_NO_WAIT)
    {
        note_fault("waited for memory, which FreeRTOS's heap never does");
    }

    *memory_ptr = heap_take((size_t)memory_size);

    return (*memory_ptr != NULL) ? TX_SUCCESS : TX_NO_MEMORY;
}

/*****************************************************************************************************/
/**
 * @brief Give memory back to the fake byte pool.
 */
UINT tx_byte_release(VOID *memory_ptr)
{
    heap_give(memory_ptr);

    return TX_SUCCESS;
}
#endif

/*****************************************************************************************************/
/**
 * @brief Start every test from a stopped clock and a fresh fake kernel.
 */
void setUp(void)
{
    now_ms             = 0U;
    hal_delays         = 0;
    rtos_sleeps        = 0;
    rtos_yields        = 0;
    last_sleep         = 0U;
    faults             = 0;
    fault              = "none";
    tick_hz            = 1000U;
    take_result        = TAKE_OK;
    mutex_create_fails = false;
    mutex_creates      = 0;
    mutex_deletes      = 0;
    ipsr               = 0U;
    irq_mask           = 0U;
    mutex_takes        = 0;
    mutex_gives        = 0;
    mutex_held         = 0;
    last_wait          = 0U;
    sleeps_to_escape   = 0;
    kernel_started     = true;
    critical_depth     = 0;
    counted_enters     = 0;
    heap_fails         = false;
    heap_takes         = 0;
    heap_gives         = 0;
    heap_held          = 0;
    last_heap_size     = 0U;
    task_create_fails  = false;
    task_answer        = ANSWER_OK;
    task_creates       = 0;
    task_deletes       = 0;
    task_suspends      = 0;
    task_resumes       = 0;
    self_ends          = 0;
    scheduler_holds    = 0;
    filled_at_create   = false;
    held_at_create     = false;
    created_name       = NULL;
    created_stack      = 0U;
    created_priority   = 0U;
    created_argument   = NULL;
    body_runs          = 0;
    body_argument      = NULL;

    memset(&mutex, 0, sizeof(mutex));
    memset(&task, 0, sizeof(task));

#if OSAL_RTOS == OSAL_RTOS_THREADX
    _tx_byte_pool_created_ptr = &test_pool;
    created_thread            = NULL;
#endif

#if OSAL_RTOS != OSAL_RTOS_NONE
    be_another_task();
#endif
}

/*****************************************************************************************************/
/**
 * @brief Every test ends with no fault from the fake kernel.
 */
void tearDown(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, faults, fault);
}

/*****************************************************************************************************/
/**
 * @brief A yield is the RTOS's own, with no sleep, and without an RTOS it does nothing.
 */
void test_a_yield_is_the_rtos_yield(void)
{
    osal_yield();

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, sleeps(), "a yield slept");
    TEST_ASSERT_EQUAL_UINT32(0U, now_ms);
#if OSAL_RTOS == OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT(0, hal_delays);
#else
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, rtos_yields, "the RTOS was not asked to yield");
#endif
}

/*****************************************************************************************************/
/**
 * @brief A pause sleeps one tick with an RTOS, and returns at once without one.
 */
void test_a_pause_sleeps_only_with_an_rtos(void)
{
    osal_pause();

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, hal_delays, "spun in HAL_Delay()");
#if OSAL_RTOS == OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, sleeps(), "waited with no other thread to run");
    TEST_ASSERT_EQUAL_UINT32(0U, now_ms);
#else
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, sleeps(), "the other threads got no turn");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rtos_yields, "yielded, which runs no lower thread");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, last_sleep, "not the shortest sleep");
    TEST_ASSERT_EQUAL_UINT32(1U, now_ms);
#endif
}

/*****************************************************************************************************/
/**
 * @brief From an interrupt, every port refuses a mutex and waits for nothing, with no RTOS call.
 */
void test_an_interrupt_gets_the_same_on_every_port(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));

    /* SysTick, say. */
    ipsr = 15U;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_MUTEX, osal_mutex_create(&mutex));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_MUTEX, osal_mutex_lock(&mutex, 10U));
    osal_mutex_unlock(&mutex);
    osal_delay_ms(10U);
    osal_delay_ms(HAL_MAX_DELAY);
    osal_pause();
    osal_yield();

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, sleeps(), "waited in an interrupt");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rtos_yields, "yielded in an interrupt");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mutex_takes, "took a mutex in an interrupt");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mutex_gives, "gave a mutex back in an interrupt");
    TEST_ASSERT_EQUAL_UINT32(0U, now_ms);
#if OSAL_RTOS != OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mutex_creates, "made a mutex in an interrupt");
#endif
}

/*****************************************************************************************************/
/**
 * @brief From an interrupt, every port refuses a task and the heap, with no RTOS call.
 */
void test_an_interrupt_gets_no_task_and_no_heap(void)
{
    static osal_task_t fresh;
    void               *memory = osal_malloc(16U);

    memset(&fresh, 0, sizeof(fresh));

    TEST_ASSERT_NOT_NULL(memory);
#if OSAL_RTOS != OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
#endif

    ipsr = 15U;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_create(&fresh, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_delete(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_suspend(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_resume(&task));
    TEST_ASSERT_NULL_MESSAGE(osal_malloc(16U), "took heap memory in an interrupt");
    osal_free(memory);

#if OSAL_RTOS != OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, task_creates, "made a task in an interrupt");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, task_deletes, "deleted a task in an interrupt");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, task_suspends, "suspended a task in an interrupt");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, task_resumes, "resumed a task in an interrupt");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heap_gives, "gave heap memory back in an interrupt");
#endif

    /* Given back from a task instead. */
    ipsr = 0U;
    osal_free(memory);
#if OSAL_RTOS != OSAL_RTOS_NONE
    be_another_task();
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heap_held, "heap memory was left taken");
#endif
}

/*****************************************************************************************************/
/**
 * @brief A critical section masks, nests, and puts the interrupts back as it found them.
 */
void test_a_critical_section_masks_and_nests(void)
{
    uint32_t outer = osal_critical_enter();
    uint32_t inner = 0U;

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, irq_mask, "the interrupts were not masked");

    inner = osal_critical_enter();
    TEST_ASSERT_EQUAL_UINT32(1U, irq_mask);

    osal_critical_exit(inner);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, irq_mask, "the inner exit let the interrupts in");

    osal_critical_exit(outer);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, irq_mask, "the interrupts were left masked");
}

/*****************************************************************************************************/
/**
 * @brief In an interrupt a critical section works too, and leaves masked what was masked.
 */
void test_a_critical_section_works_in_an_interrupt(void)
{
    uint32_t state = 0U;

    ipsr = 15U;

    state = osal_critical_enter();
    TEST_ASSERT_EQUAL_UINT32(1U, irq_mask);
    osal_critical_exit(state);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, irq_mask, "the interrupts were left masked");

    /* Inside a critical section of the code it cut into. */
    irq_mask = 1U;
    state    = osal_critical_enter();
    osal_critical_exit(state);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, irq_mask, "let in the interrupts the code below had masked");
}

/*****************************************************************************************************/
/**
 * @brief Before the RTOS starts, leaving a critical section gives the interrupts back on every port.
 */
void test_a_critical_section_before_the_start_gives_the_interrupts_back(void)
{
    uint32_t state = 0U;

    kernel_started = false;

    state = osal_critical_enter();
    TEST_ASSERT_EQUAL_UINT32(1U, irq_mask);
    osal_critical_exit(state);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, irq_mask, "masked until the RTOS starts, where other ports are not");
}

/*****************************************************************************************************/
/**
 * @brief The priority range and the smallest stack come from the RTOS.
 */
void test_the_priorities_and_stack_come_from_the_rtos(void)
{
#if OSAL_RTOS == OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_UINT32(0U, OSAL_TASK_PRIORITY_MAX);
    TEST_ASSERT_EQUAL_UINT32(0U, OSAL_TASK_STACK_MIN);
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V1
    TEST_ASSERT_EQUAL_UINT32((uint32_t)configMAX_PRIORITIES - 1U, OSAL_TASK_PRIORITY_MAX);
    TEST_ASSERT_EQUAL_UINT32(512U, OSAL_TASK_STACK_MIN);
#elif OSAL_RTOS == OSAL_RTOS_CMSIS_V2
    TEST_ASSERT_EQUAL_UINT32(54U, OSAL_TASK_PRIORITY_MAX);
    TEST_ASSERT_EQUAL_UINT32(512U, OSAL_TASK_STACK_MIN);
#else
    TEST_ASSERT_EQUAL_UINT32(31U, OSAL_TASK_PRIORITY_MAX);
    TEST_ASSERT_EQUAL_UINT32(200U, OSAL_TASK_STACK_MIN);
#endif
}

/*****************************************************************************************************/
/**
 * @brief The heap gives memory and takes it back, and 0 bytes or NULL ask nobody.
 */
void test_the_heap_gives_and_takes_back(void)
{
    void *memory = osal_malloc(32U);

    TEST_ASSERT_NOT_NULL(memory);
    memset(memory, 0xA5, 32U);
#if OSAL_RTOS != OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT(1, heap_takes);
    TEST_ASSERT_EQUAL_size_t(32U, last_heap_size);
#endif

    osal_free(memory);
#if OSAL_RTOS != OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT(1, heap_gives);
    TEST_ASSERT_EQUAL_INT(0, heap_held);
#endif

    TEST_ASSERT_NULL_MESSAGE(osal_malloc(0U), "0 bytes gave memory");
    osal_free(NULL);
#if OSAL_RTOS != OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, heap_takes, "0 bytes asked the RTOS");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, heap_gives, "NULL was given to the RTOS");
#endif
}

/*****************************************************************************************************/
/**
 * @brief NULL is refused, never followed, and a task without a function is refused.
 */
void test_a_null_task_is_refused(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_task_create(NULL, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_task_create(&task, "t", NULL, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_task_delete(NULL));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_task_suspend(NULL));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_task_resume(NULL));
#if OSAL_RTOS != OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT(0, task_creates);
#endif
}

#if OSAL_RTOS == OSAL_RTOS_NONE
/*****************************************************************************************************/
/**
 * @brief Bare metal has no task to create, delete, suspend or resume, and says so.
 */
void test_bare_metal_has_no_task(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_create(&task, "t", task_body, &marker, 512U, 0U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_delete(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_suspend(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_resume(&task));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, body_runs, "the function ran with no task");
}
#endif

/*****************************************************************************************************/
/**
 * @brief A mutex is made, taken and given back, and taken again.
 */
void test_a_mutex_may_be_taken_again_by_its_holder(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 0U));
    TEST_ASSERT_EQUAL_INT_MESSAGE(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 0U), "refused its own holder");
    osal_mutex_unlock(&mutex);
    osal_mutex_unlock(&mutex);

    TEST_ASSERT_EQUAL_INT(mutex_takes, mutex_gives);
    TEST_ASSERT_EQUAL_INT(0, mutex_held);
}

/*****************************************************************************************************/
/**
 * @brief A second create on the same mutex works on every port, and gives a new mutex.
 */
void test_a_second_create_gives_a_new_mutex(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));
    TEST_ASSERT_EQUAL_INT_MESSAGE(OSAL_ERR_NONE, osal_mutex_create(&mutex), "refused a second create");
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 10U));
    osal_mutex_unlock(&mutex);
#if OSAL_RTOS == OSAL_RTOS_THREADX
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mutex_deletes, "the old ThreadX mutex was not deleted first");
#endif
}

/*****************************************************************************************************/
/**
 * @brief NULL is refused, never followed.
 */
void test_a_null_mutex_is_refused(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_mutex_create(NULL));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_mutex_lock(NULL, 10U));
    osal_mutex_unlock(NULL);

    TEST_ASSERT_EQUAL_INT(0, mutex_creates);
    TEST_ASSERT_EQUAL_INT(0, mutex_takes);
    TEST_ASSERT_EQUAL_INT(0, mutex_gives);
}

/*****************************************************************************************************/
/**
 * @brief A mutex is made, taken and given back, and taken again.
 */
void test_a_mutex_locks_and_unlocks(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 100U));
    osal_mutex_unlock(&mutex);
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 0U));
    osal_mutex_unlock(&mutex);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mutex_held, "the mutex was kept");
    TEST_ASSERT_EQUAL_INT(mutex_takes, mutex_gives);
#if OSAL_RTOS != OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mutex_creates, "not exactly one RTOS mutex");
    TEST_ASSERT_EQUAL_INT(2, mutex_takes);
#else
    TEST_ASSERT_EQUAL_INT(0, mutex_takes);
#endif
}

/*****************************************************************************************************/
/**
 * @brief A delay waits as long as asked, sleeping on the RTOS when there is one.
 */
void test_a_delay_waits_as_long_as_asked(void)
{
    osal_delay_ms(5U);

    TEST_ASSERT_EQUAL_UINT32(5U, now_ms);
    TEST_ASSERT_EQUAL_INT(1, sleeps());
#if OSAL_RTOS == OSAL_RTOS_NONE
    TEST_ASSERT_EQUAL_INT(1, hal_delays);
#else
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, hal_delays, "spun instead of sleeping");
    TEST_ASSERT_EQUAL_UINT32(5U, last_sleep);
#endif
}

/*****************************************************************************************************/
/**
 * @brief A delay of zero returns at once, asking nobody.
 */
void test_a_delay_of_zero_returns_at_once(void)
{
    osal_delay_ms(0U);

    TEST_ASSERT_EQUAL_UINT32(0U, now_ms);
    TEST_ASSERT_EQUAL_INT(0, sleeps());
}

/*****************************************************************************************************/
/**
 * @brief A delay of HAL_MAX_DELAY never returns: it sleeps the longest it can, again and again.
 */
void test_a_delay_for_ever_never_returns(void)
{
    sleeps_to_escape = 3;

    if (setjmp(escape) == 0)
    {
        osal_delay_ms(HAL_MAX_DELAY);
        TEST_FAIL_MESSAGE("a delay of HAL_MAX_DELAY returned");
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, sleeps(), "did not sleep again after the longest sleep");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xFFFFFFFFU, last_sleep, "not the longest sleep");
}

/*****************************************************************************************************/
/**
 * @brief The shortest delay, 1 ms, still waits. Libraries poll with it.
 */
void test_a_delay_of_one_still_waits(void)
{
    osal_delay_ms(1U);

    TEST_ASSERT_EQUAL_UINT32(1U, now_ms);
    TEST_ASSERT_EQUAL_INT(1, sleeps());
}

#if OSAL_RTOS != OSAL_RTOS_NONE
/*****************************************************************************************************/
/**
 * @brief A mutex the RTOS cannot make is reported.
 */
void test_a_mutex_the_rtos_cannot_make_is_reported(void)
{
    mutex_create_fails = true;

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_MUTEX, osal_mutex_create(&mutex));
}

/*****************************************************************************************************/
/**
 * @brief A mutex another thread keeps for the whole wait is a timeout, and nothing is given back.
 */
void test_a_mutex_held_elsewhere_times_out(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));

    take_result = TAKE_TIMEOUT;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TIMEOUT, osal_mutex_lock(&mutex, 100U));

    take_result = TAKE_BUSY;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TIMEOUT, osal_mutex_lock(&mutex, 0U));

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mutex_gives, "gave back a mutex it never had");
}

/*****************************************************************************************************/
/**
 * @brief A mutex the RTOS refuses, as from an interrupt, is reported as such.
 */
void test_a_refused_mutex_is_reported(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));

    take_result = TAKE_REFUSED;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_MUTEX, osal_mutex_lock(&mutex, 100U));
}

/*****************************************************************************************************/
/**
 * @brief The wait for the mutex follows the timeout, and HAL_MAX_DELAY waits for ever.
 */
void test_the_mutex_wait_follows_the_timeout(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 25U));
    TEST_ASSERT_EQUAL_UINT32(25U, last_wait);
    osal_mutex_unlock(&mutex);

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, HAL_MAX_DELAY));
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xFFFFFFFFU, last_wait, "should wait for ever");
    osal_mutex_unlock(&mutex);
}

/*****************************************************************************************************/
/**
 * @brief A heap with not enough left gives NULL.
 */
void test_a_full_heap_gives_null(void)
{
    heap_fails = true;

    TEST_ASSERT_NULL(osal_malloc(32U));
    TEST_ASSERT_EQUAL_INT(1, heap_takes);
}

/*****************************************************************************************************/
/**
 * @brief A task runs its function with its argument, and one that returns stops for good, keeping its memory.
 */
void test_a_task_that_returns_stops_for_good(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, &marker, OSAL_TASK_STACK_MIN, 1U));

    sleeps_to_escape = 1;

    if (setjmp(escape) == 0)
    {
        run_created_task();
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, body_runs, "the function did not run");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&marker, body_argument, "the function did not get its argument");
#if OSAL_RTOS != OSAL_RTOS_THREADX
    /* FreeRTOS would stop the CPU after a return, ThreadX ends the task. */
    TEST_ASSERT_TRUE_MESSAGE(stopped_for_good(), "a task that returned did not stop for good");
#endif
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, task_deletes, "a task that returned was deleted");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heap_gives, "a task that returned gave its memory back");

    /* Another task deletes it, and its memory comes back. */
    be_another_task();
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
    TEST_ASSERT_EQUAL_INT(1, task_deletes);
    TEST_ASSERT_EQUAL_INT(0, heap_held);
}

/*****************************************************************************************************/
/**
 * @brief A task gets the priority, stack and name it asked for, in the RTOS's own terms.
 */
void test_a_task_gets_the_priority_stack_and_name_asked(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, NULL, task_body, NULL, 1024U, 0U));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(NATIVE_LOWEST, created_priority, "0 is not the RTOS's lowest");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(NATIVE_STACK(1024U), created_stack, "the stack is not in the RTOS's unit");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("osal", created_name, "no name did not give osal");
#if OSAL_RTOS == OSAL_RTOS_THREADX
    TEST_ASSERT_EQUAL_UINT_MESSAGE(created_thread->priority, created_thread->threshold, "preemption threshold left on");
#endif
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE,
                          osal_task_create(&task, "worker", task_body, NULL, OSAL_TASK_STACK_MIN, OSAL_TASK_PRIORITY_MAX));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(NATIVE_HIGHEST, created_priority, "the highest is not the RTOS's highest");
    TEST_ASSERT_EQUAL_UINT32(NATIVE_STACK(OSAL_TASK_STACK_MIN), created_stack);
    TEST_ASSERT_EQUAL_STRING("worker", created_name);
#if OSAL_RTOS == OSAL_RTOS_THREADX
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, created_thread->time_slice, "tasks of one priority take no turns");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(TX_AUTO_START, created_thread->auto_start, "the task was not started");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, heap_held, "the stack did not come from the pool");
#endif
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
}

/*****************************************************************************************************/
/**
 * @brief A priority above the highest, and a stack below the RTOS's smallest, are refused unasked.
 */
void test_a_task_out_of_range_is_refused(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN,
                                                             OSAL_TASK_PRIORITY_MAX + 1U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID,
                          osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN - 1U, 0U));
#if OSAL_RTOS != OSAL_RTOS_THREADX
    /* FreeRTOS counts the stack in 16 bit words, so a bigger one would wrap. */
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_INVALID, osal_task_create(&task, "t", task_body, NULL, 0x40000U, 0U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, 0x3FFFCU, 0U));
    TEST_ASSERT_EQUAL_INT(1, task_creates);
#else
    TEST_ASSERT_EQUAL_INT(0, task_creates);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heap_takes, "a refused task took a stack");
#endif
}

/*****************************************************************************************************/
/**
 * @brief A task the RTOS cannot make is reported, and leaves nothing taken.
 */
void test_a_task_the_rtos_cannot_make_is_reported(void)
{
    task_create_fails = true;

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heap_held, "the stack of a task never made was kept");

    /* Nothing is left that a delete would act on, and it can be made again. */
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_delete(&task));
    task_create_fails = false;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
}

/*****************************************************************************************************/
/**
 * @brief A task is created once: a second create is refused until it is deleted.
 */
void test_a_task_is_created_once_until_deleted(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT_MESSAGE(OSAL_ERR_TASK, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U),
                                  "a second create lost the first task");
    TEST_ASSERT_EQUAL_INT(1, task_creates);

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT(2, task_creates);
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
}

/*****************************************************************************************************/
/**
 * @brief A task another task deletes is gone, with its memory, and is refused after.
 */
void test_a_task_deleted_by_another_is_gone(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
    TEST_ASSERT_EQUAL_INT(1, task_deletes);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heap_held, "its memory did not come back");

    TEST_ASSERT_EQUAL_INT_MESSAGE(OSAL_ERR_TASK, osal_task_delete(&task), "deleted it twice");
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_suspend(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_resume(&task));
    TEST_ASSERT_EQUAL_INT(1, task_deletes);
    TEST_ASSERT_EQUAL_INT(0, task_suspends);
    TEST_ASSERT_EQUAL_INT(0, task_resumes);
}

/*****************************************************************************************************/
/**
 * @brief A task that deletes itself never returns, only stops, and keeps its memory until another deletes it.
 */
void test_a_task_that_deletes_itself_only_stops(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));

    be_the_task();
    sleeps_to_escape = 1;

    if (setjmp(escape) == 0)
    {
        (void)osal_task_delete(&task);
        TEST_FAIL_MESSAGE("a task that deleted itself came back");
    }

    TEST_ASSERT_TRUE_MESSAGE(stopped_for_good(), "it did not stop for good");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, task_deletes, "it was deleted from inside itself");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heap_gives, "its memory went while it was on it");

    be_another_task();
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
    TEST_ASSERT_EQUAL_INT(1, task_deletes);
    TEST_ASSERT_EQUAL_INT(0, heap_held);
}

/*****************************************************************************************************/
/**
 * @brief A task is suspended and resumed, by another task or by itself.
 */
void test_a_task_suspends_and_resumes(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_suspend(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_resume(&task));

    be_the_task();
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_suspend(&task));

    TEST_ASSERT_EQUAL_INT(2, task_suspends);
    TEST_ASSERT_EQUAL_INT(1, task_resumes);

    be_another_task();
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
}

/*****************************************************************************************************/
/**
 * @brief What the RTOS refuses is reported, and a task whose delete was refused is kept.
 */
void test_a_refused_task_call_is_reported(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));

    task_answer = ANSWER_REFUSED;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_suspend(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_resume(&task));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_delete(&task));
#if OSAL_RTOS == OSAL_RTOS_THREADX
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, heap_held, "the stack of a task whose delete was refused was given back");
#endif

    task_answer = ANSWER_OK;
    TEST_ASSERT_EQUAL_INT_MESSAGE(OSAL_ERR_NONE, osal_task_delete(&task), "a task whose delete was refused was lost");
    TEST_ASSERT_EQUAL_INT(0, heap_held);
}

/*****************************************************************************************************/
/**
 * @brief A task is filled in before it can run, and a task can be made before the RTOS starts.
 */
void test_a_new_task_finds_itself_filled_in(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, &marker, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_TRUE_MESSAGE(filled_at_create, "the task could run before osal_task_t was filled in");
#if OSAL_RTOS != OSAL_RTOS_THREADX
    /* With FreeRTOS the handle comes back after the task may have run, so
       the scheduler is held meanwhile, and let go after. */
    TEST_ASSERT_TRUE_MESSAGE(held_at_create, "the scheduler was not held while the task was made");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, scheduler_holds, "the scheduler was left held");
#endif
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));

    /* Where the RTOS makes its first tasks. */
    kernel_started = false;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, scheduler_holds, "the scheduler was left held");
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
}
#endif

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
/*****************************************************************************************************/
/**
 * @brief CMSIS-RTOS v1 is given milliseconds, as it takes them, and turns them into ticks itself.
 */
void test_v1_is_given_milliseconds(void)
{
    tick_hz = 100U;

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 25U));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(25U, last_wait, "the timeout was changed on its way");
    osal_mutex_unlock(&mutex);

    osal_delay_ms(25U);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(25U, last_sleep, "the delay was changed on its way");
}
#endif

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V1) || (OSAL_RTOS == OSAL_RTOS_CMSIS_V2)
/*****************************************************************************************************/
/**
 * @brief In a task, the critical section is FreeRTOS's own count, so an RTOS call inside keeps it masked.
 */
void test_a_task_critical_section_is_freertos_own(void)
{
    uint32_t state = osal_critical_enter();

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, counted_enters, "not FreeRTOS's own critical section");

    /* An RTOS call made inside, with a critical section of its own. */
    taskENTER_CRITICAL();
    taskEXIT_CRITICAL();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, irq_mask, "an RTOS call inside let the interrupts in");

    osal_critical_exit(state);
    TEST_ASSERT_EQUAL_UINT32(0U, irq_mask);
    TEST_ASSERT_EQUAL_INT(0, critical_depth);

    /* The same with the scheduler held: the RTOS still runs. */
    scheduler_holds = 1;
    counted_enters  = 0;
    state           = osal_critical_enter();
    osal_critical_exit(state);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, counted_enters, "a held scheduler was taken for an RTOS not started");
}
#endif

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V2
/*****************************************************************************************************/
/**
 * @brief A scheduler the caller held already stays held: only a lock osal took is let go.
 */
void test_v2_lets_go_only_its_own_lock(void)
{
    scheduler_holds = 1;

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, scheduler_holds, "let go a lock the caller held");
}
#endif

#if OSAL_RTOS == OSAL_RTOS_THREADX
/*****************************************************************************************************/
/**
 * @brief ThreadX's answers for a task that has ended, or was not suspended, are FreeRTOS's: done.
 */
void test_threadx_answers_as_freertos_does(void)
{
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));

    task_answer = ANSWER_ENDED;
    TEST_ASSERT_EQUAL_INT_MESSAGE(OSAL_ERR_NONE, osal_task_suspend(&task), "an ended task's suspend was an error");

    task_answer = ANSWER_NOT_SUSPENDED;
    TEST_ASSERT_EQUAL_INT_MESSAGE(OSAL_ERR_NONE, osal_task_resume(&task), "resuming a running task was an error");

    task_answer = ANSWER_LIFTED;
    TEST_ASSERT_EQUAL_INT_MESSAGE(OSAL_ERR_NONE, osal_task_resume(&task), "a suspend called off was an error");

    task_answer = ANSWER_OK;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_task_delete(&task));
}

/*****************************************************************************************************/
/**
 * @brief With no byte pool, or not enough in it, there is no memory and no task.
 */
void test_threadx_with_no_pool_has_no_memory(void)
{
    heap_fails = true;
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, task_creates, "a task was made with no stack");

    heap_fails                = false;
    _tx_byte_pool_created_ptr = NULL;
    TEST_ASSERT_NULL(osal_malloc(32U));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_TASK, osal_task_create(&task, "t", task_body, NULL, OSAL_TASK_STACK_MIN, 0U));
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, heap_takes, "a missing pool was asked for memory");
    TEST_ASSERT_EQUAL_INT(0, task_creates);
}
#endif

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V2) || (OSAL_RTOS == OSAL_RTOS_THREADX)
/*****************************************************************************************************/
/**
 * @brief Milliseconds become whole ticks, the rest dropped as pdMS_TO_TICKS() does, and a wait never none.
 *
 * Through CMSIS-RTOS v2 that is FreeRTOS's own pdMS_TO_TICKS(). ThreadX has no
 * such macro, and osal does the same sum.
 */
void test_ticks_drop_the_rest_but_never_a_wait(void)
{
    tick_hz = 100U;

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_create(&mutex));
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 25U));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2U, last_wait, "25 ms at 100 Hz is 2 whole ticks");
    osal_mutex_unlock(&mutex);

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 5U));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, last_wait, "a wait of 5 ms became no wait");
    osal_mutex_unlock(&mutex);

    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, 0U));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, last_wait, "no wait became one");
    osal_mutex_unlock(&mutex);

    /* Made into ticks, HAL_MAX_DELAY would be a time on a slow tick: 49 days at
       1 kHz is the RTOS's "for ever" only by chance. */
    TEST_ASSERT_EQUAL_INT(OSAL_ERR_NONE, osal_mutex_lock(&mutex, HAL_MAX_DELAY));
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xFFFFFFFFU, last_wait, "for ever became a time on a slow tick");
    osal_mutex_unlock(&mutex);

    osal_delay_ms(1U);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, last_sleep, "1 ms at 100 Hz is still a tick");
}

/*****************************************************************************************************/
/**
 * @brief On a fast tick, a millisecond is several ticks.
 */
void test_a_fast_tick_counts_every_tick(void)
{
    tick_hz = 10000U;

    osal_delay_ms(1U);
    TEST_ASSERT_EQUAL_UINT32(10U, last_sleep);
    TEST_ASSERT_EQUAL_UINT32(1U, now_ms);
}
#endif

/*****************************************************************************************************/
/**
 * @brief Run every test.
 *
 * @return 0 when every test passed.
 */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_a_yield_is_the_rtos_yield);
    RUN_TEST(test_a_pause_sleeps_only_with_an_rtos);
    RUN_TEST(test_an_interrupt_gets_the_same_on_every_port);
    RUN_TEST(test_an_interrupt_gets_no_task_and_no_heap);
    RUN_TEST(test_a_critical_section_masks_and_nests);
    RUN_TEST(test_a_critical_section_works_in_an_interrupt);
    RUN_TEST(test_a_critical_section_before_the_start_gives_the_interrupts_back);
    RUN_TEST(test_the_priorities_and_stack_come_from_the_rtos);
    RUN_TEST(test_the_heap_gives_and_takes_back);
    RUN_TEST(test_a_null_task_is_refused);
    RUN_TEST(test_a_mutex_may_be_taken_again_by_its_holder);
    RUN_TEST(test_a_second_create_gives_a_new_mutex);
    RUN_TEST(test_a_null_mutex_is_refused);
    RUN_TEST(test_a_mutex_locks_and_unlocks);
    RUN_TEST(test_a_delay_waits_as_long_as_asked);
    RUN_TEST(test_a_delay_of_zero_returns_at_once);
    RUN_TEST(test_a_delay_of_one_still_waits);
    RUN_TEST(test_a_delay_for_ever_never_returns);

#if OSAL_RTOS == OSAL_RTOS_NONE
    RUN_TEST(test_bare_metal_has_no_task);
#else
    RUN_TEST(test_a_mutex_the_rtos_cannot_make_is_reported);
    RUN_TEST(test_a_mutex_held_elsewhere_times_out);
    RUN_TEST(test_a_refused_mutex_is_reported);
    RUN_TEST(test_the_mutex_wait_follows_the_timeout);
    RUN_TEST(test_a_full_heap_gives_null);
    RUN_TEST(test_a_task_that_returns_stops_for_good);
    RUN_TEST(test_a_task_gets_the_priority_stack_and_name_asked);
    RUN_TEST(test_a_task_out_of_range_is_refused);
    RUN_TEST(test_a_task_the_rtos_cannot_make_is_reported);
    RUN_TEST(test_a_task_is_created_once_until_deleted);
    RUN_TEST(test_a_task_deleted_by_another_is_gone);
    RUN_TEST(test_a_task_that_deletes_itself_only_stops);
    RUN_TEST(test_a_task_suspends_and_resumes);
    RUN_TEST(test_a_refused_task_call_is_reported);
    RUN_TEST(test_a_new_task_finds_itself_filled_in);
#endif

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
    RUN_TEST(test_v1_is_given_milliseconds);
#endif

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V1) || (OSAL_RTOS == OSAL_RTOS_CMSIS_V2)
    RUN_TEST(test_a_task_critical_section_is_freertos_own);
#endif

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V2
    RUN_TEST(test_v2_lets_go_only_its_own_lock);
#endif

#if OSAL_RTOS == OSAL_RTOS_THREADX
    RUN_TEST(test_threadx_answers_as_freertos_does);
    RUN_TEST(test_threadx_with_no_pool_has_no_memory);
#endif

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V2) || (OSAL_RTOS == OSAL_RTOS_THREADX)
    RUN_TEST(test_ticks_drop_the_rest_but_never_a_wait);
    RUN_TEST(test_a_fast_tick_counts_every_tick);
#endif

    return UNITY_END();
}

/*
 * ****************************************************************************************************
 * Private function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Sleeps of either kind, since which one is used depends on the build.
 *
 * @return HAL_Delay() calls plus RTOS sleeps.
 */
static int sleeps(void)
{
    return hal_delays + rtos_sleeps;
}

/*****************************************************************************************************/
/**
 * @brief Jump out of a call that never returns, once a test's chosen number of sleeps is reached.
 */
static void escape_if_due(void)
{
    if ((sleeps_to_escape > 0) && ((sleeps() + self_ends) >= sleeps_to_escape))
    {
        longjmp(escape, 1);
    }
}

/*****************************************************************************************************/
/**
 * @brief A task's function: notes that it ran and what it was given, and returns.
 *
 * @param[in] argument  What osal_task_create() was given.
 */
static void task_body(void *argument)
{
    body_runs++;
    body_argument = argument;
}

#if OSAL_RTOS != OSAL_RTOS_NONE
/*****************************************************************************************************/
/**
 * @brief Note something the fake kernel caught osal doing wrong.
 *
 * @param[in] what  What it was, for the failure message.
 */
static void note_fault(const char *what)
{
    if (faults == 0)
    {
        fault = what;
    }

    faults++;
}

/*****************************************************************************************************/
/**
 * @brief Take the fake mutex, keeping count, and give the answer the test chose.
 *
 * @param[in] right_mutex  Whether it was asked for the mutex osal made.
 * @param[in] wait         The wait it was given, in the RTOS's own unit.
 * @return The answer the test chose.
 */
static take_t mutex_take(bool right_mutex, uint32_t wait)
{
    if (!right_mutex)
    {
        note_fault("asked for a mutex osal never made");
    }

    mutex_takes++;
    last_wait = wait;

    if (take_result == TAKE_OK)
    {
        mutex_held++;
    }

    return take_result;
}

/*****************************************************************************************************/
/**
 * @brief Give the fake mutex back, keeping count.
 *
 * @param[in] right_mutex  Whether it was the mutex osal made.
 */
static void mutex_give(bool right_mutex)
{
    if (!right_mutex)
    {
        note_fault("gave back a mutex osal never made");
    }

    if (mutex_held == 0)
    {
        note_fault("gave back a mutex it did not hold");
    }
    else
    {
        mutex_held--;
    }

    mutex_gives++;
}

/*****************************************************************************************************/
/**
 * @brief Sleep on the fake RTOS, moving the clock on.
 *
 * @param[in] ticks  The sleep in the RTOS's own unit, as it was asked for.
 * @param[in] ms     The same sleep in milliseconds.
 */
static void rtos_sleep(uint32_t ticks, uint32_t ms)
{
    rtos_sleeps++;
    last_sleep = ticks;
    now_ms    += ms;
    escape_if_due();
}

/*****************************************************************************************************/
/**
 * @brief Take memory from the fake heap, keeping count, unless the test says it is full.
 *
 * @param[in] size  Bytes.
 * @return The memory, or NULL.
 */
static void *heap_take(size_t size)
{
    void *memory = NULL;

    if (ipsr != 0U)
    {
        note_fault("took heap memory in an interrupt");
    }

    heap_takes++;
    last_heap_size = size;

    if (!heap_fails)
    {
        memory = malloc(size);
        heap_held++;
    }

    return memory;
}

/*****************************************************************************************************/
/**
 * @brief Give memory back to the fake heap, keeping count.
 *
 * @param[in] memory  Memory from heap_take().
 */
static void heap_give(void *memory)
{
    if (ipsr != 0U)
    {
        note_fault("gave heap memory back in an interrupt");
    }

    if (heap_held == 0)
    {
        note_fault("gave back memory never taken");
    }
    else
    {
        heap_held--;
    }

    heap_gives++;
    free(memory);
}

/*****************************************************************************************************/
/**
 * @brief Check, as the RTOS makes a task, that it could find itself if it ran at once.
 *
 * @param[in] made  The osal_task_t the new task runs from.
 */
static void check_create(const osal_task_t *made)
{
    if (ipsr != 0U)
    {
        note_fault("made a task in an interrupt");
    }

    filled_at_create = (made == &task) && (made->function == task_body);
    held_at_create   = (scheduler_holds > 0);

    if (kernel_started && (OSAL_RTOS != OSAL_RTOS_THREADX) && !held_at_create)
    {
        note_fault("the new task could run before osal_task_t was filled in");
    }
}

/*****************************************************************************************************/
/**
 * @brief Check that the RTOS was asked about the task osal made.
 *
 * @param[in] right_task  Whether it was.
 */
static void check_task(bool right_task)
{
    if (!right_task)
    {
        note_fault("asked about a task osal never made");
    }

    if (ipsr != 0U)
    {
        note_fault("acted on a task in an interrupt");
    }
}

/*****************************************************************************************************/
/**
 * @brief Make the task osal created the one the CPU runs, so a call names itself.
 */
static void be_the_task(void)
{
#if OSAL_RTOS == OSAL_RTOS_THREADX
    running = &task.thread;
#else
    running = &test_thread_object;
#endif
}

/*****************************************************************************************************/
/**
 * @brief Make another task the one the CPU runs, as the caller of every test is.
 */
static void be_another_task(void)
{
#if OSAL_RTOS == OSAL_RTOS_THREADX
    running = &other_thread;
#else
    running = &other_thread_object;
#endif
}

/*****************************************************************************************************/
/**
 * @brief Start the task osal made, as the RTOS would: its entry, with what the RTOS was given.
 */
static void run_created_task(void)
{
    be_the_task();

#if OSAL_RTOS == OSAL_RTOS_THREADX
    created_thread->entry(created_thread->input);
#else
    created_entry(created_argument);
#endif
}

/*****************************************************************************************************/
/**
 * @brief Whether the running task stopped for good: slept for ever, or ended itself on ThreadX.
 *
 * @return true when it did.
 */
static bool stopped_for_good(void)
{
    return ((rtos_sleeps > 0) && (last_sleep == 0xFFFFFFFFU)) || (self_ends > 0);
}
#endif
