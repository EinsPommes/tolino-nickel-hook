/* tolino-nickel-hook - Stage A: proof of life.
 *
 * Loaded into the Nickel process via LD_PRELOAD. Its only job is to prove that
 * our code runs inside Qt6 Nickel: the constructor appends one line to
 * /tmp/hook-alive.log and returns. It deliberately touches no Qt and no
 * libnickel symbol, so it cannot crash or hang the reader UI.
 *
 * Log goes to tmpfs (/tmp), never to /mnt/onboard, so it never holds the USB
 * drive open (which would break "Connect to PC").
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define HOOK_LOG "/tmp/hook-alive.log"

__attribute__((constructor))
static void tnh_proof_of_life(void)
{
    FILE *log = fopen(HOOK_LOG, "a");
    if (log == NULL) {
        return;
    }

    char stamp[32];
    time_t now = time(NULL);
    struct tm parts;
    if (localtime_r(&now, &parts) != NULL &&
        strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", &parts) > 0) {
        fprintf(log, "%s hook loaded in pid %ld\n", stamp, (long) getpid());
    } else {
        fprintf(log, "hook loaded in pid %ld\n", (long) getpid());
    }

    fclose(log);
}
