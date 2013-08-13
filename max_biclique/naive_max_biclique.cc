/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include <max_biclique/naive_max_biclique.hh>
#include <max_biclique/print_incumbent.hh>

#include <graph/bit_graph.hh>
#include <graph/degree_sort.hh>

#include <algorithm>

using namespace parasols;

namespace
{
    template <unsigned size_>
    auto expand(
            const FixedBitGraph<size_> & graph,
            const MaxBicliqueParams & params,
            MaxBicliqueResult & result,
            const std::vector<int> & o,
            FixedBitSet<size_> & ca,
            FixedBitSet<size_> & cb,
            FixedBitSet<size_> & pa,
            FixedBitSet<size_> & pb
            ) -> void
    {
        ++result.nodes;

        // for each v in pa...
        while (! pa.empty()) {
            int v = pa.first_set_bit();

            // bound, timeout or early exit?
            FixedBitSet<size_> pcommon = pa;
            pcommon.intersect_with(pb);
            unsigned bound = ca.popcount() + pa.popcount() + cb.popcount() + pb.popcount() - pcommon.popcount();
            if (bound <= 2 * result.size || result.size >= params.stop_after_finding || params.abort.load())
                return;

            // consider taking v
            ca.set(v);
            pa.unset(v);

            // filter p to contain vertices adjacent to v
            FixedBitSet<size_> new_pb = pb;
            graph.intersect_with_row(v, new_pb);

            if (new_pb.empty()) {
                // potential new best
                if (ca.popcount() == cb.popcount() && ca.popcount() > result.size) {
                    result.size = ca.popcount();
                    result.members_a.clear();
                    for (int i = 0 ; i < graph.size() ; ++i)
                        if (ca.test(i))
                            result.members_a.insert(o[i]);
                    result.members_b.clear();
                    for (int i = 0 ; i < graph.size() ; ++i)
                        if (cb.test(i))
                            result.members_b.insert(o[i]);
                    print_incumbent(params, result.size);
                }
            }
            else {
                /* swap a and b */
                expand(graph, params, result, o, cb, ca, new_pb, pa);
            }

            // now consider not taking v
            ca.unset(v);
        }
    }

    template <unsigned size_>
    auto naive(const Graph & graph, const MaxBicliqueParams & params) -> MaxBicliqueResult
    {
        MaxBicliqueResult result;
        result.size = params.initial_bound;

        FixedBitSet<size_> ca, cb; // current candidate clique
        ca.resize(graph.size());
        cb.resize(graph.size());

        FixedBitSet<size_> pa, pb; // potential additions
        pa.resize(graph.size());
        pa.set_all();
        pb.resize(graph.size());
        pb.set_all();

        std::vector<int> o(graph.size()); // vertex ordering
        std::iota(o.begin(), o.end(), 0);
        degree_sort(graph, o, true);

        // re-encode graph as a bit graph
        FixedBitGraph<size_> bit_graph;
        bit_graph.resize(graph.size());

        for (int i = 0 ; i < graph.size() ; ++i)
            for (int j = 0 ; j < graph.size() ; ++j)
                if (graph.adjacent(o[i], o[j]))
                    bit_graph.add_edge(i, j);

        // go!
        expand(bit_graph, params, result, o, ca, cb, pa, pb);

        return result;
    }
}

auto parasols::naive_max_biclique(const Graph & graph, const MaxBicliqueParams & params) -> MaxBicliqueResult
{
    /* This is pretty horrible: in order to avoid dynamic allocation, select
     * the appropriate specialisation for our graph's size. */
    static_assert(max_graph_words == 256, "Need to update here if max_graph_size is changed.");
    if (graph.size() < bits_per_word)
        return naive<1>(graph, params);
    else if (graph.size() < 2 * bits_per_word)
        return naive<2>(graph, params);
    else if (graph.size() < 4 * bits_per_word)
        return naive<4>(graph, params);
    else if (graph.size() < 8 * bits_per_word)
        return naive<8>(graph, params);
    else if (graph.size() < 16 * bits_per_word)
        return naive<16>(graph, params);
    else if (graph.size() < 32 * bits_per_word)
        return naive<32>(graph, params);
    else if (graph.size() < 64 * bits_per_word)
        return naive<64>(graph, params);
    else if (graph.size() < 128 * bits_per_word)
        return naive<128>(graph, params);
    else if (graph.size() < 256 * bits_per_word)
        return naive<256>(graph, params);
    else
        throw GraphTooBig();
}
