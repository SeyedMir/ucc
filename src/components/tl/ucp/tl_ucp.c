/**
 * Copyright (C) Mellanox Technologies Ltd. 2020.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "tl_ucp.h"
#include "utils/ucc_malloc.h"
ucc_status_t ucc_tl_ucp_get_lib_attr(const ucc_base_lib_t *lib, ucc_base_attr_t *base_attr);
ucc_status_t ucc_tl_ucp_get_context_attr(const ucc_base_context_t *context, ucc_base_attr_t *base_attr);

static ucc_config_field_t ucc_tl_ucp_lib_config_table[] = {
    {"", "", NULL, ucc_offsetof(ucc_tl_ucp_lib_config_t, super),
     UCC_CONFIG_TYPE_TABLE(ucc_tl_lib_config_table)},

    {NULL}
};

static ucs_config_field_t ucc_tl_ucp_context_config_table[] = {
    {"", "", NULL, ucc_offsetof(ucc_tl_ucp_context_config_t, super),
     UCC_CONFIG_TYPE_TABLE(ucc_tl_context_config_table)},

    {"PRECONNECT", "1024",
     "Threshold that defines the number of ranks in the UCC team/context "
     "below which the team/context enpoints will be preconnected during "
     "corresponding team/context create call",
     ucc_offsetof(ucc_tl_ucp_context_config_t, preconnect),
     UCC_CONFIG_TYPE_UINT},

    {NULL}};

UCC_CLASS_DEFINE_NEW_FUNC(ucc_tl_ucp_lib_t, ucc_base_lib_t,
                          const ucc_base_lib_params_t *,
                          const ucc_base_config_t *);

UCC_CLASS_DEFINE_DELETE_FUNC(ucc_tl_ucp_lib_t, ucc_base_lib_t);

UCC_CLASS_DEFINE_NEW_FUNC(ucc_tl_ucp_context_t, ucc_base_context_t,
                          const ucc_base_context_params_t *,
                          const ucc_base_config_t *);

UCC_CLASS_DEFINE_DELETE_FUNC(ucc_tl_ucp_context_t, ucc_base_context_t);

UCC_CLASS_DEFINE_NEW_FUNC(ucc_tl_ucp_team_t, ucc_base_team_t,
                          ucc_base_context_t *, const ucc_base_team_params_t *);

ucc_status_t ucc_tl_ucp_team_create_test(ucc_base_team_t *tl_team);
ucc_status_t ucc_tl_ucp_team_destroy(ucc_base_team_t *tl_team);
ucc_status_t ucc_tl_ucp_coll_init(ucc_base_coll_op_args_t *coll_args,
                                  ucc_base_team_t *team,
                                  ucc_coll_task_t **task);

UCC_TL_IFACE_DECLARE(ucp, UCP);
