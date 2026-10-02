#ifndef MO_EA_OPTIMIZATION_HPP
#define MO_EA_OPTIMIZATION_HPP

#include <mockturtle/mockturtle.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <random>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace mockturtle;

namespace mo_ea
{

// Represents a node in the chromosome's tree structure.
struct Node
{
    bool is_leaf;
    uint32_t index; // PI index if leaf, otherwise internal node index
    std::vector<std::shared_ptr<Node>> children;
    kitty::dynamic_truth_table tt;

    Node( bool leaf, uint32_t idx ) : is_leaf( leaf ), index( idx ) {}
};

// Represents a chromosome (a single circuit).
struct Chromosome
{
    std::shared_ptr<Node> root;
    double fitness = 0.0;
    mig_network mig; // The mockturtle network representation
};

// The main class for the evolutionary algorithm.
class EvolutionaryOptimizer
{
public:
    EvolutionaryOptimizer( const mig_network& original_mig, int population_size = 100, int generations = 50 )
        : original_mig( original_mig ), population_size( population_size ), generations( generations )
    {
        // Get the expected truth table from the original MIG.
        const auto num_pis = original_mig.num_pis();
        if ( num_pis <= 6 ) // Truth tables are feasible for a small number of inputs.
        {
            truth_table_cache<kitty::dynamic_truth_table> cache;
            default_simulator<kitty::dynamic_truth_table> sim( num_pis );
            original_mig.foreach_po( [&]( auto const& f ) {
                expected_tts.push_back( simulate<kitty::dynamic_truth_table>( original_mig, sim )[0] );
            } );
        }
    }

    mig_network run()
    {
        if ( original_mig.num_pis() > 6 )
        {
            std::cout << "[warn] MO_EA optimization is supported for up to 6 PIs due to truth table simulation complexity. Skipping MO_EA.\n";
            return original_mig;
        }

        initialize_population();
        for ( int i = 0; i < generations; ++i )
        {
            evolve();
            std::cout << "Generation " << i + 1 << ", Best Fitness: " << get_best_chromosome().fitness << std::endl;
        }
        return get_best_chromosome().mig;
    }

private:
    void initialize_population()
    {
        for ( int i = 0; i < population_size; ++i )
        {
            population.push_back( create_random_chromosome() );
        }
        evaluate_population();
    }

    Chromosome create_random_chromosome()
    {
        Chromosome chrom;
        chrom.mig = mig_network();
        std::vector<mig_network::signal> pis;
        for ( uint32_t i = 0; i < original_mig.num_pis(); ++i )
        {
            pis.push_back( chrom.mig.create_pi() );
        }

        std::vector<mig_network::signal> current_signals = pis;
        std::random_device rd;
        std::mt19937 gen( rd() );
        std::uniform_int_distribution<> dis( 0, current_signals.size() - 1 );

        for ( int i = 0; i < original_mig.num_gates(); ++i )
        {
            mig_network::signal a = current_signals[dis( gen )];
            mig_network::signal b = current_signals[dis( gen )];
            mig_network::signal c = current_signals[dis( gen )];
            current_signals.push_back( chrom.mig.create_maj( a, b, c ) );
        }

        original_mig.foreach_po( [&]( auto const&, auto i ) {
            chrom.mig.create_po( current_signals[dis( gen )] );
        } );

        return chrom;
    }

    void evaluate_population()
    {
        for ( auto& chrom : population )
        {
            calculate_fitness( chrom );
        }
        std::sort( population.begin(), population.end(), []( const Chromosome& a, const Chromosome& b ) {
            return a.fitness > b.fitness;
        } );
    }

