/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2019-2020.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "ucg_test.h"

#include "ucp/api/ucp.h"


using namespace std;

static class ucg_resource_factory g_ucg_resource_factory;

ucg_resource_factory *ucg_test::m_resource_factory = &g_ucg_resource_factory;

ucg_test::ucg_test()
{
    m_ucg_context = NULL;
    m_ucg_worker = NULL;

    init_ucg_component();
}

ucg_test::~ucg_test()
{
    if (m_ucg_worker != NULL) {
        ucg_worker_destroy(m_ucg_worker);
        m_ucg_worker = NULL;
    }

    if (m_ucg_context != NULL) {
        ucg_cleanup(m_ucg_context);
        m_ucg_context = NULL;
    }
}

void ucg_test::init_ucg_component()
{
    ucg_params_t params;
    ucg_config_t *config = NULL;

    /* Read options */
    (void)ucg_config_read("MPI", NULL, &config);

    /* Initialize UCX context */
    params.field_mask = UCP_PARAM_FIELD_FEATURES |
                        UCP_PARAM_FIELD_REQUEST_SIZE |
                        UCP_PARAM_FIELD_REQUEST_INIT |
                        UCP_PARAM_FIELD_REQUEST_CLEANUP |
                        // UCP_PARAM_FIELD_TAG_SENDER_MASK |
                        UCP_PARAM_FIELD_MT_WORKERS_SHARED |
                        UCP_PARAM_FIELD_ESTIMATED_NUM_EPS;
    params.features = UCP_FEATURE_TAG |
                      UCP_FEATURE_RMA |
                      UCP_FEATURE_AMO32 |
                      UCP_FEATURE_AMO64 |
                      UCP_FEATURE_GROUPS;
    // params.request_size      = sizeof(ompi_request_t);
    // params.request_init      = mca_coll_ucx_request_init;
    // params.request_cleanup   = mca_coll_ucx_request_cleanup;
    params.mt_workers_shared = 0; /* we do not need mt support for context
                                     since it will be protected by worker */
    params.estimated_num_eps = 0;

    (void)ucg_init(&params, config, &m_ucg_context);

    ucg_worker_params_t work_params;

    /* TODO check MPI thread mode */
    work_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    work_params.thread_mode = UCS_THREAD_MODE_SINGLE;
    work_params.thread_mode = UCS_THREAD_MODE_SINGLE;

    (void)ucg_worker_create(m_ucg_context, &work_params, &m_ucg_worker);
}

ucg_collective_type_t ucg_test::create_allreduce_coll_type() const {
    ucg_collective_type_t type;
    type.modifiers = (ucg_collective_modifiers) (UCG_GROUP_COLLECTIVE_MODIFIER_AGGREGATE |
                                                 UCG_GROUP_COLLECTIVE_MODIFIER_BROADCAST);
    type.root = 0;
    return type;
}

ucg_collective_type_t ucg_test::create_alltoallv_coll_type() const {
    ucg_collective_type_t type;
    type.modifiers = (ucg_collective_modifiers) (UCG_GROUP_COLLECTIVE_MODIFIER_ALLTOALLV |
                                                 UCG_GROUP_COLLECTIVE_MODIFIER_VARIABLE_LENGTH);
    type.root = 0;
    return type;
}

ucg_collective_type_t ucg_test::create_bcast_coll_type() const {
    ucg_collective_type_t type;
    type.modifiers = (ucg_collective_modifiers) (UCG_GROUP_COLLECTIVE_MODIFIER_BROADCAST |
                                                 UCG_GROUP_COLLECTIVE_MODIFIER_SINGLE_SOURCE);
    type.root = 0;
    return type;
}

ucg_collective_type_t ucg_test::create_barrier_coll_type() const {
    ucg_collective_type_t type;
    type.modifiers = (ucg_collective_modifiers) (UCG_GROUP_COLLECTIVE_MODIFIER_AGGREGATE |
                                                 UCG_GROUP_COLLECTIVE_MODIFIER_BROADCAST |
                                                 UCG_GROUP_COLLECTIVE_MODIFIER_BARRIER);
    type.root = 0;
    return type;
}

ucg_collective_params_t *ucg_test::create_allreduce_params() const {
    size_t count = 2;
    int *send_buf = new int[count];
    int *recv_buf = new int[count];
    for (size_t i = 0; i < count; i++) {
        send_buf[i] = i;
        recv_buf[i] = -1;
    }

    ucg_ompi_op *op = new ucg_ompi_op();
    op->commutative = false;

    m_resource_factory->ctype = COLL_TYPE_ALLREDUCE;
    return m_resource_factory->create_collective_params(create_alltoallv_coll_type().modifiers,
                                                        0, send_buf, count, recv_buf, sizeof(int), NULL, op);
}

