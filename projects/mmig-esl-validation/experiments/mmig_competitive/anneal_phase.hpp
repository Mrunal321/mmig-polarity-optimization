#pragma once

// Deterministic multi-start phase search for a pure MAJ/INV graph.  The
// objective can count all complemented edges or only nonconstant edges. This is
// a search heuristic over the self-duality identity, not a new Boolean rule.

#include <mockturtle/networks/mig.hpp>

#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace esl_anneal_phase
{
struct stats
{
  uint64_t before_cost = 0;
  uint64_t after_cost = 0;
  uint64_t attempted = 0;
  uint64_t accepted = 0;
  uint64_t restarts = 0;
};

struct edge
{
  uint32_t source = 0;
  uint32_t target = 0; // zero denotes a primary output
  bool complemented = false;
};

inline mockturtle::mig_network optimize( mockturtle::mig_network const& input,
                                          stats& st, uint64_t seed = 1,
                                          uint32_t restarts = 12,
                                          uint32_t sweeps = 30,
                                          bool include_constants = false,
                                          bool accept_neutral = false )
{
  std::vector<uint32_t> gates;
  input.foreach_gate( [&]( auto n ) {
    if ( input.is_min( n ) )
      throw std::runtime_error( "anneal phase requires a pure MIG" );
    gates.push_back( static_cast<uint32_t>( n ) );
  } );
  if ( gates.empty() ) return input.clone();

  std::vector<uint8_t> is_gate( input.size(), 0 );
  for ( auto n : gates ) is_gate[n] = 1;
  std::vector<edge> edges;
  std::vector<std::vector<uint32_t>> incident( input.size() );
  auto add_edge = [&]( uint32_t source, uint32_t target, bool complemented ) {
    auto const id = static_cast<uint32_t>( edges.size() );
    edges.push_back( { source, target, complemented } );
    if ( is_gate[source] ) incident[source].push_back( id );
    if ( target ) incident[target].push_back( id );
  };
  for ( auto n : gates )
    input.foreach_fanin( n, [&]( auto const& f ) {
      auto const source = static_cast<uint32_t>( input.get_node( f ) );
      if ( include_constants || !input.is_constant( source ) )
        add_edge( source, n, input.is_complemented( f ) );
    } );
  input.foreach_po( [&]( auto const& f ) {
    auto const source = static_cast<uint32_t>( input.get_node( f ) );
    if ( include_constants || !input.is_constant( source ) )
      add_edge( source, 0, input.is_complemented( f ) );
  } );

  std::vector<uint8_t> phase( input.size(), 0 ), best( input.size(), 0 );
  auto edge_bit = [&]( edge const& e ) {
    return bool( e.complemented ^ ( is_gate[e.source] && phase[e.source] ) ^
                 ( e.target && phase[e.target] ) );
  };
  auto recount = [&]() {
    uint64_t cost = 0;
    for ( auto const& e : edges ) cost += edge_bit( e );
    return cost;
  };
  auto gain = [&]( uint32_t n ) {
    int64_t g = 0;
    for ( auto id : incident[n] ) g += edge_bit( edges[id] ) ? 1 : -1;
    return g;
  };

  st.before_cost = recount();
  uint64_t best_cost = st.before_cost;
  std::mt19937_64 rng( seed );
  std::uniform_int_distribution<size_t> gate_pick( 0, gates.size() - 1 );
  std::uniform_real_distribution<double> chance( 0.0, 1.0 );
  st.restarts = restarts;
  for ( uint32_t restart = 0; restart < restarts; ++restart )
  {
    phase = best;
    uint64_t running = best_cost;
    // Deterministic perturbations at several scales explore distinct phase
    // basins.  Restart zero begins unperturbed; the incumbent is preserved.
    if ( restart )
    {
      double const fraction = ( restart % 4 == 1 ) ? 0.01 :
                              ( restart % 4 == 2 ) ? 0.10 :
                              ( restart % 4 == 3 ) ? 0.35 : 0.75;
      for ( size_t j = 0,
            count = static_cast<size_t>( gates.size() * fraction ) + restart;
            j < count; ++j )
      {
        auto n = gates[gate_pick( rng )];
        auto g = gain( n );
        phase[n] ^= 1;
        running = static_cast<uint64_t>( static_cast<int64_t>( running ) - g );
      }
    }

    for ( uint32_t sweep = 0; sweep < sweeps; ++sweep )
    {
      double const fraction = double( sweep ) / double( sweeps > 1 ? sweeps - 1 : 1 );
      double const temperature = 2.5 * std::pow( 0.03 / 2.5, fraction );
      for ( size_t j = 0; j < gates.size(); ++j )
      {
        auto n = gates[gate_pick( rng )];
        auto g = gain( n );
        ++st.attempted;
        if ( g > 0 || ( accept_neutral && g == 0 ) ||
             ( g < 0 && chance( rng ) < std::exp( double( g ) / temperature ) ) )
        {
          phase[n] ^= 1;
          running = static_cast<uint64_t>( static_cast<int64_t>( running ) - g );
          ++st.accepted;
          if ( running < best_cost )
          {
            best_cost = running;
            best = phase;
          }
        }
      }
      if ( running != recount() )
        throw std::runtime_error( "anneal phase cost accounting disagrees with recount" );
    }
  }
  st.after_cost = best_cost;

  auto result = input.clone();
  for ( auto n : gates )
    for ( auto& child : result._storage->nodes[n].children )
      child.weight ^= best[n] ^ ( is_gate[child.index] && best[child.index] );
  for ( auto& output : result._storage->outputs )
    if ( is_gate[output.index] ) output.weight ^= best[output.index];
  result._storage->hash.clear();
  for ( auto n : gates ) result._storage->hash[result._storage->nodes[n]] = n;
  return result;
}
} // namespace esl_anneal_phase
