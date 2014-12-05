#include "473_mm.h"
#include "errno.h"

// Data Structures
typedef struct virtual_page virtual_page;
struct virtual_page {
    int number;
    void* start;
    int size;
    int modified;
    virtual_page *next;
    virtual_page *prev;
};

typedef struct virtual_page_queue virtual_page_queue;
struct virtual_page_queue {
    virtual_page *head;
    virtual_page *tail;
    int size;
};

// Function prototypes
void handle_segv_fifo(siginfo_t *si);
void handle_segv_clock(siginfo_t *si);

void protect (void*, int, int);


int isInQueue(int);
int getPageNum(void*);
struct virtual_page* getPageByPageNum(int);
struct virtual_page* pop();
struct virtual_page* push(void*);
int getQueueSize();
struct virtual_page* getNewPage(void*);

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
   // printf("[SEGV HANDLER] sig=%d address=%p\n", sig, si->si_addr);

    if (POLICY == 1) {
        handle_segv_fifo(si);
    } else if (POLICY == 2) {
        handle_segv_clock(si);
    } else {
        printf("Invalid policy.\n");
        exit(EXIT_FAILURE);
    }
    // For testing. don't need to exit if done correctly
    //exit(EXIT_FAILURE);
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

    int protection = 0;
    int pageNum = getPageNum(si->si_addr);
    void* pPage = VM_START + pageNum*PAGE_SIZE;

    if (isInQueue(pageNum)) {
        protection = PROT_READ|PROT_WRITE;
        struct virtual_page* pPage = getPageByPageNum(pageNum);
        pPage->modified = 1;
    } else {
        push(pPage);
        protection = PROT_READ;
        FAULT_COUNT++;
    }
    protect(pPage, PAGE_SIZE, protection);

}

void handle_segv_clock(siginfo_t *si) {

}

void protect (void* addr, int pageSize, int protection) {
    if (mprotect(addr, pageSize, protection) == -1) {
        printf("Error: errno=%d\n", errno);
        perror("SignalHandler could not mprotect.\n");
        exit(errno);
    }
    return;
}


int isInQueue(int pageNum) {
    struct virtual_page* curr = PAGE_QUEUE->head;
    if(PAGE_QUEUE->head->number<0)return 0;
    while (curr != NULL) {
        if (curr->number == pageNum) {
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

int getPageNum(void* pAddr) {
return (int)((pAddr - VM_START) / PAGE_SIZE);
}

struct virtual_page* getPageByPageNum(int pageNum) {
    if (PAGE_QUEUE->head == NULL) {
        return NULL;
    }
    if (PAGE_QUEUE->head->number == pageNum) {
        return PAGE_QUEUE->head;
    }
    struct virtual_page* curr = PAGE_QUEUE->head->next;
    while (curr != NULL && curr != PAGE_QUEUE->head) {
        if (curr->number == pageNum) {
            return curr;
        }
        curr = curr->next;
    }
    perror("getPageByPageNum returning NULL.\n");
    exit(-1);
    return NULL;
}



struct virtual_page* pop() {
    if (PAGE_QUEUE->tail == NULL) {
        perror("pop Tail is NULL.\n");
        exit(0);
        return NULL;
    }
    struct virtual_page* newTail = PAGE_QUEUE->tail->prev;
    if (newTail == NULL) {
        perror("pop newTail is NULL.\n");
        exit(0);
        return NULL;
    }
    if (PAGE_QUEUE->tail->modified) {
        WRITE_BACK_COUNT++;
    }
    newTail->next = NULL;
    free(PAGE_QUEUE->tail);
    PAGE_QUEUE->tail = newTail;
    return PAGE_QUEUE->tail;
}

int getQueueSize() {
    struct virtual_page* curr = PAGE_QUEUE->head;
    int size = 0;
    while (curr != NULL) {
        curr = curr->next;
        size++;
    }
    return size;
}

struct virtual_page* getNewPage(void* pAddr) {
    struct virtual_page* newPage = (struct virtual_page*) malloc(sizeof(struct virtual_page));
    newPage->prev = NULL;
    newPage->next = NULL;
    newPage->number = getPageNum(pAddr);
    newPage->modified = 0;
    newPage->start = pAddr;
}

struct virtual_page* push(void* pAddr) {
    struct virtual_page* newPage = getNewPage(pAddr);
    int pageNum = newPage->number;
    if (PAGE_QUEUE->head->number<0) {
        PAGE_QUEUE->head = newPage;
        PAGE_QUEUE->head->next = NULL;
        PAGE_QUEUE->head->prev = NULL;
    PAGE_QUEUE->tail = PAGE_QUEUE->head;
    return PAGE_QUEUE->head;
    }
    if (getQueueSize() == NUMBER_OF_FRAMES) {
        if (PAGE_QUEUE->tail == NULL) {
            printf("ERROR push Tail == NULL, exiting\n");
            exit(-1);
        }
        void* start = VM_START + PAGE_QUEUE->tail->number*PAGE_SIZE;
        protect(start, PAGE_SIZE, PROT_NONE);
        pop();
    }
    newPage->next = PAGE_QUEUE->head;
    PAGE_QUEUE->head->prev = newPage;
    newPage->prev = NULL;
    PAGE_QUEUE->head = newPage;
    return PAGE_QUEUE->head;
}

