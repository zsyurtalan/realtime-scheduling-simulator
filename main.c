
//UNIVERSITY OF THESSALY
//E604 REAL-TIME SYSTEMS ASSIGNMENT 2

//necessary libraries
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <math.h>
#include <sys/syscall.h> 
#define NUM_ITERATIONS 8 //every task works for 8 iterations
#ifdef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif
//sched_attr struct for SCHED_DEADLINE
struct sched_attr {
    unsigned int size;
    unsigned int sched_policy;
    unsigned long long sched_flags;
    int sched_nice;
    unsigned int sched_priority;
    unsigned long long sched_runtime;   
    unsigned long long sched_deadline;  
    unsigned long long sched_period;    
};
 
static int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags){
    return syscall(SYS_sched_setattr, pid, attr, flags);
}
//result table
typedef struct {
    const char *label;
    int total_tasks;
    int total_misses;
    double avg_response;
    double avg_latency;
} Result;
static Result results[20];
static int result_count = 0;
//task structure
typedef struct {
    int id;
    const char *name;
    long period_ms;
    long exec_ms;
    long deadline_ms;
    int priority;
    int miss_count;
    long total_latency_ms;
    long total_response_ms;
    int jobs_done;
}Task;
//base task set
static Task base_tasks[4] = {
    //id,name,period,execution time,deadline,priority,miss count,total_lat,total_resp,jobs_done
    {1, "T1", 50, 10, 50, 0, 0,0 ,0,0},
    {2, "T2", 100, 20, 100, 0, 0 ,0 ,0,0},
    {3, "T3", 200, 40, 200, 0, 0, 0,0,0},
    {4, "T4", 400, 80, 400, 0, 0, 0,0,0},
};
//current time in ms
static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}
//This busy_work() function impose CPU load by sin cos operations
static void busy_work(long ms){
    long end=now_ms()+ms;
    volatile double x=0.5;
    while (now_ms()<end){
        x+=sin(x)*cos(x);
    }
}
//equal priority
static void assign_equal(Task tasks[],int n){
    for(int i=0;i<n;i++){
        tasks[i].priority=50; //all tasks have same priority
    }
}
//RMS 
static void assign_rms(Task tasks[],int n){
    Task sorted[4];
    memcpy(sorted,tasks,n*sizeof(Task));
    for(int i=0;i<n-1;i++){
        for(int j=i+1;j<n;j++){
            if(sorted[j].period_ms<sorted[i].period_ms){
                Task tmp=sorted[i];
                sorted[i]=sorted[j];
                sorted[j]=tmp;
            }
        }
    }
    int priority_levels[]={99,80,60,40}; //higher priority for shorter period
    for (int rank=0;rank<n;rank++){
        for(int i=0;i<n;i++){
            if(tasks[i].id==sorted[rank].id){
                tasks[i].priority=priority_levels[rank];
                break;
            }
        }
    }
}
//dynamic initial
static void assign_dynamic(Task tasks[],int n){
    for(int i=0;i<n;i++){
        tasks[i].priority=50; //initial priority based on period
    }
}
//standart task thread
static  void *task_thread(void *arg){
    Task *t=(Task *)arg;
    struct sched_param sp;
    sp.sched_priority=t->priority;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset); // pin to core 0
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    pthread_setschedparam(pthread_self(),SCHED_FIFO,&sp);
    long period_start=now_ms();
    for (int i=0;i<NUM_ITERATIONS;i++){
        long release=period_start;
        long deadline=release+t->deadline_ms;
        long start=now_ms();
        busy_work(t->exec_ms);
        long finish=now_ms();
        long latency=start-release;
        long response=finish-release;
        t->total_latency_ms+=latency;
        t->total_response_ms+=response;
        t->jobs_done++;
        if(finish>deadline){
            t->miss_count++;
    }
    /*
    //you can see latency,response time and deadline miss status for each task iteration
    printf("[%s] L:%ldms R:%ldms MISS:%d\n",
                t->name, latency, response,
               (finish > deadline)); */
    period_start += t->period_ms;
    struct timespec ts;
    ts.tv_sec = period_start / 1000;
    ts.tv_nsec = (period_start % 1000) * 1000000L;
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
    }
    return NULL;
}
//dynamic priority thread
static void *task_thread_dynamic(void *arg){
    Task *t=(Task *)arg;
    struct sched_param sp;
    sp.sched_priority=t->priority;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset); // pin to core 0
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    pthread_setschedparam(pthread_self(),SCHED_FIFO,&sp);
    long period_start=now_ms();
    for (int i=0;i<NUM_ITERATIONS;i++){
        long release=period_start;
        long deadline=release+t->deadline_ms;
        long start=now_ms();
        busy_work(t->exec_ms);
        long finish=now_ms();
        long latency=start-release;
        long response=finish-release;
        t->total_latency_ms+=latency;
        t->total_response_ms+=response;
        t->jobs_done++;
        if(finish>deadline){
            t->miss_count++;
            int new_priority=50+t->miss_count*5; //increase priority by 5 for each miss
            if(new_priority>99) new_priority=99; //cap at max
            sp.sched_priority=new_priority;
            pthread_setschedparam(pthread_self(),SCHED_FIFO,&sp);
            t->priority=new_priority; //update task priority for next iteration
        }
        period_start += t->period_ms;
        struct timespec ts;
        ts.tv_sec = period_start / 1000;
        ts.tv_nsec = (period_start % 1000) * 1000000L;
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
    }
    return NULL;
}
//deadline thread
static void *task_thread_deadline(void *arg){
    Task *t=(Task *)arg;
    struct sched_attr attr={0};
    attr.size=sizeof(attr);
    attr.sched_policy=SCHED_DEADLINE;
    attr.sched_runtime=t->exec_ms*1000000L*2; //convert ms to ns
    attr.sched_deadline=t->deadline_ms*1000000L;
    attr.sched_period=t->period_ms*1000000L;
    sched_setattr(0,&attr,0);
    long period_start=now_ms();
    for (int i=0;i<NUM_ITERATIONS;i++){
        long release=period_start;
        long deadline=release+t->deadline_ms;
        long start=now_ms();
        busy_work(t->exec_ms);
        long finish=now_ms();
        long latency=start-release;
        long response=finish-release;
        t->total_latency_ms+=latency;
        t->total_response_ms+=response;
        t->jobs_done++;
        if(finish>deadline){
            t->miss_count++;
        }
        period_start += t->period_ms;
        struct timespec ts;
        ts.tv_sec = period_start / 1000;
        ts.tv_nsec = (period_start % 1000) * 1000000L;
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
    }
    return NULL;
}
//run mode
static void run_mode(const char *label,
        Task tasks[],int n,
        int policy,
        void *(*fn)(void *)){
    //printf("=== %s ===\n", label);
    pthread_t th[n];
    for(int i=0;i<n;i++){
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setschedpolicy(&attr, policy);
        struct sched_param sp;
        sp.sched_priority=tasks[i].priority;
        pthread_attr_setschedparam(&attr, &sp);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_create(&th[i],&attr,fn,&tasks[i]);
    }
    for(int i=0;i<n;i++){
        pthread_join(th[i], NULL);
    }
    int total_misses=0;
    double avg_latency=0,avg_response=0;
    for(int i=0;i<n;i++){
        total_misses+=tasks[i].miss_count;
        avg_latency+=tasks[i].jobs_done>0? (double)tasks[i].total_latency_ms/tasks[i].jobs_done : 0.0;
        avg_response+=tasks[i].jobs_done>0? (double)tasks[i].total_response_ms/tasks[i].jobs_done : 0.0;
    }
    avg_latency/=n;
    avg_response/=n;
    results[result_count++] = (Result){label, n, total_misses, avg_response, avg_latency};
    
}
//main function
int main(){
    Task t1[4],t2[4],t3[4];
    memcpy(t1,base_tasks,sizeof(base_tasks));
    memcpy(t2,base_tasks,sizeof(base_tasks));
    memcpy(t3,base_tasks,sizeof(base_tasks));
    //CFS
    assign_equal(t1,4);
    run_mode("CFS (SCHED_OTHER)", t1, 4, SCHED_OTHER, task_thread);
    //FIFO
    memcpy(t1,base_tasks,sizeof(base_tasks));//every experiment begins with a clean set of tasks
    assign_equal(t1,4);
    run_mode("FIFO+Equal Priority", t1, 4, SCHED_FIFO, task_thread);
    memcpy(t1,base_tasks,sizeof(base_tasks));
    assign_rms(t1,4);
    run_mode("FIFO+RMS Priority", t1, 4, SCHED_FIFO, task_thread);
    memcpy(t1, base_tasks, sizeof(base_tasks));
    assign_dynamic(t1,4);
    run_mode("FIFO+Dynamic Priority", t1, 4, SCHED_FIFO, task_thread_dynamic);
    //RR
    memcpy(t2, base_tasks, sizeof(base_tasks));
    assign_equal(t2,4);
    run_mode("RR+Equal Priority", t2, 4, SCHED_RR, task_thread);
    memcpy(t2, base_tasks, sizeof(base_tasks));
    assign_rms(t2,4);
    run_mode("RR+RMS Priority", t2, 4, SCHED_RR, task_thread);
    memcpy(t3, base_tasks, sizeof(base_tasks));
    assign_dynamic(t3,4);
    run_mode("RR+Dynamic Priority", t3, 4, SCHED_RR, task_thread_dynamic);
    //Deadline  
    memcpy(t3,base_tasks,sizeof(base_tasks));
    run_mode("SCHED_DEADLINE", t3, 4, SCHED_DEADLINE, task_thread_deadline);
    printf("| %-25s | %-11s | %-15s | %-16s | %-16s |\n",
       "Scheduler", "Total Tasks", "Deadline Misses", "Avg Response(ms)", "Avg Latency(ms)");
    
    for(int i = 0; i < result_count; i++){
      printf("| %-25s | %11d | %15d | %13.2f ms | %13.2f ms |\n",
      results[i].label, results[i].total_tasks,
      results[i].total_misses, results[i].avg_response, results[i].avg_latency);
    } 
    result_count = 0;
    //CFS
    memcpy(t1,base_tasks,sizeof(base_tasks));
    assign_equal(t1,4);
    run_mode("CFS (SCHED_OTHER)", t1, 4, SCHED_OTHER, task_thread);
    //FIFO
    memcpy(t1,base_tasks,sizeof(base_tasks));//every experiment begins with a clean set of tasks
    assign_equal(t1,4);
    run_mode("PIN: FIFO+Equal Priority", t1, 4, SCHED_FIFO, task_thread);
    memcpy(t1,base_tasks,sizeof(base_tasks));
    assign_rms(t1,4);
    run_mode("PIN : FIFO+RMS Priority", t1, 4, SCHED_FIFO, task_thread);
    memcpy(t1, base_tasks, sizeof(base_tasks));
    assign_dynamic(t1,4);
    run_mode("PIN :FIFO+Dynamic Priority", t1, 4, SCHED_FIFO, task_thread_dynamic);
    //RR
    memcpy(t2, base_tasks, sizeof(base_tasks));
    assign_equal(t2,4);
    run_mode("PIN : RR+Equal Priority", t2, 4, SCHED_RR, task_thread);
    memcpy(t2, base_tasks, sizeof(base_tasks));
    assign_rms(t2,4);
    run_mode("PIN : RR+RMS Priority", t2, 4, SCHED_RR, task_thread);
    memcpy(t3, base_tasks, sizeof(base_tasks));
    assign_dynamic(t3,4);
    run_mode("PIN : RR+Dynamic Priority", t3, 4, SCHED_RR, task_thread_dynamic);
    //Deadline  
    memcpy(t3,base_tasks,sizeof(base_tasks));
    run_mode("PIN : SCHED_DEADLINE", t3, 4, SCHED_DEADLINE, task_thread_deadline);
    printf("EXPERIMENTS WITH CPU PINNING (core 0)\n");
    printf("| %-25s | %-11s | %-15s | %-16s | %-16s |\n",
       "Scheduler", "Total Tasks", "Deadline Misses", "Avg Response(ms)", "Avg Latency(ms)");
    
    for(int i = 0; i < result_count; i++){
      printf("| %-25s | %11d | %15d | %13.2f ms | %13.2f ms |\n",
      results[i].label, results[i].total_tasks,
      results[i].total_misses, results[i].avg_response, results[i].avg_latency);
    } 
    return 0;
}
