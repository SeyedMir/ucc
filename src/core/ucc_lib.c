/**
 * Copyright (C) Mellanox Technologies Ltd. 2020.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "config.h"

#include "ucc_global_opts.h"
#include "ucc_lib.h"
#include "utils/ucc_log.h"
#include "utils/ucc_malloc.h"
#include "utils/ucc_math.h"
#include "components/cl/ucc_cl.h"
#include "components/tl/ucc_tl.h"
#include "ucc_mc.h"

UCS_CONFIG_DEFINE_ARRAY(cl_types, sizeof(ucc_cl_type_t),
                        UCS_CONFIG_TYPE_ENUM(ucc_cl_names));

static ucc_config_field_t ucc_lib_config_table[] = {
    {"CLS", "all", "Comma separated list of CL components to be used",
     ucc_offsetof(ucc_lib_config_t, cls), UCC_CONFIG_TYPE_ARRAY(cl_types)},

    {NULL}
};

UCC_CONFIG_REGISTER_TABLE(ucc_lib_config_table, "UCC", NULL, ucc_lib_config_t,
                          &ucc_config_global_list)

static inline int ucc_cl_requested(const ucc_lib_config_t *cfg, int cl_type)
{
    int i;
    for (i = 0; i < cfg->cls.count; i++) {
        if (cfg->cls.types[i] == cl_type) {
            return 1;
        }
    }
    return 0;
}

static inline void ucc_copy_lib_params(ucc_lib_params_t *dst,
                                       const ucc_lib_params_t *src)
{
    dst->mask = src->mask;
    UCC_COPY_PARAM_BY_FIELD(dst, src, UCC_LIB_PARAM_FIELD_THREAD_MODE,
                            thread_mode);
    UCC_COPY_PARAM_BY_FIELD(dst, src, UCC_LIB_PARAM_FIELD_COLL_TYPES,
                            coll_types);
    UCC_COPY_PARAM_BY_FIELD(dst, src, UCC_LIB_PARAM_FIELD_REDUCTION_TYPES,
                            reduction_types);
    UCC_COPY_PARAM_BY_FIELD(dst, src, UCC_LIB_PARAM_FIELD_SYNC_TYPE, sync_type);
    UCC_COPY_PARAM_BY_FIELD(dst, src, UCC_LIB_PARAM_FIELD_REDUCTION_WRAPPER,
                            reduction_wrapper);
}

/* Core logic for the selection of CL components:
   1. If user does not provide a set of required CLs then we try to
   use only those components that support the required input params.
   This means, if some component does not pass params check it is skipped.

   2. In contrast, if user explicitly requests a list of CLs to use,
   then we try to load ALL of them and report the supported attributes
   based on that selection. */
