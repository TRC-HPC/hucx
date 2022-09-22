#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>
#include "utilities.h"

ucs_status_t receive_from_ucx(ucp_worker_h worker, ucp_ep_h ep, void *data, uint32_t data_size)
{
    ucs_status_t status;
    size_t msg_length = 0;
    test_req_t *request;
    test_req_t ctx = {0};
    ucp_request_param_t param = {0};
    ctx.client = 9;
    param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                         UCP_OP_ATTR_FIELD_USER_DATA |
                         UCP_OP_ATTR_FIELD_FLAGS;
    param.user_data = &ctx;
    param.flags = UCP_STREAM_RECV_FLAG_WAITALL;
    param.cb.recv_stream = stream_recv_cb;

    request = ucp_stream_recv_nbx(ep, data, data_size, &msg_length, &param);

    status = request_wait(worker, request, &ctx);
    if (status != UCS_OK)
        fprintf(stderr, "unable to receive UCX message (%s)\n", ucs_status_string(status));

    return status;
}

ucs_status_t send_to_ucx(ucp_worker_h worker, ucp_ep_h ep, void *data, uint32_t data_size)
{
    ucs_status_ptr_t request;
    ucs_status_t status;
    ucp_request_param_t param = {0};

    param.op_attr_mask = UCP_OP_ATTR_FLAG_FAST_CMPL;

    do
    {
        request = ucp_stream_send_nbx(ep, data, data_size, &param);
        ucp_worker_progress(worker);
    } while ((status = UCS_PTR_IS_ERR(request)));

    return status;
}

/**
 * Initialize the UCP context and worker.
 */
int init_context(ucp_context_h *ucp_context, ucp_worker_h *ucp_worker)
{
    /* UCP objects */
    ucp_params_t ucp_params;
    ucs_status_t status;
    int ret = 0;

    memset(&ucp_params, 0, sizeof(ucp_params));

    /* UCP initialization */
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_STREAM;
    ucp_params.features |= UCP_FEATURE_AMO64 | UCP_FEATURE_RMA;

    status = ucp_init(&ucp_params, NULL, ucp_context);
    if (status != UCS_OK)
    {
        fprintf(stderr, "failed to ucp_init (%s)\n", ucs_status_string(status));
        ret = -1;
        goto err;
    }

    ret = init_worker(*ucp_context, ucp_worker);
    if (ret != 0)
    {
        goto err_cleanup;
    }

    return ret;

err_cleanup:
    ucp_cleanup(*ucp_context);
err:
    return ret;
}

/**
 * Create a ucp worker on the given ucp context.
 */
int init_worker(ucp_context_h ucp_context, ucp_worker_h *ucp_worker)
{
    ucp_worker_params_t worker_params;
    ucs_status_t status;
    int ret = 0;

    memset(&worker_params, 0, sizeof(worker_params));

    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_MULTI;

    status = ucp_worker_create(ucp_context, &worker_params, ucp_worker);
    if (status != UCS_OK)
    {
        fprintf(stderr, "failed to ucp_worker_create (%s)\n", ucs_status_string(status));
        ret = -1;
    }

    return ret;
}

/**
 * Progress the request until it completes.
 */
ucs_status_t request_wait(ucp_worker_h ucp_worker, void *request,
                          test_req_t *ctx)
{
    ucs_status_t status;
    int i = 0;

    /* if operation was completed immediately */
    if (request == NULL)
    {
        return UCS_OK;
    }

    if (UCS_PTR_IS_ERR(request))
    {
        return UCS_PTR_STATUS(request);
    }

    while (ctx->complete == 0)
    {
        ucp_worker_progress(ucp_worker);
        i++;
        usleep(1);
        if (i > 1000000)
        {
            return UCS_ERR_IO_ERROR;
        }
    }
    status = ucp_request_check_status(request);

    ucp_request_free(request);

    return status;
}

