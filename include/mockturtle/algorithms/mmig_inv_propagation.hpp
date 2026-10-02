#pragma once

#include <cstdint>

#include "../utils/stopwatch.hpp"
#include "../views/fanout_view.hpp"
#include "../traits.hpp"

namespace mockturtle
{

struct mmig_inv_propagation_params
{
  bool enable_dual_inversion{ true };
  uint32_t dual_inv_min_fanout{ 2u };
};

struct mmig_inv_propagation_stats
{
  int32_t num_rewritten{ 0 };
  int32_t num_dual_rewritten{ 0 };
  int32_t estimated_gain{ 0 };
  stopwatch<>::duration time_total{ 0 };
};

namespace detail
{

template<class Ntk>
class mmig_inv_propagation_impl
{
public:
  mmig_inv_propagation_impl( Ntk& ntk, mmig_inv_propagation_params const& ps, mmig_inv_propagation_stats& st )
      : ntk( ntk ),
        ps( ps ),
        st( st )
  {
  }

  void run()
  {
    stopwatch t( st.time_total );
    ntk.foreach_gate( [&]( auto const& n ) {
      signal<Ntk> fanins[3];
      uint32_t inv_count = 0;
      ntk.foreach_fanin( n, [&]( auto const& f, auto i ) {
        fanins[i] = f;
        if ( ntk.is_complemented( f ) )
        {
          ++inv_count;
        }
      } );

      if ( inv_count == 3u )
      {
        rewrite_triple_inversion( n, fanins );
      }
      else if ( inv_count == 2u && ps.enable_dual_inversion )
      {
        rewrite_dual_inversion( n, fanins );
      }
    } );
  }

private:
  /* --- Triple inversion ---
   * MAJ(!a,!b,!c) = MIN(a,b,c)   [no output complement needed, saves 3]
   * MIN(!a,!b,!c) = MAJ(a,b,c)   [no output complement needed, saves 3]
   *
   * Fallback for pure-MAJ networks (no MIN gate): uses De Morgan
   *   MAJ(!a,!b,!c) = !MAJ(a,b,c), net saving = 2.
   */
  void rewrite_triple_inversion( node<Ntk> const& n, signal<Ntk> fanins[3] )
  {
    auto const a = !fanins[0];
    auto const b = !fanins[1];
    auto const c = !fanins[2];

    signal<Ntk> replacement{};
    if constexpr ( has_is_min_v<Ntk> && has_create_min_v<Ntk> )
    {
      if ( ntk.is_min( n ) )
      {
        /* MIN(!a,!b,!c) = MAJ(a,b,c) — flip gate type, no output inversion */
        replacement = ntk.create_maj( a, b, c );
      }
      else
      {
        /* MAJ(!a,!b,!c) = MIN(a,b,c) — flip gate type, no output inversion */
        replacement = ntk.create_min( a, b, c );
      }
    }
    else
    {
      /* Pure-MAJ fallback: MAJ(!a,!b,!c) = !MAJ(a,b,c), saves 2 */
      replacement = !ntk.create_maj( a, b, c );
    }

    ntk.substitute_node( n, replacement );
    ntk.replace_in_outputs( n, replacement );
    ++st.num_rewritten;
    st.estimated_gain += 3;
  }

  /* --- Dual inversion ---
   * MAJ(!a,!b,c) = MIN(a,b,!c)
   * MIN(!a,!b,c) = MAJ(a,b,!c)
   * The output polarity is unchanged. Two complemented input edges become
   * one, so the local gain is one regardless of fanout edge polarities.
   */
  void rewrite_dual_inversion( node<Ntk> const& n, signal<Ntk> fanins[3] )
  {
    if constexpr ( !has_is_min_v<Ntk> || !has_create_min_v<Ntk> )
    {
      return;
    }

    /* Preserve the existing minimum-fanout control for compatibility. */
    fanout_view<Ntk> fntk{ ntk };
    uint32_t total_refs = 0u;
    fntk.foreach_fanout( n, [&]( auto const& parent ) {
      fntk.foreach_fanin( parent, [&]( auto const& f ) {
        if ( fntk.get_node( f ) == n )
          ++total_refs;
      } );
    } );
    ntk.foreach_po( [&]( auto const& s, auto ) {
      if ( ntk.get_node( s ) == n )
        ++total_refs;
    } );
    if ( total_refs < ps.dual_inv_min_fanout )
      return;

    /* Identify the two inverted fanins and one non-inverted */
    uint32_t inv_indices[2];
    uint32_t non_inv_index = 0;
    uint32_t ip = 0;
    for ( uint32_t i = 0u; i < 3u; ++i )
    {
      if ( ntk.is_complemented( fanins[i] ) )
      {
        inv_indices[ip++] = i;
      }
      else
      {
        non_inv_index = i;
      }
    }

    /* Strip inversions from the two inverted fanins, add inversion to the
     * non-inverted one */
    auto const a = !fanins[inv_indices[0]]; // strip inversion
    auto const b = !fanins[inv_indices[1]]; // strip inversion
    auto const c = !fanins[non_inv_index];  // add inversion

    /* MAJ(!a,!b,c) = MIN(a,b,!c)
     * MIN(!a,!b,c) = MAJ(a,b,!c) */
    signal<Ntk> replacement{};
    if constexpr ( has_is_min_v<Ntk> && has_create_min_v<Ntk> )
    {
      if ( ntk.is_min( n ) )
      {
        /* MIN(!a, !b, c) = MAJ(a, b, !c) */
        replacement = ntk.create_maj( a, b, c );
      }
      else
      {
        /* MAJ(!a, !b, c) = MIN(a, b, !c) */
        replacement = ntk.create_min( a, b, c );
      }
    }

    ntk.substitute_node( n, replacement );
    ntk.replace_in_outputs( n, replacement );
    ++st.num_dual_rewritten;
    ++st.num_rewritten;
    st.estimated_gain += 1;
  }

  Ntk& ntk;
  mmig_inv_propagation_params const& ps;
  mmig_inv_propagation_stats& st;
};

} // namespace detail

template<class Ntk>
void mmig_inv_propagation( Ntk& ntk,
                           mmig_inv_propagation_params const& ps = {},
                           mmig_inv_propagation_stats* pst = nullptr )
{
  static_assert( has_foreach_gate_v<Ntk>, "Ntk does not implement foreach_gate" );
  static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement foreach_fanin" );
  static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement is_complemented" );
  static_assert( has_create_maj_v<Ntk>, "Ntk does not implement create_maj" );
  static_assert( has_substitute_node_v<Ntk>, "Ntk does not implement substitute_node" );
  static_assert( has_replace_in_outputs_v<Ntk>, "Ntk does not implement replace_in_outputs" );

  mmig_inv_propagation_stats st;
  detail::mmig_inv_propagation_impl<Ntk> impl( ntk, ps, st );
  impl.run();
  if ( pst != nullptr )
  {
    *pst = st;
  }
}

template<class Ntk>
void mmig_inv_propagation( Ntk& ntk, mmig_inv_propagation_stats* pst )
{
  mmig_inv_propagation( ntk, mmig_inv_propagation_params{}, pst );
}

} // namespace mockturtle
