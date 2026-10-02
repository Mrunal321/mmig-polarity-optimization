// Diagnostic build only. It includes the frozen source without editing zg2.
// Replays the archived Stage 2 in memory and exports native/lowered controls.
#define main archived_blif2mig_main
#include "../examples/blif2mig_2.cpp"
#undef main

int main( int argc, char** argv )
{
  if ( argc != 3 )
  {
    std::cerr << "usage: phase_audit <zg2_stage1.blif> <output_prefix>\n";
    return 2;
  }
  klut_network klut;
  if ( lorina::read_blif( argv[1], blif_reader( klut ) ) != lorina::return_code::success )
  {
    return 2;
  }
  auto ntk = convert_klut_to_graph<mig_network>( klut );
  ntk = cleanup_dangling( ntk );
  auto const out = std::string( argv[2] );
  auto write_types = [&]( std::string const& name, mig_network const& graph ) {
    std::ofstream stream( out + name + ".types" );
    graph.foreach_gate( [&]( auto n ) { stream << graph.node_to_index( n ) << ':' << graph.is_min( n ) << '\n'; } );
  };
  write_blif( ntk, out + ".imported.blif" );
  write_types( ".imported", ntk );
  esl_mixed_phase::phase_stats mixed_stats{};
  ntk = esl_mixed_phase::optimize( ntk, mixed_stats );
  write_blif( ntk, out + ".mixed.blif" );
  write_types( ".mixed", ntk );
  esl_two_phase::stats two_stats{};
  ntk = esl_two_phase::optimize( ntk, two_stats );
  write_blif( ntk, out + ".native.blif" );
  write_types( ".native", ntk );
  auto lowered = esl_two_phase::expand_min_to_maj( ntk );
  write_blif( lowered, out + ".lowered_raw.blif" );
  write_types( ".lowered_raw", lowered );
  esl_polarity::phase_stats pure_stats{};
  lowered = esl_polarity::optimize( lowered, pure_stats );
  write_blif( lowered, out + ".lowered_phase.blif" );
  write_types( ".lowered_phase", lowered );
  std::cout << "imported\n";
  print_stats( convert_klut_to_graph<mig_network>( klut ), "Imported Stage 1" );
  std::cout << "native\n";
  print_stats( ntk, "Native Stage 2" );
  std::cout << "lowered raw\n";
  print_stats( esl_two_phase::expand_min_to_maj( ntk ), "Lowered raw" );
  std::cout << "lowered phase\n";
  print_stats( lowered, "Lowered phase" );
  return 0;
}
