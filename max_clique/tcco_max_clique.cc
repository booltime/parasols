/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include <max_clique/tcco_max_clique.hh>
#include <max_clique/cco_base.hh>
#include <max_clique/print_incumbent.hh>

#include <threads/queue.hh>
#include <threads/atomic_incumbent.hh>

#include <graph/template_voodoo.hh>
#include <graph/merge_cliques.hh>

#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace parasols;

using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

namespace
{
    const constexpr int number_of_depths = 5;
    const constexpr int number_of_steal_points = number_of_depths - 1;

    struct Subproblem
    {
        std::vector<int> offsets;
    };

    struct QueueItem
    {
        Subproblem subproblem;
    };

    struct StealPoint
    {
        std::mutex mutex;
        std::condition_variable cv;

        bool is_finished;

        bool has_data;
        std::vector<int> data;

        bool was_stolen;

        StealPoint() :
            is_finished(false),
            has_data(false),
            was_stolen(false)
        {
            mutex.lock();
        }

        void publish(std::vector<int> & s)
        {
            if (is_finished)
                return;

            data = s;
            has_data = true;
            cv.notify_all();
            mutex.unlock();
        }

        bool steal(std::vector<int> & s) __attribute__((noinline))
        {
            std::unique_lock<std::mutex> guard(mutex);

            while ((! has_data) && (! is_finished))
                cv.wait(guard);

            if (! is_finished && has_data) {
                s = data;
                was_stolen = true;
                return true;
            }
            else
                return false;
        }

        bool unpublish_and_keep_going()
        {
            if (is_finished)
                return true;

            mutex.lock();
            has_data = false;
            return ! was_stolen;
        }

        void finished()
        {
            is_finished = true;
            has_data = false;
            cv.notify_all();
            mutex.unlock();
        }
    };

    struct alignas(16) StealPoints
    {
        std::vector<StealPoint> points;

        StealPoints() :
            points{ number_of_steal_points }
        {
        }
    };

    template <typename Base_, bool merge_queue_, unsigned size_, typename VertexType_>
    struct MergeQueue
    {
        MergeQueue(Base_ * const)
        {
        }

        void add(const std::vector<unsigned> &)
        {
        }

        bool finish(
                MaxCliqueResult &,
                const std::vector<int> &)
        {
            return false;
        }
    };

    template <typename Base_, unsigned size_, typename VertexType_>
    struct MergeQueue<Base_, true, size_, VertexType_>
    {
        Base_ * const base;

        Queue<std::vector<unsigned> > pending_merges;
        std::thread merge_thread;

        std::list<std::set<int> > priors;
        std::set<int> best_merge;

        MergeQueue(Base_ * const b) :
            base(b),
            pending_merges(1, false)
        {
            merge_thread = std::thread([&] {
                    std::vector<unsigned> next;
                    while (pending_merges.dequeue_blocking(next)) {
                        std::set<int> next_set(next.begin(), next.end());

                        for (auto & prior : priors) {
                            auto merged = merge_cliques([&] (int a, int b) { return base->graph.adjacent(a, b); }, prior, next_set);
                            if (base->best_anywhere.update(merged.size())) {
                                best_merge = merged;
                                print_position(base->params, "merged " + std::to_string(prior.size()) + " and " + std::to_string(next_set.size())
                                    + " to get " + std::to_string(merged.size()), std::vector<int>{ });
                                print_incumbent(base->params, merged.size());
                                pending_merges.enqueue(std::vector<unsigned>(merged.begin(), merged.end()));
                            }
                        }

                        priors.push_back(std::move(next_set));
                        print_position(base->params, "priors has size " + std::to_string(priors.size()), std::vector<int>{ });
                    }
                    });
        }

        void add(const std::vector<unsigned> & c)
        {
            pending_merges.enqueue(std::vector<unsigned>{ c });
        }

        bool finish(
                MaxCliqueResult & global_result,
                const std::vector<int> & order)
        {
            bool result = false;

            pending_merges.initial_producer_done();
            merge_thread.join();

            if (global_result.size < best_merge.size()) {
                global_result.size = best_merge.size();
                global_result.members.clear();
                for (auto & v : best_merge)
                    global_result.members.insert(order[v]);

                result = true;
            }

            return result;
        }
    };

