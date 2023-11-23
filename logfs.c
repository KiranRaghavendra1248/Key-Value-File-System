/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * logfs.c
 */

#include <pthread.h>
#include "device.h"
#include "logfs.h"

#define WCACHE_BLOCKS 32
#define RCACHE_BLOCKS 256

/**
 * Needs:
 *   pthread_create()
 *   pthread_join()
 *   pthread_mutex_init()
 *   pthread_mutex_destroy()
 *   pthread_mutex_lock()
 *   pthread_mutex_unlock()
 *   pthread_cond_init()
 *   pthread_cond_destroy()
 *   pthread_cond_wait()
 *   pthread_cond_signal()
 */

/* research the above Needed API and design accordingly */

int shouldRunProducer = 0;
const void *writeDataBuffer = NULL;
uint64_t writeDataLength = 0;
int terminateProducer = 0;
int terminateConsumer = 0;

struct logfs{
    struct device *device;
    pthread_t user_thread, worker_thread;
    pthread_mutex_t wcache_mutex;
    pthread_cond_t wcache_cond_user_thread;
    pthread_cond_t wcache_cond_worker_thread;
    void * write_cache;
    void * read_cache;
    uint64_t wcache_head;
    uint64_t wcache_tail;
    uint64_t block_size;
};

static void *write_cache_worker_thread(void *arg) { 
    /* Consumer */
    struct logfs *logfs;
    logfs = (struct logfs *)arg;

    return NULL;
}

static void *write_cache_user_thread(void *arg) { 
    /* Producer*/
    struct logfs *logfs;
    logfs = (struct logfs *)arg;

    return NULL;
}


struct logfs *logfs_open(const char *pathname){
    struct logfs * logfs;
    pthread_t user_thread, worker_thread;

    logfs = (struct logfs *)malloc(sizeof(struct logfs));
    if(!logfs){
        TRACE("Failed Malloc for logfs!!");
        return NULL;
    }
    logfs->device = device_open(pathname);
    if (!logfs->device) {
        FREE(logfs);
        TRACE("Failed device open");
        return NULL;
    }
    logfs->block_size = device_block(logfs->device);
    logfs->read_cache = malloc(logfs->block_size * RCACHE_BLOCKS);
    if(!logfs->read_cache){
        FREE(logfs);
        TRACE("Failed Malloc for read_cache");
        return NULL;
    }
    logfs->write_cache = malloc(logfs->block_size * WCACHE_BLOCKS);
    if(!logfs->write_cache){
        FREE(logfs->read_cache);
        FREE(logfs);
        TRACE("Failed Malloc for write_cache");
        return NULL;
    }
    logfs->wcache_head = 0;
    logfs->wcache_tail = 0;

    pthread_mutex_init(&logfs->wcache_mutex, NULL);
    pthread_cond_init(&logfs->wcache_cond_user_thread, NULL);
    pthread_cond_init(&logfs->wcache_cond_worker_thread, NULL);

    pthread_create(&user_thread, NULL, write_cache_user_thread, logfs);
    pthread_create(&worker_thread, NULL, write_cache_worker_thread, logfs);

    logfs->user_thread = user_thread;
    logfs->worker_thread = worker_thread;

    return logfs;
}

void logfs_close(struct logfs *logfs){
    if(NULL == logfs){
        return;
    }
    FREE(logfs->read_cache);
    FREE(logfs->write_cache);
    /* Flush data */

    /*Signal worker and user to end execution*/
    terminateProducer = 1;
    terminateConsumer = 1;

    pthread_join(logfs->worker_thread, NULL);
    pthread_join(logfs->user_thread, NULL);
    pthread_mutex_destroy(&logfs->wcache_mutex);
    pthread_mutex_destroy(&logfs->wcache_cond_user_thread);
    pthread_cond_destroy(&logfs->wcache_cond_worker_thread);
    device_close(logfs->device);
    FREE(logfs);
}

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len){

}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len){
    pthread_mutex_lock(&logfs->wcache_mutex);
    shouldRunProducer = 1;
    writeDataBuffer = buf;
    writeDataLength = len;
    pthread_cond_signal(&logfs->wcache_cond_user_thread);
    pthread_mutex_unlock(&logfs->wcache_mutex);
}