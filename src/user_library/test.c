/* 
 * This file is part of the User Mode Thread Scheduling (Kernel Module).
 * Copyright (c) 2021 Tiziano Colagrossi.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief This file is a test file for the UMS module and library
 * 
 * @file test.c
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */

#include "test.h"

#define T1 "[thread 1] "
#define T2 "[thread 2] "
#define TT "[thread time] "

struct testJobArgs{
    int a;
    int b;
};

struct timespec start_s, finish_s, delta_s;

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

void *scheduler(void *arguments){
    printf(SCHED "start\n\n");
    dequeued_cq_t deq_wkt;

    int ret;

    ret = DequeueUmsCompletionListItems(&deq_wkt);
    printf(SCHED "\t ret of dequeue need to be 0 and is %d\n", ret);
    printf(SCHED "\t dequeued value (first 5) [%d, %d, %d, %d, %d]\n", deq_wkt.pids[0],  deq_wkt.pids[1],  deq_wkt.pids[2],  deq_wkt.pids[3],  deq_wkt.pids[4]);

    while(TRUE){
        ret = DequeueUmsCompletionListItems(&deq_wkt);
        if(ret == EXIT_UMS_MOD || ret == EXIT_FAILURE) break;
        ret = ExecuteUmsThread(deq_wkt.pids[0]);
        if(ret == THREAD_RUNNG) continue;
        if(ret == EXIT_FAILURE) break;
    }
    
    return NULL;
}

void *schedulerTimeEnter(void *arguments){
    printf(SCHED "start time enter\n\n");
    dequeued_cq_t deq_wkt;

    int ret;

    while(TRUE){
        ret = DequeueUmsCompletionListItems(&deq_wkt);
        if(ret == EXIT_UMS_MOD || ret == EXIT_FAILURE) break;
        ret = ExecuteUmsThread(deq_wkt.pids[0]);
        if(ret == THREAD_RUNNG) continue;
        if(ret == EXIT_FAILURE) break;
        clock_gettime( CLOCK_REALTIME, &finish_s );
        sub_timespec(start_s, finish_s, &delta_s);
        printf(SCHED "%d.%.9ld sec\n", (int)delta_s.tv_sec, delta_s.tv_nsec);
    }
    
    return NULL;
}

void *schedulerTimeExit(void *arguments){
    printf(SCHED "start time exit\n\n");
    dequeued_cq_t deq_wkt;

    int ret;

    while(TRUE){
        ret = DequeueUmsCompletionListItems(&deq_wkt);
        if(ret == EXIT_UMS_MOD || ret == EXIT_FAILURE) break;
        clock_gettime( CLOCK_REALTIME, &start_s );
        ret = ExecuteUmsThread(deq_wkt.pids[0]);
        if(ret == THREAD_RUNNG) continue;
        if(ret == EXIT_FAILURE) break;
    }
    
    return NULL;
}

void *testJob1(void *arguments){
    struct testJobArgs *args = (struct testJobArgs *)arguments;
    printf(T1 "testJob args a %d b %d\n", args->a, args->b);
    int ret = UmsThreadYield();
    for (int i = 0; i<10; i++){
        printf("dentro t1\n");
        UmsThreadYield();
        sleep(1);
    }

    // attempt to cause a page fault
    // printf(T1 "attempting a page fault\n");
    // int * arr = (int *) malloc(1000 * sizeof(int));
    // for(int i = 0; i<1000; i++){
    //     arr[i] = i;
    // }
    // printf(T1 "end write in mem\n");

    printf(T1 "after yield ret %d\n",ret);
    return NULL;
}

void *testJob2(void *arguments){
    struct testJobArgs *args = (struct testJobArgs *)arguments;
    int sum = args->a + args->b;
    printf(T2 "testJob args a %d b %d sum = %d\n", args->a, args->b, sum);
    for (int i = 0; i<10; i++){
        printf("dentro t2\n");

        UmsThreadYield();
        sleep(1);
    }
    return NULL;
}

void *testJob3(void *arguments){
    for (int i = 0; i<10; i++){
        printf("dentro t3\n");
        UmsThreadYield();
        sleep(1);
    }
    return NULL;
}

void *testJob4(void *arguments){
    for (int i = 0; i<10; i++){
        printf("dentro t4\n");
        UmsThreadYield();
        sleep(1);
    }
    return NULL;
}

void *testJob5(void *arguments){
    for (int i = 0; i<10; i++){
        printf("dentro t5\n");
        UmsThreadYield();
        sleep(1);
    }
    return NULL;
}

void *testJob6(void *arguments){
    for (int i = 0; i<10; i++){
        printf("dentro t6\n");
        UmsThreadYield();
        sleep(1);
    }
    return NULL;
}

void *testJob7(void *arguments){
    for (int i = 0; i<10; i++){
        printf("dentro t7\n");
        UmsThreadYield();
        sleep(1);
    }
    return NULL;
}

void *testJob8(void *arguments){
    for (int i = 0; i<10; i++){
        printf("dentro t8\n");
        UmsThreadYield();
        sleep(1);
    }
    return NULL;
}

void *testJobTimeEnter(void *arguments){
    for (int i=0; i<1000; i++){
        clock_gettime( CLOCK_REALTIME, &start_s );
        UmsThreadYield();
    }

    return NULL;
}

void *testJobTimeExit(void *arguments){
    for (int i = 0; i<1000; i++){
        UmsThreadYield();
        clock_gettime( CLOCK_REALTIME, &finish_s );
        sub_timespec(start_s, finish_s, &delta_s);
        printf(TT "%d.%.9ld sec\n", (int)delta_s.tv_sec, delta_s.tv_nsec);
    }

    return NULL;
}

