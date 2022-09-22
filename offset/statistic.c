#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>
/*////////////////statistics///////////*/
#include <sys/ipc.h>
#include <sys/shm.h>
/**** thread ****************/
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "utilities.h"
#include "client.h"

#define CPU_SET(cpu, cpusetp) __CPU_SET_S(cpu, sizeof(cpu_set_t), cpusetp)
#define CPU_ZERO(cpusetp) __CPU_ZERO_S(sizeof(cpu_set_t), cpusetp)

extern int pthread_attr_setaffinity_np(pthread_attr_t *attr,
									   size_t cpusetsize, const cpu_set_t *cpuset);

ucp_ep_h gclient_ep;
ucp_worker_h global_worker;
ucp_rkey_h global_rkey_p = {0};
void *mem_base_ptr = NULL;
TIME_t test_time[NUM_OF_RESULT];
TEST_INFO_t tests;
uint64_t run_count, thread_count, result, stat_data, offset;
uint64_t thread_number_test;
struct timeval t_start;
FILE *file_p;

int main(int argc, char **argv)
{
	uint8_t rKey_buff_ptr[100];
	size_t rKey_buff_size;
	double avarage;
	uint64_t av_count;
	STATISTIC_t res;
	ucp_context_h ucp_context;
	int ret;
	ucs_status_t status;

	// default test info
	tests.test_number = 1;
	tests.size_of_chank = CHANKS_SIZE;
	tests.chanks_number = CHANKS;
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
		goto out;
	}

	gettimeofday(&t_start, NULL);
	send_to_ucx(global_worker, gclient_ep, "89765432", 4);
	receive_from_ucx(global_worker, gclient_ep, &rKey_buff_size, sizeof(rKey_buff_size));
	receive_from_ucx(global_worker, gclient_ep, rKey_buff_ptr, rKey_buff_size);
	receive_from_ucx(global_worker, gclient_ep, &mem_base_ptr, sizeof(mem_base_ptr));
	printf("received rKey_buff   %p  %ld\n", mem_base_ptr, rKey_buff_size);
	status = ucp_ep_rkey_unpack(gclient_ep, (void *)rKey_buff_ptr, &global_rkey_p);

	if (status != UCS_OK || mem_base_ptr == NULL)
	{
		perror("ucp_ep_rkey_unpack");
		exit(-1);
	}

	/* ==============  run test    ===== */

	while (1)
	{
		while (1)
		{
			ucp_get(gclient_ep, &thread_count, sizeof(thread_count), THREADS_COUNT_ADDR, global_rkey_p);
			thread_number_test = thread_count;
			if (print_menu())
				break;
		}
		for (int run_thread_count = 1; run_thread_count <= thread_number_test; run_thread_count++)
		{
			for (int i = 1; i <= NUM_OF_TEST; i++)
			{
				stat_data = 0;
				tests.test_number = i;
				run_count = run_thread_count;
				ucp_put(gclient_ep, &stat_data, sizeof(stat_data), ST_TIME_SEM_ADDR, global_rkey_p);
				stat_data = STATISTIC_ADDR;
				ucp_put(gclient_ep, &stat_data, sizeof(stat_data), ST_OFFSET_ADDR, global_rkey_p);
				ucp_put(gclient_ep, &run_count, sizeof(run_count), END_COUNT_ADDR, global_rkey_p);
				ucp_put(gclient_ep, &run_count, sizeof(run_count), RUN_COUNT_ADDR, global_rkey_p);
				ucp_put(gclient_ep, &tests, sizeof(tests), TEST_INFO_ADDR, global_rkey_p);

				usleep(100);
				while (1)
				{
					ucp_get(gclient_ep, &thread_count, sizeof(thread_count), END_COUNT_ADDR, global_rkey_p);
					ucp_get(gclient_ep, &run_count, sizeof(run_count), RUN_COUNT_ADDR, global_rkey_p);
					ucp_atomic_cswap64(gclient_ep, 0, run_count, END_COUNT_ADDR, global_rkey_p, &result);
					if ((long)result <= 0)
					{
						// print results
						ucp_get(gclient_ep, &run_count, sizeof(run_count), ST_OFFSET_ADDR, global_rkey_p);
						offset = STATISTIC_ADDR;
						av_count = avarage = 0;
						while (offset < run_count)
						{
							ucp_get(gclient_ep, &res, sizeof(res), offset, global_rkey_p);
							offset += sizeof(STATISTIC_t);
							av_count++;
							avarage = avarage - avarage / av_count + res.test_av_time / av_count;
						}
						if (i == 1)
						{
							test_time[run_thread_count - 1].test1_time = avarage;
						}
						else if (i == 2)
						{
							test_time[run_thread_count - 1].test2_time = avarage;
						}
						else if (i == 3)
						{
							test_time[run_thread_count - 1].test3_time = avarage;
						}
						break;
					}
					usleep(100);
				}
			}
		}
	}

	/* ==============  run test    ===== */
	/* Close the endpoint to the server */
	ep_close(global_worker, gclient_ep, UCP_EP_CLOSE_MODE_FORCE);

out:
	// ucp_worker_destroy(ucp_worker);
	// ucp_cleanup(ucp_context);
err:
	return 0;
} /*// END main */

