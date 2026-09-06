#include "../test/test_debug.h"
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

// Signal handler for the signal raised by abort.
void signal_handler(int sig) {
    if (sig == SIGABRT) {
        exit(EXIT_SUCCESS); // Exit successfully because the expected abort occurred.
    }
}

int main(void) {
    // Register the handler for SIGABRT.
    signal(SIGABRT, signal_handler);

    gc_init();
    gc_scope_token scope = gc_scope_begin();

    // Read the current free-space size.
    size_t free_size = test_gc_free_bytes();

    // Define a pointer for the allocated memory.
    void *ptr;
    gc_local_var(&ptr);

    // Allocate some memory first to confirm that the collector works normally.
    ptr = gc_malloc(free_size / 4);
    assert(ptr != NULL);

    // Trigger one garbage collection.
    gc_collect();

    // Refresh the free-space size.
    free_size = test_gc_free_bytes();

    // Request twice the available space, which should fail and call std::abort.
    ptr = gc_malloc(free_size * 2);

    // Reaching this point means the library did not abort after allocation failure.

    // Clean up resources.
    gc_scope_end(scope);
    gc_cleanup();

    // Reaching this point means the test failed.
    return EXIT_FAILURE;
}
