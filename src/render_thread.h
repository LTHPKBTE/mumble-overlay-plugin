/*
 * render_thread.h — Dedicated render thread for the overlay window
 *
 * Spawns / stops a thread that runs overlay_window_frame() in a loop.
 */

#ifndef RENDER_THREAD_H
#define RENDER_THREAD_H

#include <stdbool.h>

#include "overlay_window.h"

/*
 * Start the render thread.
 * `cfg`   – initial window configuration
 * `poll`  – callback for fetching active speakers
 * `userdata` – opaque pointer passed to `poll`
 * Returns 0 on success.
 */
int render_thread_start(const overlay_config_t *cfg,
                        overlay_poll_speakers_fn poll, void *userdata);

/*
 * Signal the render thread to stop and wait for it to finish.
 */
void render_thread_stop(void);

/*
 * Returns true while the render thread is running.
 */
bool render_thread_is_running(void);

#endif /* RENDER_THREAD_H */
