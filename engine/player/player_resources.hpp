// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#pragma once

#include "config.hpp"

#include "sc_enums.hpp"

#include <array>
#include <limits>

// Resources
struct player_resources_t
{
  std::array<double, RESOURCE_MAX> base, initial, max, current, temporary,
      base_multiplier, initial_multiplier;
  std::array<int, RESOURCE_MAX> infinite_resource;
  std::array<bool, RESOURCE_MAX> active_resource, hasted;
  // Initial user-input resources
  std::array<double, RESOURCE_MAX> initial_opt;
  // Start-of-combat resource level
  std::array<double, RESOURCE_MAX> start_at;
  std::array<double, RESOURCE_MAX> base_regen_per_second;
  std::array<double, RESOURCE_MAX> regen_multiplier;

  player_resources_t() :
    base(),
    initial(),
    max(),
    current(),
    temporary(),
    infinite_resource(),
    start_at(),
    base_regen_per_second(),
    regen_multiplier()
  {
    base_multiplier.fill( 1.0 );
    initial_multiplier.fill( 1.0 );
    active_resource.fill( true );
    hasted.fill( false );
    initial_opt.fill( -1.0 );
    regen_multiplier.fill( 1.0 );
  }

  double pct( resource_e rt ) const
  { return max[ rt ] ? current[ rt ] / max[ rt ] : 0.0; }

  bool is_infinite( resource_e rt ) const
  { return infinite_resource[ rt ] != 0; }

  bool is_active( resource_e rt ) const
  { return active_resource[ rt ] && current[ rt ] >= 0.0; }
};