/**CallBacks************************************/
/**
 * The callback on the receiving side, which is invoked after finishing receive
 * the message.
 */
void stream_recv_cb(void *request, ucs_status_t status, size_t length, void *user_data)
{
    test_req_t *ctx = user_data;
    ctx->complete = 1;
}
/**
 * Error handling callback.
 */
void err_cb(void *arg, ucp_ep_h ep, ucs_status_t status)
{
    printf(">>error handling callback was invoked with status %d (%s)\n",
           status, ucs_status_string(status));
    exit(0);
}
/**CallBacks end*********************************/
/**
 * Set an address for the server to listen on - INADDR_ANY on a well known port.
 */
void set_sock_addr(const char *address_str, struct sockaddr_storage *saddr)
{
    struct sockaddr_in *sa_in;

    /* The server will listen on INADDR_ANY */
    memset(saddr, 0, sizeof(*saddr));
    sa_in = (struct sockaddr_in *)saddr;
    if (address_str != NULL)
    {
        inet_pton(AF_INET, address_str, &sa_in->sin_addr);
    }
    else
    {
        sa_in->sin_addr.s_addr = INADDR_ANY;
    }
    sa_in->sin_family = AF_INET;
    sa_in->sin_port = htons(DEFAULT_PORT);
}

char *sockaddr_get_ip_str(const struct sockaddr_storage *sock_addr,
                          char *ip_str, size_t max_size)
{
    struct sockaddr_in addr_in;
    struct sockaddr_in6 addr_in6;

    switch (sock_addr->ss_family)
    {
    case AF_INET:
        memcpy(&addr_in, sock_addr, sizeof(struct sockaddr_in));
        inet_ntop(AF_INET, &addr_in.sin_addr, ip_str, max_size);
        return ip_str;
    case AF_INET6:
        memcpy(&addr_in6, sock_addr, sizeof(struct sockaddr_in6));
        inet_ntop(AF_INET6, &addr_in6.sin6_addr, ip_str, max_size);
        return ip_str;
    default:
        return "Invalid address family";
    }
}

char *sockaddr_get_port_str(const struct sockaddr_storage *sock_addr,
                            char *port_str, size_t max_size)
{
    struct sockaddr_in addr_in;
    struct sockaddr_in6 addr_in6;

    switch (sock_addr->ss_family)
    {
    case AF_INET:
        memcpy(&addr_in, sock_addr, sizeof(struct sockaddr_in));
        snprintf(port_str, max_size, "%d", ntohs(addr_in.sin_port));
        return port_str;
    case AF_INET6:
        memcpy(&addr_in6, sock_addr, sizeof(struct sockaddr_in6));
        snprintf(port_str, max_size, "%d", ntohs(addr_in6.sin6_port));
        return port_str;
    default:
        return "Invalid address family";
    }
}
/**
 * Close UCP endpoint.
 *
 * @param [in]  worker  Handle to the worker that the endpoint is associated
 *                      with.
 * @param [in]  ep      Handle to the endpoint to close.
 * @param [in]  flags   Close UCP endpoint mode. Please see
 *                      @a ucp_ep_close_flags_t for details.
 */
void ep_close(ucp_worker_h ucp_worker, ucp_ep_h ep, uint64_t flags)
{
    ucp_request_param_t param;

    param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
    param.flags = flags;
    /*close_req          = */
    ucp_ep_close_nbx(ep, &param);
}

