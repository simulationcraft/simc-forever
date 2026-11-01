// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.hpp"
#include "action/parse_effects.hpp"
#include "class_modules/apl/rogue.hpp"

namespace { // UNNAMED NAMESPACE

struct rogue_t final : public player_t
{
  rogue_t( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) :
    player_t( sim, ROGUE, name, r ) {}

  resource_e primary_resource() const override { return RESOURCE_ENERGY; }

  std::string default_potion() const override   { return rogue_apl::potion( this ); }
  std::string default_flask() const override    { return rogue_apl::flask( this ); }
  std::string default_food() const override     { return rogue_apl::food( this ); }

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void init_action_list() override;

  void init_base_stats() override;
};

template <class Base>
struct rogue_action_t : public parse_action_effects_t<Base>
{
private:
  using ab = parse_action_effects_t<Base>;

public:
  using base_t = rogue_action_t<Base>;

  template <typename... Args>
  rogue_action_t( Args&&... args ) : ab( std::forward<Args>( args )... ) {}

  rogue_t* p()
  { return debug_cast<rogue_t*>( this->player ); }

  const rogue_t* p() const
  { return debug_cast<const rogue_t*>( this->player ); }
};

void rogue_t::init_base_stats()
{
  base.attack_power_per_strength = 1.0;
  base.attack_power_per_agility  = 1.0;

  player_t::init_base_stats();

  // TODO - can this base stuff be moved into core?

  base.all_crit           = 0.0;
  base.spell_crit_chance  = 0.0;

  base.dodge_per_agility = 2.0 * base.attack_crit_per_agility;
  auto base_dodge = ( dbc->race_base( race ).agility + dbc->attribute_base( type, level() ).agility )
               * base.dodge_per_agility;

  base.attack_crit_chance = get_passive_player_value( 0.0, "attack_crit" );
  base.parry              = get_passive_player_value( 0.0, "parry" );
  base.dodge              = get_passive_player_value( base_dodge, "dodge" );

  base.stats.attack_power = 2.0 * level() - 20.0;
}

// Abilities

using rogue_attack_t = rogue_action_t<melee_attack_t>;
using rogue_spell_t  = rogue_action_t<spell_t>;

struct rogue_auto_attack_t : public auto_attack_t
{
  rogue_auto_attack_t( rogue_t* p, util::string_view options_str )
    : auto_attack_t( options_str, p )
  {
    parse_options( options_str );

    p->main_hand_attack = new white_swing_t( "main_hand", p->equipped_weapons[ SLOT_MAIN_HAND ], this );
    if ( p->equipped_weapons[ SLOT_OFF_HAND ].type == WEAPON_NONE )
      p->melee_attacks.erase( SLOT_OFF_HAND );
    else
      p->off_hand_attack = new white_swing_t( "off_hand", p->equipped_weapons[ SLOT_OFF_HAND ], this );
  }
};

constexpr std::array sinister_strike_ranks {
  spell_container_t().set_spell(  1752 ),
  spell_container_t().set_spell(  1757 ),
  spell_container_t().set_spell(  1758 ),
  spell_container_t().set_spell(  1759 ),
  spell_container_t().set_spell(  1760 ),
  spell_container_t().set_spell(  8621 ),
  spell_container_t().set_spell( 11293 ),
  spell_container_t().set_spell( 11294 ),
};
struct sinister_strike_t : public rogue_attack_t
{
  sinister_strike_t( rogue_t* player, util::string_view options_str )
    :  rogue_attack_t( "sinister_strike", player, player->find_rank( sinister_strike_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

constexpr std::array eviscerate_ranks {
  spell_container_t().set_spell(  2098 ),
  spell_container_t().set_spell(  6760 ),
  spell_container_t().set_spell(  6761 ),
  spell_container_t().set_spell(  6762 ),
  spell_container_t().set_spell(  8623 ),
  spell_container_t().set_spell(  8624 ),
  spell_container_t().set_spell( 11299 ),
  spell_container_t().set_spell( 11300 ),
  spell_container_t().set_spell( 31016 ),
};
struct eviscerate_t : public rogue_attack_t
{
  eviscerate_t( rogue_t* player, util::string_view options_str )
    :  rogue_attack_t( "eviscerate", player, player->find_rank( eviscerate_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

action_t* rogue_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "auto_attack" ) return new rogue_auto_attack_t( this, options_str );

  // builders
  if ( name == "sinister_strike" ) return new sinister_strike_t( this, options_str );

  // finishers
  if ( name == "eviscerate" ) return new eviscerate_t( this, options_str );

  return player_t::create_action( name, options_str );
}

void rogue_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    player_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  rogue_apl::rogue( this );
  use_default_action_list = true;
  player_t::init_action_list();
}

// ROGUE MODULE INTERFACE ===================================================

class rogue_module_t : public module_t
{
public:
  rogue_module_t() : module_t( ROGUE ) {}

  player_t* create_player( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) const override
  {
    return new rogue_t( sim, name, r );
  }

  bool valid() const override
  { return true; }

  void register_hotfixes() const override {}

  void register_actor_initializers( sim_t* ) const override {}
};

} // UNNAMED NAMESPACE

const module_t* module_t::rogue()
{
  static rogue_module_t m;
  return &m;
}
