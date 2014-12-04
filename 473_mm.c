#include "473_mm.h"

// Data Structures
typedef struct virtual_page virtual_page;
struct virtual_page {
    int number;
    void* start;
    int size;
    int modified;
    virtual_page *next;
};

typedef struct virtual_page_queue virtual_page_queue;
struct virtual_page_queue {
    virtual_page *head;
    int size;
};

// Function prototypes
void handle_segv_fifo(siginfo_t *si);
void handle_segv_clock(siginfo_t *si);

// Global variables
void *VM_START;
int VM_SIZE;
int NUMBER_OF_FRAMES;
int PAGE_SIZE;
int POLICY;
int FAULT_COUNT = 0;
int WRITE_BACK_COUNT = 0;
virtual_page_queue *PAGE_QUEUE;

static void segv_handler(int sig, siginfo_t *si, void *unused) {
    printf("[SEGV HANDLER] sig=%d address=%p\n", sig, si->si_addr);

    if (POLICY == 1) {
        handle_segv_fifo(si);
    } else if (POLICY == 2) {
        handle_segv_clock(si);
    } else {
        printf("Invalid policy.\n");
        exit(EXIT_FAILURE);
    }
    // For testing. don't need to exit if done correctly
    exit(EXIT_FAILURE);
}

void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy) {
    printf("[mm_init] vm=%p vm_size=%d n_frames=%d page_size=%d policy=%d\n", vm, vm_size, n_frames, page_size, policy);

    // Initialize global variables
    VM_START = vm;
    VM_SIZE = vm_size;
    NUMBER_OF_FRAMES = n_frames;
    PAGE_SIZE = page_size;
    POLICY = policy;

    // Initialize SIGSEGV handler
    struct sigaction segv_action;
    segv_action.sa_flags = SA_SIGINFO;
    sigemptyset(&segv_action.sa_mask);
    segv_action.sa_sigaction = segv_handler;
    if (sigaction(SIGSEGV, &segv_action, NULL) == -1) {
        printf("sigaction failed\n");
    }

    PAGE_QUEUE = malloc(sizeof(PAGE_QUEUE));
    mprotect(vm, vm_size, PROT_NONE);

    return;
}

unsigned long mm_report_npage_faults() {
    return FAULT_COUNT;
}

unsigned long mm_report_nwrite_backs() {
    return WRITE_BACK_COUNT;
}

void handle_segv_fifo(siginfo_t *si) {

    // Find page in queue from address
    // If the page is in the queue -> This means we're trying to write to a page already in virtual memory (Not a page fault)
    //      mprotect it with PROT_READ|PROT_WRITE
    //      set modified field for page = 1
    // Else -> Means we're accessing a page that was evicted or was never brought into virtual memory yet
    //      add new page to the queue
    //          -> check if size of queue is = NUMBER_OF_FRAMES. If it is, need to do eviction.
    //              When evicting a page, mprotect it with PROT_READ|PROT_WRITE
    //      mprotect new page with PROT_READ
    //      increment FAULT_COUNT

}

void handle_segv_clock(siginfo_t *si) {

}
