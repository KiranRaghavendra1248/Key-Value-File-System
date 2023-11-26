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
int terminateConsumer = 0;

struct logfs{
    struct device *device;
    pthread_t worker_thread;
    pthread_mutex_t wcache_mutex;
    pthread_cond_t wcache_cond_worker_thread;
    void * write_cache, * read_cache, * wcache_head, * wcache_tail;
    uint64_t block_size;
    uint64_t offset;
    size_t read_cache_size, write_cache_size;
};

static void flushData(struct logfs *logfs){

    uint64_t len;

    len = logfs->block_size;

    /* Write to device */
    device_write(logfs->device, logfs->wcache_tail, logfs->offset, len);
    logfs->offset += len;

    /* Move tail forward */
    logfs->wcache_tail += len;
    if(logfs->wcache_tail == logfs->write_cache_size){
        logfs->wcache_tail = logfs->write_cache;
    }
}

static void *write_cache_worker_thread(struct logfs *logfs) { 
    /* Consumer */
    while(0 == terminateConsumer){
        /* Acquire mutex lock*/
        pthread_mutex_lock(&logfs->wcache_mutex);

        /* If condition not met, release mutex and wait/sleep */
        while((logfs->wcache_head == logfs->wcache_tail) && (0 == terminateConsumer)){
            /* pthread_cond_wait call handles the unlocking of the mutex before waiting and relocks it upon waking up */
            pthread_cond_wait(&logfs->wcache_cond_worker_thread, &logfs->wcache_mutex);
        }
        if(1 == terminateConsumer){
            break;
        }
        /* Flush data */
        flushData(logfs);

        /* Release mutex */
        pthread_mutex_unlock(&logfs->wcache_mutex);
    }
    return NULL;
}

static void *write_cache_user_thread(struct logfs *logfs, const void *buf, uint64_t len) { 
    /* Producer */

    /* Acquire mutex lock */
    pthread_mutex_lock(&logfs->wcache_mutex);

    /* Perform write to the write buffer */
    memcpy(logfs->wcache_head, buf, len);
    logfs->wcache_head += len;
    if(logfs->wcache_head == logfs->write_cache_size){
        logfs->wcache_head = logfs->write_cache;
    }

    /* Signal consumer */
    pthread_cond_signal(&logfs->wcache_cond_worker_thread);

    /* Release mutex */
    pthread_mutex_unlock(&logfs->wcache_mutex);

    return NULL;
}

struct logfs *logfs_open(const char *pathname){
    struct logfs * logfs;
    pthread_t worker_thread;

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
    logfs->wcache_head = logfs->wcache_tail = logfs->write_cache;
    logfs->offset = 0;
    logfs->write_cache_size = logfs->block_size * WCACHE_BLOCKS;
    logfs->read_cache_size = logfs->block_size * RCACHE_BLOCKS;

    pthread_mutex_init(&logfs->wcache_mutex, NULL);
    pthread_cond_init(&logfs->wcache_cond_worker_thread, NULL);
    pthread_create(&worker_thread, NULL, write_cache_worker_thread, logfs);
    return logfs;
}

void logfs_close(struct logfs *logfs){
    if(NULL == logfs){
        return;
    }
    FREE(logfs->read_cache);
    FREE(logfs->write_cache);
    /* Flush data */
    while(logfs->wcache_head != logfs->wcache_tail){
        flushData(logfs);
    }

    /*Signal worker to end execution*/
    terminateConsumer = 1;
    pthread_cond_signal(&logfs->wcache_cond_worker_thread);

    pthread_join(logfs->worker_thread, NULL);

    pthread_mutex_destroy(&logfs->wcache_mutex);
    pthread_cond_destroy(&logfs->wcache_cond_worker_thread);
    device_close(logfs->device);
    FREE(logfs);
}

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len){

}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len){
    uint64_t remaining;
    const void *temp_buf, *traverse_ptr;
    remaining = len;
    traverse_ptr = buf;
    temp_buf = malloc(logfs->block_size);
            if(!temp_buf){
                TRACE("Malloc Failed for temp_buf!!");
                logfs_close(logfs);
                return -1;
            }
    while(remaining > 0){
        if(remaining >= logfs->block_size){
            memset(temp_buf,0,sizeof(temp_buf));
            memcpy(temp_buf, traverse_ptr, logfs->block_size);
            traverse_ptr += logfs->block_size;
            remaining -= logfs->block_size;
        }
        else{
            memset(temp_buf,0,sizeof(temp_buf));
            memcpy(temp_buf, traverse_ptr, remaining);
            traverse_ptr += remaining;
            remaining = 0;
        }
        write_cache_user_thread(logfs, temp_buf, logfs->block_size);
    }
    FREE(temp_buf);
    return 0;
}