ucg_collective_params_t *ucg_test::create_alltoallv_params() const {
    size_t count = 2;

    int *send_buf       = new int[count];
    int *recv_buf       = new int[count];
    int *send_counts    = new int[count];
    int *sdispls        = new int[count];
    int *recv_counts    = new int[count];
    int *rdispls        = new int[count];

    for (size_t i = 0; i < count; i++) {
        send_buf[i] = i;
        recv_buf[i] = -1;
    }

    for (size_t i = 0; i < count; i++) {
        send_counts[i] = 1;
        recv_counts[i] = 1;
    }

    sdispls[0] = 0;
    rdispls[0] = 0;

    for (size_t i = 0; i < (count - 1); i++) {
        sdispls[i + 1] = sdispls[i] + send_counts[i];
        rdispls[i + 1] = rdispls[i] + recv_counts[i];
    }

    m_resource_factory->ctype = COLL_TYPE_ALLTOALLV;
    return m_resource_factory->create_var_collective_params(create_alltoallv_coll_type().modifiers, 0, send_buf,
        send_counts, sdispls, recv_buf, recv_counts, rdispls, sizeof(int), NULL);
}

ucg_collective_params_t *ucg_test::create_bcast_params() const {
    size_t count = 2;
    int *send_buf = new int[count];
    int *recv_buf = new int[count];
    for (size_t i = 0; i < count; i++) {
        send_buf[i] = i;
        recv_buf[i] = -1;
    }

    m_resource_factory->ctype = COLL_TYPE_BCAST;
    return m_resource_factory->create_collective_params(create_bcast_coll_type().modifiers,
                                                        0, send_buf, count, recv_buf, sizeof(int), NULL, NULL);
}

int mca_coll_ucg_datatype_convert_for_ut(void *mpi_dt, ucp_datatype_t *ucp_dt)
{
    if (mpi_dt != NULL) {
        ucs_info("mca_coll_ucg_datatype_convert_for_ut");
    }

    *ucp_dt = UCP_DATATYPE_CONTIG;
    return 0;
}

ucg_builtin_config_t *ucg_resource_factory::create_config(
    unsigned bcast_alg, unsigned allreduce_alg, unsigned barrier_alg)
{
    ucg_builtin_config_t *config = new ucg_builtin_config_t;
    config->super.ft = UCG_PLAN_FT_IGNORE;

    config->bmtree.degree_inter_fanout = 8;
    config->bmtree.degree_inter_fanin = 8;
    config->bmtree.degree_intra_fanout = 2;
    config->bmtree.degree_intra_fanin = 2;

    config->recursive.factor = 2;

    config->cache_size = 1000;
    config->mem_reg_opt_cnt = 10;
    config->bcopy_to_zcopy_opt = 1;
    config->pipelining = 0;
    config->short_max_tx = 200;
    config->bcopy_max_tx = 32768;

    config->bcast_algorithm = bcast_alg;
    config->allreduce_algorithm = allreduce_alg;
    config->barrier_algorithm = barrier_alg;

    return config;
}

ucs_status_t resolve_address_callback(void *cb_group_obj, ucg_group_member_index_t index,
                                      ucg_address_t **addr, size_t *addr_len)
{
    *addr_len = 0;
    return UCS_OK;
}

static ucg_group_member_index_t mpi_global_idx_dummy(void *cb_group_obj, ucg_group_member_index_t index)
{
    return 0;
}

typedef struct {
    uint32_t node_id    : 16;
    uint32_t sock_id    : 8;
    uint32_t reserved   : 8;
} rank_location_t;

#define MAX_MEMBER_COUNT 128
rank_location_t g_locs[MAX_MEMBER_COUNT];

enum ucg_group_member_distance mca_coll_ucx_get_distance_for_ut(void *comm, int rank1, int rank2)
{
    rank_location_t loc1;
    rank_location_t loc2;

    if (rank1 == rank2) {
        return UCG_GROUP_MEMBER_DISTANCE_SELF;
    }

    loc1 = g_locs[rank1];
    loc2 = g_locs[rank2];
    if (loc1.node_id != loc2.node_id) {
        return UCG_GROUP_MEMBER_DISTANCE_NET;
    }
    if (loc1.sock_id != loc2.sock_id) {
        return UCG_GROUP_MEMBER_DISTANCE_HOST;
    }

