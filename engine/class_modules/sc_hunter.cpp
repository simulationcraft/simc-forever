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
  void create_buffs() override;

  struct buffs_t
  {
    buff_t* aspect_of_the_hawk = nullptr;
  } buff;
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
// Hunter Buffs
// ================================================================================

constexpr std::array aspect_of_the_hawk_ranks {
  spell_container_t().set_spell( 13165 ),
  spell_container_t().set_spell( 14318 ),
  spell_container_t().set_spell( 14319 ),
  spell_container_t().set_spell( 14320 ),
  spell_container_t().set_spell( 14321 ),
  spell_container_t().set_spell( 14322 ),
  spell_container_t().set_spell( 25296 ),
};
struct aspect_of_the_hawk_t : public hunter_spell_t
{
  aspect_of_the_hawk_t( hunter_t* p, util::string_view options_str ) :
    hunter_spell_t( "aspect_of_the_hawk", p, p->find_rank( aspect_of_the_hawk_ranks )->spell( p ) )
  {
    parse_options( options_str );

    harmful  = false;
    may_miss = false;

    target   = p;
  }

  void execute() override
  {
    hunter_spell_t::execute();

    p()->buff.aspect_of_the_hawk->trigger();
  }
};


// ================================================================================
// Hunter Ranged
// ================================================================================

struct hunter_ranged_attack_t : public hunter_ranged_t
{
  bool first = true;

  hunter_ranged_attack_t( util::string_view name, player_t* player )
    : hunter_ranged_t( name, player, player->find_spell( 75 ) )
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

  void reset() override
  {
    hunter_ranged_t::reset();
    first = true;
  }

  timespan_t execute_time() const override
  {
    if ( !player->in_combat )
      return 10_ms;
    if ( first )
      return 100_ms;
    return hunter_ranged_t::execute_time();
  }

  void execute() override
  {
    first = false;
    hunter_ranged_t::execute();
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


constexpr std::array arcane_shot_ranks {
  spell_container_t().set_spell(  3044 ),
  spell_container_t().set_spell( 14281 ),
  spell_container_t().set_spell( 14282 ),
  spell_container_t().set_spell( 14283 ),
  spell_container_t().set_spell( 14284 ),
  spell_container_t().set_spell( 14285 ),
  spell_container_t().set_spell( 14286 ),
  spell_container_t().set_spell( 14287 ),
};
struct arcane_shot_t : public hunter_ranged_t
{
  arcane_shot_t( hunter_t* p, util::string_view options_str )
    : hunter_ranged_t( "arcane_shot", p, p->find_rank( arcane_shot_ranks )->spell( p ) )
  {
    parse_options( options_str );

    // TODO Forever: Verify at ranks higher than 1. Only tested at low levels
    attack_power_mod.direct = 0.10;
  }
};

constexpr std::array serpent_sting_ranks {
  spell_container_t().set_spell(  1978 ),
  spell_container_t().set_spell( 13549 ),
  spell_container_t().set_spell( 13550 ),
  spell_container_t().set_spell( 13551 ),
  spell_container_t().set_spell( 13552 ),
  spell_container_t().set_spell( 13553 ),
  spell_container_t().set_spell( 13554 ),
  spell_container_t().set_spell( 13555 ),
  spell_container_t().set_spell( 25295 ),
};
struct serpent_sting_t : public hunter_ranged_t
{
  serpent_sting_t( hunter_t* p, util::string_view options_str )
    : hunter_ranged_t( "serpent_sting", p, p->find_rank( serpent_sting_ranks )->spell( p ) )
  {
    parse_options( options_str );

    // TODO Forever: Verify with ranks higher than 1. Only tested at low levels
    attack_power_mod.tick = 0.03;
  }
};

void hunter_t::init_base_stats()
{
  if ( base.distance < 1 )
  {
    base.distance = find_spell( 75 )->max_range();

    // TODO: Set melee distance for survival hunter
  }
  base.attack_power_per_strength = 1.0;
  base.attack_power_per_agility  = 1.0;

  base.ranged_attack_power_per_agility = 2.0;

  player_t::init_base_stats();

  base.attack_crit_chance = 0;
  // TODO Forever: Only checked on low level characters
  base.spell_crit_chance = 0.036;

  base.stats.attack_power        = 2.0 * level() - 20.0;
  base.stats.ranged_attack_power = 2.0 * level() - 20.0;

  // TODO Forever: Dodge, parry chance
}

action_t* hunter_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "auto_shot" )          return new hunter_auto_shot_t( this, options_str );
  if ( name == "arcane_shot" )        return new arcane_shot_t( this, options_str );
  if ( name == "serpent_sting" )      return new serpent_sting_t( this, options_str );
  if ( name == "aspect_of_the_hawk" ) return new aspect_of_the_hawk_t( this, options_str );

  return player_t::create_action( name, options_str );
}

void hunter_t::create_buffs()
{
  buff.aspect_of_the_hawk = make_buff<stat_buff_t>( this, "aspect_of_the_hawk", find_rank( aspect_of_the_hawk_ranks )->spell( this ) );

  player_t::create_buffs();
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
