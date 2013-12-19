TARGET := mpi_max_clique

SOURCES := mpi_max_clique.cc

TGT_LDFLAGS := -L${TARGET_DIR}
TGT_LDLIBS := -lsolver -lmax_clique_mpi -lmax_clique -lthreads -lgraph $(boost_ldlibs) $(boost_mpi_ldlibs)
TGT_PREREQS := libmax_clique.a libmax_clique_mpi.a libgraph.a libthreads.a libsolver.a
TGT_CXX := ${MPICXX}
TGT_LINKER := ${MPICXX}
