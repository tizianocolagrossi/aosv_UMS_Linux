#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define STACK_DEFAULT_SIZE 12288

uint64_t  rsp;

int test_chiamata(int n){
    printf("mumero: %d\n", n);
    return 3;
}

/*
* In this function I will try to allocate memory and use it
* as a stack for the main function ad after i will try to call
* a dummy function
*/
int main (){

    {
        __asm__ __volatile__(        
            "movq %%rsp, %[sp] ;"
            : 
            [sp] "=r" (rsp)
        );
    }
    printf("Stack before change: %p\n", (void *) rsp);
    
    /* 
    * Here I allocate memory space for the stack 
    * and after i calculate the stack base in order
    * to use it, because the stack address(sp) decrease
    */
    int stack_size = STACK_DEFAULT_SIZE;
    void * stack_top = malloc(stack_size);
    void * stack_base  = stack_top + stack_size; 
    
    printf("expexted stack     : %p\n", (void *) stack_base);

    {
        void * new_bp = stack_base;
        __asm__ __volatile__(        
            "mov %[sp], %%rsp;"
            : :
            [sp] "r" (new_bp)
        );
    }
    
    {
        __asm__ __volatile__(        
            "movq %%rsp, %[sp] ;"
            : 
            [sp] "=r" (rsp)
        );
    }
    printf("Stack after change : %p\n", (void *) rsp);

    int N = test_chiamata(4);
    printf("mumero: %d\n", N);

    free(stack_top);
    exit(0);

}