    return UCG_GROUP_MEMBER_DISTANCE_SOCKET;
}

ucg_group_params_t *ucg_resource_factory::create_group_params(
    ucg_rank_info my_rank_info, const std::vector<ucg_rank_info> &rank_infos)
{
    ucg_group_params_t *args = new ucg_group_params_t();
    args->member_count = rank_infos.size();
    args->member_index = my_rank_info.rank;
    args->cid = 0;
    args->mpi_reduce_f = NULL;
    args->resolve_address_f = &resolve_address_callback;
    args->release_address_f = NULL;
    args->cb_group_obj = NULL;
    args->op_is_commute_f = ompi_op_is_commute;
    args->mpi_dt_convert = mca_coll_ucg_datatype_convert_for_ut;
    args->mpi_rank_distance = mca_coll_ucx_get_distance_for_ut;
    args->topo_args.nrank_uncontinue = 0;
    args->topo_args.srank_uncontinue = 0;
    args->topo_args.ppn_unbalance = 0;
    args->topo_args.pps_unbalance = 0;
    args->topo_args.bind_to_none = 0;
    args->topo_args.ppn_local = args->member_count / (rank_infos[args->member_count - 1].nodex_idx + 1);
    args->topo_args.pps_local = args->topo_args.ppn_local;
    args->topo_args.ppn_max = args->topo_args.ppn_local;
    args->topo_args.node_nums = 1;
    args->node_index = (uint16_t *) malloc(args->member_count * sizeof(*args->node_index));
    args->mpi_global_idx_f = mpi_global_idx_dummy;

    for (size_t i = 0; i < rank_infos.size(); i++) {
        args->node_index[i] = rank_infos[i].nodex_idx;
        g_locs[i].node_id = rank_infos[i].nodex_idx;
        g_locs[i].sock_id = rank_infos[i].socket_idx;
    }

    return args;
}

ucg_group_h ucg_resource_factory::create_group(const ucg_group_params_t *params, ucg_worker_h ucg_worker)
{
    ucg_group_h group;
    ucg_group_create(ucg_worker, params, &group);
    return group;
}

ucg_collective_params_t *ucg_resource_factory::create_collective_params(
    ucg_collective_modifiers modifiers, ucg_group_member_index_t root,
    void *send_buffer, int count, void *recv_buffer, size_t dt_len, void *dt_ext, void *op_ext)
{
    ucg_collective_params_t *params = new ucg_collective_params_t();
    params->type.modifiers = modifiers;
    params->type.root = root;
    params->coll_type = ctype;
    params->send.buf = send_buffer;
    params->send.count = count;
    params->send.dt_len = dt_len;
    params->send.dt_ext = dt_ext;
    params->send.op_ext = op_ext;

    params->recv.buf = recv_buffer;
    params->recv.count = count;
    params->recv.dt_len = dt_len;
    params->recv.dt_ext = dt_ext;
    params->recv.op_ext = op_ext;

    return params;
}

ucg_collective_params_t *ucg_resource_factory::create_var_collective_params(ucg_collective_modifiers modifiers,
    ucg_group_member_index_t root, void *send_buffer, int *send_counts, int *sdispls, void *recv_buffer,
    int *recv_counts, int *rdispls, size_t dt_len, void *dt_ext)
{
    ucg_collective_params_t *params = new ucg_collective_params_t();
    params->type.modifiers = modifiers;
    params->type.root = root;
    params->coll_type = ctype;
    params->send.buf = send_buffer;
    params->send.counts = send_counts;
    params->send.dt_len = dt_len;
    params->send.dt_ext = dt_ext;
    params->send.displs = sdispls;

    params->recv.buf = recv_buffer;
    params->recv.counts = recv_counts;
    params->recv.dt_len = dt_len;
    params->recv.dt_ext = dt_ext;
    params->recv.displs = rdispls;

    return params;
}

void ucg_resource_factory::create_balanced_rank_info(std::vector<ucg_rank_info> &rank_infos,
                                                     size_t nodes, size_t ppn, bool map_by_socket)
{
    int rank = 0;
    ucg_rank_info info;

    for (size_t i = 0; i < nodes; i++) {
        for (size_t j = 0; j < ppn; j++) {
            info.rank = rank++;
            info.nodex_idx = i;
            if (map_by_socket) {
                info.socket_idx = j < ppn / 2 ? 0 : 1;
            } else {
                info.socket_idx = 0;
            }

            rank_infos.push_back(info);
        }
    }
}

int ompi_op_is_commute(void *op)
{
    return (int) ((ucg_ompi_op *) op)->commutative;
}
