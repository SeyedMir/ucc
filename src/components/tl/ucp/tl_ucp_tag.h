/**
 * Copyright (C) Mellanox Technologies Ltd. 2020.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */
#ifndef UCC_TL_UCP_TAG_H_
#define UCC_TL_UCP_TAG_H_
#include "utils/ucc_compiler_def.h"

/*
 * UCP tag structure:
 *
 *    01234           567     01234567 01234567 01234567 01234567 01234567 01234567 01234567
 *                |         |                  |                          |
 *   RESERVED (5) | CTX (3) | message tag (16) |     source rank (24)     |  team id (16)
 */

#define UCC_TL_UCP_RESERVED_BITS 5
#define UCC_TL_UCP_CTX_BITS 3
#define UCC_TL_UCP_TAG_BITS 16
#define UCC_TL_UCP_SENDER_BITS 24
#define UCC_TL_UCP_ID_BITS 16

#define UCC_TL_UCP_RESERVED_BITS_OFFSET                                        \
    (UCC_TL_UCP_ID_BITS + UCC_TL_UCP_SENDER_BITS + UCC_TL_UCP_TAG_BITS +       \
     UCC_TL_UCP_CTX_BITS)

#define UCC_TL_UCP_CTX_BITS_OFFSET                                             \
    (UCC_TL_UCP_ID_BITS + UCC_TL_UCP_SENDER_BITS + UCC_TL_UCP_TAG_BITS)

#define UCC_TL_UCP_TAG_BITS_OFFSET (UCC_TL_UCP_ID_BITS + UCC_TL_UCP_SENDER_BITS)
#define UCC_TL_UCP_SENDER_BITS_OFFSET (UCC_TL_UCP_ID_BITS)
#define UCC_TL_UCP_ID_BITS_OFFSET 0

#define UCC_TL_UCP_MAX_TAG UCC_MASK(UCC_TL_UCP_TAG_BITS)
#define UCC_TL_UCP_MAX_SENDER UCC_MASK(UCC_TL_UCP_SENDER_BITS)
#define UCC_TL_UCP_MAX_ID UCC_MASK(UCC_TL_UCP_ID_BITS)

#define UCC_TL_UCP_TAG_SENDER_MASK                                             \
    UCC_MASK(UCC_TL_UCP_ID_BITS + UCC_TL_UCP_SENDER_BITS)
#endif
