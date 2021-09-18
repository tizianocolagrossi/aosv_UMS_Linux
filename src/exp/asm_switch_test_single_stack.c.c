#include <stdio.h>
#include <stdint.h>
#include <time.h>

uint64_t  rsp_current_stored;
uint64_t  rsp_to_restore;


void fun1();
void fun2();

// THIS IS ONLY A PoC

struct timespec start, finish, delta;

enum { NS_PER_SECOND = 1000000000 };

void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td)
{
    td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_nsec < 0)
    {
        td->tv_nsec += NS_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_nsec > 0)
    {
        td->tv_nsec -= NS_PER_SECOND;
        td->tv_sec++;
    }
}

/*
* this function save the context 
* is used only here to emulate a stored context and a call to a new funtion that 
* doesn't have a context stored (so is a new function)
*/
void save_context(){

     // context saved here (sp in global variable)
     {
        __asm__ __volatile__(
            "pushq %%rbx;"
            "pushq %%rbx;"
            "pushq %%r12;"
            "pushq %%r13;"
            "pushq %%r14;"
            "pushq %%r15;"
            "pushfq;"

            "movq %%rsp, %[sp] ;"
            : 
            [sp] "=m" (rsp_to_restore)
            
        );
    }
    // call to a function that doesn't have a context stored
    fun2();
}


/*
* this function here insted save the actial context (fun2) and restore the previous 
* one (fun1) and continue to run fun1
*/
void context_switch(){
    //save current flow (fun2)
    {
        __asm__ __volatile__(
            
            "pushq %%rbp;"
            "pushq %%rbx;"
            "pushq %%r12;"
            "pushq %%r13;"
            "pushq %%r14;"
            "pushq %%r15;"
            "pushfq;"

            "movq %%rsp, %[sp] ;"

            "movq %[spn], %%rsp;"

            "popfq;"
            "pop %%r15;"
            "pop %%r14;"
            "pop %%r13;"
            "pop %%r12;"
            "pop %%rbx;"
            "pop %%rbp;"

            
            : 
            [sp] "=m" (rsp_current_stored)
            :
            [spn] "m" (rsp_to_restore)
            
        );
    }
}



// 1st called
void fun1(){
    printf("fun1\n");
    save_context();
    clock_gettime( CLOCK_REALTIME, &finish );
    sub_timespec(start, finish, &delta);
    printf("%d.%.9ld\n", (int)delta.tv_sec, delta.tv_nsec);
    printf("fun1_1\n");
}


// switch here (only fun1 context stored before)
// now we save this context and repristinate the 
// previous one (fun1)
void fun2(){
    printf("fun2\n");
    clock_gettime( CLOCK_REALTIME, &start );
    context_switch();
}

int main(int argc, char** argv)
{          
    fun1();
    printf("main return\n");
    return 0;
}

/* output 
root@NODE-00:/mnt/e/GitHub/aosv_pj/scheduling2# ./a.out 
fun1
fun2
0.000000400 // time for switch (0.4us) approximate
fun1_1

*/