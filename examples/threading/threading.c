#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data *data = (struct thread_data *) thread_param;

    usleep(data->wait_to_obtain_ms * 1000);

    if (0 != pthread_mutex_lock(data->mutex)) {
        ERROR_LOG("Failed to lock mutex.");
        data->thread_complete_success = false;
        return thread_param;
    }

    usleep(data->wait_to_release_ms * 1000);

    if (0 != pthread_mutex_unlock(data->mutex)) {
        ERROR_LOG("Failed to unlock mutex.");
        data->thread_complete_success = false;
        return thread_param;
    }

    data->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *data = malloc(sizeof(struct thread_data));
    if (NULL == data) {
        ERROR_LOG("Failed to allocate memory for thread_data.");
        return false;
    }

    data->thread_complete_success = false;
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;

    int ret = pthread_create(thread, NULL, threadfunc, data);
    if (0 != ret) {
        ERROR_LOG("Failed to create thread.");
        free(data);
        return false;
    }

	// In case of success data will be deallocated by called function
    return true;
}

