#pragma once

// Joint input/output phase assignment for a mixed MAJ/MIN graph.
// For gate n, p[n] flips all three input phases; q[n] flips its represented
// output phase. The gate kind toggles iff p[n] XOR q[n]. Since both MAJ and
// MIN are self-dual and complements of each other, this preserves function.
// All measured gains exclude constant-source edges, as in the M1 baseline.

#include <mockturtle/networks/mig.hpp>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace esl_two_phase
{
struct stats
{
  uint64_t before_nonconst = 0;
  uint64_t after_nonconst = 0;
  uint64_t input_flips = 0;
  uint64_t output_flips = 0;
  uint64_t type_toggles = 0;
  uint64_t sweeps = 0;
};

struct edge
{
  uint32_t source = 0;
  uint32_t target = 0; // 0 denotes a primary output
  bool complemented = false;
};

inline mockturtle::mig_network optimize( mockturtle::mig_network const& input, stats& st )
{
  using network = mockturtle::mig_network;
  std::vector<uint32_t> gates;
  input.foreach_gate( [&]( auto n ) { gates.push_back( static_cast<uint32_t>( n ) ); } );
  std::vector<uint8_t> is_gate( input.size(), 0 ), p( input.size(), 0 ), q( input.size(), 0 );
  for ( auto n : gates ) is_gate[n] = 1;

  std::vector<edge> edges;
  std::vector<std::vector<uint32_t>> incoming( input.size() ), outgoing( input.size() );
  auto add_edge = [&]( uint32_t source, uint32_t target, bool complemented ) {
    auto const id = static_cast<uint32_t>( edges.size() );
    edges.push_back( { source, target, complemented } );
    if ( target != 0 ) incoming[target].push_back( id );
    if ( is_gate[source] ) outgoing[source].push_back( id );
  };
  for ( auto n : gates )
  {
    input.foreach_fanin( n, [&]( auto const& f ) {
      auto const source = static_cast<uint32_t>( input.get_node( f ) );
      if ( !input.is_constant( source ) )
        add_edge( source, n, input.is_complemented( f ) );
    } );
  }
  input.foreach_po( [&]( auto const& f ) {
    auto const source = static_cast<uint32_t>( input.get_node( f ) );
    if ( !input.is_constant( source ) )
      add_edge( source, 0, input.is_complemented( f ) );
  } );
  auto bit = [&]( edge const& e ) {
    return bool( e.complemented ^ ( is_gate[e.source] && q[e.source] ) ^
                 ( e.target != 0 && p[e.target] ) );
  };
  auto cost = [&]() {
    uint64_t total = 0;
    for ( auto const& e : edges ) total += bit( e );
    return total;
  };
  auto gain = [&]( std::vector<uint32_t> const& incident ) {
    int64_t total = 0;
    for ( auto id : incident ) total += bit( edges[id] ) ? 1 : -1;
    return total;
  };

  st.before_nonconst = cost();
  uint64_t running = st.before_nonconst;
  bool changed = true;
  while ( changed )
  {
    changed = false;
    ++st.sweeps;
    for ( auto n : gates )
    {
      auto const in_gain = gain( incoming[n] );
      if ( in_gain > 0 )
      {
        p[n] ^= 1;
        running -= static_cast<uint64_t>( in_gain );
        ++st.input_flips;
        changed = true;
      }
      auto const out_gain = gain( outgoing[n] );
      if ( out_gain > 0 )
      {
        q[n] ^= 1;
        running -= static_cast<uint64_t>( out_gain );
        ++st.output_flips;
        changed = true;
      }
    }
    if ( running != cost() )
      throw std::runtime_error( "mMIG two-phase cost accounting disagrees with recount" );
  }
  st.after_nonconst = running;

  auto result = input.clone();
  for ( auto n : gates )
  {
    if ( p[n] ^ q[n] )
    {
      result._storage->nodes[n].data[1].h2 ^= 2u; // MAJ <-> MIN
      ++st.type_toggles;
    }
    for ( auto& child : result._storage->nodes[n].children )
      child.weight ^= p[n] ^ ( is_gate[child.index] && q[child.index] );
  }
  for ( auto& output : result._storage->outputs )
    if ( is_gate[output.index] ) output.weight ^= q[output.index];
  result._storage->hash.clear();
  for ( auto n : gates )
  {
    auto key = result._storage->nodes[n];
    if ( result.is_min( n ) ) key.children[0].index |= uint64_t{ 1 } << 62;
    result._storage->hash[key] = n;
  }
  return result;
}

// Charge every explicit MIN to the MAJ+INV technology by moving its output
// complement onto all fanout edges, then leave a pure-MAJ network for the
// identical phase optimizer used by the MIG reference.
inline mockturtle::mig_network expand_min_to_maj( mockturtle::mig_network const& input )
{
  auto result = input.clone();
  std::vector<uint8_t> was_min( input.size(), 0 );
  input.foreach_gate( [&]( auto n ) { was_min[n] = input.is_min( n ); } );
  input.foreach_gate( [&]( auto n ) {
    result._storage->nodes[n].data[1].h2 &= ~2u;
    for ( auto& child : result._storage->nodes[n].children )
      child.weight ^= was_min[child.index];
  } );
  for ( auto& output : result._storage->outputs ) output.weight ^= was_min[output.index];
  result._storage->hash.clear();
  input.foreach_gate( [&]( auto n ) { result._storage->hash[result._storage->nodes[n]] = n; } );
  return result;
}
} // namespace esl_two_phase
