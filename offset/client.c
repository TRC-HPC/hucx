#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
/*////////////////statistics///////////*/
#include <sys/ipc.h>
#include <sys/shm.h>
/**** thread ****************/
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <ucs/sys/stubs.h>
#include "client.h"

#define CPU_SET(cpu, cpusetp) __CPU_SET_S(cpu, sizeof(cpu_set_t), cpusetp)
#define CPU_ZERO(cpusetp) __CPU_ZERO_S(sizeof(cpu_set_t), cpusetp)

extern int pthread_attr_setaffinity_np(pthread_attr_t *attr,
									   size_t cpusetsize, const cpu_set_t *cpuset);
ucp_ep_h gclient_ep;
ucp_worker_h global_worker;
ucp_rkey_h global_rkey_p = {0};
void *mem_base_ptr = NULL;
uint8_t rKey_buff_ptr[100];
size_t rKey_buff_size;
struct timeval t_start;
// for test 2
size_t rKey_buff_size_test2;
uint8_t rKey_buff_ptr_test2[100];
void *mem_base_ptr_test2 = NULL;
ucp_rkey_h global_rkey_p_test2 = {0};
char buff[CHANKS_SIZE] = {0};
static metadata meta_log = {0};

int main(int argc, char **argv)
{
	ucp_context_h ucp_context;
	int ret;
	ucs_status_t status;
	pthread_t threads[THREADS];
	uint64_t i;
	pthread_attr_t attr;
	cpu_set_t cpus;
	int numberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

	/* Initialize thread */
	pthread_attr_init(&attr);

	/* Initialize the UCX required objects */
	ret = init_context(&ucp_context, &global_worker);
	if (ret != 0)
	{
		goto err;
	}

	status = start_client(global_worker, &gclient_ep);
	if (status != UCS_OK)
	{

		fprintf(stderr, "failed to start client (%s)\n", ucs_status_string(status));
		ret = -1;
		goto err;
	}

	gettimeofday(&t_start, NULL);
	send_to_ucx(global_worker, gclient_ep, "89765432", 4);
	receive_from_ucx(global_worker, gclient_ep, &rKey_buff_size, sizeof(rKey_buff_size));
	receive_from_ucx(global_worker, gclient_ep, rKey_buff_ptr, rKey_buff_size);
	receive_from_ucx(global_worker, gclient_ep, &mem_base_ptr, sizeof(mem_base_ptr));

	status = ucp_ep_rkey_unpack(gclient_ep, (void *)rKey_buff_ptr, &global_rkey_p);
	if (status != UCS_OK || mem_base_ptr == NULL)
	{
		perror("ucp_ep_rkey_unpack");
		exit(-1);
	}

	// for test 2
	receive_from_ucx(global_worker, gclient_ep, &rKey_buff_size_test2, sizeof(rKey_buff_size_test2));
	receive_from_ucx(global_worker, gclient_ep, rKey_buff_ptr_test2, rKey_buff_size_test2);
	receive_from_ucx(global_worker, gclient_ep, &mem_base_ptr_test2, sizeof(mem_base_ptr_test2));

	status = ucp_ep_rkey_unpack(gclient_ep, (void *)rKey_buff_ptr_test2, &global_rkey_p_test2);
	if (status != UCS_OK || mem_base_ptr_test2 == NULL)
	{
		perror("ucp_ep_rkey_unpack");
		exit(-1);
	}

	printf("\033[2J\033[1;20f \033[10;0f");
	printf("\033[3;0f Client Active\n");

	//# ============== run test ==============
	while (1)
	{
		for (i = 0; i < THREADS && i < numberOfProcessors; i++)
		{
			CPU_ZERO(&cpus);
			CPU_SET(i, &cpus);
			pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
			pthread_create(&threads[i], NULL, run_client_test, (void *)i);
		}
		for (i = 0; i < THREADS && i < numberOfProcessors; i++)
		{
			pthread_join(threads[i], NULL);
		}
	}

	/* ==============  run test    ===== */
	/* Close the endpoint to the server */
	ep_close(global_worker, gclient_ep, UCP_EP_CLOSE_MODE_FORCE);
err:
	return 0;
}

