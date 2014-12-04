#include "473_mm.h"

void *VM_START;
int VM_SIZE;
int NUMBER_OF_FRAMES;
int PAGE_SIZE;
int POLICY;

static void segv_handler(int sig, siginfo_t *si, void *unused) {
    printf("sig=%d address=%p\n", sig, si->si_addr);
    exit(1);
}

void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy) {
    printf("vm=%p vm_size=%d n_frames=%d page_size=%d policy=%d\n", vm, vm_size, n_frames, page_size, policy);

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

    // SEGSIGV handler test
    mprotect(vm, page_size, PROT_NONE);
    int* test = vm;
    *(test + 1) = 5;

    return;
}

unsigned long mm_report_npage_faults() {
    printf("mm_report_npage_faults\n");
    return 0;
}

unsigned long mm_report_nwrite_backs() {
    printf("mm_report_nwrite_backs\n");
    return 0;
}
