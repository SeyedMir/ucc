#
# Copyright (C) Mellanox Technologies Ltd. 2020.  ALL RIGHTS RESERVED.
#

sources =            \
	tl_ucp.h         \
	tl_ucp.c         \
	tl_ucp_lib.c     \
	tl_ucp_context.c \
	tl_ucp_team.c    \
	tl_ucp_ep.h      \
	tl_ucp_ep.c      \
	tl_ucp_addr.h    \
	tl_ucp_addr.c    \
	tl_ucp_coll.c

module_LTLIBRARIES = libucc_tl_ucp.la
libucc_tl_ucp_la_SOURCES  = $(sources)
libucc_tl_ucp_la_CPPFLAGS = $(AM_CPPFLAGS) $(UCX_CPPFLAGS)
libucc_tl_ucp_la_LDFLAGS  = -version-info $(SOVERSION) --as-needed $(UCX_LDFLAGS)
libucc_tl_ucp_la_LIBADD   = $(UCX_LIBADD)

include $(top_srcdir)/config/module.am
