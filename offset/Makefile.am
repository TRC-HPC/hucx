#
# Copyright (C) Mellanox Technologies Ltd. 2001-2018.  ALL RIGHTS RESERVED.
#
# Copyright (C) UT-Battelle, LLC. 2015. ALL RIGHTS RESERVED.
# See file LICENSE for terms.
#

examplesdir = $(pkgdatadir)/smart
dist_examples_DATA =  server client statistic

if HAVE_CUDA
EXAMPLE_CUDA_LD_FLAGS = $(CUDA_LDFLAGS)
EXAMPLE_CUDA_LIBS = $(CUDA_LIBS)
# cuda.h couldn't be compiled with -pedantic flag
EXAMPLE_CUDA_CFLAGS =
EXAMPLE_CUDA_CPPFLAGS = $(CUDA_CPPFLAGS) -DHAVE_CUDA
else
EXAMPLE_CUDA_LD_FLAGS =
EXAMPLE_CUDA_LIBS =
EXAMPLE_CUDA_CFLAGS = $(CFLAGS_PEDANTIC)
EXAMPLE_CUDA_CPPFLAGS =
endif

EXAMPLE_CCLD_FLAGS = -lucs -ggdb -I$(includedir) -L$(libdir) -Wall -Werror -Wl,-rpath,$(libdir) \
                     $(EXAMPLE_CUDA_LD_FLAGS) $(EXAMPLE_CUDA_LIBS) $(EXAMPLE_CUDA_CPPFLAGS)

installcheck-local:
	@echo "INSTALLCHECK: Compiling examples with installed library"
	$(CC) -o tcp_adapter_server   $(examplesdir)/tcp_adapter_server.c   -lucp $(EXAMPLE_CCLD_FLAGS)
	$(CC) -o ucp_client_server $(examplesdir)/ucp_client_server.c -lucp $(EXAMPLE_CCLD_FLAGS)
	$(CC) -o clients $(examplesdir)/clients.c  -lucp $(EXAMPLE_CCLD_FLAGS)
	$(RM) *.o tcp_adapter_server ucp_client_server clients

if HAVE_EXAMPLES

noinst_PROGRAMS = \
	server \
	client  \
	statistic

server_SOURCES  = server.c utilities.c   
server_CFLAGS   = $(BASE_CFLAGS) $(EXAMPLE_CUDA_CFLAGS)
server_CPPFLAGS = $(BASE_CPPFLAGS) $(EXAMPLE_CUDA_CPPFLAGS)
server_LDFLAGS  = $(EXAMPLE_CUDA_LD_FLAGS)
server_LDADD    = $(top_builddir)/src/ucs/libucs.la \
                             $(top_builddir)/src/ucp/libucp.la \
                             $(EXAMPLE_CUDA_LIBS)

client_SOURCES  = client.c utilities.c  
client_CFLAGS   = $(BASE_CFLAGS) $(EXAMPLE_CUDA_CFLAGS)
client_CPPFLAGS = $(BASE_CPPFLAGS) $(EXAMPLE_CUDA_CPPFLAGS)
client_LDFLAGS  = $(EXAMPLE_CUDA_LD_FLAGS)
client_LDADD    = $(top_builddir)/src/ucs/libucs.la \
								$(top_builddir)/src/ucp/libucp.la \
								$(EXAMPLE_CUDA_LIBS)
								
statistic_SOURCES  = statistic.c  utilities.c 
statistic_CFLAGS   = $(BASE_CFLAGS) $(EXAMPLE_CUDA_CFLAGS)
statistic_CPPFLAGS = $(BASE_CPPFLAGS) $(EXAMPLE_CUDA_CPPFLAGS)
statistic_LDFLAGS  = $(EXAMPLE_CUDA_LD_FLAGS)
statistic_LDADD    = $(top_builddir)/src/ucs/libucs.la \
								$(top_builddir)/src/ucp/libucp.la \
								$(EXAMPLE_CUDA_LIBS)

endif
