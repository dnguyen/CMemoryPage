#include "473_mm.h"
#include "errno.h"

// Data Structures
typedef struct virtual_page virtual_page;
struct virtual_page {
    int number;
    void* start;
    int size;
    int modified;
    int referenced;
    virtual_page *next;
    virtual_page *prev;
};

typedef struct virtual_page_queue virtual_page_queue;
struct virtual_page_queue {
    virtual_page *head;
    virtual_page *tail;
    virtual_page *hand;
    int size;
};

// Function prototypes
void handle_segv_fifo(siginfo_t *si);
void handle_segv_clock(siginfo_t *si);

int translate_to_page_number(void*);
virtual_page *get_page(virtual_page_queue*, void*);
void enqueue(virtual_page_queue*, virtual_page*);
virtual_page* dequeue(virtual_page_queue*);
virtual_page* init_page(int, void*, int, int);

// Functions for clock algorithm
void clock_init(virtual_page_queue*, int);
virtual_page *circular_get_page(virtual_page_queue*, void*);
void circular_enqueue(virtual_page_queue*, virtual_page*);
void circular_replace(virtual_page_queue*, virtual_page*, virtual_page*);

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
    if (POLICY == 1) {
        handle_segv_fifo(si);
    } else if (POLICY == 2) {
        handle_segv_clock(si);
    } else {
        printf("Invalid policy.\n");
        exit(EXIT_FAILURE);
    }
}

void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy) {

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

    // Initialize the queue
    PAGE_QUEUE = malloc(sizeof(virtual_page_queue));
    PAGE_QUEUE->head = NULL;
    PAGE_QUEUE->tail = NULL;
    PAGE_QUEUE->hand = NULL;
    PAGE_QUEUE->size = 0;

    if (POLICY == 2) {
        // Initially create n blank virtual pages and add them to the circular list
        clock_init(PAGE_QUEUE, n_frames);
    }

    // Protect entire memory space to fire initial segv
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

    int page_number = translate_to_page_number(si->si_addr);
    void* page_start_addr = VM_START + page_number * PAGE_SIZE;
    virtual_page *page = get_page(PAGE_QUEUE, si->si_addr);

    if (page != NULL) {
        // We know we're doing a write here because:
        //      - page is already in the queue
        //      - page was initially given PROT_READ when it was added to the queue
        // so, set the page as modified and allow reads and writes to the page
        page->modified = 1;
        mprotect(page->start, page->size, PROT_READ|PROT_WRITE);
    } else {
        virtual_page *new_page = init_page(page_number, page_start_addr, 0, 0);
        enqueue(PAGE_QUEUE, new_page);
        FAULT_COUNT++;

        // Since the page is now in the queue, allow reads to this page.
        mprotect(new_page->start, new_page->size, PROT_READ);
    }

}

void handle_segv_clock(siginfo_t *si) {
    
    int page_number = translate_to_page_number(si->si_addr);
    void* page_start_addr = VM_START + page_number * PAGE_SIZE;
    virtual_page *page = circular_get_page(PAGE_QUEUE, si->si_addr);

    if (page != NULL) {
        page->modified = 1;
        page->referenced = 1;
        mprotect(page->start, page->size, PROT_READ|PROT_WRITE);
    } else {
        virtual_page *new_page = init_page(page_number, page_start_addr, 0, 1);
        circular_enqueue(PAGE_QUEUE, new_page);
        FAULT_COUNT++;
        mprotect(new_page->start, new_page->size, PROT_READ);
    }
}

// Initializes blank referenced pages for clock algorithm
void clock_init(virtual_page_queue* queue, int n) {

    // Create initial head page, then attach the rest of the pages
    // Need to create n empty pages
    virtual_page *head_page = init_page(-1, VM_START + PAGE_SIZE, 0, 1);
    queue->head = head_page;
    queue->tail = head_page;

    // Attach empty pages to head page
    int i = 0;
    for (i = 0; i < n-1; i++) {
        void* page_start_addr = VM_START + i * PAGE_SIZE;
        virtual_page *new_page = init_page(-1, page_start_addr, 0, 1);
        new_page->prev = queue->tail;
        new_page->next = queue->head;
        queue->tail->next = new_page;
        queue->tail = new_page;
    }

    // Set the hand of the clock to the head. and Ensure the queue is circular.
    queue->hand = queue->head;
    queue->head->prev = queue->tail;
    queue->tail->next = queue->head;
}

// Returns the page that contains the request address
// If the page is not in the queue, return null
virtual_page *get_page(virtual_page_queue* queue, void* address) {
    int page_num = translate_to_page_number(address);

    virtual_page *current = queue->head;
    while (current != NULL) {
        if (current->number == page_num) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void enqueue(virtual_page_queue* queue, virtual_page* page) {
    // Check if we need to evict any pages first.
    if (queue->size >= NUMBER_OF_FRAMES) {
        virtual_page *evicted_page = dequeue(PAGE_QUEUE);
        // If the page was modified, increment the write back count.
        if (evicted_page->modified == 1) {
            WRITE_BACK_COUNT++;
        }
    }

    if (queue->head == NULL) {
        queue->head = page;
        queue->tail = page;
        page->next = NULL;
    } else {
        page->prev = queue->tail;
        queue->tail->next = page;
        queue->tail = page;
    }
    queue->size++;
}

virtual_page* dequeue(virtual_page_queue* queue) {
    virtual_page *temp = queue->head;
    if (queue->head->next != NULL) {
        queue->head = queue->head->next;
        queue->head->prev = NULL;
    } else {
        queue->head = NULL;
    }
    queue->size--;

    mprotect(temp->start, temp->size, PROT_NONE);

    return temp;
}

int translate_to_page_number(void* address) {
    return (int)((address - VM_START) / PAGE_SIZE);
}

virtual_page *circular_get_page(virtual_page_queue* queue, void* address) {
    int page_num = translate_to_page_number(address);

    if (queue->head != NULL) {
        // Keep traversing the list until we hit the head again.
        virtual_page *current = queue->head;
        do {
            if (current->number == page_num) {
                return current;
            }
            current = current->next;
        } while (current != queue->head);
    }

    return NULL;
}

void circular_enqueue(virtual_page_queue* queue, virtual_page* page) {
    virtual_page *current = queue->hand;
    while (current->referenced == 1) {
        current->referenced = 0;
        current = current->next;
    }

    circular_replace(PAGE_QUEUE, current, page);
    queue->hand = current->next;
}

// Replaces old_page with new_page
void circular_replace(virtual_page_queue* queue, virtual_page* old_page, virtual_page* new_page) {

    if (old_page->modified == 1) {
        WRITE_BACK_COUNT++;
    }

    new_page->next = old_page->next;
    new_page->prev = old_page->prev;

    old_page->prev->next = new_page;
    old_page->next->prev = new_page;
    if (old_page == queue->head) {
        queue->head = new_page;
    }

    if (old_page == queue->tail) {
        queue->tail = new_page;
    }
    // Ensure queue remains circular
    queue->head->prev = queue->tail;
    queue->tail->next = queue->head;

    // protect old page
    mprotect(old_page->start, old_page->size, PROT_NONE);

    free(old_page);
}

// Initializes a new virtual page
virtual_page* init_page(int number, void* start_addr, int modified, int referenced) {
    virtual_page *new_page = malloc(sizeof(virtual_page));

    new_page->start = start_addr;
    new_page->size = PAGE_SIZE;
    new_page->number = number;
    new_page->referenced = referenced;
    new_page->modified = modified;   

    return new_page;
}