int main(int argc, char **argv){
    int ret;

    printf("Start test UMS\n");

    printf("section 1: test completion queue creation\n");
    int new_id_cq1 = CreateCompletionQueue();
    
    // int new_id_cq2 = CreateCompletionQueue();
    // int new_id_cq3 = CreateCompletionQueue();
    // printf("\t created completion queue with id %d, %d and %d\n",new_id_cq1, new_id_cq2, new_id_cq3);
    printf("\t created completion queue with id %d\n",new_id_cq1);

    sleep(2);
    printf("\n\n\n");
    printf("section 2: test creation of worker thread\n");
    struct testJobArgs myJobArgs1;
    myJobArgs1.a = 10;
    myJobArgs1.b = 20;
    int pid_wkt1 = CreateNewWorker(&testJob1, (void *)&myJobArgs1);
    
    struct testJobArgs myJobArgs2;
    myJobArgs2.a = 30;
    myJobArgs2.b = 50;
    int pid_wkt2 = CreateNewWorker(&testJob2, (void *)&myJobArgs2);
    int pid_wkt3 = CreateNewWorker(&testJob3, NULL);
    printf("\t created wkt with pid %d, %d, and %d\n",pid_wkt1, pid_wkt2, pid_wkt3);

    sleep(2);
    printf("\n\n\n");
    printf("section 3: test cq append and flush and dequeue\n");
    sleep(1);
    ret = AppendToCompletionQueue(new_id_cq1, pid_wkt1);
    printf("\t attempt to append worker to a cq res: %d\n", ret);
    sleep(1);
    ret = AppendToCompletionQueue(new_id_cq1, pid_wkt2);
    printf("\t attempt to append worker to a cq res: %d\n", ret);
    sleep(1);
    ret = AppendToCompletionQueue(new_id_cq1, pid_wkt3);
    printf("\t attempt to append worker to a cq res: %d\n", ret);
    sleep(1);
    ret = FlushCompletionQueue(new_id_cq1);
    printf("\t attempt to flush a cq res: %d\n", ret);
    sleep(1);
    
    dequeued_cq_t deq_wkt;
    ret = DequeueUmsCompletionListItems(&deq_wkt);
    printf("\t ret of dequeue need to be -1 and is %d\n", ret);
    sleep(1);
    printf("\t a dequeue can be call only by an UMS\n");

    sleep(2);
    printf("\n\n\n");
    printf("section 4: Create 2 UMS thread that share the same completion queue\n");
    
    pthread_t ums1 = UMS_thread_create(scheduler, new_id_cq1, 0);
    pthread_t ums2 = UMS_thread_create(scheduler, new_id_cq1, 1);

    resetUMSFlag();
    pthread_join(ums1, NULL);
    pthread_join(ums2, NULL);


    printf("section 5: test UMS scheduler\n");

    int new_id_cq2 = CreateCompletionQueue();
    printf("\t created completion queue with id %d\n",new_id_cq1);

    int pid_wkt = CreateNewWorker(&testJob3, NULL);
    AppendToCompletionQueue(new_id_cq2, pid_wkt);

    pid_wkt = CreateNewWorker(&testJob4, NULL);
    AppendToCompletionQueue(new_id_cq2, pid_wkt);

    pid_wkt = CreateNewWorker(&testJob5, NULL);
    AppendToCompletionQueue(new_id_cq2, pid_wkt);

    pid_wkt = CreateNewWorker(&testJob6, NULL);
    AppendToCompletionQueue(new_id_cq2, pid_wkt);

    pid_wkt = CreateNewWorker(&testJob7, NULL);
    AppendToCompletionQueue(new_id_cq2, pid_wkt);

    pid_wkt = CreateNewWorker(&testJob8, NULL);
    AppendToCompletionQueue(new_id_cq2, pid_wkt);

    ret = FlushCompletionQueue(new_id_cq2);
    printf("\t start ums with default scheduler entry point and one sched thread for each core\n");
    ums_t * ums = EnterUmsSchedulingMode(NULL, new_id_cq2);
    sleep(10);
    
    printf("\t adding two more worker thread int the completion queue\n");
    pid_wkt = CreateNewWorker(&testJob1, (void *)&myJobArgs1);
    AppendToCompletionQueue(new_id_cq2, pid_wkt);
    pid_wkt = CreateNewWorker(&testJob2, (void *)&myJobArgs2);
    AppendToCompletionQueue(new_id_cq2, pid_wkt);
    ret = FlushCompletionQueue(new_id_cq2);

    ExitFromUmsSchedulingMode(ums);

    // sleep(20);

    // printf("section 5: Time calc\n");
    
    // int pid_wkt_enter = CreateNewWorker(&testJobTimeEnter, NULL);
    // int pid_wkt_exit = CreateNewWorker(&testJobTimeExit, NULL);
    
    // int new_id_cq2 = CreateCompletionQueue();
    // ret = AppendToCompletionQueue(new_id_cq2, pid_wkt_enter);
    // ret = FlushCompletionQueue(new_id_cq2);
    // printf("Time enter\n");
    // EnterUmsSchedulingMode(schedulerTimeEnter, new_id_cq2, 3);

    // sleep(10);

    // printf("Time exit\n");
    // int new_id_cq3 = CreateCompletionQueue();
    // ret = AppendToCompletionQueue(new_id_cq3, pid_wkt_exit);
    // ret = FlushCompletionQueue(new_id_cq3);

    // EnterUmsSchedulingMode(schedulerTimeExit, new_id_cq3, 0);

    // sleep(10);




    printf("Fine Test\n");
}




