#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <ucs/memory/memory_type.h>
#include <ucp/api/ucp.h>
#include <sys/time.h>

#include "utilities.h"

#define THREADS 5
#define NUM_OF_TEST 3
#define CHANKS 1000
#define CHANKS_SIZE 10240
#define NUM_OF_RESULT 100
#define NUM_OF_TESTS_TO_AVG 1

typedef struct
{
    uint64_t address;
    uint64_t ClientID;
    uint64_t size;
    uint64_t chanks;
    void *mem_base_ptr;
    uint8_t rKey_buff_ptr[100];
    size_t rKey_buff_size;
} metadata;

/*** defines & structs *********/
void *run_client_test(void *);
void test1(uint64_t chanks, uint64_t size);
void test2(uint64_t chanks, uint64_t size);
void test3(uint64_t chanks, uint64_t size);
#endif /* CLIENT_H */
