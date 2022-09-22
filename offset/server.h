#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <ucs/memory/memory_type.h>
#include <ucp/api/ucp.h>

#include "utilities.h"

#define MAX_CLIENTS 1000000

typedef struct
{
    ucp_worker_h data_worker;
    ucp_ep_h ep;
    uint8_t complete;
} client_ep_t;

typedef struct server_req
{
    int complete;
    client_ep_t *client;
} server_req_t;

/*** defines & structs *********/
void server_stream_recv_cb(void *request, ucs_status_t status, size_t length, void *user_data);
ucs_status_t server_receive_from_ucx(client_ep_t *w_ep, void *data, uint32_t data_size);
int run_server(ucp_context_h ucp_context, ucp_worker_h ucp_worker,
               char *listen_addr);
ucs_status_t start_server(ucp_worker_h ucp_worker, ucx_server_ctx_t *context,
                          ucp_listener_h *listener_p, const char *address_str);
void server_conn_handle_cb(ucp_conn_request_h conn_request, void *arg);
ucs_status_t server_create_ep(ucp_worker_h data_worker,
                              ucp_conn_request_h conn_request,
                              ucp_ep_h *server_ep, void *arg);
void server_err_cb(void *arg, ucp_ep_h ep, ucs_status_t status);
void *thread_clean(void *arg);

#endif /* SERVER_H */