    template <CCOPermutations perm_, CCOInference inference_, bool merge_queue_, unsigned size_, typename VertexType_>
    struct TCCO : CCOBase<perm_, inference_, size_, VertexType_, TCCO<perm_, inference_, merge_queue_, size_, VertexType_> >
    {
        using Base = CCOBase<perm_, inference_, size_, VertexType_, TCCO<perm_, inference_, merge_queue_, size_, VertexType_> >;

        using Base::graph;
        using Base::original_graph;
        using Base::params;
        using Base::expand;
        using Base::order;
        using Base::colour_class_order;

        AtomicIncumbent best_anywhere; // global incumbent

        std::list<std::set<int> > previouses;
        std::mutex previouses_mutex;

        MergeQueue<TCCO, merge_queue_, size_, VertexType_> merge_queue;

        TCCO(const Graph & g, const MaxCliqueParams & p) :
            Base(g, p),
            merge_queue(this)
        {
        }

        auto run() -> MaxCliqueResult
        {
            best_anywhere.update(params.initial_bound);

            MaxCliqueResult global_result;
            global_result.size = params.initial_bound;
            std::mutex global_result_mutex;

            /* work queues */
            std::vector<std::unique_ptr<Queue<QueueItem> > > queues;
            for (unsigned depth = 0 ; depth < number_of_depths ; ++depth)
                queues.push_back(std::unique_ptr<Queue<QueueItem> >{ new Queue<QueueItem>{ params.n_threads, false } });

            /* initial job */
            queues[0]->enqueue(QueueItem{ Subproblem{ std::vector<int>{} } });
            if (queues[0]->want_producer())
                queues[0]->initial_producer_done();

            /* threads and steal points */
            std::list<std::thread> threads;
            std::vector<StealPoints> thread_steal_points(params.n_threads);

            // initial colouring
            std::array<VertexType_, size_ * bits_per_word> initial_p_order;
            std::array<VertexType_, size_ * bits_per_word> initial_colours;
            {
                FixedBitSet<size_> initial_p;
                initial_p.resize(graph.size());
                initial_p.set_all();
                colour_class_order(SelectColourClassOrderOverload<perm_>(), initial_p, initial_p_order, initial_colours);
            }

            /* workers */
            for (unsigned i = 0 ; i < params.n_threads ; ++i) {
                threads.push_back(std::thread([&, i] {
                            auto start_time = steady_clock::now(); // local start time
                            auto overall_time = duration_cast<milliseconds>(steady_clock::now() - start_time);

                            MaxCliqueResult local_result; // local result

                            for (unsigned depth = 0 ; depth < number_of_depths ; ++depth) {
                                if (queues[depth]->want_producer()) {
                                    /* steal */
                                    for (unsigned j = 0 ; j < params.n_threads ; ++j) {
                                        if (j == i)
                                            continue;

                                        std::vector<int> stole;
                                        if (thread_steal_points.at(j).points.at(depth - 1).steal(stole)) {
                                            print_position(params, "stole after", stole);
                                            stole.pop_back();
                                            for (auto & s : stole)
                                                --s;
                                            while (++stole.back() < graph.size())
                                                queues[depth]->enqueue(QueueItem{ Subproblem{ stole } });
                                        }
                                        else
                                            print_position(params, "did not steal", stole);
                                    }
                                    queues[depth]->initial_producer_done();
                                }

                                while (true) {
                                    // get some work to do
                                    QueueItem args;
                                    if (! queues[depth]->dequeue_blocking(args))
                                        break;

                                    print_position(params, "dequeued", args.subproblem.offsets);

                                    std::vector<unsigned> c;
                                    c.reserve(graph.size());

                                    FixedBitSet<size_> p; // local potential additions
                                    p.resize(graph.size());
                                    p.set_all();

                                    std::vector<int> position;
                                    position.reserve(graph.size());
                                    position.push_back(0);

                                    // do some work
                                    expand(c, p, initial_p_order, initial_colours, position, local_result,
                                            &args.subproblem, &thread_steal_points.at(i));

                                    // record the last time we finished doing useful stuff
                                    overall_time = duration_cast<milliseconds>(steady_clock::now() - start_time);
                                }

                                if (depth < number_of_steal_points)
                                    thread_steal_points.at(i).points.at(depth).finished();
                            }

                            // merge results
                            {
                                std::unique_lock<std::mutex> guard(global_result_mutex);
                                global_result.merge(local_result);
                                global_result.times.push_back(overall_time);
                            }
                            }));
            }

            // wait until they're done, and clean up threads
            for (auto & t : threads)
                t.join();

            if (merge_queue.finish(global_result, order))
                print_incumbent(params, global_result.size, std::vector<int>{ });

            return global_result;
        }

