/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include <subgraph_isomorphism/cb_subgraph_isomorphism.hh>

#include <graph/bit_graph.hh>
#include <graph/template_voodoo.hh>
#include <graph/degree_sort.hh>

#include <algorithm>
#include <limits>
#include <random>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/max_cardinality_matching.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/graph_utility.hpp>

using namespace parasols;

namespace
{
    enum class Search
    {
        Aborted,
        Unsatisfiable,
        Satisfiable
    };

    struct Empty
    {
    };

    inline void add_conflict(Empty &, unsigned)
    {
    }

    inline void merge_conflicts(Empty &, const Empty &)
    {
    }

    inline void initialise_conflicts(Empty &, unsigned)
    {
    }

    inline bool caused_conflict(Empty &, unsigned)
    {
        return true;
    }

    template <unsigned n_words_>
    inline void add_conflict(FixedBitSet<n_words_> & c, unsigned v)
    {
        c.set(v);
    }

    template <unsigned n_words_>
    inline void merge_conflicts(FixedBitSet<n_words_> & c, const FixedBitSet<n_words_> & d)
    {
        c.union_with(d);
    }

    template <unsigned n_words_>
    inline void initialise_conflicts(FixedBitSet<n_words_> & c, unsigned n)
    {
        c.resize(n);
    }

    template <unsigned n_words_>
    inline bool caused_conflict(FixedBitSet<n_words_> & c, unsigned n)
    {
        return c.test(n);
    }

    template <unsigned n_words_, bool backjump_, bool domination_, int k_, int l_>
    struct CBSGI
    {
        struct Domain
        {
            unsigned v;
            unsigned popcount;
            FixedBitSet<n_words_> values;
            typename std::conditional<backjump_, FixedBitSet<n_words_>, Empty>::type conflicts;
        };

        using Domains = std::vector<Domain>;
        using Assignments = std::vector<unsigned>;

        const SubgraphIsomorphismParams & params;
        const bool all_different;
        const bool probe;

        std::vector<FixedBitSet<n_words_> > pattern_dominations, target_dominations;

        static constexpr int max_graphs = 1 + (l_ - 1) * k_;
        std::array<FixedBitGraph<n_words_>, max_graphs> target_graphs;
        std::array<FixedBitGraph<n_words_>, max_graphs> pattern_graphs;

        std::vector<int> order;

        unsigned pattern_size, target_size;

