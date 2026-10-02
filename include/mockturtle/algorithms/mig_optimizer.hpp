#pragma once

#include <mockturtle/mockturtle.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/mig_algebraic_rewriting.hpp>
#include <mockturtle/views/depth_view.hpp>
#include <mockturtle/views/fanout_view.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <mockturtle/utils/network_utils.hpp>
#include <mockturtle/algorithms/node_resynthesis/mig_npn.hpp>

#include <vector>
#include <iostream>
#include <algorithm>

namespace mockturtle
{

/*! \brief MIG algebraic size optimization. */
void mig_algebraic_size_optimization(mig_network& mig)
{
  mig_algebraic_depth_rewriting_params ps;
  ps.allow_area_increase = false;
  mig_algebraic_depth_rewriting(mig, ps);
  mig = cleanup_dangling(mig);
}

/*! \brief MIG algebraic depth optimization. */
void mig_algebraic_depth_optimization(mig_network& mig)
{
  depth_view depth_mig{mig}; // Wrap the MIG network with depth_view
  mig_algebraic_depth_rewriting_params ps;
  ps.allow_area_increase = true;
  mig_algebraic_depth_rewriting(depth_mig, ps); // Use depth_mig instead of mig
  mig = cleanup_dangling(depth_mig); // Cleanup dangling nodes in the depth view
}

namespace detail {

template<class Ntk>
class critical_voters_impl
{
public:
  critical_voters_impl(Ntk const& ntk)
    : ntk(ntk),
      fanout_ntk(ntk),
      criticality(ntk)
  {
    compute_criticality();
  }

  std::pair<node<Ntk>, node<Ntk>> get_critical_voters()
  {
    std::vector<std::pair<node<Ntk>, double>> node_criticality;
    ntk.foreach_node([&](auto n) {
      if (ntk.is_constant(n) || ntk.is_pi(n)) return;
      node_criticality.push_back({n, criticality[n]});
    });

    std::sort(node_criticality.begin(), node_criticality.end(), [](auto const& a, auto const& b) {
      return a.second > b.second;
    });

    if (node_criticality.size() < 2)
    {
      return {0, 0};
    }

    return {node_criticality[0].first, node_criticality[1].first};
  }

private:
  void compute_criticality()
  {
    ntk.foreach_node([&](auto n) {
      if (ntk.is_constant(n)) return;
      criticality[n] = 0.0;
      fanout_ntk.foreach_fanout(n, [&](auto const& fon) {
        double path_criticality = 1.0 / 3.0;
        std::vector<node<Ntk>> visited;
        compute_criticality_recur(fon, path_criticality, visited);
        criticality[n] += path_criticality;
      });
    });
  }

  void compute_criticality_recur(node<Ntk> const& n, double& path_criticality, std::vector<node<Ntk>>& visited)
  {
    if (std::find(visited.begin(), visited.end(), n) != visited.end()) return;
    visited.push_back(n);

    fanout_ntk.foreach_fanout(n, [&](auto const& fon) {
      path_criticality *= (1.0 / 3.0);
      compute_criticality_recur(fon, path_criticality, visited);
    });
  }

  Ntk const& ntk;
  fanout_view<Ntk> fanout_ntk;
  node_map<double, Ntk> criticality;
};

}

/*! \brief MIG Boolean depth optimization. */
void mig_boolean_depth_optimization(mig_network& mig)
{
  std::cout << "[i] running Boolean depth optimization" << std::endl;

  // 1. Find critical voters
  detail::critical_voters_impl<mig_network> critical_voters_logic(mig);
  auto const [voter1, voter2] = critical_voters_logic.get_critical_voters();

  if (voter1 == 0 || voter2 == 0)
  {
    std::cout << "[w] could not find two critical voters, skipping Boolean optimization" << std::endl;
    return;
  }

  // 2. Create three erroneous versions of the MIG
  mig_network mig_a = mig.clone();
  mig_network mig_b = mig.clone();
  mig_network mig_c = mig.clone();

  auto const voter1_a = mig_a.get_node(mig.make_signal(voter1));
  auto const voter2_a = mig_a.get_node(mig.make_signal(voter2));
  mig_a.substitute_node(voter1_a, mig_a.create_not(mig_a.make_signal(voter2_a)));

  auto const voter1_b = mig_b.get_node(mig.make_signal(voter1));
  auto const voter2_b = mig_b.get_node(mig.make_signal(voter2));
  mig_b.foreach_gate([&](auto n) {
    if (!mig_b.is_maj(n)) return;
    bool has_voter1 = false, has_voter2 = false;
    mig_b.foreach_fanout(n, [&](auto const& f) {
      if (mig_b.get_node(f) == voter1_b) has_voter1 = true;
      if (mig_b.get_node(f) == voter2_b) has_voter2 = true;
    });
    if (has_voter1 && has_voter2)
    {
      mig_b.substitute_node(n, mig_b.make_signal(voter1_b));
    }
  });

  auto const voter1_c = mig_c.get_node(mig.make_signal(voter1));
  auto const voter2_c = mig_c.get_node(mig.make_signal(voter2));
  fanout_view fanout_mig_c{mig_c};
  fanout_mig_c.foreach_gate([&](auto n) {
    if (!mig_c.is_maj(n)) return;
    bool has_voter1 = false, has_voter2 = false;
    fanout_mig_c.foreach_fanout(n, [&](auto const& f) {
      if (mig_c.get_node(f) == voter1_c) has_voter1 = true;
      if (mig_c.get_node(f) == voter2_c) has_voter2 = true;
    });
    if (has_voter1 && has_voter2)
    {
      mig_c.substitute_node(n, mig_c.make_signal(voter2_c));
    }
  });

  // 3. Optimize each version algebraically
  mig_algebraic_depth_optimization(mig_a);
  mig_algebraic_depth_optimization(mig_b);
  mig_algebraic_depth_optimization(mig_c);

  // 4. Combine the results with a majority gate
  mig_network result;
  node_map<signal<mig_network>, mig_network> old_to_new(mig);

  old_to_new[mig.get_constant(false)] = result.get_constant(false);
  mig.foreach_pi([&](auto n, auto i) {
    old_to_new[n] = result.create_pi();
  });

  mig.foreach_po([&](auto const& f, auto i) {
    std::vector<signal<mig_network>> po_signals;
    insert_ntk(result, std::back_inserter(po_signals), mig_a, [&](auto n) { return old_to_new[n]; });
    insert_ntk(result, std::back_inserter(po_signals), mig_b, [&](auto n) { return old_to_new[n]; });
    insert_ntk(result, std::back_inserter(po_signals), mig_c, [&](auto n) { return old_to_new[n]; });
    result.create_po(result.create_maj(po_signals[0], po_signals[1], po_signals[2]));
  });

  mig = result;
  mig_algebraic_depth_optimization(mig);
}

/*! \brief Top-level MIG optimization script. */
void top_level_mig_optimization(mig_network& mig)
{
  std::cout << "[i] starting top-level MIG optimization" << std::endl;

  mig_algebraic_depth_optimization(mig);
  mig_algebraic_size_optimization(mig);
  mig_boolean_depth_optimization(mig);
  mig_algebraic_depth_optimization(mig);
  mig_algebraic_size_optimization(mig);
  mig_algebraic_depth_optimization(mig);
  mig_algebraic_size_optimization(mig);

  std::cout << "[i] finished top-level MIG optimization" << std::endl;
}

} // namespace mockturtle