        auto increment_nodes(
                MaxCliqueResult & local_result,
                Subproblem * const,
                StealPoints * const
                ) -> void
        {
            ++local_result.nodes;
        }

        auto recurse(
                std::vector<unsigned> & c,
                FixedBitSet<size_> & p,
                const std::array<VertexType_, size_ * bits_per_word> & initial_p_order,
                const std::array<VertexType_, size_ * bits_per_word> & initial_colours,
                std::vector<int> & position,
                MaxCliqueResult & local_result,
                Subproblem * const subproblem,
                StealPoints * const steal_points
                ) -> bool
        {
            if (steal_points && c.size() < number_of_steal_points)
                steal_points->points.at(c.size() - 1).publish(position);

            expand(c, p, initial_p_order, initial_colours, position, local_result,
                subproblem && c.size() < subproblem->offsets.size() ? subproblem : nullptr,
                steal_points && c.size() < number_of_steal_points ? steal_points : nullptr);

            if (steal_points && c.size() < number_of_steal_points)
                return steal_points->points.at(c.size() - 1).unpublish_and_keep_going();
            else
                return true;
        }

        auto potential_new_best(
                const std::vector<unsigned> & c,
                const std::vector<int> & position,
                MaxCliqueResult & local_result,
                Subproblem * const,
                StealPoints * const
                ) -> void
        {
            if (params.enumerate) {
                if (best_anywhere.beaten_by(c.size())) {
                    ++local_result.result_count;
                    local_result.size = c.size();
                    local_result.members.clear();
                    for (auto & v : c)
                        local_result.members.insert(order[v]);
                }
            }
            else {
                if (best_anywhere.update(c.size())) {
                    local_result.size = c.size();
                    local_result.members.clear();
                    for (auto & v : c)
                        local_result.members.insert(order[v]);
                    print_incumbent(params, local_result.size, position);
                }

                merge_queue.add(c);
            }
        }

        auto get_best_anywhere_value() -> unsigned
        {
            return best_anywhere.get();
        }

        auto get_skip_and_stop(
                unsigned c_popcount,
                MaxCliqueResult &,
                Subproblem * const subproblem,
                StealPoints * const,
                int & skip,
                int &,
                bool & keep_going
                ) -> void
        {
            if (subproblem && c_popcount < subproblem->offsets.size()) {
                skip = subproblem->offsets.at(c_popcount);
                keep_going = false;
            }
        }
    };
}

template <CCOPermutations perm_, CCOInference inference_, bool merge_queue_>
auto parasols::tcco_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult
{
    return select_graph_size<ApplyPermInferenceMQ<TCCO, perm_, inference_, merge_queue_>::template Type, MaxCliqueResult>(
            AllGraphSizes(), graph, params);
}

template auto parasols::tcco_max_clique<CCOPermutations::None, CCOInference::None, false>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::tcco_max_clique<CCOPermutations::Defer1, CCOInference::None, false>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;

template auto parasols::tcco_max_clique<CCOPermutations::None, CCOInference::None, true>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;
template auto parasols::tcco_max_clique<CCOPermutations::Defer1, CCOInference::None, true>(const Graph &, const MaxCliqueParams &) -> MaxCliqueResult;

