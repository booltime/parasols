/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef PARASOLS_GUARD_MAX_CLIQUE_EDCCO_MAX_CLIQUE_HH
#define PARASOLS_GUARD_MAX_CLIQUE_EDCCO_MAX_CLIQUE_HH 1

#include <graph/graph.hh>
#include <cco/cco.hh>
#include <max_clique/cco_base.hh>
#include <max_clique/max_clique_params.hh>
#include <max_clique/max_clique_result.hh>

namespace parasols
{
    /**
     * Super duper max clique algorithm, with early discrepancies.
     */
    template <CCOPermutations, CCOInference, CCOMerge>
    auto edcco_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult;

    /**
     * Super duper max clique algorithm, with increasing discrepancies.
     */
    template <CCOPermutations, CCOInference, CCOMerge>
    auto id1cco_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult;

    /**
     * Super duper max clique algorithm, with increasing discrepancies.
     */
    template <CCOPermutations, CCOInference, CCOMerge>
    auto id2cco_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult;

    /**
     * Super duper max clique algorithm, with increasing discrepancies.
     */
    template <CCOPermutations, CCOInference, CCOMerge>
    auto id3cco_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult;

    /**
     * Super duper max clique algorithm, with increasing discrepancies.
     */
    template <CCOPermutations, CCOInference, CCOMerge>
    auto id4cco_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult;
}

#endif