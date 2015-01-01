/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include <subgraph_isomorphism/cb_subgraph_isomorphism.hh>

#include <graph/bit_graph.hh>
#include <graph/template_voodoo.hh>
#include <graph/degree_sort.hh>

#include <algorithm>
#include <limits>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/max_cardinality_matching.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/graph_utility.hpp>

using namespace parasols;

namespace
{
    template <unsigned n_words_, typename>
    struct CBSGI
    {
        struct Domain
        {
            unsigned v;
            unsigned popcount;
            FixedBitSet<n_words_> values;
        };

        using Domains = std::vector<Domain>;
        using Assignments = std::vector<unsigned>;

        const SubgraphIsomorphismParams & params;

        static constexpr int max_graphs = 5;
        std::array<FixedBitGraph<n_words_>, max_graphs> target_graphs;
        std::array<FixedBitGraph<n_words_>, max_graphs> pattern_graphs;

        std::vector<int> order;

        unsigned pattern_size, target_size;

        CBSGI(const Graph & target, const Graph & pattern, const SubgraphIsomorphismParams & a) :
            params(a),
            order(target.size()),
            pattern_size(pattern.size()),
            target_size(target.size())
        {
            std::iota(order.begin(), order.end(), 0);
            degree_sort(target, order, false);

            pattern_graphs.at(0).resize(pattern_size);
            for (unsigned i = 0 ; i < pattern_size ; ++i)
                for (unsigned j = 0 ; j < pattern_size ; ++j)
                    if (pattern.adjacent(i, j))
                        pattern_graphs.at(0).add_edge(i, j);

            target_graphs.at(0).resize(target_size);
            for (unsigned i = 0 ; i < target_size ; ++i)
                for (unsigned j = 0 ; j < target_size ; ++j)
                    if (target.adjacent(order.at(i), order.at(j)))
                        target_graphs.at(0).add_edge(i, j);
        }

        auto propagate(Domains & new_domains, unsigned branch_v, unsigned f_v) -> bool
        {
            /* how many unassigned neighbours do we have, and what is their domain? */
            std::array<unsigned, max_graphs> unassigned_neighbours;
            std::fill(unassigned_neighbours.begin(), unassigned_neighbours.end(), 0);

            std::array<FixedBitSet<n_words_>, max_graphs> unassigned_neighbours_domains_union;
            for (int g = 0 ; g < max_graphs ; ++g) {
                unassigned_neighbours_domains_union.at(g).resize(target_size);
                unassigned_neighbours_domains_union.at(g).unset_all();
            }

            for (auto & d : new_domains) {
                d.values.unset(f_v);
                for (int g = 0 ; g < max_graphs ; ++g)
                    if (pattern_graphs.at(g).adjacent(branch_v, d.v)) {
                        /* knock out values */
                        target_graphs.at(g).intersect_with_row(f_v, d.values);

                        /* enough values remaining between all our neighbours we've seen so far? */
                        unassigned_neighbours_domains_union.at(g).union_with(d.values);
                        if (++unassigned_neighbours.at(g) > unassigned_neighbours_domains_union.at(g).popcount())
                            return false;

                        // todo: something clever with singletons
                        // todo: fast path if we've got lots of values
                    }

                // todo: avoid recalculating this sometimes
                d.popcount = d.values.popcount();
            }

            return true;
        }

        auto search(Assignments & assignments, Domains & domains, unsigned long long & nodes) -> bool
        {
            if (params.abort->load())
                return false;

            ++nodes;

            Domain * branch_domain = nullptr;
            for (auto & d : domains)
                if ((! branch_domain) || d.popcount < branch_domain->popcount || (d.popcount == branch_domain->popcount && d.v < branch_domain->v))
                    branch_domain = &d;

            if (! branch_domain)
                return true;

            auto remaining = branch_domain->values;
            auto branch_v = branch_domain->v;
            for (unsigned n = 0, n_end = branch_domain->popcount ; n < n_end ; ++n) {
                /* try assigning f_v to v */
                unsigned f_v = remaining.first_set_bit();
                remaining.unset(f_v);
                assignments.at(branch_v) = f_v;

                /* set up new domains */
                bool elide_copy = n == n_end - 1;
                Domains new_domains = (elide_copy ? std::move(domains) : Domains());
                if (elide_copy) {
                    *branch_domain = new_domains.at(new_domains.size() - 1);
                    new_domains.pop_back();
                }
                else {
                    new_domains.reserve(domains.size() - 1);
                    for (auto & d : domains)
                        if (d.v != branch_v)
                            new_domains.push_back(d);
                }

                if (! propagate(new_domains, branch_v, f_v))
                    continue;

                if (search(assignments, new_domains, nodes))
                    return true;
            }

            return false;
        }

