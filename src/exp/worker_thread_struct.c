#include <setjmp.h>

#define STACK_DEFAULT_SIZE 12288

/*
* * This struct contain the info for a worker thread
*   data like context (used by setjmp and longjmp)
*   the function to run the stack of the thread
*   and also the state of the thread 
*/
struct work_thread{
    
    jmp_buf context;

    void (* func) (void *);
    void *arg;

    void * stack_base;
    void * stack_top;
    int    stack_size;
    
    enum states{
        CREATED,
        WAITING,
        RUNNING
    }state;

};