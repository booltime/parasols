/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef PARASOLS_GUARD_SUBGRAPH_ISOMORPHISM_CB_SUBGRAPH_ISOMORPHISM_HH
#define PARASOLS_GUARD_SUBGRAPH_ISOMORPHISM_CB_SUBGRAPH_ISOMORPHISM_HH 1

#include <subgraph_isomorphism/subgraph_isomorphism_params.hh>
#include <subgraph_isomorphism/subgraph_isomorphism_result.hh>
#include <graph/graph.hh>

namespace parasols
{
    auto cb_subgraph_isomorphism(const std::pair<Graph, Graph> &, const SubgraphIsomorphismParams &) -> SubgraphIsomorphismResult;

    auto cbj_subgraph_isomorphism(const std::pair<Graph, Graph> &, const SubgraphIsomorphismParams &) -> SubgraphIsomorphismResult;

    auto cbi_subgraph_isomorphism(const std::pair<Graph, Graph> &, const SubgraphIsomorphismParams &) -> SubgraphIsomorphismResult;

    auto cbji_subgraph_isomorphism(const std::pair<Graph, Graph> &, const SubgraphIsomorphismParams &) -> SubgraphIsomorphismResult;
}

#endif