        auto initialise_domains(Domains & domains, int g_end) -> bool
        {
            unsigned remaining_target_vertices = target_size;
            FixedBitSet<n_words_> allowed_target_vertices;
            allowed_target_vertices.resize(target_size);
            allowed_target_vertices.set_all();

            while (true) {
                std::array<std::vector<int>, max_graphs> patterns_degrees;
                std::array<std::vector<int>, max_graphs> targets_degrees;

                for (int g = 0 ; g < g_end ; ++g) {
                    patterns_degrees.at(g).resize(pattern_size);
                    targets_degrees.at(g).resize(target_size);
                }

                /* pattern and target degree sequences */
                for (int g = 0 ; g < g_end ; ++g) {
                    for (unsigned i = 0 ; i < pattern_size ; ++i)
                        patterns_degrees.at(g).at(i) = pattern_graphs.at(g).degree(i);

                    for (unsigned i = 0 ; i < target_size ; ++i) {
                        FixedBitSet<n_words_> remaining = allowed_target_vertices;
                        target_graphs.at(g).intersect_with_row(i, remaining);
                        targets_degrees.at(g).at(i) = remaining.popcount();
                    }
                }

                /* pattern and target neighbourhood degree sequences */
                std::array<std::vector<std::vector<int> >, max_graphs> patterns_ndss;
                std::array<std::vector<std::vector<int> >, max_graphs> targets_ndss;

                for (int g = 0 ; g < g_end ; ++g) {
                    patterns_ndss.at(g).resize(pattern_size);
                    targets_ndss.at(g).resize(target_size);
                }

                for (int g = 0 ; g < g_end ; ++g) {
                    for (unsigned i = 0 ; i < pattern_size ; ++i) {
                        for (unsigned j = 0 ; j < pattern_size ; ++j) {
                            if (pattern_graphs.at(g).adjacent(i, j))
                                patterns_ndss.at(g).at(i).push_back(patterns_degrees.at(g).at(j));
                        }
                        std::sort(patterns_ndss.at(g).at(i).begin(), patterns_ndss.at(g).at(i).end(), std::greater<int>());
                    }

                    for (unsigned i = 0 ; i < target_size ; ++i) {
                        for (unsigned j = 0 ; j < target_size ; ++j) {
                            if (target_graphs.at(g).adjacent(i, j))
                                targets_ndss.at(g).at(i).push_back(targets_degrees.at(g).at(j));
                        }
                        std::sort(targets_ndss.at(g).at(i).begin(), targets_ndss.at(g).at(i).end(), std::greater<int>());
                    }
                }

                for (unsigned i = 0 ; i < pattern_size ; ++i) {
                    domains.at(i).v = i;
                    domains.at(i).values.unset_all();
                    domains.at(i).values.resize(target_size);

                    for (unsigned j = 0 ; j < target_size ; ++j) {
                        bool ok = true;

                        for (int g = 0 ; g < g_end ; ++g) {
                            if (! allowed_target_vertices.test(j)) {
                                ok = false;
                            }
                            else if (pattern_graphs.at(g).adjacent(i, i) && ! target_graphs.at(g).adjacent(j, j)) {
                                ok = false;
                            }
                            else if (params.induced && target_graphs.at(g).adjacent(j, j) && ! pattern_graphs.at(g).adjacent(i, i)) {
                                ok = false;
                            }
                            else if (targets_ndss.at(g).at(j).size() >= patterns_ndss.at(g).at(i).size()) {
                                for (unsigned x = 0 ; x < patterns_ndss.at(g).at(i).size() ; ++x) {
                                    if (targets_ndss.at(g).at(j).at(x) < patterns_ndss.at(g).at(i).at(x)) {
                                        ok = false;
                                        break;
                                    }
                                }
                            }
                            else
                                ok = false;

                            if (! ok)
                                break;
                        }

                        if (ok)
                            domains.at(i).values.set(j);
                    }

                    domains.at(i).popcount = domains.at(i).values.popcount();
                }

                FixedBitSet<n_words_> domains_union;
                domains_union.resize(pattern_size);
                for (auto & d : domains)
                    domains_union.union_with(d.values);

                unsigned domains_union_popcount = domains_union.popcount();
                if (domains_union_popcount < unsigned(pattern_size)) {
                    return false;
                }
                else if (domains_union_popcount == remaining_target_vertices)
                    break;

                allowed_target_vertices.intersect_with(domains_union);
                remaining_target_vertices = allowed_target_vertices.popcount();
            }

            return true;
        }

