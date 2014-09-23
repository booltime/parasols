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
        std::make_pair( std::string{ "cconra" },    cco_max_clique<CCOPermutations::RepairAll, CCOInference::None, CCOMerge::None>),
        std::make_pair( std::string{ "ccodra" },    cco_max_clique<CCOPermutations::RepairAllDefer1, CCOInference::None, CCOMerge::None>),
        std::make_pair( std::string{ "cconrs" },    cco_max_clique<CCOPermutations::RepairSelected, CCOInference::None, CCOMerge::None>),
        std::make_pair( std::string{ "ccodrs" },    cco_max_clique<CCOPermutations::RepairSelectedDefer1, CCOInference::None, CCOMerge::None>),
        std::make_pair( std::string{ "cconrsf" },   cco_max_clique<CCOPermutations::RepairSelectedFast, CCOInference::None, CCOMerge::None>),
        std::make_pair( std::string{ "cconraf" },   cco_max_clique<CCOPermutations::RepairAllFast, CCOInference::None, CCOMerge::None>),

        std::make_pair( std::string{ "cconlgd" },   cco_max_clique<CCOPermutations::None, CCOInference::LazyGlobalDomination, CCOMerge::None>),
        std::make_pair( std::string{ "ccodlgd" },   cco_max_clique<CCOPermutations::Defer1, CCOInference::LazyGlobalDomination, CCOMerge::None>),

        std::make_pair( std::string{ "cconmp" },    cco_max_clique<CCOPermutations::None, CCOInference::None, CCOMerge::Previous>),
        std::make_pair( std::string{ "ccodmp" },    cco_max_clique<CCOPermutations::Defer1, CCOInference::None, CCOMerge::Previous>),

        std::make_pair( std::string{ "cconma" },    cco_max_clique<CCOPermutations::None, CCOInference::None, CCOMerge::All>),
        std::make_pair( std::string{ "ccodma" },    cco_max_clique<CCOPermutations::Defer1, CCOInference::None, CCOMerge::All>),

        std::make_pair( std::string{ "tccon" },     tcco_max_clique<CCOPermutations::None, CCOInference::None, false>),
        std::make_pair( std::string{ "tccod" },     tcco_max_clique<CCOPermutations::Defer1, CCOInference::None, false>),

        std::make_pair( std::string{ "tcconmq" },   tcco_max_clique<CCOPermutations::None, CCOInference::None, true>),
        std::make_pair( std::string{ "tccodmq" },   tcco_max_clique<CCOPermutations::Defer1, CCOInference::None, true>)
    };
}

#endif
