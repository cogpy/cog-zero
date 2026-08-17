#include "test_runner.h"

#include <opencog/agentzero/PolicyOptimizer.h>
#include <opencog/agentzero/ExperienceManager.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

static std::vector<Experience> make_eval_data(AtomSpacePtr as)
{
    Handle ctx  = as->add_node(CONCEPT_NODE, "pol_ctx");
    Handle task = as->add_node(CONCEPT_NODE, "pol_task");
    Handle out  = as->add_node(CONCEPT_NODE, "pol_out");

    std::vector<Experience> exps;
    for (int i = 0; i < 4; ++i) {
        Experience e;
        e.type = (i % 2 == 0) ? ExperienceType::SUCCESS : ExperienceType::FAILURE;
        e.context = ctx;
        e.task = task;
        e.outcome = out;
        e.importance = 0.5 + 0.1 * i;
        e.timestamp = std::chrono::system_clock::now();
        exps.push_back(e);
    }
    return exps;
}

TEST(PolicyOptimizer_Initialize)
{
    auto as = make_as();
    PolicyOptimizer po(as);
    ASSERT_TRUE(po.getPolicyBase() != Handle::UNDEFINED);
    ASSERT_FALSE(po.isMOSESAvailable()); // shim / no moses
}

TEST(PolicyOptimizer_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        PolicyOptimizer po(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(PolicyOptimizer_SeedAndBest)
{
    auto as = make_as();
    PolicyOptimizerConfig cfg;
    cfg.population_size = 8;
    cfg.max_generations = 2;
    PolicyOptimizer po(as, cfg);

    Handle seed = as->add_node(CONCEPT_NODE, "seed_policy");
    po.seedPopulation({seed});
    ASSERT_GE(po.getPopulation().size(), static_cast<size_t>(1));
    ASSERT_TRUE(po.getBestPolicy() != Handle::UNDEFINED);
}

TEST(PolicyOptimizer_OptimizePolicy)
{
    auto as = make_as();
    PolicyOptimizerConfig cfg;
    cfg.population_size = 10;
    cfg.max_generations = 3;
    cfg.mutation_rate = 0.3;
    cfg.crossover_rate = 0.5;
    cfg.elite_fraction = 0.2;
    cfg.tournament_size = 3;
    PolicyOptimizer po(as, cfg);

    Handle seed = as->add_node(CONCEPT_NODE, "opt_seed");
    auto exps = make_eval_data(as);

    Handle best = po.optimizePolicy(seed, exps);
    ASSERT_TRUE(best != Handle::UNDEFINED);

    double fitness = po.evaluatePolicy(best, exps);
    ASSERT_GE(fitness, 0.0);
    ASSERT_LE(fitness, 1.0);

    auto stats = po.getOptimizationStatistics();
    ASSERT_GE(stats.at("population_size"), 1.0);
    ASSERT_GE(stats.at("current_generation"), 1.0);
}

TEST(PolicyOptimizer_CustomEvaluator)
{
    auto as = make_as();
    PolicyOptimizerConfig cfg;
    cfg.population_size = 6;
    cfg.max_generations = 2;
    PolicyOptimizer po(as, cfg);

    Handle seed = as->add_node(CONCEPT_NODE, "custom_seed");
    auto exps = make_eval_data(as);

    auto eval = [](const Handle& /*policy*/, const std::vector<Experience>& experiences) {
        return experiences.empty() ? 0.0 : 0.75;
    };

    Handle best = po.optimizePolicy(seed, exps, eval);
    ASSERT_TRUE(best != Handle::UNDEFINED);
    ASSERT_NEAR(po.evaluatePolicy(best, exps, eval), 0.75, 1e-9);
}