    void calculate_fitness( Chromosome& chrom )
    {
        const auto num_pis = chrom.mig.num_pis();
        if ( num_pis > 6 )
        {
            chrom.fitness = 0;
            return;
        }

        default_simulator<kitty::dynamic_truth_table> sim( num_pis );
        const auto tts = simulate<kitty::dynamic_truth_table>( chrom.mig, sim );

        double match_score = 0;
        for ( size_t i = 0; i < expected_tts.size(); ++i )
        {
            if ( i < tts.size() )
            {
                match_score += kitty::count_ones( ~( tts[i] ^ expected_tts[i] ) );
            }
        }

        if ( match_score < kitty::count_ones( ~kitty::dynamic_truth_table( num_pis ) ) * expected_tts.size() )
        {
            chrom.fitness = match_score; // Fitness is the number of correct bits.
        }
        else // Perfect match, now optimize for gates and depth.
        {
            depth_view d{ chrom.mig };
            chrom.fitness = 10000.0 - ( chrom.mig.num_gates() * 10.0 + d.depth() );
        }
    }

    void evolve()
    {
        std::vector<Chromosome> new_population;
        // Elitism: Keep the best 10% of the population.
        int elite_size = population_size * 0.1;
        for ( int i = 0; i < elite_size; ++i )
        {
            new_population.push_back( population[i] );
        }

        while ( new_population.size() < population_size )
        {
            Chromosome parent1 = tournament_selection();
            Chromosome parent2 = tournament_selection();
            Chromosome offspring = crossover( parent1, parent2 );
            mutate( offspring );
            local_search( offspring );
            new_population.push_back( offspring );
        }
        population = new_population;
        evaluate_population();
    }

    Chromosome tournament_selection()
    {
        std::random_device rd;
        std::mt19937 gen( rd() );
        std::uniform_int_distribution<> dis( 0, population.size() - 1 );
        int tournament_size = 5;
        Chromosome best = population[dis( gen )];
        for ( int i = 1; i < tournament_size; ++i )
        {
            Chromosome contender = population[dis( gen )];
            if ( contender.fitness > best.fitness )
            {
                best = contender;
            }
        }
        return best;
    }

    Chromosome crossover( const Chromosome& p1, const Chromosome& p2 )
    {
        // For simplicity, this example returns the fitter parent.
        // A more complex crossover would involve merging the MIG structures.
        return p1.fitness > p2.fitness ? p1 : p2;
    }

    void mutate( Chromosome& chrom )
    {
        if ( chrom.mig.num_gates() == 0 ) return;

        std::random_device rd;
        std::mt19937 gen( rd() );
        std::uniform_int_distribution<> gate_dis( 0, chrom.mig.num_gates() - 1 );
        const auto random_gate_index = gate_dis( gen );
        const auto n = chrom.mig.index_to_node( chrom.mig.num_pis() + 1 + random_gate_index );

        std::vector<mig_network::signal> signals;
        chrom.mig.foreach_pi( [&]( auto const& pi_node ) {
            signals.push_back( chrom.mig.make_signal( pi_node ) );
        } );
        chrom.mig.foreach_gate( [&]( auto const& g_node ) {
            signals.push_back( chrom.mig.make_signal( g_node ) );
        } );

        if ( signals.empty() ) return;

        std::uniform_int_distribution<> sig_dis( 0, signals.size() - 1 );

        std::vector<mig_network::signal> fanins;
        chrom.mig.foreach_fanin( n, [&]( auto const& f ) {
            fanins.push_back( f );
        } );

        if ( fanins.empty() ) return;

        // Replace one of the fanins with a random signal
        fanins[0] = signals[sig_dis( gen )];

        auto const new_node = chrom.mig.create_maj( fanins[0], fanins[1], fanins[2] );
        chrom.mig.substitute_node( n, new_node );
    }

    void local_search( Chromosome& chrom )
    {
        // A simple local search: try to replace a gate with a constant.
        if ( chrom.mig.num_gates() > 1 )
        {
            // This is a placeholder for a more complex local search.
        }
    }

    const Chromosome& get_best_chromosome() const
    {
        return population[0];
    }

private:
    mig_network original_mig;
    std::vector<kitty::dynamic_truth_table> expected_tts;
    std::vector<Chromosome> population;
    int population_size;
    int generations;
};

} // namespace mo_ea

#endif // MO_EA_OPTIMIZATION_HPP