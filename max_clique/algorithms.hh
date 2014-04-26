/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#ifndef PARASOLS_GUARD_MAX_CLIQUE_ALGORITHMS_HH
#define PARASOLS_GUARD_MAX_CLIQUE_ALGORITHMS_HH 1

#include <max_clique/naive_max_clique.hh>
#include <max_clique/bmcsa_max_clique.hh>
#include <max_clique/tbmcsa_max_clique.hh>
#include <max_clique/dbmcsa_max_clique.hh>
#include <max_clique/cco_max_clique.hh>
#include <max_clique/tcco_max_clique.hh>

#include <utility>
#include <string>

namespace parasols
{
    auto max_clique_algorithms = {
        std::make_pair( std::string{ "naive" },     naive_max_clique),

        std::make_pair( std::string{ "bmcsa" },     bmcsa_max_clique),

        std::make_pair( std::string{ "tbmcsa" },    tbmcsa_max_clique ),

        std::make_pair( std::string{ "dbmcsa" },    dbmcsa_max_clique ),

        std::make_pair( std::string{ "ccon" },      cco_max_clique<CCOPermutations::None, CCOInference::None, CCOMerge::None>),
        std::make_pair( std::string{ "ccod" },      cco_max_clique<CCOPermutations::Defer1, CCOInference::None, CCOMerge::None>),
        std::make_pair( std::string{ "ccos" },      cco_max_clique<CCOPermutations::Sort, CCOInference::None, CCOMerge::None>),

        std::make_pair( std::string{ "ccongd" },    cco_max_clique<CCOPermutations::None, CCOInference::GlobalDomination, CCOMerge::None>),
        std::make_pair( std::string{ "ccodgd" },    cco_max_clique<CCOPermutations::Defer1, CCOInference::GlobalDomination, CCOMerge::None>),
        std::make_pair( std::string{ "ccosgd" },    cco_max_clique<CCOPermutations::Sort, CCOInference::GlobalDomination, CCOMerge::None>),

        std::make_pair( std::string{ "cconlgd" },   cco_max_clique<CCOPermutations::None, CCOInference::LazyGlobalDomination, CCOMerge::None>),
        std::make_pair( std::string{ "ccodlgd" },   cco_max_clique<CCOPermutations::Defer1, CCOInference::LazyGlobalDomination, CCOMerge::None>),
        std::make_pair( std::string{ "ccoslgd" },   cco_max_clique<CCOPermutations::Sort, CCOInference::LazyGlobalDomination, CCOMerge::None>),

        std::make_pair( std::string{ "cconm" },     cco_max_clique<CCOPermutations::None, CCOInference::None, CCOMerge::All>),
        std::make_pair( std::string{ "ccodm" },     cco_max_clique<CCOPermutations::Defer1, CCOInference::None, CCOMerge::All>),
        std::make_pair( std::string{ "ccosm" },     cco_max_clique<CCOPermutations::Sort, CCOInference::None, CCOMerge::All>),

        std::make_pair( std::string{ "tccon" },     tcco_max_clique<CCOPermutations::None, CCOInference::None>),
        std::make_pair( std::string{ "tccod" },     tcco_max_clique<CCOPermutations::Defer1, CCOInference::None>),
        std::make_pair( std::string{ "tccos" },     tcco_max_clique<CCOPermutations::Sort, CCOInference::None>)
    };
}

#endif
