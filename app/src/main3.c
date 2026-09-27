/*
 * Lecture 3 - Homework BONUS: Debounce with a delayable work item
 *
 * sensor_sim fires bursts of 5 events within ~20ms.
 * Each event calls k_work_reschedule(&debounce_work, 30ms), which
 * pushes the deadline out again. The handler therefore runs ONCE,
 * ~30ms after the LAST event of each burst - not 5 times.
 *
 * Build this INSTEAD of main.c (both define main()).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(homework_bonus, LOG_LEVEL_DBG);

#define STACK_SIZE         1024
#define BURST_COUNT        5      /* number of bursts to generate        */
#define EVENTS_PER_BURST   5      /* events inside one burst             */
#define BURST_SPACING_MS   4      /* 5 events * 4ms -> burst spans ~16ms */
#define BURST_GAP_MS       200    /* quiet time between bursts           */
#define DEBOUNCE_MS        30     /* settle time before handler runs     */

static atomic_t pending_events;   /* raw events since last handler run */
static int total_events;
static int total_handler_calls;
static uint32_t last_event_tick;

/* ------------------------------------------------------------------ */
/*  Debounced handler - runs once per burst                            */
/* ------------------------------------------------------------------ */

static void sensor_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    uint32_t now = k_uptime_get_32();
    int collapsed = (int)atomic_clear(&pending_events);

    total_handler_calls++;

    LOG_INF("[HANDLER] call %d  tick=%u  collapsed=%d events  "
            "settle=%u ms after last event",
            total_handler_calls, now, collapsed, now - last_event_tick);

    if (total_handler_calls == BURST_COUNT) {
        LOG_INF("[SUMMARY] raw_events=%d  handler_calls=%d  saved=%d",
                total_events, total_handler_calls,
                total_events - total_handler_calls);
    }
}

K_WORK_DELAYABLE_DEFINE(debounce_work, sensor_handler);

/* ------------------------------------------------------------------ */
/*  sensor_sim - BURST_COUNT bursts of EVENTS_PER_BURST rapid events   */
/* ------------------------------------------------------------------ */

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    for (int b = 0; b < BURST_COUNT; b++) {
        k_msleep(BURST_GAP_MS);

        LOG_INF("[SENSOR] --- burst %d start ---", b);

        for (int e = 0; e < EVENTS_PER_BURST; e++) {
            if (e > 0) {
                k_msleep(BURST_SPACING_MS);
            }

            total_events++;
            atomic_inc(&pending_events);
            last_event_tick = k_uptime_get_32();

            /*
             * Restart the 30ms timer on every event. With a non-zero
             * delay this returns 1 (scheduled) each time; the earlier
             * deadline is replaced, so only the last one fires.
             */
            int ret = k_work_reschedule(&debounce_work, K_MSEC(DEBOUNCE_MS));

            if (ret < 0) {
                LOG_ERR("reschedule failed: %d", ret);
            }

            LOG_INF("[SENSOR] burst %d event %d  reschedule tick=%u  ret=%d",
                    b, e, last_event_tick, ret);
        }
    }

    LOG_INF("[SENSOR] all bursts produced");
}

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_sim_fn, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
    LOG_INF("=== L3 Homework BONUS: Debounce ===");
    LOG_INF("%d bursts x %d events (%dms apart), debounce %dms",
            BURST_COUNT, EVENTS_PER_BURST, BURST_SPACING_MS, DEBOUNCE_MS);
    LOG_INF("Expect %d handler calls, not %d",
            BURST_COUNT, BURST_COUNT * EVENTS_PER_BURST);

    k_msleep(BURST_COUNT * (BURST_GAP_MS + EVENTS_PER_BURST * BURST_SPACING_MS)
             + DEBOUNCE_MS + 500);

    return 0;
}
