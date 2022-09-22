
/*
 * Server connection data
 *
 * */
#include <ucp/api/ucp.h>
#include <ucs/datastruct/mpool.h>
#include <string.h>    /* memset */
#include <arpa/inet.h> /* inet_addr */
#include <unistd.h>    /* getopt */
#include <stdlib.h>    /* atoi */
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h> // mmap

#include "server.h"

void *mem_base_ptr = NULL;
void *rKey_buff_ptr = NULL;
size_t rKey_buff_size = 0;
size_t mem_size = 0;
ucp_mem_h sem_mem_handle = {0};
client_ep_t *client_list[MAX_CLIENTS] = {0};

// test2
void *mem_base_ptr_test2 = NULL;
void *rKey_buff_ptr_test2 = NULL;
size_t rKey_buff_size_test2 = 0;
size_t mem_size_test2 = 0;
ucp_mem_h sem_mem_handle_test2 = {0};

int main(int argc, char **argv)
{
    int ret;
    char *listen_addr = "0.0.0.0";
    pthread_t threads1;

    /* UCP objects */
    ucp_context_h ucp_context;
    ucp_worker_h ucp_worker;

    /* Initialize the UCX required objects */
    ret = init_context(&ucp_context, &ucp_worker);
    if (ret != 0)
    {
        goto err;
    }

    pthread_create(&threads1, NULL, thread_clean, NULL);
    printf("\033[2J\033[0;20f \033[80;0f");
    /* Client-Server initialization */
    ret = run_server(ucp_context, ucp_worker, listen_addr);

    ucp_worker_destroy(ucp_worker);
    ucp_cleanup(ucp_context);
err:
    return ret;
} /*End main*/

void *run_client(void *arg)
{
    uint32_t i;
    client_ep_t *c_epw = (client_ep_t *)arg;
    server_receive_from_ucx(c_epw, &i, sizeof(i));
    send_to_ucx(c_epw->data_worker, c_epw->ep, &rKey_buff_size, sizeof(rKey_buff_size));
    send_to_ucx(c_epw->data_worker, c_epw->ep, rKey_buff_ptr, rKey_buff_size);
    send_to_ucx(c_epw->data_worker, c_epw->ep, &mem_base_ptr, sizeof(mem_base_ptr));
    // foe test 2
    send_to_ucx(c_epw->data_worker, c_epw->ep, &rKey_buff_size_test2, sizeof(rKey_buff_size_test2));
    send_to_ucx(c_epw->data_worker, c_epw->ep, rKey_buff_ptr_test2, rKey_buff_size_test2);
    send_to_ucx(c_epw->data_worker, c_epw->ep, &mem_base_ptr_test2, sizeof(mem_base_ptr_test2));

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (!client_list[i])
        {
            client_list[i] = c_epw;
            break;
        }
    }
    return NULL;
}

void *thread_clean(void *arg)
{

    int i, count = 0;

    while (BASE_ADDR == 0)
        usleep(10);
    while (1)
    {
        for (count = 0, i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_list[i])
            {
                if (client_list[i]->complete == 100)
                {
                    ep_close(client_list[i]->data_worker, client_list[i]->ep, UCP_EP_CLOSE_MODE_FORCE);
                    free(client_list[i]);
                    client_list[i] = NULL;
                }
                else
                {
                    count++;
                    ucp_worker_progress(client_list[i]->data_worker);
                }
            }
        }
        if (mem_base_ptr)
        {
            printf("\033[3;6f Server Active \n");
            printf("\033[5;0f Thread count === %ld ===\n", *(uint64_t *)THREADS_COUNT_ADDR);
        }
        usleep(100);
    }
    return NULL;
}

