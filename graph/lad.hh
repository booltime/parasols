/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef PARASOLS_GUARD_GRAPH_LAD_HH
#define PARASOLS_GUARD_GRAPH_LAD_HH 1

#include <graph/graph.hh>
#include <string>

namespace parasols
{
    /**
     * Thrown if we come across bad data in a LAD format file.
     */
    class InvalidLADFile :
        public std::exception
    {
        private:
            std::string _what;

        public:
            InvalidLADFile(const std::string & filename, const std::string & message) throw ();

            auto what() const throw () -> const char *;
    };

    /**
     * Read a LAD format file into a Graph.
     *
     * \throw InvalidLADFile
     */
    auto read_lad(const std::string & filename) -> Graph;
}

#endif