static ucc_status_t ucc_cl_lib_init(const ucc_lib_params_t *user_params,
                                    const ucc_lib_config_t *config,
                                    ucc_lib_info_t *lib)
{
    int                   n_cls = ucc_global_config.cl_framework.n_components;
    uint64_t              supported_coll_types = 0;
    ucc_thread_mode_t     supported_tm         = UCC_THREAD_MULTIPLE;
    ucc_lib_params_t      params               = *user_params;
    ucc_cl_lib_config_t  *cl_config            = NULL;
    ucc_cl_iface_t       *cl_iface;
    ucc_base_lib_t *      b_lib;
    ucc_base_lib_params_t b_params;
    ucc_cl_lib_t         *cl_lib;
    ucc_status_t          status;
    int                   i;

    lib->cl_libs =
        (ucc_cl_lib_t **)ucc_malloc(sizeof(ucc_cl_lib_t *) * n_cls, "cl_libs");
    if (!lib->cl_libs) {
        ucc_error("failed to allocate %zd bytes for cl_libs",
                  sizeof(ucc_cl_lib_t *) * n_cls);
        status = UCC_ERR_NO_MEMORY;
        goto error;
    }
    if (!(params.mask & UCC_LIB_PARAM_FIELD_THREAD_MODE)) {
        params.mask |= UCC_LIB_PARAM_FIELD_THREAD_MODE;
        params.thread_mode = UCC_THREAD_SINGLE;
    }
    ucc_copy_lib_params(&b_params.params, &params);
    ucc_assert(config->cls.count >= 1);
    lib->specific_cls_requested = (0 == ucc_cl_requested(config, UCC_CL_ALL));
    lib->n_cl_libs_opened     = 0;
    for (i = 0; i < n_cls; i++) {
        cl_iface = ucc_derived_of(ucc_global_config.cl_framework.components[i],
                                  ucc_cl_iface_t);
        /* User requested specific list of CLs and current cl_iface is not part
           of the list: skip it. */
        if (lib->specific_cls_requested &&
            (0 == ucc_cl_requested(config, cl_iface->type))) {
            continue;
        }
        if (params.thread_mode > cl_iface->attr.thread_mode) {
            /* Requested THREAD_MODE is not supported by the CL:
               1. If cls == "all" - just skip this CL
               2. If specific CLs are requested: continue and user will
                  have to query result attributes and check thread mode*/
            if (!lib->specific_cls_requested) {
                ucc_info("requested thread_mode is not supported by the CL: %s",
                         cl_iface->super.name);
                continue;
            }
        }
        status = ucc_cl_lib_config_read(cl_iface, lib->full_prefix, config,
                                        &cl_config);
        if (UCC_OK != status) {
            ucc_error("failed to read CL \"%s\" lib configuration",
                      cl_iface->super.name);
            goto error_cfg_read;
        }
        status = cl_iface->lib.init(&b_params, &cl_config->super, &b_lib);
        if (UCC_OK != status) {
            if (lib->specific_cls_requested) {
                ucc_error("lib_init failed for component: %s",
                          cl_iface->super.name);
                goto error_cl_init;
            } else {
                ucc_info("lib_init failed for component: %s, skipping",
                         cl_iface->super.name);
                ucc_base_config_release(&cl_config->super);
                continue;
            }
        }
        ucc_base_config_release(&cl_config->super);
        cl_lib                          = ucc_derived_of(b_lib, ucc_cl_lib_t);
        lib->cl_libs[lib->n_cl_libs_opened++] = cl_lib;
        supported_coll_types |= cl_iface->attr.coll_types;
        if (cl_iface->attr.thread_mode < supported_tm) {
            supported_tm = cl_iface->attr.thread_mode;
        }
        ucc_info("lib_prefix \"%s\": initialized component \"%s\" priority %d",
                 config->full_prefix, cl_iface->super.name, cl_lib->priority);
    }

    if (lib->n_cl_libs_opened == 0) {
        ucc_error("lib_init failed: no CLs left after filtering");
        status = UCC_ERR_NO_MESSAGE;
        goto error;
    }

    /* Check if the combination of the selected CLs provides all the
       requested coll_types: not an error, just print a message if not
       all the colls are supproted */
    if (params.mask & UCC_LIB_PARAM_FIELD_COLL_TYPES &&
        ((params.coll_types & supported_coll_types) != params.coll_types)) {
        ucc_debug("selected set of CLs does not provide all the requested "
                  "coll_types");
    }
    if (params.thread_mode > supported_tm) {
        ucc_debug("selected set of CLs does not provide the requested "
                  "thread_mode");
    }
    lib->attr.coll_types  = supported_coll_types;
    lib->attr.thread_mode = ucc_min(supported_tm, params.thread_mode);
    return UCC_OK;

error_cl_init:
    ucc_base_config_release(&cl_config->super);
error_cfg_read:
    for (i = 0; i < lib->n_cl_libs_opened; i++) {
        lib->cl_libs[i]->iface->lib.finalize(&lib->cl_libs[i]->super);
    }
error:
    if (lib->cl_libs)
        ucc_free(lib->cl_libs);
    return status;
}

