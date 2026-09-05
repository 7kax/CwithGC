#include "gc.h"

/**
 * @brief Verify that an oversized allocation terminates the program via abort.
 *
 * Test procedure:
 * 1. The program should normally terminate when it requests an oversized allocation.
 * 2. The signal handler converts the expected abort into a successful exit.
 * 3. Reaching the end of main means that the allocation did not abort and the test failed.
 */

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

// Unsafe version.
int _main() {
    int *ptr;

    // Allocate memory normally.
    ptr = malloc(sizeof(int) * 10);

    // Request an oversized allocation; malloc should return NULL.
    ptr = malloc(__LONG_LONG_MAX__);

    return 0;
}

// Signal handler for the signal raised by abort.
void signal_handler(int sig) {
    if (sig == SIGABRT) {
        exit(EXIT_SUCCESS); // Exit successfully because the expected abort occurred.
    }
}

int main() {
    // Register the handler for SIGABRT.
    signal(SIGABRT, signal_handler);

    gc_init();

    // Read the current heap size.
    size_t heap_size = gc_heap_size();

    // Read the current free-space size.
    size_t free_size = gc_free_size();

    // Define a pointer for the allocated memory.
    void *ptr;
    gc_local_var(&ptr);

    // Allocate some memory first to confirm that the collector works normally.
    ptr = gc_malloc(free_size / 4);
    assert(ptr != NULL);

    // Trigger one garbage collection.
    gc_collect();

    // Refresh the free-space size.
    free_size = gc_free_size();

    // Request twice the available space, which should fail and call std::abort.
    ptr = gc_malloc(free_size * 2);

    // Reaching this point means the library did not abort after allocation failure.

    // Clean up resources.
    gc_pop();
    gc_cleanup();

    // Reaching this point means the test failed.
    return EXIT_FAILURE;
}
