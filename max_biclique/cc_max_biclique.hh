/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef PARASOLS_GUARD_MAX_BICLIQUE_CC_MAX_BICLIQUE_HH
#define PARASOLS_GUARD_MAX_BICLIQUE_CC_MAX_BICLIQUE_HH 1

#include <graph/graph.hh>
#include <max_biclique/max_biclique_params.hh>
#include <max_biclique/max_biclique_result.hh>

namespace parasols
{
    /**
     * Less stupid max biclique algorithm.
     */
    auto cc_max_biclique(const Graph & graph, const MaxBicliqueParams &) -> MaxBicliqueResult;
}

#endif
