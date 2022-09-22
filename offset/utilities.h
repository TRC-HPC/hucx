
#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <ucp/api/ucp.h>

#define PAGE_SIZE 4096
#define DEFAULT_PORT 42145

#define INFO_PAGES 100
#define META_PAGES 10
#define DATA_PAGES 100000
#define DATA_TEST2_PAGES 500000

#define INFO_DATA_SIZE (INFO_PAGES * PAGE_SIZE)
#define META_DATA_SIZE (META_PAGES * PAGE_SIZE)
#define DATA_SIZE (DATA_PAGES * PAGE_SIZE)

#define ALLOCATED_PAGES (INFO_DATA_SIZE + META_PAGES + DATA_PAGES)
#define ALLOCATED_SIZE (PAGE_SIZE * ALLOCATED_PAGES)
#define ALLOCATED_SIZE_TEST2 (PAGE_SIZE * DATA_TEST2_PAGES)

#define BASE_ADDR ((uint64_t)mem_base_ptr)
#define INFO_DATA_ADDR ((uint64_t)BASE_ADDR)
#define META_DATA_ADDR ((uint64_t)INFO_DATA_ADDR + INFO_DATA_SIZE)
#define DATA_ADDR ((uint64_t)META_DATA_ADDR + META_DATA_SIZE)

#define BASE_ADDR_TEST2 ((uint64_t)mem_base_ptr_test2)
#define INFO_DATA_ADDR_TEST2 ((uint64_t)BASE_ADDR_TEST2)
#define TEST2_ADDR ((uint64_t)INFO_DATA_ADDR_TEST2 + INFO_DATA_SIZE)

#define STAT_HEADER_SIZE (SIZE_64_BIT * 30)

#define SEMAFOR_ADDR ((uint64_t)INFO_DATA_ADDR)
#define META_OFFSET_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT)
#define DATA_OFFSET_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT * 1)
// statistics
#define ST_TIME_SEM_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT * 2)
#define ST_OFFSET_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT * 3)
#define THREADS_COUNT_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT * 4)  // set count threads test
#define RUN_COUNT_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT * 5)      // set count threads test
#define END_COUNT_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT * 6)      // set count threads test
#define SEMAFOR_TEST2_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT * 10) // set count threads test
#define TEST_INFO_ADDR ((uint64_t)INFO_DATA_ADDR + SIZE_64_BIT * 20)

#define STATISTIC_ADDR ((uint64_t)INFO_DATA_ADDR + STAT_HEADER_SIZE)
#define STATISTIC_END_ADDR (META_DATA_ADDR - sizeof(STATISTIC_t))
#define STAT_ARRAY_SIZE ((INFO_DATA_SIZE - STAT_HEADER_SIZE) / sizeof(STATISTIC_t))

#define IP_STRING_LEN 50
#define PORT_STRING_LEN 8

#define SIZE_64_BIT 8
#define SERVR_ADDRESS "192.168.20.108"

typedef struct STATISTIC
{
    double test_av_time;
} STATISTIC_t;

typedef struct TEST_INFO
{
    int test_number;
    int chanks_number;
    int size_of_chank;
} TEST_INFO_t;

typedef struct TIME
{
    double test1_time;
    double test2_time;
    double test3_time;
} TIME_t;
/**
 * Server's application context to be used in the user's connection request
 * callback.
 * It holds the server's listener and the handle to an incoming connection request.
 */
typedef struct ucx_server_ctx
{
    volatile ucp_conn_request_h conn_request;
    ucp_listener_h listener;
} ucx_server_ctx_t;
/**
 * Stream request context. Holds a value to indicate whether or not the
 * request is completed.
 */
typedef struct test_req
{
    int complete;
    int client;
} test_req_t;

ucs_status_t receive_from_ucx(ucp_worker_h worker, ucp_ep_h ep, void *data, uint32_t data_size);
ucs_status_t send_to_ucx(ucp_worker_h worker, ucp_ep_h ep, void *data, uint32_t data_size);

int init_context(ucp_context_h *ucp_context, ucp_worker_h *ucp_worker);
int init_worker(ucp_context_h ucp_context, ucp_worker_h *ucp_worker);

ucs_status_t request_wait(ucp_worker_h ucp_worker, void *request,
                          test_req_t *ctx);
void err_cb(void *arg, ucp_ep_h ep, ucs_status_t status);
void set_sock_addr(const char *address_str, struct sockaddr_storage *saddr);
char *sockaddr_get_port_str(const struct sockaddr_storage *sock_addr,
                            char *port_str, size_t max_size);
char *sockaddr_get_ip_str(const struct sockaddr_storage *sock_addr,
                          char *ip_str, size_t max_size);
void stream_recv_cb(void *request, ucs_status_t status, size_t length, void *user_data);

void ep_close(ucp_worker_h ucp_worker, ucp_ep_h ep, uint64_t flags);

/****************** time  **********/
uint64_t time_dif(struct timeval tv1, struct timeval tv2);
/**** client ***********/
ucs_status_t start_client(ucp_worker_h ucp_worker, ucp_ep_h *client_ep);
/************* memory ********************************/
ucs_status_t mem_map_alloc(size_t length, int non_blk_flag, ucp_context_h ucp_context, ucp_mem_h *mem_handle);
uint64_t *mem_base_address(ucp_mem_h mem_handle);
uint32_t get_mem_ucx_total_size(ucp_mem_h mem_handle);
uint64_t *mem_ucx_allocate(uint32_t size, ucp_context_h ucp_context, ucp_mem_h *mem_handle);

/************* statistics ********************************/
void print_result();
int print_menu();

#endif
