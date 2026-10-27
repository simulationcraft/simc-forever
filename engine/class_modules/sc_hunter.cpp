//==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.hpp"
#include "action/parse_effects.hpp"
#include "class_modules/apl/hunter.hpp"

// TODO: Default to max distance
// TODO: ItemDamageRanged for ranged weapon damage multipliers
// TODO: Does "plain" +Attack Power contribute to RAP?

namespace
{
// =========================================================================
// Hunter
// =========================================================================

struct hunter_t final : public player_t
{
  hunter_t( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) :
    player_t( sim, HUNTER, name, r ) {}

  resource_e primary_resource() const override { return RESOURCE_MANA; }

  void init_base_stats() override;

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void init_action_list() override;
};

template <class Base>
struct hunter_action_t : public parse_action_effects_t<Base>
{
  private:
    using ab = parse_action_effects_t<Base>;

  public:
    using base_t = hunter_action_t<Base>;

    template <typename... Args>
    hunter_action_t( Args&&... args ) : ab( std::forward<Args>( args )... ) {}

    hunter_t* p()
    { return debug_cast<hunter_t*>( this->player ); }

    const hunter_t* p() const
    { return debug_cast<const hunter_t*>( this->player ); }
};

using hunter_melee_t  = hunter_action_t<melee_attack_t>;
using hunter_ranged_t = hunter_action_t<ranged_attack_t>;
using hunter_spell_t  = hunter_action_t<spell_t>;

// ================================================================================
// Hunter Ranged
// ================================================================================

struct hunter_ranged_attack_t : public hunter_ranged_t
{
  hunter_ranged_attack_t( util::string_view name, player_t* player )
    : hunter_ranged_t( name, player, spell_data_t::nil() )
  {
    weapon            = &player->equipped_weapons[ SLOT_RANGED ];
    weapon_multiplier = 1.0;
    base_execute_time = weapon->swing_time;
    school            = SCHOOL_PHYSICAL;
    trigger_gcd       = 0_ms;

    background        = true;
    repeating         = true;
    may_crit          = true;
    special           = false;
  }
};

struct hunter_auto_shot_t : public hunter_ranged_t
{
  hunter_auto_shot_t( hunter_t* p, util::string_view options_str )
    : hunter_ranged_t( "auto_shot", p, p->find_spell( 75 ) )
  {
    parse_options( options_str );

    trigger_gcd           = 0_ms;
    ignore_false_positive = true;

    assert( p->equipped_weapons[ SLOT_RANGED ].type != WEAPON_NONE );

    p->ranged_attack = new hunter_ranged_attack_t( "auto_shot", p );
  }

  // TODO: Copy midnight's immediate first shot

  void execute() override
  {
    player->ranged_attack->schedule_execute();
  }

  bool ready() override
  {
    // TODO: Copied from auto attacks and not correct for hunter.
    // Figure out the auto shot timers around movement. Roughly seemed like you could move until about 0.5s before
    // the next shot to avoid interruption
    if ( player->is_moving() )
    {
      return false;
    }

    return player->ranged_attack->execute_event == nullptr;
  }
};

void hunter_t::init_base_stats()
{
  base.attack_power_per_strength = 1.0;
  base.attack_power_per_agility  = 1.0;

  base.ranged_attack_power_per_agility = 2.0;

  player_t::init_base_stats();

  base.attack_crit_chance = 0;

  base.stats.attack_power        = 2.0 * level() - 20.0;
  base.stats.ranged_attack_power = 2.0 * level() - 20.0;
}

action_t* hunter_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "auto_shot" ) return new hunter_auto_shot_t( this, options_str );

  return player_t::create_action( name, options_str );
}

void hunter_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    player_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  hunter_apl::hunter( this );
  use_default_action_list = true;
  player_t::init_action_list();
}

// HUNTER MODULE INTERFACE ==================================================

struct hunter_module_t: public module_t
{
  hunter_module_t(): module_t( HUNTER ) {}

  player_t* create_player( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) const override
  {
    auto  p = new hunter_t( sim, name, r );
    return p;
  }

  bool valid() const override { return true; }

  void register_hotfixes() const override {}

  void register_actor_initializers( sim_t* ) const override {}
};

} // UNNAMED NAMESPACE

const module_t* module_t::hunter()
{
  static hunter_module_t m;
  return &m;
}
