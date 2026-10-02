/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022 EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file mmig_window_resynthesis.hpp
  \brief Conservative truth-table based local mMIG resynthesis.

  This pass searches small local cuts and synthesizes 0/1/2-gate MAJ/MIN
  candidates for the root truth table.  It is deliberately conservative:
  candidates are checked by local truth table, then accepted only when the
  cleaned-up full network improves by gate count or by inverted edges at equal
  gate count.  Final ABC CEC should still guard flows using this pass.
*/

#pragma once

#include "cleanup.hpp"

#include "simulation.hpp"
#include "../traits.hpp"
#include "../utils/stopwatch.hpp"
#include "../views/depth_view.hpp"

#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mockturtle
{

enum class mmig_inverter_objective_policy : uint8_t
{
  legacy,
  gate_inv,
  safe_gate_inv,
  inv_safe,
  majinv_gate_inv
};

struct mmig_window_resynthesis_params
{
  uint32_t cut_size{ 4u };
  uint32_t max_internal_nodes{ 8u };
  uint32_t max_expr_candidates{ 4096u };
  uint32_t max_matches_per_root{ 32u };
  uint32_t max_roots{ 0u }; /* 0 means all gates */
  uint32_t max_exact_pis{ 16u };
  uint32_t min_inverter_gain{ 1u };
  uint32_t max_inv_increase_on_gate_gain{ 0u };
  mmig_inverter_objective_policy objective_policy{ mmig_inverter_objective_policy::legacy };
  bool allow_gate_increase{ false };
  bool preserve_depth{ false };
  bool allow_output_polarity{ false };
  bool rank_by_local_inverters{ false };
  bool skip_trivial_expressions{ false };
  bool spread_roots{ false };
  bool verbose{ false };
};

struct mmig_window_resynthesis_stats
{
  stopwatch<>::duration time_total{ 0 };
  uint32_t roots_seen{ 0u };
  uint32_t roots_skipped{ 0u };
  uint32_t roots_resynthesized{ 0u };
  uint32_t candidates_generated{ 0u };
  uint32_t candidates_matched{ 0u };
  uint32_t candidates_evaluated{ 0u };
  uint32_t candidates_rejected{ 0u };
  uint32_t exact_checks{ 0u };
  uint32_t exact_rejected{ 0u };
  int32_t total_gate_gain{ 0 };
  int32_t total_inverter_gain{ 0 };
};

namespace detail
{

template<class Ntk>
struct mmig_window_cost
{
  uint32_t gates{ 0u };
  uint32_t depth{ 0u };
  uint32_t inverted_edges{ 0u };
  uint32_t majinv_inverted_edges{ 0u };
};

template<class Ntk>
mmig_window_cost<Ntk> compute_mmig_window_cost( Ntk const& ntk )
{
  mmig_window_cost<Ntk> cost{};
  cost.gates = ntk.num_gates();
  depth_view<Ntk> dv{ ntk };
  cost.depth = dv.depth();

  uint64_t inv = 0u;
  uint64_t majinv = 0u;
  ntk.foreach_gate( [&]( auto const& n ) {
    ntk.foreach_fanin( n, [&]( auto const& f ) {
      bool const complemented = ntk.is_complemented( f );
      if ( complemented )
      {
        ++inv;
      }
      majinv += bool( complemented ^ ntk.is_min( ntk.get_node( f ) ) );
    } );
  } );
  ntk.foreach_po( [&]( auto const& s, auto ) {
    bool const complemented = ntk.is_complemented( s );
    if ( complemented )
    {
      ++inv;
    }
    majinv += bool( complemented ^ ntk.is_min( ntk.get_node( s ) ) );
  } );
  cost.inverted_edges = static_cast<uint32_t>( std::min<uint64_t>( inv, std::numeric_limits<uint32_t>::max() ) );
  cost.majinv_inverted_edges = static_cast<uint32_t>( std::min<uint64_t>( majinv, std::numeric_limits<uint32_t>::max() ) );
  return cost;
}

template<class Ntk>
struct mmig_window
{
  std::vector<node<Ntk>> internal;
  std::vector<node<Ntk>> leaves;
};

template<typename Node>
bool contains_node( std::vector<Node> const& nodes, Node const& n )
{
  return std::find( nodes.begin(), nodes.end(), n ) != nodes.end();
}

template<class Ntk>
bool collect_mmig_window( Ntk const& ntk, node<Ntk> const& root, mmig_window_resynthesis_params const& ps, mmig_window<Ntk>& win )
{
  win.internal.clear();
  win.leaves.clear();

  if ( ntk.is_constant( root ) || ntk.is_pi( root ) )
  {
    return false;
  }
  if constexpr ( has_is_dead_v<Ntk> )
  {
    if ( ntk.is_dead( root ) )
    {
      return false;
    }
  }

  std::vector<node<Ntk>> stack{ root };
  while ( !stack.empty() && win.internal.size() < ps.max_internal_nodes )
  {
    auto const n = stack.back();
    stack.pop_back();

    if ( contains_node( win.internal, n ) )
    {
      continue;
    }
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
    {
      continue;
    }
    if constexpr ( has_is_dead_v<Ntk> )
    {
      if ( ntk.is_dead( n ) )
      {
        continue;
      }
    }

    win.internal.emplace_back( n );
    ntk.foreach_fanin( n, [&]( auto const& f ) {
      auto const child = ntk.get_node( f );
      if ( ntk.is_constant( child ) || ntk.is_pi( child ) )
      {
        return;
      }
      if constexpr ( has_is_dead_v<Ntk> )
      {
        if ( ntk.is_dead( child ) )
        {
          return;
        }
      }
      if ( win.internal.size() + stack.size() < ps.max_internal_nodes )
      {
        stack.emplace_back( child );
      }
    } );
  }

  for ( auto const& n : win.internal )
  {
    ntk.foreach_fanin( n, [&]( auto const& f ) {
      auto const child = ntk.get_node( f );
      if ( ntk.is_constant( child ) )
      {
        return;
      }
      if ( contains_node( win.internal, child ) )
      {
        return;
      }
      if ( !contains_node( win.leaves, child ) )
      {
        win.leaves.emplace_back( child );
      }
    } );
  }

  return !win.leaves.empty() && win.leaves.size() <= ps.cut_size;
}

template<class Ntk>
class mmig_window_truth
{
public:
  mmig_window_truth( Ntk const& ntk, mmig_window<Ntk> const& win )
      : _ntk( ntk ), _win( win ), _num_vars( static_cast<uint32_t>( win.leaves.size() ) )
  {
    for ( auto i = 0u; i < _win.leaves.size(); ++i )
    {
      _leaf_to_var[_win.leaves[i]] = static_cast<uint32_t>( i );
    }
  }

  kitty::dynamic_truth_table eval_node( node<Ntk> const& n )
  {
    auto it = _memo.find( n );
    if ( it != _memo.end() )
    {
      return it->second;
    }

    kitty::dynamic_truth_table result( _num_vars );
    if ( _ntk.is_constant( n ) )
    {
      _memo[n] = result;
      return result;
    }

    if ( auto lit = _leaf_to_var.find( n ); lit != _leaf_to_var.end() )
    {
      kitty::create_nth_var( result, lit->second );
      _memo[n] = result;
      return result;
    }

    std::vector<kitty::dynamic_truth_table> fs;
    _ntk.foreach_fanin( n, [&]( auto const& f ) {
      fs.emplace_back( eval_signal( f ) );
    } );
    if ( fs.size() != 3u )
    {
      _memo[n] = result;
      return result;
    }

    auto maj = ( fs[0] & fs[1] ) | ( fs[0] & fs[2] ) | ( fs[1] & fs[2] );
    result = _ntk.is_min( n ) ? ~maj : maj;
    _memo[n] = result;
    return result;
  }

  kitty::dynamic_truth_table eval_signal( signal<Ntk> const& s )
  {
    auto tt = eval_node( _ntk.get_node( s ) );
    return _ntk.is_complemented( s ) ? ~tt : tt;
  }

private:
  Ntk const& _ntk;
  mmig_window<Ntk> const& _win;
  uint32_t _num_vars;
  std::unordered_map<node<Ntk>, uint32_t> _leaf_to_var;
  std::unordered_map<node<Ntk>, kitty::dynamic_truth_table> _memo;
};

enum class mmig_expr_kind : uint8_t
{
  constant,
  leaf,
  maj,
  min
};

struct mmig_expr
{
  mmig_expr_kind kind{ mmig_expr_kind::constant };
  bool complemented{ false };
  uint32_t leaf_index{ 0u };
  uint32_t a{ 0u };
  uint32_t b{ 0u };
  uint32_t c{ 0u };
  uint32_t gates{ 0u };
  uint32_t local_inverted_edges{ 0u };
  uint32_t local_majinv_inverted_edges{ 0u };
  kitty::dynamic_truth_table tt;

  explicit mmig_expr( uint32_t vars = 0u ) : tt( vars ) {}
};

struct mmig_window_match
{
  uint32_t expr_index{ 0u };
  bool output_complemented{ false };
};

inline bool same_truth_table( kitty::dynamic_truth_table const& a, kitty::dynamic_truth_table const& b )
{
  return a == b;
}

template<class Ntk>
signal<Ntk> instantiate_mmig_expr( Ntk& ntk,
                                   std::vector<mmig_expr> const& exprs,
                                   uint32_t idx,
                                   std::vector<node<Ntk>> const& leaves,
                                   std::unordered_map<uint32_t, signal<Ntk>>& memo )
{
  if ( auto it = memo.find( idx ); it != memo.end() )
  {
    return it->second;
  }

  auto const& e = exprs[idx];
  signal<Ntk> s{};
  switch ( e.kind )
  {
  case mmig_expr_kind::constant:
    s = ntk.get_constant( e.complemented );
    break;
  case mmig_expr_kind::leaf:
    s = ntk.make_signal( leaves[e.leaf_index] );
    if ( e.complemented )
    {
      s = !s;
    }
    break;
  case mmig_expr_kind::maj:
  {
    auto a = instantiate_mmig_expr( ntk, exprs, e.a, leaves, memo );
    auto b = instantiate_mmig_expr( ntk, exprs, e.b, leaves, memo );
    auto c = instantiate_mmig_expr( ntk, exprs, e.c, leaves, memo );
    s = ntk.create_maj( a, b, c );
  }
  break;
  case mmig_expr_kind::min:
  {
    auto a = instantiate_mmig_expr( ntk, exprs, e.a, leaves, memo );
    auto b = instantiate_mmig_expr( ntk, exprs, e.b, leaves, memo );
    auto c = instantiate_mmig_expr( ntk, exprs, e.c, leaves, memo );
    s = ntk.create_min( a, b, c );
  }
  break;
  }

  memo[idx] = s;
  return s;
}

template<class Ntk>
bool signal_depends_on_node( Ntk const& ntk, signal<Ntk> const& s, node<Ntk> const& target )
{
  std::vector<node<Ntk>> stack{ ntk.get_node( s ) };
  std::unordered_set<node<Ntk>> seen;
  while ( !stack.empty() )
  {
    auto const n = stack.back();
    stack.pop_back();
    if ( n == target )
    {
      return true;
    }
    if ( !seen.insert( n ).second )
    {
      continue;
    }
    if ( ntk.is_constant( n ) || ntk.is_pi( n ) )
    {
      continue;
    }
    if constexpr ( has_is_dead_v<Ntk> )
    {
      if ( ntk.is_dead( n ) )
      {
        continue;
      }
    }
    ntk.foreach_fanin( n, [&]( auto const& f ) {
      stack.emplace_back( ntk.get_node( f ) );
    } );
  }
  return false;
}

template<class Ntk>
bool exact_full_output_equivalent( Ntk const& before, Ntk const& after, uint32_t max_exact_pis )
{
  if ( before.num_pis() != after.num_pis() || before.num_pos() != after.num_pos() )
  {
    return false;
  }
  if ( before.num_pis() > max_exact_pis )
  {
    return true;
  }

  default_simulator<kitty::dynamic_truth_table> sim( static_cast<unsigned>( before.num_pis() ) );
  auto const before_tts = simulate<kitty::dynamic_truth_table>( before, sim );
  auto const after_tts = simulate<kitty::dynamic_truth_table>( after, sim );
  if ( before_tts.size() != after_tts.size() )
  {
    return false;
  }
  for ( auto i = 0u; i < before_tts.size(); ++i )
  {
    if ( before_tts[i] != after_tts[i] )
    {
      return false;
    }
  }
  return true;
}

class mmig_expr_database
{
public:
  explicit mmig_expr_database( uint32_t num_vars, mmig_window_resynthesis_params const& ps )
      : _num_vars( num_vars ), _ps( ps )
  {
    build_base();
    build_one_gate();
    build_two_gate();
  }

  std::vector<mmig_expr> const& expressions() const
  {
    return _exprs;
  }

private:
  void add_expr( mmig_expr expr )
  {
    if ( _exprs.size() >= _ps.max_expr_candidates )
    {
      return;
    }
    _exprs.emplace_back( std::move( expr ) );
  }

  void build_base()
  {
    mmig_expr zero( _num_vars );
    zero.kind = mmig_expr_kind::constant;
    zero.complemented = false;
    add_expr( zero );

    mmig_expr one( _num_vars );
    one.kind = mmig_expr_kind::constant;
    one.complemented = true;
    one.local_majinv_inverted_edges = 1u;
    one.tt = ~one.tt;
    add_expr( one );

    for ( auto i = 0u; i < _num_vars; ++i )
    {
      mmig_expr x( _num_vars );
      x.kind = mmig_expr_kind::leaf;
      x.leaf_index = i;
      kitty::create_nth_var( x.tt, i );
      add_expr( x );

      mmig_expr nx = x;
      nx.complemented = true;
      nx.tt = ~nx.tt;
      nx.local_inverted_edges = 1u;
      nx.local_majinv_inverted_edges = 1u;
      add_expr( nx );
    }
    _base_end = static_cast<uint32_t>( _exprs.size() );
  }

  void add_gate_expr( mmig_expr_kind kind, uint32_t a, uint32_t b, uint32_t c )
  {
    auto const maj = ( _exprs[a].tt & _exprs[b].tt ) | ( _exprs[a].tt & _exprs[c].tt ) | ( _exprs[b].tt & _exprs[c].tt );
    auto const function = ( kind == mmig_expr_kind::min ) ? ~maj : maj;
    if ( _ps.skip_trivial_expressions )
    {
      // Literal/constant functions already have zero-gate implementations.
      // Also omit a gate that merely repeats or complements one input.
      for ( auto i = 0u; i < _base_end; ++i )
        if ( function == _exprs[i].tt ) return;
      for ( auto i : { a, b, c } )
        if ( function == _exprs[i].tt || function == ~_exprs[i].tt ) return;
    }
    mmig_expr e( _num_vars );
    e.kind = kind;
    e.a = a;
    e.b = b;
    e.c = c;
    e.gates = 1u + _exprs[a].gates + _exprs[b].gates + _exprs[c].gates;
    e.local_inverted_edges = _exprs[a].local_inverted_edges + _exprs[b].local_inverted_edges + _exprs[c].local_inverted_edges;
    for ( auto i : { a, b, c } )
      e.local_majinv_inverted_edges += _exprs[i].local_majinv_inverted_edges +
          ( _exprs[i].kind == mmig_expr_kind::min ? 1u : 0u );
    e.tt = function;
    add_expr( std::move( e ) );
  }

  void build_one_gate()
  {
    auto const n = _base_end;
    for ( auto a = 0u; a < n; ++a )
    {
      for ( auto b = a; b < n; ++b )
      {
        for ( auto c = b; c < n; ++c )
        {
          add_gate_expr( mmig_expr_kind::maj, a, b, c );
          add_gate_expr( mmig_expr_kind::min, a, b, c );
        }
      }
    }
    _one_gate_end = static_cast<uint32_t>( _exprs.size() );
  }

  void build_two_gate()
  {
    auto const one_begin = _base_end;
    auto const one_end = _one_gate_end;
    for ( auto sub = one_begin; sub < one_end; ++sub )
    {
      for ( auto b = 0u; b < _base_end; ++b )
      {
        for ( auto c = b; c < _base_end; ++c )
        {
          add_gate_expr( mmig_expr_kind::maj, sub, b, c );
          add_gate_expr( mmig_expr_kind::min, sub, b, c );
          if ( _exprs.size() >= _ps.max_expr_candidates )
          {
            return;
          }
        }
      }
    }
  }

  uint32_t _num_vars;
  mmig_window_resynthesis_params const& _ps;
  uint32_t _base_end{ 0u };
  uint32_t _one_gate_end{ 0u };
  std::vector<mmig_expr> _exprs;
};

template<class Ntk>
bool is_mmig_window_candidate_better( mmig_window_cost<Ntk> const& after,
                                      mmig_window_cost<Ntk> const& before,
                                      mmig_window_resynthesis_params const& ps )
{
  if ( ps.preserve_depth && after.depth > before.depth )
  {
    return false;
  }
  if ( after.gates < before.gates )
  {
    return true;
  }
  if ( after.gates == before.gates && after.inverted_edges + ps.min_inverter_gain <= before.inverted_edges )
  {
    return true;
  }
  if ( ps.allow_gate_increase && after.inverted_edges + ps.min_inverter_gain <= before.inverted_edges )
  {
    return true;
  }
  return false;
}

template<class Ntk>
bool is_mmig_window_candidate_admissible( mmig_window_cost<Ntk> const& after,
                                          mmig_window_cost<Ntk> const& before,
                                          mmig_window_resynthesis_params const& ps )
{
  if ( ps.preserve_depth && after.depth > before.depth )
  {
    return false;
  }

  switch ( ps.objective_policy )
  {
  case mmig_inverter_objective_policy::legacy:
    return is_mmig_window_candidate_better( after, before, ps );

  case mmig_inverter_objective_policy::gate_inv:
    if ( after.gates < before.gates )
    {
      return after.inverted_edges <= before.inverted_edges + ps.max_inv_increase_on_gate_gain;
    }
    if ( after.gates == before.gates && after.inverted_edges + ps.min_inverter_gain <= before.inverted_edges )
    {
      return true;
    }
    return ps.allow_gate_increase && after.inverted_edges + ps.min_inverter_gain <= before.inverted_edges;

  case mmig_inverter_objective_policy::majinv_gate_inv:
    if ( after.gates < before.gates )
      return after.majinv_inverted_edges <= before.majinv_inverted_edges + ps.max_inv_increase_on_gate_gain;
    if ( after.gates == before.gates )
      return after.majinv_inverted_edges + ps.min_inverter_gain <= before.majinv_inverted_edges;
    return ps.allow_gate_increase &&
           after.majinv_inverted_edges + ps.min_inverter_gain <= before.majinv_inverted_edges;

  case mmig_inverter_objective_policy::safe_gate_inv:
    return after.gates <= before.gates &&
           after.depth <= before.depth &&
           after.inverted_edges <= before.inverted_edges &&
           ( after.gates < before.gates || after.depth < before.depth || after.inverted_edges + ps.min_inverter_gain <= before.inverted_edges );

  case mmig_inverter_objective_policy::inv_safe:
    return after.gates <= before.gates &&
           after.depth <= before.depth &&
           after.inverted_edges + ps.min_inverter_gain <= before.inverted_edges;
  }

  return false;
}

template<class Ntk>
bool is_mmig_window_candidate_preferred( mmig_window_cost<Ntk> const& lhs,
                                         mmig_window_cost<Ntk> const& rhs,
                                         mmig_window_resynthesis_params const& ps )
{
  switch ( ps.objective_policy )
  {
  case mmig_inverter_objective_policy::legacy:
    return is_mmig_window_candidate_better( lhs, rhs, ps );

  case mmig_inverter_objective_policy::gate_inv:
  case mmig_inverter_objective_policy::safe_gate_inv:
    if ( lhs.gates != rhs.gates )
    {
      return lhs.gates < rhs.gates;
    }
    if ( lhs.inverted_edges != rhs.inverted_edges )
    {
      return lhs.inverted_edges < rhs.inverted_edges;
    }
    return lhs.depth < rhs.depth;

  case mmig_inverter_objective_policy::majinv_gate_inv:
    if ( lhs.gates != rhs.gates ) return lhs.gates < rhs.gates;
    if ( lhs.majinv_inverted_edges != rhs.majinv_inverted_edges )
      return lhs.majinv_inverted_edges < rhs.majinv_inverted_edges;
    return lhs.depth < rhs.depth;

  case mmig_inverter_objective_policy::inv_safe:
    if ( lhs.inverted_edges != rhs.inverted_edges )
    {
      return lhs.inverted_edges < rhs.inverted_edges;
    }
    if ( lhs.gates != rhs.gates )
    {
      return lhs.gates < rhs.gates;
    }
    return lhs.depth < rhs.depth;
  }

  return false;
}

} // namespace detail

template<class Ntk>
void mmig_window_resynthesis( Ntk& ntk,
                              mmig_window_resynthesis_params const& ps = {},
                              mmig_window_resynthesis_stats* pst = nullptr )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( std::is_same_v<typename Ntk::base_type, mig_network>, "Network type is not MIG-based" );
  static_assert( has_is_min_v<Ntk>, "Ntk does not implement is_min" );
  static_assert( has_create_min_v<Ntk>, "Ntk does not implement create_min" );
  static_assert( has_clone_v<Ntk>, "Ntk does not implement clone" );

  mmig_window_resynthesis_stats st{};
  stopwatch t( st.time_total );

  std::unordered_map<uint32_t, detail::mmig_expr_database> dbs;

  bool restart = true;
  while ( restart )
  {
    restart = false;

    std::vector<node<Ntk>> roots;
    ntk.foreach_gate( [&]( auto const& n ) {
      if ( ps.spread_roots || ps.max_roots == 0u || roots.size() < ps.max_roots )
      {
        roots.emplace_back( n );
      }
    } );
    if ( ps.spread_roots && ps.max_roots && roots.size() > ps.max_roots )
    {
      auto const all = std::move( roots );
      roots.clear();
      for ( auto i = 0u; i < ps.max_roots; ++i )
        roots.push_back( all[static_cast<size_t>( i ) * all.size() / ps.max_roots] );
    }

    for ( auto const& root : roots )
    {
      ++st.roots_seen;
      if ( root >= ntk.size() )
      {
        ++st.roots_skipped;
        continue;
      }
      if constexpr ( has_is_dead_v<Ntk> )
      {
        if ( ntk.is_dead( root ) )
        {
          ++st.roots_skipped;
          continue;
        }
      }

      detail::mmig_window<Ntk> win;
      if ( !detail::collect_mmig_window( ntk, root, ps, win ) )
      {
        ++st.roots_skipped;
        continue;
      }

      detail::mmig_window_truth<Ntk> truth( ntk, win );
      auto const target = truth.eval_node( root );
      auto const vars = static_cast<uint32_t>( win.leaves.size() );
      if ( dbs.find( vars ) == dbs.end() )
      {
        dbs.emplace( vars, detail::mmig_expr_database( vars, ps ) );
      }
      auto const& exprs = dbs.at( vars ).expressions();
      st.candidates_generated += static_cast<uint32_t>( std::min<size_t>( exprs.size(), std::numeric_limits<uint32_t>::max() ) );

      std::vector<detail::mmig_window_match> matches;
      auto const target_inv = ~target;
      for ( auto i = 0u; i < exprs.size(); ++i )
      {
        if ( detail::same_truth_table( exprs[i].tt, target ) )
        {
          matches.push_back( detail::mmig_window_match{ static_cast<uint32_t>( i ), false } );
        }
        if ( ps.allow_output_polarity && detail::same_truth_table( exprs[i].tt, target_inv ) )
        {
          matches.push_back( detail::mmig_window_match{ static_cast<uint32_t>( i ), true } );
        }
      }
      std::sort( matches.begin(), matches.end(), [&]( auto a, auto b ) {
        auto const gates_a = exprs[a.expr_index].gates;
        auto const gates_b = exprs[b.expr_index].gates;
        if ( gates_a != gates_b )
        {
          return gates_a < gates_b;
        }
        if ( ps.rank_by_local_inverters )
        {
          bool const expanded = ps.objective_policy == mmig_inverter_objective_policy::majinv_gate_inv;
          auto const& ea = exprs[a.expr_index];
          auto const& eb = exprs[b.expr_index];
          auto const inv_a = expanded ? ea.local_majinv_inverted_edges +
              bool( a.output_complemented ^ ( ea.kind == detail::mmig_expr_kind::min ) ) :
              ea.local_inverted_edges + ( a.output_complemented ? 1u : 0u );
          auto const inv_b = expanded ? eb.local_majinv_inverted_edges +
              bool( b.output_complemented ^ ( eb.kind == detail::mmig_expr_kind::min ) ) :
              eb.local_inverted_edges + ( b.output_complemented ? 1u : 0u );
          if ( inv_a != inv_b )
          {
            return inv_a < inv_b;
          }
        }
        if ( a.output_complemented != b.output_complemented )
          return a.output_complemented < b.output_complemented;
        return ps.skip_trivial_expressions && a.expr_index < b.expr_index;
      } );
      if ( matches.size() > ps.max_matches_per_root )
      {
        matches.resize( ps.max_matches_per_root );
      }
      st.candidates_matched += static_cast<uint32_t>( matches.size() );

      auto const before = detail::compute_mmig_window_cost( ntk );
      bool accepted = false;
      Ntk best_ntk{};
      auto best_cost = before;

      for ( auto const match : matches )
      {
        ++st.candidates_evaluated;
        auto trial = ntk.clone();
        std::unordered_map<uint32_t, signal<Ntk>> memo;
        auto repl = detail::instantiate_mmig_expr( trial, exprs, match.expr_index, win.leaves, memo );
        if ( match.output_complemented )
        {
          repl = !repl;
        }
        if ( detail::signal_depends_on_node( trial, repl, root ) )
        {
          ++st.candidates_rejected;
          continue;
        }
        trial.substitute_node( root, repl );
        trial = cleanup_dangling( trial );
        if ( ntk.num_pis() <= ps.max_exact_pis )
        {
          ++st.exact_checks;
          if ( !detail::exact_full_output_equivalent( ntk, trial, ps.max_exact_pis ) )
          {
            ++st.exact_rejected;
            ++st.candidates_rejected;
            continue;
          }
        }
        auto const after = detail::compute_mmig_window_cost( trial );
        if ( detail::is_mmig_window_candidate_admissible( after, before, ps ) &&
             detail::is_mmig_window_candidate_preferred( after, best_cost, ps ) )
        {
          best_ntk = std::move( trial );
          best_cost = after;
          accepted = true;
        }
        else
        {
          ++st.candidates_rejected;
        }
      }

      if ( accepted )
      {
        st.total_gate_gain += static_cast<int32_t>( before.gates ) - static_cast<int32_t>( best_cost.gates );
        st.total_inverter_gain += ps.objective_policy == mmig_inverter_objective_policy::majinv_gate_inv
            ? static_cast<int32_t>( before.majinv_inverted_edges ) - static_cast<int32_t>( best_cost.majinv_inverted_edges )
            : static_cast<int32_t>( before.inverted_edges ) - static_cast<int32_t>( best_cost.inverted_edges );
        ntk = std::move( best_ntk );
        ++st.roots_resynthesized;
        restart = true;
        break;
      }
    }
  }

  if ( pst != nullptr )
  {
    *pst = st;
  }
}

} // namespace mockturtle
