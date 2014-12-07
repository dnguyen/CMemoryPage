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
void print_queue(virtual_page_queue *queue);
virtual_page *get_page(virtual_page_queue*, void*);
void enqueue(virtual_page_queue*, virtual_page*);
virtual_page* dequeue(virtual_page_queue*);

// Functions for clock algorithm
virtual_page *circular_get_page(virtual_page_queue*, void*);
void circular_enqueue(virtual_page_queue*, virtual_page*);
void circular_print_queue(virtual_page_queue*);
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

    // Initialize the queue
    PAGE_QUEUE = malloc(sizeof(virtual_page_queue));
    PAGE_QUEUE->head = NULL;
    PAGE_QUEUE->tail = NULL;
    PAGE_QUEUE->hand = NULL;
    PAGE_QUEUE->size = 0;

    // Protect entire memory space to fire initial segv
    printf("[PROTECTING ENTIRE VM] {%p - %p}\n", vm, vm+vm_size);
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

    int page_number = translate_to_page_number(si->si_addr);
    void* page_start_addr = VM_START + page_number * PAGE_SIZE;
    virtual_page *page = get_page(PAGE_QUEUE, si->si_addr);

    printf("[HANDLE SEGV] request address=%p page_num=%d\n", si->si_addr, page_number);
    if (page != NULL) {
        printf("\t[PAGE IN QUEUE] start=%p num=%d size=%d\n", page->start, page->number, page->size);
        print_queue(PAGE_QUEUE);

        // We know we're doing a write here because:
        //      - page is already in the queue
        //      - page was initially given PROT_READ when it was added to the queue
        // so, set the page as modified and allow reads and writes to the page
        page->modified = 1;
        mprotect(page->start, page->size, PROT_READ|PROT_WRITE);
    } else {
        printf("\t[PAGE NOT IN QUEUE]\n");
        virtual_page *new_page = malloc(sizeof(virtual_page));
        new_page->start = page_start_addr;
        new_page->size = PAGE_SIZE;
        new_page->number = page_number;
        enqueue(PAGE_QUEUE, new_page);
        print_queue(PAGE_QUEUE);
        FAULT_COUNT++;
        printf("\t[PROTECTING] page=%d {%p - %p}\n", new_page->number, new_page->start, new_page->start+new_page->size);
        // Since the page is now in the queue, allow reads to this page.
        mprotect(new_page->start, new_page->size, PROT_READ);
    }

}

void handle_segv_clock(siginfo_t *si) {
    
    int page_number = translate_to_page_number(si->si_addr);
    void* page_start_addr = VM_START + page_number * PAGE_SIZE;
    virtual_page *page = circular_get_page(PAGE_QUEUE, si->si_addr);

    printf("[HANDLE SEGV] request address=%p page_num=%d\n", si->si_addr, page_number);
    if (page != NULL) {
       printf("\t[PAGE IN QUEUE]\n");
       circular_print_queue(PAGE_QUEUE);
       page->modified = 1;
       mprotect(page->start, page->size, PROT_READ|PROT_WRITE);
    } else {
       printf("\t[PAGE NOT IN QUEUE]\n");
        virtual_page *new_page = malloc(sizeof(virtual_page));
        new_page->start = page_start_addr;
        new_page->size = PAGE_SIZE;
        new_page->number = page_number;
        new_page->referenced = 1;
        circular_enqueue(PAGE_QUEUE, new_page);
        circular_print_queue(PAGE_QUEUE);
        FAULT_COUNT++;
        mprotect(new_page->start, new_page->size, PROT_READ);
    }
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
    printf("\t[ENQUEUE] page=%d\n", page->number);
    // Check if we need to evict any pages first.
    if (queue->size >= NUMBER_OF_FRAMES) {
        printf("\t\t[MAX FRAMES]\n");
        virtual_page *evicted_page = dequeue(PAGE_QUEUE);
        // If the page was modified, increment the write back count.
        if (evicted_page->modified == 1) {
            WRITE_BACK_COUNT++;
        }
        print_queue(PAGE_QUEUE);
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
    printf("\t[DEQUEUE]\n");
    virtual_page *temp = queue->head;
    if (queue->head->next != NULL) {
        queue->head = queue->head->next;
        queue->head->prev = NULL;
    } else {
        queue->head = NULL;
    }
    queue->size--;

    printf("\t[PROTECTING] page num=%d {%p - %p}\n", temp->number, temp->start, temp->start+temp->size);
    mprotect(temp->start, temp->size, PROT_NONE);

    return temp;
}

int translate_to_page_number(void* address) {
    return (int)((address - VM_START) / PAGE_SIZE);
}

virtual_page *circular_get_page(virtual_page_queue* queue, void* address) {
    int page_num = translate_to_page_number(address);

    if (queue->head != NULL) {
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
    printf("\t[ENQUEUE] page=%d\n", page->number);

    if (queue->head == NULL) {
        queue->head = page;
        queue->tail = page;
        page->next = page;
        page->prev = queue->tail;
        queue->hand = page;
        queue->size++;
    } else {

        if (queue->size >= NUMBER_OF_FRAMES) {
            printf("\t\t[MAX FRAMES]\n");
            virtual_page *current = queue->hand;
            circular_print_queue(PAGE_QUEUE);
            while (current->referenced != 0) {
                current->referenced = 0;
                current = current->next;
            }
            queue->hand = current->next;
            printf("\t\t[FOUND PAGE TO EVICT] page=%d ref=%d\n", current->number, current->referenced);
            circular_replace(PAGE_QUEUE, current, page);
            circular_print_queue(PAGE_QUEUE);
            mprotect(page->start, page->size, PROT_READ);
        } else {
            queue->tail->next = page;
            page->prev = queue->tail;
            page->next = queue->head;
            queue->tail = page;
            queue->size++;
        }
    }

    // Ensure queue remains circular
    queue->head->prev = queue->tail;
    queue->tail->next = queue->head;
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

void print_queue(virtual_page_queue *queue) {
    virtual_page *current = queue->head;
    printf("\t[QUEUE] size=%d  ", queue->size);
    while (current != NULL) {
        printf("[%d {%p - %p}] ", current->number, current->start, current->start+current->size);
        current = current->next;
    }
    printf("\n");
}

void circular_print_queue(virtual_page_queue *queue) {
    virtual_page *current = queue->head;
    printf("\t[QUEUE] size=%d hand=%d ", queue->size, queue->hand->number);
    do {
        printf("[%d r=%d p=%d n=%d] ", current->number, current->referenced, current->prev->number, current->next->number);
        current = current->next;    
    } while (current != queue->head);
    printf("\n");
}