static ucc_status_t ucc_tl_lib_init(const ucc_lib_params_t *user_params,
                                    const ucc_lib_config_t *config,
                                    ucc_lib_info_t *lib)
{
    ucc_status_t          status;
    int                   i, n_tls;
    uint64_t              required_tls;
    ucc_cl_lib_attr_t     cl_lib_attr;
    ucc_cl_lib_t         *cl_lib;
    ucc_tl_lib_t         *tl_lib;
    ucc_tl_lib_config_t  *tl_config;
    ucc_base_lib_t *      b_lib;
    ucc_base_lib_params_t b_params;
    ucc_tl_iface_t *tl_iface;

    ucc_copy_lib_params(&b_params.params, user_params);
    required_tls = 0;
    for (i = 0; i < lib->n_cl_libs_opened; i++) {
        cl_lib = lib->cl_libs[i];
        status = cl_lib->iface->lib.get_attr(&cl_lib->super, &cl_lib_attr.super);
        if (UCC_OK != status) {
            ucc_error("failed to query cl lib %s attr", cl_lib->iface->super.name);
            return status;
        }
        required_tls |= cl_lib_attr.tls;
    }
    n_tls = ucc_global_config.tl_framework.n_components;
    lib->tl_libs =
        (ucc_tl_lib_t **)ucc_malloc(sizeof(ucc_tl_lib_t *) * n_tls, "tl_libs");
    lib->n_tl_libs_opened = 0;
    if (!lib->cl_libs) {
        ucc_error("failed to allocate %zd bytes for tl_libs",
                  sizeof(ucc_tl_lib_t *) * n_tls);
        return UCC_ERR_NO_MEMORY;
    }

    for (i=0; i<n_tls; i++) {
        tl_iface = ucc_derived_of(ucc_global_config.tl_framework.components[i],
                                  ucc_tl_iface_t);
        /* Check if loaded tl component is required by any of the CLs if yes - init it
           Failure to init a TL is not critical. Let CLs deal with it later during
           cl_context_create.
         */
        if (tl_iface->type & required_tls) {
            status = ucc_tl_lib_config_read(tl_iface, lib->full_prefix, config,
                                            &tl_config);
            if (UCC_OK != status) {
                ucc_warn("failed to read TL \"%s\" lib configuration",
                         tl_iface->super.name);
                continue;
            }
            status = tl_iface->lib.init(&b_params, &tl_config->super, &b_lib);
            ucc_base_config_release(&tl_config->super);
            if (UCC_OK != status) {
                ucc_info("lib_init failed for component: %s, skipping",
                         tl_iface->super.name);
                continue;
            }
            tl_lib = ucc_derived_of(b_lib, ucc_tl_lib_t);
            lib->tl_libs[lib->n_tl_libs_opened++] = tl_lib;
        }
    }
    return UCC_OK;
}

ucc_status_t ucc_init_version(unsigned api_major_version,
                              unsigned api_minor_version,
                              const ucc_lib_params_t *params,
                              const ucc_lib_config_h config, ucc_lib_h *lib_p)
{
    unsigned        major_version, minor_version, release_number;
    ucc_status_t    status;
    ucc_lib_info_t *lib;

    *lib_p = NULL;

    if (UCC_OK != (status = ucc_constructor())) {
        return status;
    }
    if (UCC_OK != (status = ucc_mc_init())) {
        return status;
    }

    ucc_get_version(&major_version, &minor_version, &release_number);

    if ((api_major_version != major_version) ||
        ((api_major_version == major_version) &&
         (api_minor_version > minor_version))) {
        ucc_warn(
            "UCC version is incompatible, required: %d.%d, actual: %d.%d.%d",
            api_major_version, api_minor_version, major_version, minor_version,
            release_number);
    }

    lib = ucc_malloc(sizeof(ucc_lib_info_t), "lib_info");
    if (!lib) {
        ucc_error("failed to allocate %zd bytes for lib_info",
                  sizeof(ucc_lib_info_t));
        return UCC_ERR_NO_MEMORY;
    }
    lib->full_prefix = strdup(config->full_prefix);
    if (!lib->full_prefix) {
        ucc_error("failed strdup for full_prefix");
        status = UCC_ERR_NO_MEMORY;
        goto error;
    }
    /* Initialize ucc lib handle using requirements from the user
       provided via params/config and available CLs in the
       CL component framework.

       The lib_p object will contain the array of ucc_cl_lib_t objects
       that are allocated using CL init/finalize interface. */
    status = ucc_cl_lib_init(params, config, lib);
    if (UCC_OK != status) {
        goto error;
    }

    status = ucc_tl_lib_init(params, config, lib);
    if (UCC_OK != status) {
        goto error;
    }

    *lib_p = lib;
    return UCC_OK;
error:
    ucc_free(lib);
    return status;
}

