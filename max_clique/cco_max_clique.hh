/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef PARASOLS_GUARD_MAX_CLIQUE_CCO_MAX_CLIQUE_HH
#define PARASOLS_GUARD_MAX_CLIQUE_CCO_MAX_CLIQUE_HH 1

#include <graph/graph.hh>
#include <max_clique/max_clique_params.hh>
#include <max_clique/max_clique_result.hh>

namespace parasols
{
    enum class CCOPermutations
    {
        None,
        Defer1,
        Defer2,
        Sort
    };

    /**
     * Max clique algorithm.
     */
    template <CCOPermutations, MaxCliqueOrder>
    auto cco_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult;
}

#endif