int run_server(ucp_context_h ucp_context, ucp_worker_h ucp_worker,
               char *listen_addr)
{
    ucx_server_ctx_t context;
    ucp_worker_h ucp_data_worker;

    // ucp_ep_h         server_ep;
    client_ep_t *c_epw;
    ucs_status_t status;

    // test_req_t *request;
    uint32_t ret;
    pthread_t thread;

    /* Create a data worker (to be used for data exchange between the server
     * and the client after the connection between them was established) */
    ret = init_worker(ucp_context, &ucp_data_worker);
    if (ret != 0)
    {
        goto err;
    }

    /* mem map */
    mem_base_ptr = mem_ucx_allocate(ALLOCATED_SIZE, ucp_context, &sem_mem_handle);
    if (mem_base_ptr == NULL)
    {
        perror("mem alocation");
        exit(1);
    }

    //* get memory size and create rKey_buff
    mem_size = get_mem_ucx_total_size(sem_mem_handle);
    status = ucp_rkey_pack(ucp_context, sem_mem_handle, &rKey_buff_ptr, &rKey_buff_size);

    // for test 2
    mem_base_ptr_test2 = mem_ucx_allocate(ALLOCATED_SIZE_TEST2, ucp_context, &sem_mem_handle_test2);
    if (mem_base_ptr_test2 == NULL)
    {
        perror("mem alocation");
        exit(1);
    }

    // semaphor addr SEMAFOR_TEST2_ADDR have address of Data
    *(uint64_t *)SEMAFOR_TEST2_ADDR = (uint64_t)mem_base_ptr_test2;

    //* get memory size and create rKey_buff for test 2
    mem_size_test2 = get_mem_ucx_total_size(sem_mem_handle_test2);
    status = ucp_rkey_pack(ucp_context, sem_mem_handle_test2, &rKey_buff_ptr_test2, &rKey_buff_size_test2);
    // end test 2

    /* Initialize the server's context. */
    context.conn_request = NULL;

    /* Create a listener on the worker created at first. The 'connection
     * worker' - used for connection establishment between client and server.
     * This listener will stay open for listening to incoming connection
     * requests from the client
     * */

    status = start_server(ucp_worker, &context, &context.listener, listen_addr);
    if (status != UCS_OK)
    {
        ret = -1;
        goto err_worker;
    }

    /* Server is always up listening */
    while (1)
    {
        /* Wait for the server to receive a connection request from the client.
         * If there are multiple clients for which the server's connection request
         * callback is invoked, i.e. several clients are trying to connect in
         * parallel, the server will handle only the first one and reject the rest */
        while (context.conn_request == NULL)
        {
            ucp_worker_progress(ucp_worker);
        }

        /* Server creates an ep to the client on the data worker.
         * This is not the worker the listener was created on.
         * The client side should have initiated the connection, leading
         * to this ep's creation */
        c_epw = malloc(sizeof(client_ep_t));
        ret = init_worker(ucp_context, &c_epw->data_worker);
        if (ret != 0)
        {
            goto err;
        }

        status = server_create_ep(c_epw->data_worker, context.conn_request, &c_epw->ep, (void *)c_epw);
        if (status != UCS_OK)
            continue;

        pthread_create(&thread, NULL, run_client, c_epw);

        // Reinitialize the server's context to be used for the next client
        context.conn_request = NULL;
    }
err_ep:
    //  ep_close(ucp_data_worker, server_ep, UCP_EP_CLOSE_MODE_FORCE);
err_listener:
    ucp_listener_destroy(context.listener);
err_worker:
    ucp_worker_destroy(ucp_data_worker);
err:
    return ret;
}

/**
 * The callback on the receiving side, which is invoked after finishing receive
 * the message.
 */
void server_stream_recv_cb(void *request, ucs_status_t status, size_t length, void *user_data)
{
    server_req_t *ctx = user_data;
    ctx->complete = 1;
}

ucs_status_t server_receive_from_ucx(client_ep_t *w_ep, void *data, uint32_t data_size)
{
    ucs_status_t status = UCS_OK;
    size_t msg_length = 0;
    test_req_t *request;
    server_req_t ctx = {0};
    ucp_request_param_t param = {0};
    ctx.client = w_ep;
    param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                         UCP_OP_ATTR_FIELD_USER_DATA |
                         UCP_OP_ATTR_FIELD_FLAGS;
    param.user_data = &ctx;
    param.flags = UCP_STREAM_RECV_FLAG_WAITALL;
    param.cb.recv_stream = server_stream_recv_cb;

    request = ucp_stream_recv_nbx(w_ep->ep, data, data_size, &msg_length, &param);
    status = request_wait(w_ep->data_worker, request, (test_req_t *)&ctx);
    if (status != UCS_OK)
        fprintf(stderr, "unable to receive UCX message (%s)\n", ucs_status_string(status));
    return status;
}

/**
 * Initialize the server side. The server starts listening on the set address.
 */