uint64_t time_dif(struct timeval tv1, struct timeval tv2)
{
    struct timeval dtv;
    dtv.tv_sec = tv2.tv_sec - tv1.tv_sec;
    dtv.tv_usec = tv2.tv_usec - tv1.tv_usec;
    if (dtv.tv_usec < 0)
    {
        dtv.tv_sec--;
        dtv.tv_usec += 1000000;
    }
    return dtv.tv_sec * 1000000 + dtv.tv_usec;
}
/************* memory ********************************/
ucs_status_t mem_map_alloc(size_t length, int non_blk_flag, ucp_context_h ucp_context, ucp_mem_h *mem_handle)
{
    ucp_mem_map_params_t mem_map_params;
    ucs_status_t status;

    mem_map_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                                UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                                UCP_MEM_MAP_PARAM_FIELD_FLAGS;
    mem_map_params.address = NULL;
    mem_map_params.length = length;
    mem_map_params.flags = UCP_MEM_MAP_ALLOCATE;
    if (non_blk_flag)
    {
        mem_map_params.flags |= UCP_MEM_MAP_NONBLOCK;
    }
    status = ucp_mem_map(ucp_context, &mem_map_params, mem_handle);
    return status;
}

uint64_t *mem_base_address(ucp_mem_h mem_handle)
{
    ucp_mem_attr_t attr = {0};
    attr.field_mask = UCP_MEM_ATTR_FIELD_ADDRESS |
                      UCP_MEM_ATTR_FIELD_MEM_TYPE;
    if (ucp_mem_query(mem_handle, &attr) != UCS_OK)
        return NULL;
    return attr.address;
}
uint32_t get_mem_ucx_total_size(ucp_mem_h mem_handle)
{
    ucp_mem_attr_t attr = {0};
    attr.field_mask = UCP_MEM_ATTR_FIELD_LENGTH |
                      UCP_MEM_ATTR_FIELD_MEM_TYPE;
    ucp_mem_query(mem_handle, &attr);
    return attr.length;
}

uint64_t *mem_ucx_allocate(uint32_t size, ucp_context_h ucp_context, ucp_mem_h *mem_handle)
{
    if (mem_map_alloc(size, 0, ucp_context, mem_handle) != UCS_OK)
    {
        fprintf(stderr, "ucp_mem_map\n");
        return NULL;
    }
    return mem_base_address(*mem_handle);
}
/************* memory end ********************************/
/**
 * Initialize the client side. Create an endpoint from the client side to be
 * connected to the remote server (to the given IP).
 */
ucs_status_t start_client(ucp_worker_h ucp_worker, ucp_ep_h *client_ep)
{
    ucp_ep_params_t ep_params;
    struct sockaddr_storage connect_addr;
    ucs_status_t status;

    set_sock_addr(SERVR_ADDRESS, &connect_addr);

    /*
     * Endpoint field mask bits:
     * UCP_EP_PARAM_FIELD_FLAGS             - Use the value of the 'flags' field.
     * UCP_EP_PARAM_FIELD_SOCK_ADDR         - Use a remote sockaddr to connect
     *                                        to the remote peer.
     * UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE - Error handling mode - this flag
     *                                        is temporarily required since the
     *                                        endpoint will be closed with
     *                                        UCP_EP_CLOSE_MODE_FORCE which
     *                                        requires this mode.
     *                                        Once UCP_EP_CLOSE_MODE_FORCE is
     *                                        removed, the error handling mode
     *                                        will be removed.
     */
    ep_params.field_mask = UCP_EP_PARAM_FIELD_FLAGS |
                           UCP_EP_PARAM_FIELD_SOCK_ADDR |
                           UCP_EP_PARAM_FIELD_ERR_HANDLER |
                           UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE;
    ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
    ep_params.err_handler.cb = err_cb;
    ep_params.err_handler.arg = NULL;
    ep_params.flags = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER;
    ep_params.sockaddr.addr = (struct sockaddr *)&connect_addr;
    ep_params.sockaddr.addrlen = sizeof(connect_addr);
    ep_params.err_handler.arg = NULL;

    status = ucp_ep_create(ucp_worker, &ep_params, client_ep);
    if (status != UCS_OK)
    {
        fprintf(stderr, "failed to connect to %s (%s)\n", SERVR_ADDRESS,
                ucs_status_string(status));
    }
    return status;
}
