#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>

typedef void *(*worker_job)(void *);

pid_t gettid(void){
    return syscall(SYS_gettid);
}

struct workerThreadJobInfo {
   worker_job job;
   void * args_routine;
};

struct testJobArgs{
    int a;
    int b;
};
  

void *testJob(void *arguments){
    struct testJobArgs *args = (struct testJobArgs *)arguments;
    pid_t tid = gettid();
    printf("testJob tid %d args a %d b %d\n",tid, args->a, args->b);
    return NULL;
}

void *workerThreadJobWrapper(void *arguments){
    struct workerThreadJobInfo *args = (struct workerThreadJobInfo *)arguments;

    pid_t tid = gettid();
    pthread_t thId = pthread_self();

    printf("Thread tid:%d, thId:%ld created\n", tid, thId);
    
    args->job(args->args_routine);
}
   
int main(){
    pthread_t thread_id1, thread_id2;
    printf("Before Thread\n");

    struct testJobArgs myJobArgs;
    myJobArgs.a = 10;
    myJobArgs.b = 20;

    struct workerThreadJobInfo args;
    
    args.job = &testJob;
    args.args_routine = &myJobArgs;

    int r1 = pthread_create(&thread_id1, NULL, workerThreadJobWrapper, (void *)&args);
    int r2 = pthread_create(&thread_id2, NULL, workerThreadJobWrapper, (void *)&args);

    printf("%ld, %ld\n", thread_id1, thread_id2);

    pthread_join(thread_id1, NULL);
    pthread_join(thread_id2, NULL);
    exit(0);
}
