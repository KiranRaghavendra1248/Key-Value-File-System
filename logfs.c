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
    void * write_cache;
    void * read_cache;
    uint64_t wcache_head;
    uint64_t wcache_tail;
    uint64_t block_size;
    uint64_t offset;
};

static void *write_cache_worker_thread(void *arg) { 
    /* Consumer */
    struct logfs *logfs;
    logfs = (struct logfs *)arg;

    return NULL;
}

uint64_t size_align(uint64_t len, uint64_t block_size){
	uint64_t r;
	if ((r = len % block_size)) {
		r = block_size - r;
	}
	return len + r;
}

static void *write_cache_user_thread(struct logfs *logfs, const void *buf, uint64_t len) { 
    /* Producer */
    void *mem_aligned_buffer;
    uint64_t mem_aligned_buffer_size;

    /* Create a block aligned buffer */
    mem_aligned_buffer_size = size_align(len, logfs->block_size);
    mem_aligned_buffer = malloc(mem_aligned_buffer_size);
    if(!mem_aligned_buffer){
        TRACE("Failed malloc for mem_aligned_buffer");
    }
    memset(mem_aligned_buffer,0,sizeof(mem_aligned_buffer));

    /* Copy data from buf to mem_aligned_buffer */
    memcpy(mem_aligned_buffer, buf, len);

    /* Acquire mutex lock */
    pthread_mutex_lock(&logfs->wcache_mutex);

    /* Perform write */
    device_write(logfs->device, mem_aligned_buffer, logfs->offset, mem_aligned_buffer_size);
    logfs->offset += mem_aligned_buffer_size;

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
    logfs->wcache_head = 0;
    logfs->wcache_tail = 0;
    logfs->offset = 0;

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

    /*Signal worker to end execution*/
    terminateConsumer = 1;

    pthread_join(logfs->worker_thread, NULL);
    pthread_mutex_destroy(&logfs->wcache_mutex);
    pthread_cond_destroy(&logfs->wcache_cond_worker_thread);
    device_close(logfs->device);
    FREE(logfs);
}

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len){

}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len){
    write_cache_user_thread(logfs, buf, len);
}