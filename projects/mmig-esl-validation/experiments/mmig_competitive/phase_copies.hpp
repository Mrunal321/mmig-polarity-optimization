#pragma once

// Materialize both MAJ and MIN for selected signals. MIN(a,b,c) is charged
// as MAJ(!a,!b,!c), including every resulting complemented edge. This trades
// extra logic gates for fewer complemented uses; it is not a free-MIN cost.
#include <mockturtle/networks/mig.hpp>
#include <mockturtle/views/topo_view.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace esl_phase_copies
{
using network = mockturtle::mig_network;
struct stats
{
  uint32_t copies = 0;
  uint64_t before_raw = 0, after_raw = 0;
  uint32_t before_gates = 0, after_gates = 0;
};

inline void rebuild( network& ntk )
{
  ntk._storage->hash.clear();
  ntk.foreach_node( [&]( auto n ) { ntk._storage->nodes[n].data[0].h1 = 0; } );
  ntk.foreach_gate( [&]( auto n ) {
    auto key = ntk._storage->nodes[n];
    if ( ntk.is_min( n ) ) key.children[0].index |= uint64_t{1} << 62;
    ntk._storage->hash[key] = n;
    for ( auto const& c : ntk._storage->nodes[n].children )
      ++ntk._storage->nodes[c.index].data[0].h1;
  } );
  for ( auto const& o : ntk._storage->outputs ) ++ntk._storage->nodes[o.index].data[0].h1;
}

// Preserve explicit phases during dangling-node removal. create_maj would
// canonicalize a complementary copy back into its original gate.
inline network compact( network const& input )
{
  network out;
  std::vector<network::signal> mapped( input.size(), out.get_constant( false ) );
  input.foreach_pi( [&]( auto n ) { mapped[n] = out.create_pi(); } );
  mockturtle::topo_view topo{ input };
  topo.foreach_gate( [&]( auto n ) {
    mockturtle::mig_storage::node_type node;
    uint32_t k = 0;
    input.foreach_fanin( n, [&]( auto f ) {
      node.children[k++] = mapped[input.get_node( f )] ^ input.is_complemented( f );
    } );
    node.data[1].h2 = input.is_min( n ) ? 2u : 0u;
    auto key = node;
    if ( input.is_min( n ) ) key.children[0].index |= uint64_t{1} << 62;
    auto it = out._storage->hash.find( key );
    if ( it != out._storage->hash.end() )
      mapped[n] = network::signal( it->second, 0 );
    else
    {
      auto id = out._storage->nodes.size();
      out._storage->nodes.push_back( node );
      out._storage->hash[key] = id;
      mapped[n] = network::signal( id, 0 );
    }
  } );
  input.foreach_po( [&]( auto f ) { out.create_po( mapped[input.get_node( f )] ^ input.is_complemented( f ) ); } );
  rebuild( out );
  return out;
}

inline uint64_t charged_raw( network const& ntk )
{
  uint64_t result = 0;
  ntk.foreach_gate( [&]( auto n ) {
    ntk.foreach_fanin( n, [&]( auto f ) { result += bool( ntk.is_complemented( f ) ^ ntk.is_min( n ) ); } );
  } );
  ntk.foreach_po( [&]( auto f ) { result += ntk.is_complemented( f ); } );
  return result;
}

struct result
{
  network mixed;
  network lowered;
};

// Generic BLIF lowers MIN immediately; retain the typed native graph too.
inline void write_typed_graph( network const& ntk, std::ostream& out )
{
  out << "MMIG_GRAPH_V1 " << ntk.num_pis() << ' ' << ntk.num_pos() << ' ' << ntk.num_gates() << '\n';
  ntk.foreach_pi( [&]( auto n ) { out << "PI " << n << '\n'; } );
  ntk.foreach_gate( [&]( auto n ) {
    out << ( ntk.is_min( n ) ? "MIN " : "MAJ " ) << n;
    ntk.foreach_fanin( n, [&]( auto f ) {
      out << ' ' << ntk.get_node( f ) << ':' << unsigned( ntk.is_complemented( f ) );
    } );
    out << '\n';
  } );
  ntk.foreach_po( [&]( auto f ) {
    out << "PO " << ntk.get_node( f ) << ':' << unsigned( ntk.is_complemented( f ) ) << '\n';
  } );
}

inline result optimize( network const& input, stats& st, uint32_t max_copies )
{
  input.foreach_gate( [&]( auto n ) {
    if ( input.is_min( n ) ) throw std::runtime_error( "phase copies require a pure MAJ starting network" );
  } );
  auto work = compact( input );
  uint32_t const initial_size = work.size();
  uint32_t const none = std::numeric_limits<uint32_t>::max();
  std::vector<uint32_t> partner( initial_size, none );
  st.before_raw = charged_raw( work );
  st.before_gates = work.num_gates();
  auto running = st.before_raw;
  while ( st.copies < max_copies )
  {
    std::vector<uint32_t> negative_uses( work.size(), 0 );
    work.foreach_gate( [&]( auto n ) {
      work.foreach_fanin( n, [&]( auto f ) {
        if ( bool( work.is_complemented( f ) ^ work.is_min( n ) ) )
          ++negative_uses[work.get_node( f )];
      } );
    } );
    work.foreach_po( [&]( auto f ) { if ( work.is_complemented( f ) ) ++negative_uses[work.get_node( f )]; } );
    uint32_t best = none;
    int64_t best_gain = 0;
    for ( uint32_t n = 1; n < initial_size; ++n )
    {
      if ( work.is_ci( n ) || partner[n] != none ) continue;
      uint32_t cost = 0;
      work.foreach_fanin( n, [&]( auto f ) {
        // A new MIN is lowered by complementing each input. Existing opposite
        // signals can supply the input without a complemented physical edge.
        if ( !work.is_complemented( f ) && partner[work.get_node( f )] == none ) ++cost;
      } );
      int64_t gain = int64_t( negative_uses[n] ) - cost;
      if ( gain > best_gain ) { best = n; best_gain = gain; }
    }
    if ( best == none ) break;
    auto copy = work._storage->nodes[best];
    copy.data[0].h1 = copy.data[0].h2 = copy.data[1].h1 = 0;
    copy.data[1].h2 = 2u; // the opposite function is an explicit MIN
    auto const id = static_cast<uint32_t>( work.size() );
    work._storage->nodes.push_back( copy );
    partner.push_back( best );
    partner[best] = id;
    work.foreach_gate( [&]( auto n ) {
      for ( auto& c : work._storage->nodes[n].children )
        if ( bool( c.weight ^ work.is_min( n ) ) && partner[c.index] != none )
        {
          c.index = partner[c.index];
          c.weight ^= 1;
        }
    } );
    for ( auto& o : work._storage->outputs )
      if ( o.weight && partner[o.index] != none ) { o.index = partner[o.index]; o.weight ^= 1; }
    running -= best_gain;
    if ( charged_raw( work ) != running )
      throw std::runtime_error( "phase-copy gain disagrees with charged MIN lowering" );
    ++st.copies;
  }
  auto mixed = compact( work );
  auto lowered = mixed.clone();
  lowered.foreach_gate( [&]( auto n ) {
    if ( lowered.is_min( n ) )
    {
      for ( auto& c : lowered._storage->nodes[n].children ) c.weight ^= 1;
      lowered._storage->nodes[n].data[1].h2 &= ~2u;
    }
  } );
  lowered = compact( lowered );
  st.after_raw = charged_raw( lowered );
  st.after_gates = lowered.num_gates();
  if ( st.after_raw > running || st.after_gates > st.before_gates + st.copies )
    throw std::runtime_error( "phase-copy compaction increased cost" );
  return { std::move( mixed ), std::move( lowered ) };
}
} // namespace esl_phase_copies
