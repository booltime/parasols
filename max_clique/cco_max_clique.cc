/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include <max_clique/cco_max_clique.hh>
#include <max_clique/cco_base.hh>
#include <max_clique/print_incumbent.hh>

#include <graph/template_voodoo.hh>
#include <graph/merge_cliques.hh>

#include <algorithm>
#include <thread>
#include <mutex>

using namespace parasols;

namespace
{
    template <CCOPermutations perm_, CCOInference inference_, CCOMerge merge_, unsigned size_, typename VertexType_>
    struct CCO : CCOBase<perm_, inference_, size_, VertexType_, CCO<perm_, inference_, merge_, size_, VertexType_> >
    {
        using Base = CCOBase<perm_, inference_, size_, VertexType_, CCO<perm_, inference_, merge_, size_, VertexType_> >;

        using Base::CCOBase;

        using Base::original_graph;
        using Base::graph;
        using Base::params;
        using Base::expand;
        using Base::order;
        using Base::colour_class_order;

        MaxCliqueResult result;

        std::list<std::set<int> > previouses;

        auto run() -> MaxCliqueResult
        {
            result.size = params.initial_bound;

            std::vector<unsigned> c;
            c.reserve(graph.size());

            FixedBitSet<size_> p; // potential additions
            p.resize(graph.size());
            p.set_all();

            std::vector<int> positions;
            positions.reserve(graph.size());
            positions.push_back(0);

            // initial colouring
            std::array<VertexType_, size_ * bits_per_word> initial_p_order;
            std::array<VertexType_, size_ * bits_per_word> initial_colours;
            colour_class_order(SelectColourClassOrderOverload<perm_>(), p, initial_p_order, initial_colours, 0);
            result.initial_colour_bound = initial_colours[graph.size() - 1];

            print_position(params, "initial colouring used " + std::to_string(result.initial_colour_bound), std::vector<int>{ });

            // go!
            expand(c, p, initial_p_order, initial_colours, positions);

            // hack for enumerate
            if (params.enumerate)
                result.size = result.members.size();

            return result;
        }

        auto increment_nodes() -> void
        {
            ++result.nodes;
        }

        auto recurse(
                std::vector<unsigned> & c,                       // current candidate clique
                FixedBitSet<size_> & p,
                const std::array<VertexType_, size_ * bits_per_word> & p_order,
                const std::array<VertexType_, size_ * bits_per_word> & colours,
                std::vector<int> & position
                ) -> bool
        {
            expand(c, p, p_order, colours, position);
            return true;
        }

        auto potential_new_best(
                const std::vector<unsigned> & c,
                const std::vector<int> & position) -> void
        {
            switch (merge_) {
                case CCOMerge::None:
                    if (c.size() > result.size) {
                        if (params.enumerate) {
                            ++result.result_count;
                            result.size = c.size() - 1;
                        }
                        else
                            result.size = c.size();

                        result.members.clear();
                        for (auto & v : c)
                            result.members.insert(order[v]);

                        print_incumbent(params, c.size(), position);
                    }
                    break;

                case CCOMerge::Previous:
                    {
                        std::set<int> new_members;
                        for (auto & v : c)
                            new_members.insert(order[v]);

                        auto merged = merge_cliques([&] (int a, int b) { return original_graph.adjacent(a, b); }, result.members, new_members);
                        if (merged.size() > result.size) {
                            result.members = merged;
                            result.size = result.members.size();
                            print_incumbent(params, result.size, position);
                        }
                    }
                    break;

                case CCOMerge::All:
                    {
                        std::set<int> new_members;
                        for (auto & v : c)
                            new_members.insert(order[v]);

                        if (previouses.empty()) {
                            result.members = new_members;
                            result.size = result.members.size();
                            previouses.push_back(result.members);
                            print_incumbent(params, result.size, position);
                        }
                        else
                            for (auto & p : previouses) {
                                auto merged = merge_cliques([&] (int a, int b) { return original_graph.adjacent(a, b); }, p, new_members);

                                if (merged.size() > result.size) {
                                    result.members = merged;
                                    result.size = result.members.size();
                                    previouses.push_back(result.members);
                                    print_incumbent(params, result.size, position);
                                }
                            }

                        previouses.push_back(result.members);
                        print_position(params, "previouses is now " + std::to_string(previouses.size()), position);
                    }
                    break;
            }
        }

        auto get_best_anywhere_value() -> unsigned
        {
            return result.size;
        }

        auto get_skip_and_stop(unsigned, int &, int &, bool &) -> void
        {
        }
    };
}

template <CCOPermutations perm_, CCOInference inference_, CCOMerge merge_>
auto parasols::cco_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult
{
    return select_graph_size<ApplyPermInferenceMerge<CCO, perm_, inference_, merge_>::template Type, MaxCliqueResult>(
            AllGraphSizes(), graph, params);
}

template auto parasols::cco_max_clique<CCOPermutations::None, CCOInference::None, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::Defer1, CCOInference::None, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::RepairAll, CCOInference::None, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::RepairAllDefer1, CCOInference::None, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::RepairSelected, CCOInference::None, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::RepairSelectedDefer1, CCOInference::None, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::RepairSelectedFast, CCOInference::None, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::RepairAllFast, CCOInference::None, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;

template auto parasols::cco_max_clique<CCOPermutations::None, CCOInference::None, CCOMerge::Previous>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::Defer1, CCOInference::None, CCOMerge::Previous>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;

template auto parasols::cco_max_clique<CCOPermutations::None, CCOInference::None, CCOMerge::All>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::Defer1, CCOInference::None, CCOMerge::All>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;

template auto parasols::cco_max_clique<CCOPermutations::None, CCOInference::LazyGlobalDomination, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::cco_max_clique<CCOPermutations::Defer1, CCOInference::LazyGlobalDomination, CCOMerge::None>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