void *run_client_test(void *param)
{
	struct timeval t1, t2;
	// uint64_t num_test 	= ((uint64_t)param)+1;
	uint64_t tdif_current = 0;
	uint64_t tdif_count = 0;
	double d_tdif_avr = 0;
	uint32_t i;
	// static uint32_t k=0;
	uint64_t thread_count = 0, result;
	uint64_t run_count = 0;
	STATISTIC_t strStat;
	uint64_t offset = 0;
	TEST_INFO_t test_info;

	/* ==============  run test    ===== */
	// update counter of tests
	ucp_atomic_add64(gclient_ep, 1, THREADS_COUNT_ADDR, global_rkey_p);
	ucp_get(gclient_ep, &thread_count, sizeof(thread_count), THREADS_COUNT_ADDR, global_rkey_p);

	while (1)
	{
		/*
		Wait while running has a variable run_count(holds how many threads to run)
		had a size of 0.
		If it is greater than 0 then it tries to subtract 1 from the variable run_count
		and then we can continue Otherwise he waits
		*/
		ucp_get(gclient_ep, &run_count, sizeof(run_count), RUN_COUNT_ADDR, global_rkey_p);
		for (i = 0; i < run_count; i++)
		{
			ucp_atomic_cswap64(gclient_ep, run_count - i, run_count - i - 1, RUN_COUNT_ADDR, global_rkey_p, &result);
			if (result == run_count - i)
				break;
		}
		if (i == run_count)
		{
			usleep(10);
			continue;
		}
		tdif_current = 0;
		tdif_count = 0;
		d_tdif_avr = 0;

		// Received which test to run
		ucp_get(gclient_ep, &test_info, sizeof(test_info), TEST_INFO_ADDR, global_rkey_p);

		for (i = 0; i < NUM_OF_TESTS_TO_AVG; i++)
		{
			usleep(300);
			if (test_info.test_number == 1)
			{
				gettimeofday(&t1, NULL);
				test1(test_info.chanks_number, test_info.size_of_chank);
				gettimeofday(&t2, NULL);
			}
			else if (test_info.test_number == 2)
			{
				gettimeofday(&t1, NULL);
				test2(test_info.chanks_number, test_info.size_of_chank);
				gettimeofday(&t2, NULL);
			}
			else if (test_info.test_number == 3)
			{
				gettimeofday(&t1, NULL);
				test3(test_info.chanks_number, test_info.size_of_chank);
				gettimeofday(&t2, NULL);
			}
			tdif_current = time_dif(t1, t2);
			tdif_count++;
			d_tdif_avr -= d_tdif_avr / tdif_count - ((double)tdif_current) / tdif_count;
		}
		/////////////////////// save  results /////////////////////////////////////////////////////////////////////////
		strStat.test_av_time = d_tdif_avr;
		while (1)
		{
			ucp_atomic_cswap64(gclient_ep, 0, 93, ST_TIME_SEM_ADDR, global_rkey_p, &result);
			if (!result)
				break;
			usleep(1);
		}

		ucp_get(gclient_ep, &offset, sizeof(offset), ST_OFFSET_ADDR, global_rkey_p);

		if (offset < STATISTIC_ADDR || offset >= STATISTIC_END_ADDR)
		{
			offset = STATISTIC_ADDR;
		}
		// set new offset
		ucp_put(gclient_ep, (const void *)(&strStat), sizeof(STATISTIC_t), offset, global_rkey_p);
		offset += sizeof(STATISTIC_t);
		ucp_put(gclient_ep, &offset, sizeof(offset), ST_OFFSET_ADDR, global_rkey_p);
		// give semafor
		ucp_atomic_cswap64(gclient_ep, 93, 0, ST_TIME_SEM_ADDR, global_rkey_p, &result);
		ucp_atomic_add64(gclient_ep, -1, END_COUNT_ADDR, global_rkey_p);
	}
	// update counter of tests
	ucp_atomic_add64(gclient_ep, -1, THREADS_COUNT_ADDR, global_rkey_p);
	pthread_exit(0);
	return NULL;
}

// test 1
void test1(uint64_t chanks, uint64_t size)
{
	uint64_t result;
	uint64_t offset;

	for (int i = 0; i < chanks; i++)
	{
		while (1)
		{
			ucp_atomic_cswap64(gclient_ep, 0, 93, SEMAFOR_ADDR, global_rkey_p, &result);
			if (!result)
				break;
			usleep(1);
		}
		// read current offset
		ucp_get(gclient_ep, &offset, sizeof(offset), DATA_OFFSET_ADDR, global_rkey_p);
		if (offset < DATA_ADDR || offset >= (DATA_ADDR + DATA_SIZE - size))
		{
			offset = DATA_ADDR;
		}
		// write chank data
		ucp_put(gclient_ep, (const void *)buff, size, offset, global_rkey_p);
		// set new offset
		offset += size;
		ucp_put(gclient_ep, &offset, sizeof(offset), DATA_OFFSET_ADDR, global_rkey_p);
		// give semafor
		ucp_atomic_cswap64(gclient_ep, 93, 0, SEMAFOR_ADDR, global_rkey_p, &result);
	}
}
// test 2
void test2(uint64_t chanks, uint64_t size)
{
	ucp_request_param_t param = {0};
	uint64_t result, res;

	for (int i = 0; i < chanks; i++)
	{
		ucp_append_nbx(gclient_ep, (const void *)buff, size,
					   SEMAFOR_TEST2_ADDR,	// address of semafor
					   global_rkey_p,		// rkey of semafor
					   global_rkey_p_test2, // rkey of data test 2
					   &result, &param);
		// Checking if there is an exception from the memory
		ucp_get(gclient_ep, &res, sizeof(res), (uint64_t)SEMAFOR_TEST2_ADDR, global_rkey_p);
		if (res - (uint64_t)mem_base_ptr_test2 > ALLOCATED_SIZE_TEST2 - 60000)
			ucp_atomic_swap64(gclient_ep, (uint64_t)mem_base_ptr_test2, (uint64_t)SEMAFOR_TEST2_ADDR, global_rkey_p, &result);
	}
}
// test 3
void test3(uint64_t chanks, uint64_t size)
{

	uint64_t result;
	uint64_t offset;

	for (int i = 0; i < chanks; i++)
	{
		while (1)
		{
			ucp_atomic_cswap64(gclient_ep, 0, 93, SEMAFOR_ADDR, global_rkey_p, &result);
			if (!result)
				break;
			usleep(1);
		}
		// read current offset
		ucp_get(gclient_ep, &offset, sizeof(offset), DATA_OFFSET_ADDR, global_rkey_p);

		if (offset < DATA_ADDR || offset >= (DATA_ADDR + DATA_SIZE - CHANKS_SIZE))
		{
			offset = DATA_ADDR;
		}
		// write chank data
		ucp_put(gclient_ep, (const void *)&meta_log, sizeof(metadata), offset, global_rkey_p);
		// set new offset
		offset += sizeof(metadata);
		ucp_put(gclient_ep, &offset, sizeof(offset), DATA_OFFSET_ADDR, global_rkey_p);
		// give semafor
		ucp_atomic_cswap64(gclient_ep, 93, 0, SEMAFOR_ADDR, global_rkey_p, &result);
	}
}
