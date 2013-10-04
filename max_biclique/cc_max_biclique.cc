/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include <max_biclique/cc_max_biclique.hh>
#include <max_biclique/print_incumbent.hh>
#include <max_biclique/clique_cover.hh>

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

        std::array<unsigned, size_ * bits_per_word> pa_order, cliques;
        clique_cover<size_>(graph, pa, pa_order, cliques);

        unsigned ca_popcount = ca.popcount();
        unsigned cb_popcount = cb.popcount();
        unsigned pa_popcount = pa.popcount();
        unsigned pb_popcount = pb.popcount();

        // for each v in pa...
        for (int n = pa_popcount - 1 ; n >= 0 ; --n) {

            // timeout or early exit?
            if (result.size >= params.stop_after_finding || params.abort.load())
                return;

            // bound
            if (cliques[n] + ca_popcount <= result.size)
                return;
            if (pb_popcount + cb_popcount <= result.size)
                return;

            // consider taking v
            int v = pa_order[n];

            ca.set(v);
            ++ca_popcount;
            pa.unset(v);

            // filter pb to contain vertices adjacent to v, and pa to contain
            // vertices not adjacent to v
            FixedBitSet<size_> new_pa = pa, new_pb = pb;
            graph.intersect_with_row_complement(v, new_pa);
            graph.intersect_with_row(v, new_pb);

            // potential new best
            if (ca_popcount == cb_popcount && ca_popcount > result.size) {
                result.size = ca_popcount;

                // depermute to get result
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

            if (! new_pb.empty()) {
                /* swap a and b */
                expand(graph, params, result, o, cb, ca, new_pb, new_pa);
            }

            // now consider not taking v
            ca.unset(v);
            --ca_popcount;

            // if cb is empty, do not take cb = { v }
            if (params.break_ab_symmetry) {
                if (cb.empty()) {
                    pb.unset(v);
                    pb_popcount = pb.popcount();
                }
            }
        }
    }

    template <unsigned size_>
    auto cc(const Graph & graph, const MaxBicliqueParams & params) -> MaxBicliqueResult
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
        degree_sort(graph, o, false);

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

auto parasols::cc_max_biclique(const Graph & graph, const MaxBicliqueParams & params) -> MaxBicliqueResult
{
    /* This is pretty horrible: in order to avoid dynamic allocation, select
     * the appropriate specialisation for our graph's size. */
    static_assert(max_graph_words == 256, "Need to update here if max_graph_size is changed.");
    if (graph.size() < bits_per_word)
        return cc<1>(graph, params);
    else if (graph.size() < 2 * bits_per_word)
        return cc<2>(graph, params);
    else if (graph.size() < 4 * bits_per_word)
        return cc<4>(graph, params);
    else if (graph.size() < 8 * bits_per_word)
        return cc<8>(graph, params);
    else if (graph.size() < 16 * bits_per_word)
        return cc<16>(graph, params);
    else if (graph.size() < 32 * bits_per_word)
        return cc<32>(graph, params);
    else if (graph.size() < 64 * bits_per_word)
        return cc<64>(graph, params);
    else if (graph.size() < 128 * bits_per_word)
        return cc<128>(graph, params);
    else if (graph.size() < 256 * bits_per_word)
        return cc<256>(graph, params);
    else
        throw GraphTooBig();
}

