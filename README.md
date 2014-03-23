Parallel Solvers
================

This is a collection of parallel solvers for hard problems. These are primarily
aimed at ``really hard'' instances, i.e. there are no sparseness restrictions,
and there are probably better (or at least much less memory-intensive)
solutions if your data is sparse.

This is for ``work in progress'' code and experimentation. If you'd like to use
this code in a real project, you'll need to rip it out. In other words, this
isn't a nice friendly library. The code contains many awful things for allowing
experimental work, and some utterly horrific template voodoo for getting an
extra 10% performance.

You will need a C++11 compiler, such as GCC 4.8, to compile this. You will also
need Boost.

We use boilermake for compilation:

    https://github.com/dmoulding/boilermake

You should just be able to type 'make' in the top level directory to compile.

By default, compiled programs go in "build/$HOSTNAME/". You can override this,
for example, by doing:

    make TARGET_DIR=./

The solvers are as follows:

solve_max_clique
----------------

This solves the maximum clique problem.

To run, do:

    solve_max_clique algorithm order filename.clq

where filename.clq is in the DIMACS format, algorithm is one of:

    naive:       Very dumb.
    ccon:        Bitset encoded version of Prosser's MCSa variant
    ccod:        Like ccon, with size 1 colour classes deferred
    ccos:        Like ccon, with sorted colour classes (slow)
    tccon:       Like ccon, threaded
    tccod:       Like ccod, threaded (probably the best choice)
    tccos:       Like ccos, threaded
    bmcsa:       Older bitset encoded version of Prosser's MCSa variant
    tbmcsa:      Older threaded bmcsa
    dbmcsa:      Another older threaded bmcsa

and order is one of:

    deg:         Degree (Prosser's 1)
    ex:          Exdegree (Prosser's 2)
    dynex:       Dynamic exdegree (Tomita's MCS ordering? Probably best)
    mw:          Minimum width (Prosser's 3. Better when all degrees are equal.)
    none:        No ordering (typically bad, except on trivial graphs)
    rev:         Reverse ordering (equally bad)
    revdeg:      Reverse deg (terrible)
    revex:       Reverse ex (terrible)
    revdynex:    Reverse dynex (terrible)
    revmw:       Reverse mw (terrible)

There are various options, use 'solve_max_clique --help' to list them.

If you are just looking for decent results, rather than experimenting, a good
choice of parameters is:

    solve_max_clique tccod dynex filename.clq

Note that we do most of our memory allocation on the stack. If you're dealing
with large graphs and you get segfaults, you probably need to increase the
stack size. Depending upon your shell, you could try something like this:

    ulimit -s 128000

The output is as follows:

    size_of_max_clique number_of_search_nodes
    witness
    runtimes

Where the first runtime is the overall time (excluding reading in the graph) in
steady-clock ms, and any additional values are per-thread runtimes. If a
timeout is specified, the first line will also say 'aborted'.

solve_max_biclique
------------------

This solves the maximum biclique problem.

Right now the code only handles balanced, induced bicliques in an arbitrary
graph.

To run, do 'solve_max_biclique algorithm filename.clq' where filename.clq is in
the DIMACS format, and algorithm is one of "naive ccd". There are various
options, use 'solve_max_biclique --help' to list them.

The output is:

    size_of_max_biclique number_of_search_nodes
    witness_a
    witness_b
    runtimes

create_random_graph
-------------------

Creates an Erdos-Reyni random graph. Usage is 'create_random_graph n p s'
where n is the number of vertices, p is the edge probability (between 0.0 and
1.0), and s is the seed (an integer). Specifying the same seed will produce the
same output each time, avoiding the need for storing large graph files.

To avoid writing to a temp file, bash lets you do this:

    solve_max_clique bmcsa1 <(create_random_graph 100 0.5 1)

create_random_bipartite_graph
-----------------------------

Creates a random bipartite graph. Usage is 'create_random_bipartite_graph n1
n2 p s' where n1 is the number of vertices in the first set, n2 is the number
of vertices in the second set, p is the edge probability (between 0.0 and 1.0),
and s is the seed (an integer). Specifying the same seed will produce the same
output each time, avoiding the need for storing large graph files.

To avoid writing to a temp file, bash lets you do this:

    solve_max_biclique naive <(create_random_bipartite_graph 20 20 0.5 1)

.. vim: set ft=markdown spell spelllang=en :