ucs_status_t start_server(ucp_worker_h ucp_worker, ucx_server_ctx_t *context,
                          ucp_listener_h *listener_p, const char *address_str)
{
    struct sockaddr_storage listen_addr;
    ucp_listener_params_t params;
    ucp_listener_attr_t attr;
    ucs_status_t status;
    char ip_str[IP_STRING_LEN];
    char port_str[PORT_STRING_LEN];

    set_sock_addr(address_str, &listen_addr);

    params.field_mask = UCP_LISTENER_PARAM_FIELD_SOCK_ADDR |
                        UCP_LISTENER_PARAM_FIELD_CONN_HANDLER;
    params.sockaddr.addr = (const struct sockaddr *)&listen_addr;
    params.sockaddr.addrlen = sizeof(listen_addr);
    params.conn_handler.cb = server_conn_handle_cb;
    params.conn_handler.arg = context;

    /* Create a listener on the server side to listen on the given address.*/
    status = ucp_listener_create(ucp_worker, &params, listener_p);
    if (status != UCS_OK)
    {
        fprintf(stderr, "failed to listen (%s)\n", ucs_status_string(status));
        goto out;
    }

    /* Query the created listener to get the port it is listening on. */
    attr.field_mask = UCP_LISTENER_ATTR_FIELD_SOCKADDR;
    status = ucp_listener_query(*listener_p, &attr);
    if (status != UCS_OK)
    {
        fprintf(stderr, "failed to query the listener (%s)\n",
                ucs_status_string(status));
        ucp_listener_destroy(*listener_p);
        goto out;
    }

    printf("\033[8;0f server is listening on IP %s port %s\n",
           sockaddr_get_ip_str(&attr.sockaddr, ip_str, IP_STRING_LEN),
           sockaddr_get_port_str(&attr.sockaddr, port_str, PORT_STRING_LEN));

    printf("\033[9;0f Waiting for connection...\n");

out:
    return status;
}

/**
 * The callback on the server side which is invoked upon receiving a connection
 * request from the client.
 */
void server_conn_handle_cb(ucp_conn_request_h conn_request, void *arg)
{
    ucx_server_ctx_t *context = arg;
    ucp_conn_request_attr_t attr;
    char ip_str[IP_STRING_LEN];
    char port_str[PORT_STRING_LEN];
    ucs_status_t status;

    attr.field_mask = UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ADDR;
    status = ucp_conn_request_query(conn_request, &attr);
    if (status == UCS_OK)
    {
        printf("\033[11;0f Server received a connection request from client at address %s:%s\n",
               sockaddr_get_ip_str(&attr.client_address, ip_str, sizeof(ip_str)),
               sockaddr_get_port_str(&attr.client_address, port_str, sizeof(port_str)));
    }
    else if (status != UCS_ERR_UNSUPPORTED)
    {
        fprintf(stderr, "failed to query the connection request (%s)\n",
                ucs_status_string(status));
    }

    if (context->conn_request == NULL)
    {
        context->conn_request = conn_request;
    }
    else
    {
        /* The server is already handling a connection request from a client,
         * reject this new one */
        printf("Rejecting a connection request. Only one client at a time is supported.\n");
        status = ucp_listener_reject(context->listener, conn_request);
        if (status != UCS_OK)
        {
            fprintf(stderr, "server failed to reject a connection request: (%s)\n",
                    ucs_status_string(status));
        }
    }
}

ucs_status_t server_create_ep(ucp_worker_h data_worker,
                              ucp_conn_request_h conn_request,
                              ucp_ep_h *server_ep, void *arg)
{
    ucp_ep_params_t ep_params;
    ucs_status_t status;

    /* Server creates an ep to the client on the data worker.
     * This is not the worker the listener was created on.
     * The client side should have initiated the connection, leading
     * to this ep's creation */
    ep_params.field_mask = UCP_EP_PARAM_FIELD_ERR_HANDLER |
                           UCP_EP_PARAM_FIELD_CONN_REQUEST;
    ep_params.conn_request = conn_request;
    ep_params.err_handler.cb = server_err_cb;
    ep_params.err_handler.arg = arg;

    status = ucp_ep_create(data_worker, &ep_params, server_ep);
    if (status != UCS_OK)
    {
        fprintf(stderr, "failed to create an endpoint on the server: (%s)\n",
                ucs_status_string(status));
    }

    return status;
}

/**
 * Error handling callback.
 */
void server_err_cb(void *arg, ucp_ep_h ep, ucs_status_t status)
{
    if (arg)
    {
        ((client_ep_t *)arg)->complete = 100;
    }
    //   printf(">>error handling callback was invoked with status %d (%s)\n",
    //         status, ucs_status_string(status));
}
