#pragma once

// Fixed-topology phase search using a full-gain pass. Each pass may cross a
// temporary inversion-cost barrier, but commits only its best positive prefix.
// This is a pure-MIG baseline; no mMIG advantage should be credited to it.
#include <mockturtle/networks/mig.hpp>
#include <cstdint>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

namespace esl_fm_phase
{
struct stats
{
  uint64_t before = 0, after = 0;
  uint32_t passes = 0, flipped = 0;
};
struct edge
{
  uint32_t source = 0, target = 0; // zero target means a primary output
  bool complemented = false;
};
struct entry
{
  int32_t gain;
  uint64_t tie;
  uint32_t node;
  uint32_t version;
  bool operator<( entry const& other ) const
  {
    if ( gain != other.gain ) return gain < other.gain;
    return tie < other.tie;
  }
};

inline mockturtle::mig_network optimize( mockturtle::mig_network const& input,
                                          stats& st, uint64_t seed = 1,
                                          uint32_t max_passes = 12,
                                          bool include_constants = true )
{
  std::vector<uint32_t> gates;
  input.foreach_gate( [&]( auto n ) {
    if ( input.is_min( n ) ) throw std::runtime_error( "full-gain phase requires a pure MIG" );
    gates.push_back( static_cast<uint32_t>( n ) );
  } );
  if ( gates.empty() ) return input.clone();
  std::vector<uint8_t> is_gate( input.size(), 0 ), best( input.size(), 0 );
  for ( auto n : gates ) is_gate[n] = 1;
  std::vector<edge> edges;
  std::vector<std::vector<uint32_t>> incident( input.size() );
  auto add_edge = [&]( uint32_t source, uint32_t target, bool complemented ) {
    uint32_t const id = static_cast<uint32_t>( edges.size() );
    edges.push_back( { source, target, complemented } );
    if ( is_gate[source] ) incident[source].push_back( id );
    if ( target ) incident[target].push_back( id );
  };
  for ( auto n : gates ) input.foreach_fanin( n, [&]( auto f ) {
    auto s = static_cast<uint32_t>( input.get_node( f ) );
    if ( include_constants || !input.is_constant( s ) ) add_edge( s, n, input.is_complemented( f ) );
  } );
  input.foreach_po( [&]( auto f ) {
    auto s = static_cast<uint32_t>( input.get_node( f ) );
    if ( include_constants || !input.is_constant( s ) ) add_edge( s, 0, input.is_complemented( f ) );
  } );
  auto bit = [&]( edge const& e, std::vector<uint8_t> const& p ) {
    return bool( e.complemented ^ ( is_gate[e.source] && p[e.source] ) ^
                 ( e.target && p[e.target] ) );
  };
  auto count = [&]( std::vector<uint8_t> const& p ) {
    uint64_t cost = 0;
    for ( auto const& e : edges ) cost += bit( e, p );
    return cost;
  };
  st.before = st.after = count( best );
  std::mt19937_64 rng( seed );
  for ( uint32_t pass = 0; pass < max_passes; ++pass )
  {
    auto phase = best;
    std::vector<int32_t> gain( input.size(), 0 );
    std::vector<uint32_t> version( input.size(), 0 );
    std::vector<uint8_t> locked( input.size(), 0 );
    for ( auto n : gates )
      for ( auto id : incident[n] ) gain[n] += bit( edges[id], phase ) ? 1 : -1;
    std::priority_queue<entry> q;
    for ( auto n : gates ) q.push( { gain[n], rng(), n, version[n] } );
    uint64_t running = st.after;
    int64_t best_prefix_gain = 0;
    size_t best_length = 0;
    std::vector<uint32_t> order;
    order.reserve( gates.size() );
    for ( size_t i = 0; i < gates.size(); ++i )
    {
      while ( !q.empty() && ( locked[q.top().node] || q.top().version != version[q.top().node] ) ) q.pop();
      if ( q.empty() ) throw std::runtime_error( "phase queue exhausted early" );
      auto n = q.top().node;
      q.pop();
      int32_t const selected_gain = gain[n];
      locked[n] = 1;
      phase[n] ^= 1;
      running = static_cast<uint64_t>( static_cast<int64_t>( running ) - selected_gain );
      order.push_back( n );
      int64_t const prefix_gain = static_cast<int64_t>( st.after ) - static_cast<int64_t>( running );
      if ( prefix_gain > best_prefix_gain )
      {
        best_prefix_gain = prefix_gain;
        best_length = order.size();
      }
      for ( auto id : incident[n] )
      {
        auto const& e = edges[id];
        auto other = e.source == n ? e.target : e.source;
        if ( !is_gate[other] || locked[other] ) continue;
        gain[other] += bit( e, phase ) ? 2 : -2;
        q.push( { gain[other], rng(), other, ++version[other] } );
      }
    }
    if ( running != count( phase ) ) throw std::runtime_error( "full-gain pass cost mismatch" );
    if ( !best_length ) break;
    for ( size_t i = 0; i < best_length; ++i ) best[order[i]] ^= 1;
    st.after -= static_cast<uint64_t>( best_prefix_gain );
    st.flipped += static_cast<uint32_t>( best_length );
    ++st.passes;
    if ( st.after != count( best ) ) throw std::runtime_error( "committed phase cost mismatch" );
  }
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
} // namespace esl_fm_phase
