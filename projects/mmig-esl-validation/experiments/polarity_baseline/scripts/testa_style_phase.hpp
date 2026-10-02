#pragma once

// Fixed-topology, pure-MIG phase optimization for the ESL baseline experiment.
// Every phase assignment is an application of MAJ self-duality. The local
// one-/two-level search follows Testa et al.'s idea, but is not their exact
// ordered-rule implementation; see IMPLEMENTATION_AUDIT.md.

#include <mockturtle/networks/mig.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace esl_polarity
{
struct phase_stats
{
  uint64_t one_level = 0;
  uint64_t two_level = 0;
  uint64_t and3 = 0;
  uint64_t and2 = 0;
  uint64_t maj3 = 0;
  uint64_t maj2 = 0;
  uint64_t other_one_level = 0;
  uint64_t iterations = 0;
  uint64_t before_nonconst = 0;
  uint64_t after_nonconst = 0;
  std::vector<uint32_t> inverted_nodes;
};

struct edge
{
  uint32_t source = 0;
  uint32_t target = 0;
  bool complemented = false;
};

inline mockturtle::mig_network optimize( mockturtle::mig_network const& input, phase_stats& stats )
{
  using network = mockturtle::mig_network;
  std::vector<uint32_t> gates;
  input.foreach_gate( [&]( auto n ) {
    if ( input.is_min( n ) )
      throw std::runtime_error( "Testa-style pass requires a pure MIG" );
    gates.push_back( static_cast<uint32_t>( n ) );
  } );
  std::vector<uint8_t> is_gate( input.size(), 0 ), phase( input.size(), 0 );
  for ( auto n : gates ) is_gate[n] = 1;

  std::vector<edge> edges;
  std::vector<std::vector<uint32_t>> incident( input.size() ), parents( input.size() );
  auto add_edge = [&]( uint32_t source, uint32_t target, bool complemented ) {
    auto const id = static_cast<uint32_t>( edges.size() );
    edges.push_back( { source, target, complemented } );
    if ( is_gate[source] ) incident[source].push_back( id );
    if ( target != 0 ) incident[target].push_back( id );
    if ( is_gate[source] && target != 0 ) parents[source].push_back( target );
  };
  for ( auto n : gates )
    input.foreach_fanin( n, [&]( auto const& f ) {
      auto const source = static_cast<uint32_t>( input.get_node( f ) );
      if ( !input.is_constant( source ) )
        add_edge( source, n, input.is_complemented( f ) );
    } );
  input.foreach_po( [&]( auto const& f ) {
    auto const source = static_cast<uint32_t>( input.get_node( f ) );
    if ( !input.is_constant( source ) )
      add_edge( source, 0, input.is_complemented( f ) );
  } );
  for ( auto& v : parents )
  {
    std::sort( v.begin(), v.end() );
    v.erase( std::unique( v.begin(), v.end() ), v.end() );
  }
  auto edge_bit = [&]( edge const& e ) {
    return bool( e.complemented ^ ( is_gate[e.source] && phase[e.source] ) ^ ( e.target && phase[e.target] ) );
  };
  auto cost = [&]() {
    uint64_t total = 0;
    for ( auto const& e : edges ) total += edge_bit( e );
    return total;
  };
  auto gain_one = [&]( uint32_t n ) {
    int64_t gain = 0;
    for ( auto id : incident[n] ) gain += edge_bit( edges[id] ) ? 1 : -1;
    return gain;
  };
  auto gain_set = [&]( std::vector<uint32_t> const& nodes ) {
    std::vector<uint8_t> selected( input.size(), 0 );
    for ( auto n : nodes ) selected[n] = 1;
    int64_t gain = 0;
    // Each relevant edge is visited once, including parallel source/target uses.
    for ( auto const& e : edges )
    {
      bool const flip = bool( selected[e.source] ^ ( e.target && selected[e.target] ) );
      if ( flip ) gain += edge_bit( e ) ? 1 : -1;
    }
    return gain;
  };
  auto classify_one = [&]( uint32_t n ) {
    uint32_t inverted = 0;
    bool has_constant = false;
    input.foreach_fanin( n, [&]( auto const& f ) {
      auto const source = static_cast<uint32_t>( input.get_node( f ) );
      if ( input.is_constant( source ) )
        has_constant = true;
      else
        inverted += bool( input.is_complemented( f ) ^ ( is_gate[source] && phase[source] ) ^ phase[n] );
    } );
    if ( has_constant && inverted == 2 ) ++stats.and3;
    else if ( has_constant && inverted == 1 ) ++stats.and2;
    else if ( !has_constant && inverted == 3 ) ++stats.maj3;
    else if ( !has_constant && inverted == 2 ) ++stats.maj2;
    else ++stats.other_one_level;
  };

  stats.before_nonconst = cost();
  uint64_t running_cost = stats.before_nonconst;
  bool changed = true;
  while ( changed )
  {
    changed = false;
    ++stats.iterations;
    for ( auto n : gates )
    {
      auto const gain = gain_one( n );
      if ( gain > 0 )
      {
        classify_one( n );
        phase[n] ^= 1;
        running_cost -= static_cast<uint64_t>( gain );
        ++stats.one_level;
        changed = true;
        continue;
      }
      // Test the center plus individually useful parents; accept only an
      // exactly verified positive simultaneous gain.
      std::vector<uint32_t> chosen{ n };
      for ( auto parent : parents[n] )
      {
        int64_t adjusted = gain_one( parent );
        for ( auto id : incident[n] )
        {
          auto const& e = edges[id];
          if ( e.source == n && e.target == parent )
            adjusted += edge_bit( e ) ? -2 : 2;
        }
        if ( adjusted > 0 ) chosen.push_back( parent );
      }
      if ( chosen.size() == 1 ) continue;
      auto const combined_gain = gain_set( chosen );
      if ( combined_gain <= 0 ) continue;
      for ( auto selected : chosen ) phase[selected] ^= 1;
      running_cost -= static_cast<uint64_t>( combined_gain );
      ++stats.two_level;
      changed = true;
    }
    if ( running_cost != cost() )
      throw std::runtime_error( "phase cost accounting disagrees with full recount" );
  }
  stats.after_nonconst = running_cost;
  for ( auto n : gates )
    if ( phase[n] ) stats.inverted_nodes.push_back( n );

  auto result = input.clone();
  for ( auto n : gates )
  {
    for ( auto& child : result._storage->nodes[n].children )
      child.weight ^= phase[n] ^ ( is_gate[child.index] && phase[child.index] );
  }
  for ( auto& output : result._storage->outputs )
    if ( is_gate[output.index] ) output.weight ^= phase[output.index];
  result._storage->hash.clear();
  for ( auto n : gates ) result._storage->hash[result._storage->nodes[n]] = n;
  return result;
}
} // namespace esl_polarity
