#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

static void log_phase(const char *phase)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] PID %d -> %s\n",
           t->tm_hour, t->tm_min, t->tm_sec, getpid(), phase);
    fflush(stdout);
}

int main(void)
{
    printf("state_probe started. PID = %d\n", getpid());
    fflush(stdout);

    /* -------- Phase 1: CPU-bound busy loop (~15s) -> 'R' -------- */
    log_phase("PHASE 1: CPU-bound busy loop starting (expect state R)");
    {
        time_t start = time(NULL);
        volatile unsigned long counter = 0;
        while (time(NULL) - start < 15) {
            counter++;              /* pure computation, no syscalls */
        }
    }

    /* -------- Phase 2: blocking sleep (~15s) -> 'S' -------- */
    log_phase("PHASE 2: sleep() starting (expect state S)");
    sleep(15);

    /* -------- Phase 3: blocking disk I/O (~5s) -> possibly 'D' ---- */
    log_phase("PHASE 3: disk write/fsync starting (expect brief state D)");
    {
        FILE *fp = fopen("/tmp/state_probe_scratch.dat", "w");
        if (fp) {
            char buf[4096];
            for (int i = 0; i < 4096; i++) buf[i] = (char)(i % 256);
            for (int i = 0; i < 2000; i++) {   /* ~8 MB written */
                fwrite(buf, 1, sizeof(buf), fp);
            }
            fflush(fp);
            fsync(fileno(fp));   /* forces the kernel to wait on the device */
            fclose(fp);
            remove("/tmp/state_probe_scratch.dat");
        }
    }

    /* -------- Phase 4: terminate -> 'Z' until reaped -------- */
    log_phase("PHASE 4: exiting now (expect state Z until parent reaps it)");
    return 0;
}