        auto build_path_graphs() -> void
        {
            pattern_graphs.at(1).resize(pattern_size);
            pattern_graphs.at(2).resize(pattern_size);
            pattern_graphs.at(3).resize(pattern_size);
            pattern_graphs.at(4).resize(pattern_size);

            for (unsigned v = 0 ; v < pattern_size ; ++v)
                for (unsigned c = 0 ; c < pattern_size ; ++c)
                    if (pattern_graphs.at(0).adjacent(c, v))
                        for (unsigned w = 0 ; w <= v ; ++w)
                            if (pattern_graphs.at(0).adjacent(c, w)) {
                                if (pattern_graphs.at(1).adjacent(v, w))
                                    pattern_graphs.at(2).add_edge(v, w);
                                else
                                    pattern_graphs.at(1).add_edge(v, w);
                            }

            for (unsigned v = 0 ; v < pattern_size ; ++v) {
                for (unsigned c = 0 ; c < pattern_size ; ++c) {
                    if (pattern_graphs.at(0).adjacent(c, v)) {
                        for (unsigned d = 0 ; d < pattern_size ; ++d) {
                            if (d != v && pattern_graphs.at(0).adjacent(c, d)) {
                                for (unsigned w = 0 ; w <= v ; ++w) {
                                    if (w != c && pattern_graphs.at(0).adjacent(d, w)) {
                                        if (pattern_graphs.at(3).adjacent(v, w))
                                            pattern_graphs.at(4).add_edge(v, w);
                                        else
                                            pattern_graphs.at(3).add_edge(v, w);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            target_graphs.at(1).resize(target_size);
            target_graphs.at(2).resize(target_size);
            target_graphs.at(3).resize(target_size);
            target_graphs.at(4).resize(target_size);

            for (unsigned v = 0 ; v < target_size ; ++v) {
                for (unsigned c = 0 ; c < target_size ; ++c) {
                    if (target_graphs.at(0).adjacent(c, v)) {
                        for (unsigned w = 0 ; w <= v ; ++w) {
                            if (target_graphs.at(0).adjacent(c, w)) {
                                if (target_graphs.at(1).adjacent(v, w))
                                    target_graphs.at(2).add_edge(v, w);
                                else
                                    target_graphs.at(1).add_edge(v, w);
                            }
                        }
                    }
                }
            }

            for (unsigned v = 0 ; v < target_size ; ++v) {
                for (unsigned c = 0 ; c < target_size ; ++c) {
                    if (target_graphs.at(0).adjacent(c, v)) {
                        for (unsigned d = 0 ; d < target_size ; ++d) {
                            if (d != v && target_graphs.at(0).adjacent(c, d)) {
                                for (unsigned w = 0 ; w <= v ; ++w) {
                                    if (w != c && target_graphs.at(0).adjacent(d, w)) {
                                        if (target_graphs.at(3).adjacent(v, w))
                                            target_graphs.at(4).add_edge(v, w);
                                        else
                                            target_graphs.at(3).add_edge(v, w);
                                    }
                                }
                            }
                        }
                    }
                }
            }

        }

        auto regin_all_different(Domains & domains) -> bool
        {
            boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS> match(pattern_size + target_size);

            unsigned consider = 0;
            for (unsigned i = 0 ; i < pattern_size ; ++i) {
                if (domains.at(i).values.popcount() < pattern_size)
                    ++consider;

                for (unsigned j = 0 ; j < target_size ; ++j) {
                    if (domains.at(i).values.test(j))
                        boost::add_edge(i, pattern_size + j, match);
                }
            }

            if (0 == consider)
                return true;

            std::vector<boost::graph_traits<decltype(match)>::vertex_descriptor> mate(pattern_size + target_size);
            boost::edmonds_maximum_cardinality_matching(match, &mate.at(0));

            std::set<int> free;
            for (unsigned j = 0 ; j < target_size ; ++j)
                free.insert(pattern_size + j);

            unsigned count = 0;
            for (unsigned i = 0 ; i < pattern_size ; ++i)
                if (mate.at(i) != boost::graph_traits<decltype(match)>::null_vertex()) {
                    ++count;
                    free.erase(mate.at(i));
                }

            if (count != unsigned(pattern_size))
                return false;

            boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> match_o(pattern_size + target_size);
            std::set<std::pair<unsigned, unsigned> > unused;
            for (unsigned i = 0 ; i < pattern_size ; ++i) {
                for (unsigned j = 0 ; j < target_size ; ++j) {
                    if (domains.at(i).values.test(j)) {
                        unused.emplace(i, j);
                        if (mate.at(i) == unsigned(j + pattern_size))
                            boost::add_edge(i, pattern_size + j, match_o);
                        else
                            boost::add_edge(pattern_size + j, i, match_o);
                    }
                }
            }

            std::set<int> pending = free, seen;
            while (! pending.empty()) {
                unsigned v = *pending.begin();
                pending.erase(pending.begin());
                if (! seen.count(v)) {
                    seen.insert(v);
                    auto w = boost::adjacent_vertices(v, match_o);
                    for ( ; w.first != w.second ; ++w.first) {
                        if (*w.first >= unsigned(pattern_size))
                            unused.erase(std::make_pair(v, *w.first - pattern_size));
                        else
                            unused.erase(std::make_pair(*w.first, v - pattern_size));
                        pending.insert(*w.first);
                    }
                }
            }

            std::vector<int> component(num_vertices(match_o)), discover_time(num_vertices(match_o));
            std::vector<boost::default_color_type> color(num_vertices(match_o));
            std::vector<boost::graph_traits<decltype(match_o)>::vertex_descriptor> root(num_vertices(match_o));
            boost::strong_components(match_o,
                    make_iterator_property_map(component.begin(), get(boost::vertex_index, match_o)),
                    root_map(make_iterator_property_map(root.begin(), get(boost::vertex_index, match_o))).
                    color_map(make_iterator_property_map(color.begin(), get(boost::vertex_index, match_o))).
                    discover_time_map(make_iterator_property_map(discover_time.begin(), get(boost::vertex_index, match_o))));

            for (auto e = unused.begin(), e_end = unused.end() ; e != e_end ; ) {
                if (component.at(e->first) == component.at(e->second + pattern_size))
                    unused.erase(e++);
                else
                    ++e;
            }

            unsigned deletions = 0;
            for (auto & u : unused)
                if (mate.at(u.first) != u.second + pattern_size) {
                    ++deletions;
                    domains.at(u.first).values.unset(u.second);
                }

            return true;
        }

        auto run() -> SubgraphIsomorphismResult
        {
            SubgraphIsomorphismResult result;

            if (pattern_size > target_size) {
                /* some of our fixed size data structures will throw a hissy
                 * fit. check this early. */
                return result;
            }

            Domains domains(pattern_size);

            build_path_graphs();

            if (! initialise_domains(domains, max_graphs))
                return result;

            if (! regin_all_different(domains))
                return result;

            Assignments assignments(pattern_size, std::numeric_limits<unsigned>::max());
            if (search(assignments, domains, result.nodes))
                for (unsigned v = 0 ; v < pattern_size ; ++v)
                    result.isomorphism.emplace(v, order.at(assignments.at(v)));

            return result;
        }
    };
}

auto parasols::cb_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const SubgraphIsomorphismParams & params) -> SubgraphIsomorphismResult
{
    return select_graph_size<CBSGI, SubgraphIsomorphismResult>(
            AllGraphSizes(), graphs.second, graphs.first, params);
}

