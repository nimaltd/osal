/**
 * @file        test_osal.c
 * @brief       Host unit tests for the osal library, built on Unity.
 * @version     1.0.0
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
 *              checked instantly.
 */

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "osal.h"

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
   the number of sleeps it chose. 0 lets every sleep return. */
static jmp_buf  escape;
static int      sleeps_to_escape = 0;

/* Everything the fake kernel caught osal doing wrong. */
static int        faults = 0;
static const char *fault = "none";

/* The exception the CPU is in, 0 in thread mode. */
static uint32_t ipsr = 0U;

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

static osal_mutex_t mutex;

#if (OSAL_RTOS == OSAL_RTOS_CMSIS_V1) || (OSAL_RTOS == OSAL_RTOS_CMSIS_V2)
/* What a CMSIS mutex handle points at. */
struct test_os_mutex
{
    int unused;
};
static struct test_os_mutex test_mutex_object;
#endif

/*
 * ****************************************************************************************************
 * Private function prototypes
 * ****************************************************************************************************
*/

static int    sleeps(void);
static void   escape_if_due(void);
#if OSAL_RTOS != OSAL_RTOS_NONE
static void   note_fault(const char *what);
static take_t mutex_take(bool right_mutex, uint32_t wait);
static void   mutex_give(bool right_mutex);
static void   rtos_sleep(uint32_t ticks, uint32_t ms);
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
    mutex_takes        = 0;
    mutex_gives        = 0;
    mutex_held         = 0;
    last_wait          = 0U;
    sleeps_to_escape   = 0;

    memset(&mutex, 0, sizeof(mutex));
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
 * @brief The thread that holds a mutex may take it again, and gives it back as often, on every port.
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
    RUN_TEST(test_a_mutex_may_be_taken_again_by_its_holder);
    RUN_TEST(test_a_second_create_gives_a_new_mutex);
    RUN_TEST(test_a_null_mutex_is_refused);
    RUN_TEST(test_a_mutex_locks_and_unlocks);
    RUN_TEST(test_a_delay_waits_as_long_as_asked);
    RUN_TEST(test_a_delay_of_zero_returns_at_once);
    RUN_TEST(test_a_delay_of_one_still_waits);
    RUN_TEST(test_a_delay_for_ever_never_returns);

#if OSAL_RTOS != OSAL_RTOS_NONE
    RUN_TEST(test_a_mutex_the_rtos_cannot_make_is_reported);
    RUN_TEST(test_a_mutex_held_elsewhere_times_out);
    RUN_TEST(test_a_refused_mutex_is_reported);
    RUN_TEST(test_the_mutex_wait_follows_the_timeout);
#endif

#if OSAL_RTOS == OSAL_RTOS_CMSIS_V1
    RUN_TEST(test_v1_is_given_milliseconds);
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
 * @brief Jump out of a delay that never returns, once a test's chosen number of sleeps is reached.
 */
static void escape_if_due(void)
{
    if ((sleeps_to_escape > 0) && (sleeps() >= sleeps_to_escape))
    {
        longjmp(escape, 1);
    }
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
#endif