ucc_status_t ucc_lib_config_read(const char *env_prefix, const char *filename,
                                 ucc_lib_config_t **config_p)
{
    ucc_lib_config_t *config;
    ucc_status_t      status;
    size_t            full_prefix_len;
    const char       *base_prefix = "UCC_";

    config = ucc_malloc(sizeof(*config), "lib_config");
    if (config == NULL) {
        ucc_error("failed to allocate %zd bytes for lib_config",
                  sizeof(*config));
        status = UCC_ERR_NO_MEMORY;
        goto err;
    }
    /* full_prefix for a UCC lib config is either just
          (i)  "UCC_" - if no "env_prefix" is provided, or
          (ii) "AAA_UCC" - where AAA is the value of "env_prefix".
       Allocate space to build prefix str and two characters ("_" and “\0”) */
    full_prefix_len =
        strlen(base_prefix) + (env_prefix ? strlen(env_prefix) : 0) + 2;
    config->full_prefix = ucc_malloc(full_prefix_len, "full_prefix");
    if (!config->full_prefix) {
        ucc_error("failed to allocate %zd bytes for full_prefix",
                  full_prefix_len);
        status = UCC_ERR_NO_MEMORY;
        goto err_free_config;
    }
    if (env_prefix) {
        ucc_snprintf_safe(config->full_prefix, full_prefix_len, "%s_%s",
                          env_prefix, base_prefix);
    } else {
        ucc_strncpy_safe(config->full_prefix, base_prefix,
                         strlen(base_prefix) + 1);
    }

    status = ucc_config_parser_fill_opts(config, ucc_lib_config_table,
                                         config->full_prefix, NULL, 0);
    if (status != UCC_OK) {
        ucc_error("failed to read UCC lib config");
        goto err_free_prefix;
    }
    *config_p = config;
    return UCC_OK;

err_free_prefix:
    ucc_free(config->full_prefix);
err_free_config:
    ucc_free(config);
err:
    return status;
}

void ucc_lib_config_release(ucc_lib_config_t *config)
{
    ucc_config_parser_release_opts(config, ucc_lib_config_table);
    ucc_free(config->full_prefix);
    ucc_free(config);
}

void ucc_lib_config_print(const ucc_lib_config_h config, FILE *stream,
                          const char *title,
                          ucc_config_print_flags_t print_flags)
{
    ucc_config_parser_print_opts(stream, title, config, ucc_lib_config_table,
                                 NULL, config->full_prefix, print_flags);
}

ucc_status_t ucc_lib_config_modify(ucc_lib_config_h config, const char *name,
                                   const char *value)
{
    return ucc_config_parser_set_value(config, ucc_lib_config_table, name,
                                       value);
}

ucc_status_t ucc_finalize(ucc_lib_info_t *lib)
{
    int i;
    ucc_assert(lib->n_cl_libs_opened > 0);
    ucc_assert(lib->cl_libs);
    /* If some CL components fails in finalize we will return
       its failure status to the user, however we will still
       try to continue and finalize other CLs */
    for (i = 0; i < lib->n_cl_libs_opened; i++) {
        lib->cl_libs[i]->iface->lib.finalize(&lib->cl_libs[i]->super);
    }
    ucc_free(lib->cl_libs);
    ucc_free(lib);
    ucc_mc_finalize();
    return UCC_OK;
}
