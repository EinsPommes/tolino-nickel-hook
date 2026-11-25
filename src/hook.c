/* tolino-nickel-hook - Stage B: render reading stats on the e-ink.
 *
 * Loaded into Nickel via LD_PRELOAD. It touches no Qt and no libnickel symbol:
 * it only reads /proc, watches a flag file, and shells out to a render script.
 * All the real work (reading the library DB, drawing with fbink) lives in that
 * script, so this C stays tiny and safe.
 *
 * Scope: LD_PRELOAD is inherited by every child Nickel spawns, so the first
 * thing we do is check we are the real `nickel` process and bail out of all the
 * helpers. Only the main process starts the worker thread.
 *
 * Trigger: a short while after load (home screen settled) we render once, then
 * keep watching a flag file so a render can be requested again without reboot.
 *
 * The render script is taken from the USB drive if present, so its logic can be
 * iterated without reflashing; otherwise the baked copy is used.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_PATH      "/tmp/hook-alive.log"
#define FLAG_PATH     "/mnt/onboard/.custom/SHOW_STATS"
#define RENDER_DRIVE  "/mnt/onboard/.custom/show-stats.sh"
#define RENDER_BAKED  "/opt/custom/show-stats.sh"
#define SETTLE_SECONDS 20

static void note(const char *message)
{
    FILE *log = fopen(LOG_PATH, "a");
    if (log == NULL) {
        return;
    }
    fprintf(log, "%s (pid %ld)\n", message, (long) getpid());
    fclose(log);
}

static int running_as_nickel(void)
{
    char comm[64] = {0};
    FILE *f = fopen("/proc/self/comm", "r");
    if (f == NULL) {
        return 0;
    }
    char *got = fgets(comm, sizeof comm, f);
    fclose(f);
    if (got == NULL) {
        return 0;
    }
    comm[strcspn(comm, "\n")] = '\0';
    return strcmp(comm, "nickel") == 0;
}

static void render_stats(void)
{
    const char *cmd = (access(RENDER_DRIVE, F_OK) == 0)
                          ? "sh " RENDER_DRIVE
                          : "sh " RENDER_BAKED;
    if (system(cmd) == -1) {
        note("render: could not start render script");
    }
}

static void *worker(void *unused)
{
    (void) unused;

    note("stage B: worker started");
    sleep(SETTLE_SECONDS);
    note("stage B: initial render");
    render_stats();

    struct stat st;
    for (;;) {
        if (stat(FLAG_PATH, &st) == 0) {
            unlink(FLAG_PATH);   /* consume the trigger before rendering */
            note("stage B: triggered render");
            render_stats();
        }
        sleep(2);
    }
    return NULL;
}

__attribute__((constructor))
static void tnh_init(void)
{
    if (!running_as_nickel()) {
        return;   /* a child/helper process, not the reader itself */
    }
    note("stage B hook active in nickel");

    pthread_t thread;
    if (pthread_create(&thread, NULL, worker, NULL) == 0) {
        pthread_detach(thread);
    } else {
        note("stage B: could not start worker thread");
    }
}