        CBSGI(const Graph & target, const Graph & pattern, const SubgraphIsomorphismParams & a, bool d, bool p, bool r, bool n, bool s) :
            params(a),
            all_different(d),
            probe(p),
            order(target.size()),
            pattern_size(pattern.size()),
            target_size(target.size())
        {
            std::iota(order.begin(), order.end(), 0);

            if (! n)
                degree_sort(target, order, r);
            else if (r)
                std::reverse(order.begin(), order.end());

            if (s) {
                std::mt19937 g;
                std::shuffle(order.begin(), order.end(), g);
            }

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

        auto propagate(Domains & new_domains, unsigned branch_v, unsigned f_v,
                typename std::conditional<backjump_, FixedBitSet<n_words_>, Empty>::type & conflicts,
                int g_end
                ) -> bool
        {
            /* how many unassigned neighbours do we have, and what is their domain? */
            std::array<unsigned, max_graphs> unassigned_neighbours;
            std::fill(unassigned_neighbours.begin(), unassigned_neighbours.end(), 0);

            std::array<FixedBitSet<n_words_>, max_graphs> unassigned_neighbours_domains_union;
            std::array<typename std::conditional<backjump_, FixedBitSet<n_words_>, Empty>::type, max_graphs> unassigned_neighbours_conflicts;
            FixedBitSet<n_words_> unassigned_neighbours_domains_mask;
            for (int g = 0 ; g < g_end ; ++g) {
                unassigned_neighbours_domains_union.at(g).resize(target_size);
                unassigned_neighbours_domains_union.at(g).unset_all();
                initialise_conflicts(unassigned_neighbours_conflicts.at(g), target_size);
            }
             unassigned_neighbours_domains_mask.resize(target_size);

            std::array<int, n_words_ * bits_per_word> domains_order;
            std::iota(domains_order.begin(), domains_order.begin() + new_domains.size(), 0);

            std::sort(domains_order.begin(), domains_order.begin() + new_domains.size(),
                    [&] (int a, int b) {
                    return (new_domains.at(a).popcount < new_domains.at(b).popcount) ||
                    (new_domains.at(a).popcount == new_domains.at(b).popcount && target_graphs.at(0).degree(a) > target_graphs.at(0).degree(b));
                    });

            for (int i = 0, i_end = new_domains.size() ; i != i_end ; ++i) {
                auto & d = new_domains.at(domains_order.at(i));
                d.values.unset(f_v);

                FixedBitSet<n_words_> future_hall_set;
                bool has_future_hall_set = false;

                for (int g = 0 ; g < g_end ; ++g) {
                    if (pattern_graphs.at(g).adjacent(branch_v, d.v)) {
                        /* knock out values */
                        target_graphs.at(g).intersect_with_row(f_v, d.values);

                        d.values.intersect_with_complement(unassigned_neighbours_domains_mask);

                        /* enough values remaining between all our neighbours we've seen so far? */
                        unassigned_neighbours_domains_union.at(g).union_with(d.values);
                        unsigned unassigned_neighbours_domains_popcount = unassigned_neighbours_domains_union.at(g).popcount();
                        ++unassigned_neighbours.at(g);
                        if (unassigned_neighbours.at(g) > unassigned_neighbours_domains_popcount) {
                            if (0 == unassigned_neighbours_domains_popcount || 0 == d.values.popcount())
                                conflicts = d.conflicts;
                            else {
                                merge_conflicts(conflicts, unassigned_neighbours_conflicts.at(g));
                                merge_conflicts(conflicts, d.conflicts);
                            }
                            return false;
                        }
                        else if (unassigned_neighbours.at(g) == unassigned_neighbours_domains_popcount) {
                            future_hall_set = unassigned_neighbours_domains_union.at(g);
                            has_future_hall_set = true;
                        }
                    }
                }

                if (has_future_hall_set) {
                    unassigned_neighbours_domains_mask.union_with(future_hall_set);
                    for (int g = 0 ; g < g_end ; ++g) {
                        unassigned_neighbours.at(g) = 0;
                        unassigned_neighbours_domains_union.at(g).unset_all();
                    }
                }

                unsigned old_popcount = d.popcount;
                d.popcount = d.values.popcount();

                if (0 == d.popcount) {
                    conflicts = d.conflicts;
                    return false;
                }

                if (d.popcount != old_popcount) {
                    add_conflict(d.conflicts, branch_v);
                }
            }

            return true;
        }

        auto search(
                int depth,
                Assignments & assignments,
                Domains & domains,
                unsigned long long & nodes,
                typename std::conditional<backjump_, FixedBitSet<n_words_>, Empty>::type & conflicts,
                unsigned long long probe_limit,
                int discrepancies_above,
                int g_end) -> Search
        {
            if (params.abort->load())
                return Search::Aborted;

            ++nodes;

            if (0 != probe_limit && nodes > probe_limit)
                return Search::Aborted;

            Domain * branch_domain = nullptr;
            for (auto & d : domains)
                if ((! branch_domain) || d.popcount < branch_domain->popcount || (d.popcount == branch_domain->popcount && d.v < branch_domain->v))
                    branch_domain = &d;

            if (! branch_domain)
                return Search::Satisfiable;

            auto remaining = branch_domain->values;
            auto branch_v = branch_domain->v;
            merge_conflicts(conflicts, branch_domain->conflicts);

            for (int f_v = remaining.first_set_bit() ; f_v != -1 ; f_v = remaining.first_set_bit()) {
                remaining.unset(f_v);

                /* try assigning f_v to v */
                assignments.at(branch_v) = f_v;

                if (domination_) {
                    /* if v cannot take f_v, it cannot take any f_v' that is dominated by f_v */
                    remaining.intersect_with_complement(target_dominations.at(f_v));
                }

                /* set up new domains */
                Domains new_domains;
                new_domains.reserve(domains.size() - 1);
                for (auto & d : domains)
                    if (d.v != branch_v)
                        new_domains.push_back(d);

                /* propagate */
                typename std::conditional<backjump_, FixedBitSet<n_words_>, Empty>::type propagate_conflicts;
                initialise_conflicts(propagate_conflicts, pattern_size);
                if (! propagate(new_domains, branch_v, f_v, propagate_conflicts, g_end)) {
                    /* analyse failure */
                    merge_conflicts(conflicts, propagate_conflicts);
                    continue;
                }

                typename std::conditional<backjump_, FixedBitSet<n_words_>, Empty>::type search_conflicts;
                initialise_conflicts(search_conflicts, pattern_size);
                switch (search(depth + 1, assignments, new_domains, nodes, search_conflicts, probe_limit, discrepancies_above, g_end)) {
                    case Search::Satisfiable:    return Search::Satisfiable;
                    case Search::Aborted:        return Search::Aborted;
                    case Search::Unsatisfiable:  break;
                }

                merge_conflicts(conflicts, search_conflicts);

                if (! caused_conflict(search_conflicts, branch_v))
                    return Search::Unsatisfiable;

                if (domination_) {
                    /* if v cannot take f_v, no v' that is dominated by v can
                     * take f_v, or any f_v' that is dominated by f_v */
                    for (auto & d : domains)
                        if (pattern_dominations.at(branch_v).test(d.v)) {
                            d.values.unset(f_v);
                            d.values.intersect_with_complement(target_dominations.at(f_v));
                        }
                }

                if (-1 != discrepancies_above && depth > discrepancies_above)
                    return Search::Unsatisfiable;
            }

            return Search::Unsatisfiable;
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
                            else if (targets_ndss.at(g).at(j).size() < patterns_ndss.at(g).at(i).size()) {
                                ok = false;
                            }
                            else {
                                for (unsigned x = 0 ; ok && x < patterns_ndss.at(g).at(i).size() ; ++x) {
                                    if (targets_ndss.at(g).at(j).at(x) < patterns_ndss.at(g).at(i).at(x))
                                        ok = false;
                                }
                            }

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

        auto build_aux_graphs() -> void
        {
            for (int g = 1 ; g < max_graphs ; ++g)
                pattern_graphs.at(g).resize(pattern_size);

            if (l_ >= 2) {
                for (unsigned v = 0 ; v < pattern_size ; ++v) {
                    auto nv = pattern_graphs.at(0).neighbourhood(v);
                    for (int c = nv.first_set_bit() ; c != -1 ; c = nv.first_set_bit()) {
                        nv.unset(c);
                        auto nc = pattern_graphs.at(0).neighbourhood(c);
                        for (int w = nc.first_set_bit() ; w != -1 && unsigned(w) <= v ; w = nc.first_set_bit()) {
                            nc.unset(w);
                            if (k_ >= 3 && pattern_graphs.at(2).adjacent(v, w))
                                pattern_graphs.at(3).add_edge(v, w);
                            else if (k_ >= 2 && pattern_graphs.at(1).adjacent(v, w))
                                pattern_graphs.at(2).add_edge(v, w);
                            else if (k_ >= 1)
                                pattern_graphs.at(1).add_edge(v, w);
                        }
                    }
                }
            }

            if (l_ >= 3) {
                for (unsigned v = 0 ; v < pattern_size ; ++v) {
                    auto nv = pattern_graphs.at(0).neighbourhood(v);
                    for (int c = nv.first_set_bit() ; c != -1 ; c = nv.first_set_bit()) {
                        nv.unset(c);
                        auto nc = pattern_graphs.at(0).neighbourhood(c);
                        for (int d = nc.first_set_bit() ; d != -1 ; d = nc.first_set_bit()) {
                            nc.unset(d);
                            if (unsigned(d) == v)
                                continue;

                            auto nd = pattern_graphs.at(0).neighbourhood(d);
                            for (int w = nd.first_set_bit() ; w != -1 && unsigned(w) <= v ; w = nd.first_set_bit()) {
                                nd.unset(w);
                                if (w == c)
                                    continue;

                                if (k_ >= 3 && pattern_graphs.at(k_ + 2).adjacent(v, w))
                                    pattern_graphs.at(k_ + 3).add_edge(v, w);
                                else if (k_ >= 2 && pattern_graphs.at(k_ + 1).adjacent(v, w))
                                    pattern_graphs.at(k_ + 2).add_edge(v, w);
                                else if (k_ >= 1)
                                    pattern_graphs.at(k_ + 1).add_edge(v, w);
                            }
                        }
                    }
                }
            }

            for (int g = 1 ; g < max_graphs ; ++g)
                target_graphs.at(g).resize(target_size);

            if (l_ >= 2) {
                for (unsigned v = 0 ; v < target_size ; ++v) {
                    auto nv = target_graphs.at(0).neighbourhood(v);
                    for (int c = nv.first_set_bit() ; c != -1 ; c = nv.first_set_bit()) {
                        nv.unset(c);
                        auto nc = target_graphs.at(0).neighbourhood(c);
                        for (int w = nc.first_set_bit() ; w != -1 && unsigned(w) <= v ; w = nc.first_set_bit()) {
                            nc.unset(w);
                            if (k_ >= 3 && target_graphs.at(2).adjacent(v, w))
                                target_graphs.at(3).add_edge(v, w);
                            else if (k_ >= 2 && target_graphs.at(1).adjacent(v, w))
                                target_graphs.at(2).add_edge(v, w);
                            else if (k_ >= 1)
                                target_graphs.at(1).add_edge(v, w);
                        }
                    }
                }
            }

            if (l_ >= 3) {
                for (unsigned v = 0 ; v < target_size ; ++v) {
                    auto nv = target_graphs.at(0).neighbourhood(v);
                    for (int c = nv.first_set_bit() ; c != -1 ; c = nv.first_set_bit()) {
                        nv.unset(c);
                        auto nc = target_graphs.at(0).neighbourhood(c);
                        for (int d = nc.first_set_bit() ; d != -1 ; d = nc.first_set_bit()) {
                            nc.unset(d);
                            if (unsigned(d) == v)
                                continue;

                            auto nd = target_graphs.at(0).neighbourhood(d);
                            for (int w = nd.first_set_bit() ; w != -1 && unsigned(w) <= v ; w = nd.first_set_bit()) {
                                nd.unset(w);
                                if (w == c)
                                    continue;

                                if (k_ >= 3 && target_graphs.at(k_ + 2).adjacent(v, w))
                                    target_graphs.at(k_ + 3).add_edge(v, w);
                                else if (k_ >= 2 && target_graphs.at(k_ + 1).adjacent(v, w))
                                    target_graphs.at(k_ + 2).add_edge(v, w);
                                else if (k_ >= 1)
                                    target_graphs.at(k_ + 1).add_edge(v, w);
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

        auto initialise_dominations() -> void
        {
            unsigned pattern_count = 0;

            pattern_dominations.resize(pattern_size);
            for (auto & d : pattern_dominations)
                d.resize(pattern_size);

            for (unsigned v = 0 ; v < pattern_size ; ++v) {
                for (unsigned w = 0 ; w < pattern_size ; ++w) {
                    if (v != w) {
                        auto n_v = pattern_graphs.at(0).neighbourhood(v);
                        auto n_w = pattern_graphs.at(0).neighbourhood(w);
                        n_v.unset(w);
                        n_w.unset(v);

                        if (n_v == n_w) {
                            ++pattern_count;
                            pattern_dominations.at(w).set(v);
                        }
                    }
                }
            }

            unsigned target_count = 0;

            target_dominations.resize(target_size);
            for (auto & d : target_dominations)
                d.resize(target_size);

            for (unsigned v = 0 ; v < target_size ; ++v) {
                auto n_v = target_graphs.at(0).neighbourhood(v);
                for (unsigned w = 0 ; w < target_size ; ++w) {
                    if (v != w) {
                        auto n_w = target_graphs.at(0).neighbourhood(w);
                        n_w.intersect_with_complement(n_v);
                        if (n_w.empty()) {
                            ++target_count;
                            target_dominations.at(v).set(w);
                        }
                    }
                }
            }
        }

        auto prepare_for_search(Domains & domains) -> void
        {
            for (auto & d : domains) {
                initialise_conflicts(d.conflicts, pattern_size);
                d.popcount = d.values.popcount();
            }
        }

        auto run() -> SubgraphIsomorphismResult
        {
            SubgraphIsomorphismResult result;

            if (pattern_size > target_size) {
                /* some of our fixed size data structures will throw a hissy
                 * fit. check this early. */
                return result;
            }

            if (domination_)
                initialise_dominations();

            if (probe) {
                Domains probe_domains(pattern_size);
                if (! initialise_domains(probe_domains, 1))
                    return result;

                Assignments assignments(pattern_size, std::numeric_limits<unsigned>::max());
                typename std::conditional<backjump_, FixedBitSet<n_words_>, Empty>::type search_conflicts;
                initialise_conflicts(search_conflicts, pattern_size);

                switch (search(0, assignments, probe_domains, result.nodes, search_conflicts, pattern_size * pattern_size, -1, 1)) {
                    case Search::Satisfiable:
                        for (unsigned v = 0 ; v < pattern_size ; ++v)
                            result.isomorphism.emplace(v, order.at(assignments.at(v)));
                        return result;

                    case Search::Unsatisfiable:
                        return result;

                    case Search::Aborted:
                        break;
                }
            }

            build_aux_graphs();

            Domains domains(pattern_size);

            if (! initialise_domains(domains, max_graphs))
                return result;

            if (all_different && ! regin_all_different(domains))
                return result;

            prepare_for_search(domains);

            Assignments assignments(pattern_size, std::numeric_limits<unsigned>::max());
            typename std::conditional<backjump_, FixedBitSet<n_words_>, Empty>::type search_conflicts;
            initialise_conflicts(search_conflicts, pattern_size);
            switch (search(0, assignments, domains, result.nodes, search_conflicts, 0, -1, max_graphs)) {
                case Search::Satisfiable:
                    for (unsigned v = 0 ; v < pattern_size ; ++v)
                        result.isomorphism.emplace(v, order.at(assignments.at(v)));
                    break;

                case Search::Unsatisfiable:
                case Search::Aborted:
                    break;
            }

            return result;
        }
    };

    template <template <unsigned, bool, bool, int, int> class SGI_, bool v_, bool w_, int n_, int m_>
    struct Apply
    {
        template <unsigned size_, typename> using Type = SGI_<size_, v_, w_, n_, m_>;
    };
}

auto parasols::cbjd_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const SubgraphIsomorphismParams & params) -> SubgraphIsomorphismResult
{
    return select_graph_size<Apply<CBSGI, true, true, 3, 3>::template Type, SubgraphIsomorphismResult>(
            AllGraphSizes(), graphs.second, graphs.first, params, true, false, false, false, false);
}

auto parasols::cbjdrev_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const SubgraphIsomorphismParams & params) -> SubgraphIsomorphismResult
{
    return select_graph_size<Apply<CBSGI, true, true, 3, 3>::template Type, SubgraphIsomorphismResult>(
            AllGraphSizes(), graphs.second, graphs.first, params, true, false, true, false, false);
}

auto parasols::cbjdfast_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const SubgraphIsomorphismParams & params) -> SubgraphIsomorphismResult
{
    return select_graph_size<Apply<CBSGI, true, true, 1, 1>::template Type, SubgraphIsomorphismResult>(
            AllGraphSizes(), graphs.second, graphs.first, params, false, false, false, false, false);
}

auto parasols::cbjdprobe_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const SubgraphIsomorphismParams & params) -> SubgraphIsomorphismResult
{
    return select_graph_size<Apply<CBSGI, true, true, 3, 3>::template Type, SubgraphIsomorphismResult>(
            AllGraphSizes(), graphs.second, graphs.first, params, true, true, false, false, false);
}

auto parasols::cbjdnovo_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const SubgraphIsomorphismParams & params) -> SubgraphIsomorphismResult
{
    return select_graph_size<Apply<CBSGI, true, true, 3, 3>::template Type, SubgraphIsomorphismResult>(
            AllGraphSizes(), graphs.second, graphs.first, params, true, false, false, true, false);
}

auto parasols::cbjdnovorev_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const SubgraphIsomorphismParams & params) -> SubgraphIsomorphismResult
{
    return select_graph_size<Apply<CBSGI, true, true, 3, 3>::template Type, SubgraphIsomorphismResult>(
            AllGraphSizes(), graphs.second, graphs.first, params, true, false, true, true, false);
}

auto parasols::cbjdrand_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const SubgraphIsomorphismParams & params) -> SubgraphIsomorphismResult
{
    return select_graph_size<Apply<CBSGI, true, true, 3, 3>::template Type, SubgraphIsomorphismResult>(
            AllGraphSizes(), graphs.second, graphs.first, params, true, false, false, true, true);
}

