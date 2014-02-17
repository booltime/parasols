/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef PARASOLS_GUARD_MAX_KCLUB_MAX_KCLUB_PARAMS_HH
#define PARASOLS_GUARD_MAX_KCLUB_MAX_KCLUB_PARAMS_HH 1

#include <graph/graph.hh>

#include <functional>
#include <vector>
#include <limits>
#include <chrono>
#include <atomic>

namespace parasols
{
    /**
     * Initial vertex ordering to use.
     */
    using MaxKClubOrderFunction = std::function<void (const Graph &, std::vector<int> &)>;

    /**
     * Parameters for a max k-club algorithm.
     */
    struct MaxKClubParams
    {
        /// Override the initial size of the incumbent.
        unsigned initial_bound = 0;

        /// Exit immediately after finding a k-club of this size.
        unsigned stop_after_finding = std::numeric_limits<unsigned>::max();

        /// If true, print every time we find a better incumbent.
        bool print_incumbents = false;

        /// If this is set to true, we should abort due to a time limit.
        std::atomic<bool> abort;

        /// The start time of the algorithm.
        std::chrono::time_point<std::chrono::steady_clock> start_time;

        /// Initial vertex ordering.
        MaxKClubOrderFunction order_function;

        /// k (>= 2)
        unsigned k;
    };

    /**
     * Used by CCO variants to control permutations.
     */
    enum class CCOPermutations
    {
        None,
        Defer1,
        Sort
    };

}

#endif
