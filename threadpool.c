#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <memory.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#define THREAD_POOL_SIZE 5
#define DO_SYS(syscall) do {		\
    if( (syscall) == -1 ) {		\
        perror( #syscall );		\
        exit(EXIT_FAILURE);		\
    }						\
} while( 0 )


///* Structs and custom types *///

// the pool holds a queue of this structure
typedef struct work_st{
      void (*routine) (int);	// the thread function
      int param;  				// argument to the function
      struct work_st* next;		// a pointer to the next task
} work_t;

// the threadpool struct
typedef struct _threadpool_st {
 	int num_threads;			// number of threads
	int qsize;	        		// number of tasks in the queue
	pthread_t *threads;			// pointer to threads array
	work_t* qhead;				// queue head pointer
	work_t* qtail;				// queue tail pointer
	pthread_mutex_t qlock;		// mutex for task queue updates
	pthread_cond_t q_not_empty;	// condition variable for waking threads up to work
	pthread_cond_t q_empty;		// condition variable for waking threads up to shut down
    int shutdown;            	// 1 if the pool is in destruction process
    int dont_accept;       		// 1 if destroy function has begun
} threadpool;

/* "dispatch_fn" declares a typed function pointer.
	a variable of type "dispatch_fn" points to a function
	with the following signature:
	void dispatch_function(char *params); */
typedef void (*dispatch_fn)(int);


///* Function prototypes *///

// threadpool
void* do_work(void* p);
threadpool* create_threadpool(int num_threads_in_pool);
void destroy_threadpool(threadpool* destroyme);
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, int param);


///* Function definitions *///


void* do_work(void* p) {
    if (p == NULL)
		return NULL;
    threadpool *tp = p;

    while (1) {
        // acquire the lock
        pthread_mutex_lock(&tp->qlock);
        // in case destruction has began
        if (tp->shutdown == 1) {
        	pthread_mutex_unlock(&(tp->qlock));
            return NULL;
        }
        // check if the queue is empty, go to sleep
        if (tp->qsize == 0 && tp->dont_accept == 1) {
            if (pthread_cond_signal(&tp->q_empty) != 0) {
                perror("pthread_cond_signal() error");
                return NULL;
            }
        }
        if (tp->qsize == 0) {
            if (pthread_cond_wait(&tp->q_not_empty, &tp->qlock) != 0) {
                perror("pthread_cond_wait() error");
                return NULL;
            }
        }
        if (tp->shutdown == 1) {
            // in case destruction has began
        	pthread_mutex_unlock(&(tp->qlock));
            pthread_exit(NULL);
            return NULL;
        } else {
            // if queue is not empty, take an assignment
            work_t *this_task = tp->qhead;
            if (tp->qsize == 1) {
                tp->qhead = NULL;
                tp->qtail = NULL;
            } else {
                tp->qhead = tp->qhead->next;
            }
            tp->qsize--;
            // release the lock
            pthread_mutex_unlock(&tp->qlock);
            if (this_task != NULL) {
                this_task->routine(this_task->param);
                free(this_task);
            }
        }
        if (tp->qsize == 0 && tp->dont_accept == 1) {
            if (pthread_cond_signal(&tp->q_empty) != 0) {
                perror("pthread_cond_signal() error");
                return NULL;
            }
        }
    }
    return NULL;
}


threadpool* create_threadpool(int num_threads_in_pool) {
    threadpool *tp = (threadpool*) malloc(sizeof(threadpool));
    if (tp == NULL) {
        perror("Error in threadpool allocation.\n");
        return NULL;
    }
    // init all tp values
    tp->num_threads = num_threads_in_pool;
    tp->qsize = 0;
    tp->qhead = NULL;
    tp->qtail = NULL;
    tp->shutdown = 0;
    tp->dont_accept = 0;
    // init lock and condvars
    if (pthread_mutex_init(&tp->qlock, NULL) != 0) {
        perror("pthread_mutex_init() error");
        return NULL;
    }
    if (pthread_cond_init(&tp->q_not_empty, NULL) != 0) {
        perror("pthread_cond_init() error");
        return NULL;
    }
    if (pthread_cond_init(&tp->q_empty, NULL) != 0) {
        perror("pthread_cond_init() error");
        return NULL;
    }
    // allocate the threads
    tp->threads = (pthread_t *) malloc(THREAD_POOL_SIZE * sizeof(pthread_t));
    if (tp->threads ==  NULL) {
        printf("Error with threads allocation in create_threadpool\n");
        free(tp);
        return NULL;
    }
    //create the threades
    int status = 0;
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        status = pthread_create(&tp->threads[i], NULL, do_work, tp);
        if (status != 0) {
            fputs("pthread create failed ", stderr);
            return NULL;
        }
    }

    return tp;
}


void destroy_threadpool(threadpool* destroyme) {
    // LOCK
	if (pthread_mutex_lock(&destroyme->qlock) != 0){
        perror("pthread_mutex_lock() error");
        return;
	}
    destroyme->dont_accept = 1;
    // wait for signal that queue is empty and can't get more job
    if (destroyme->qsize > 0) {
        if (pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock) != 0) {
            perror("pthread_cond_wait() error");
            return;
        }
    }
    // change shutdown flag to 1 - when thread see it he know to exit..
    destroyme->shutdown = 1;

	//unlock
	if(pthread_mutex_unlock(&destroyme->qlock)!=0){
        perror("pthread_mutex_unlock() error");
        return;
	}
    //wake all the threads so they find out that -->
    // --> shoudown is 1 and they exit
    if (pthread_cond_broadcast(&destroyme->q_not_empty) != 0) {
        perror("pthread_cond_broadcast() error");
        return;
    }
    int rc = 0;
    // collect all the zombies
    for (int i = 0; i < destroyme->num_threads; i++) {
        rc = pthread_join(destroyme->threads[i], NULL);
        if(rc) {
            printf("error in pthread join\n");
            return;
        }
    }
    //delete the lock
    if (pthread_mutex_destroy(&destroyme->qlock) != 0) {
        perror("pthread_cond_destroy() error");
        return;
    }
    if (pthread_cond_destroy(&destroyme->q_not_empty) != 0) {
        perror("pthread_cond_destroy() error");
        return;
    }
    if (pthread_cond_destroy(&destroyme->q_empty) != 0) {
        perror("pthread_cond_destroy() error");
        return;
    }
    // clean all the dynamic memory - malloc
    free(destroyme->threads);
    free(destroyme);
}


void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, int param) {
    if (from_me->dont_accept == 1) {
        printf("destroy function has begun - can't accept new job\n");
        return;
    }

    // init work_t element
    work_t * w1 = (work_t*) malloc(sizeof(work_t));
    w1->routine = dispatch_to_here;
    w1->param = param;
    w1->next = NULL;
    //lock before update from_me- threadpool
    pthread_mutex_lock(&from_me->qlock);
    //update the queue
    if (from_me->qsize != 0) {
        from_me->qtail->next = w1;
        from_me->qtail = w1;
	}
	else {
		from_me->qhead = w1;
		from_me->qtail = w1;
	}
    from_me->qsize++;
    pthread_mutex_unlock(&from_me->qlock);
    //raise a signal to say there is a new job
    if (pthread_cond_signal(&from_me->q_not_empty) != 0) {
        perror("pthread_cond_signal() error");
        return;
    }
}