int print_menu()
{
	int menu = -1;
	int choice = -1;
	printf("\033[2J\033[3;20f \033[18;0f");
	print_result();
	printf("\033[3;0f  |                     Statistics --%2d test                   |\n", NUM_OF_TEST);
	printf("\033[4;0f  |____________________________________________________________| \n");
	printf("\033[5;0f  | Threads number --%10ld  |                             |\n", thread_count);
	printf("\033[6;0f  | Chanks number  --%10d  |  Size of chank --%10d |\n", tests.chanks_number, tests.size_of_chank);
	printf("\033[7;0f  |____________________________________________________________| \n");
	printf("\033[10;0f Menu: 1) Run all test - (%ld client/Threads)           \n", thread_count);
	printf("\033[11;0f       2) Change chanks number                          \n");
	printf("\033[12;0f       3) Change size of chanks                         \n");
	printf("\033[13;0f       4) Save in File                                  \n");
	printf("\033[14;0f                                                        \n");
	printf("\033[15;0f                                                        \n");
	printf("\033[15;0f       Enter your choice: ");
	scanf("%d", &menu);

	if (menu == 1)
		return 1;
	else if (menu == 2)
	{
		printf("\033[15;0f                                                        \n");
		printf("\033[15;0f       Enter chanks number: ");
		scanf("%d", &choice);
		if (choice > 0 && choice <= CHANKS)
			tests.chanks_number = choice;
		else
		{
			printf("\033[15;0f                                                        \n");
			printf("\033[15;0f       Error in chanks number -- max %d \n ", CHANKS);
			sleep(5);
		}
	}
	else if (menu == 3)
	{
		printf("\033[15;0f                                                        \n");
		printf("\033[15;0f       Enter size of chanks: ");
		scanf("%d", &choice);
		if (choice > 0 && choice <= CHANKS_SIZE)
			tests.size_of_chank = choice;
		else
		{
			printf("\033[15;0f                                                        \n");
			printf("\033[15;0f       Error in size of chanks -- max %d \n ", CHANKS_SIZE);
			sleep(5);
		}
	}
	else if (menu == 4)
	{
		char name_file[100];
		uint64_t test1 = 0;
		uint64_t test2 = 0;
		uint64_t test3 = 0;

		sprintf(name_file, "C_%d_S_%d.txt", tests.chanks_number, tests.size_of_chank);
		file_p = fopen(name_file, "w+");
		fprintf(file_p, "Time in microsecond\n");
		fprintf(file_p, "Chanks size       = %d\n", tests.size_of_chank);
		fprintf(file_p, "Chanks number     = %d\n", tests.chanks_number);
		fprintf(file_p, "Thread |   Test 1   |    Test 2  |    Test 3  |\n");

		for (int i = 1; i <= thread_count; i++)
		{
			test1 += test_time[i - 1].test1_time;
			test2 += test_time[i - 1].test2_time;
			test3 += test_time[i - 1].test3_time;
			fprintf(file_p, "  %3d  | %10.3f | %10.3f | %10.3f |\n", i,
					test_time[i - 1].test1_time,
					test_time[i - 1].test2_time,
					test_time[i - 1].test3_time);
		}
		fprintf(file_p, "Thread |   Test 1   |    Test 2  |    Test 3  |\n");
		fprintf(file_p, "______________________________________________|\n");
		fprintf(file_p, "  Efficiency test 1 & 2 -- %3.0f %%\n", ((float)(test1 - test2) / (float)(test1)) * 100);
		fprintf(file_p, "   Efficiency test 2 & 3 -- %3.0f %%\n", ((float)(test3 - test2) / (float)(test3)) * 100);
		fclose(file_p);
		printf("\033[15;0f                                                        \n");
		printf("\033[15;0f  Saved in file\n");
		sleep(5);
	}
	return 0;
}

void print_result()
{
	// Efficiency = (test#1 – test#2)/(test#1)
	uint64_t test1 = 0;
	uint64_t test2 = 0;
	uint64_t test3 = 0;
	if (test_time[0].test1_time > 0 && test_time[0].test2_time > 0 && test_time[0].test3_time > 0)
	{
		printf("\t\t\t\t\t\t\t\t  Thread  |   Test 1   |   Test 2   |   Test 3   |\n");
		printf("\t\t\t\t\t\t\t\t _________|____________|____________|____________|\n");
		for (int i = 1; i <= thread_count; i++)
		{
			test1 += test_time[i - 1].test1_time;
			test2 += test_time[i - 1].test2_time;
			test3 += test_time[i - 1].test3_time;
			printf("\t\t\t\t\t\t\t\t    %3d   | %10.3f | %10.3f | %10.3f |\n", i,
				   test_time[i - 1].test1_time,
				   test_time[i - 1].test2_time,
				   test_time[i - 1].test3_time);
			printf("\t\t\t\t\t\t\t\t _________|____________|____________|____________|\n");
		}
		printf("\t\t\t\t\t\t\t\t  Thread  |   Test 1   |   Test 2   |   Test 3   |\n");
		printf("\t\t\t\t\t\t\t\t _________|____________|____________|____________|\n");

		printf("\t\t\t\t\t\t\t\t Efficiency test 1 & 2 -- %3.0f %%\n", ((float)(test1 - test2) / (float)(test1)) * 100);
		printf("\t\t\t\t\t\t\t\t Efficiency test 2 & 3 -- %3.0f %%\n", ((float)(test3 - test2) / (float)(test3)) * 100);
	}
}
