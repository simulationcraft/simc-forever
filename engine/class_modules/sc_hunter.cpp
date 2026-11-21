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

struct hunter_t final : public parse_player_effects_t
{
  using base_t = parse_player_effects_t;
  
  hunter_t( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) :
    base_t( sim, HUNTER, name, r ) {}

  resource_e primary_resource() const override { return RESOURCE_MANA; }

  void init_spells() override;
  void init_base_stats() override;
  void init_action_list() override;
  void init_finished() override;

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void create_buffs() override;

  void parse_player_effects() {}
  void apply_action_effects( action_t* ) {}

  struct buffs_t
  {
    buff_t* aspect_of_the_hawk = nullptr;
  } buff;

  struct
  {
    spell_ranks_t aspect_of_the_hawk;
    spell_ranks_t arcane_shot;
    spell_ranks_t serpent_sting;
  } rank;
};

template <class Base>
struct hunter_action_t : public parse_action_effects_t<Base>
{
  private:
    using ab = parse_action_effects_t<Base>;

  public:
    using base_t = hunter_action_t<Base>;

    template <typename... Args>
    hunter_action_t( Args&&... args ) : ab( std::forward<Args>( args )... ) {
      if ( ab::data().ok() )
        p()->apply_action_effects( this );
    }

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
struct aspect_of_the_hawk_t : public hunter_spell_t
{
  aspect_of_the_hawk_t( hunter_t* player, util::string_view options_str ) :
    hunter_spell_t( "aspect_of_the_hawk", player, player->rank.aspect_of_the_hawk.find_rank()->spell() )
  {
    parse_options( options_str );

    harmful  = false;
    may_miss = false;

    target   = player;
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

  hunter_ranged_attack_t( hunter_t* player, util::string_view name )
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
  hunter_auto_shot_t( hunter_t* player, util::string_view options_str )
    : hunter_ranged_t( "auto_shot", player, player->find_spell( 75 ) )
  {
    parse_options( options_str );

    trigger_gcd           = 0_ms;
    ignore_false_positive = true;

    assert( player->equipped_weapons[ SLOT_RANGED ].type != WEAPON_NONE );

    player->ranged_attack = new hunter_ranged_attack_t( player, "auto_shot" );
  }

  void execute() override
  {
    p()->ranged_attack->schedule_execute();
  }

  bool ready() override
  {
    // TODO: Copied from auto attacks and not correct for hunter.
    // Figure out the auto shot timers around movement. Roughly seemed like you could move until about 0.5s before
    // the next shot to avoid interruption
    if ( p()->is_moving() )
    {
      return false;
    }

    return p()->ranged_attack->execute_event == nullptr;
  }
};

struct arcane_shot_t : public hunter_ranged_t
{
  arcane_shot_t( hunter_t* player, util::string_view options_str )
    : hunter_ranged_t( "arcane_shot", player, player->rank.arcane_shot.find_rank()->spell() )
  {
    parse_options( options_str );

    // TODO Forever: Verify at ranks higher than 1. Only tested at low levels
    attack_power_mod.direct = 0.10;
  }
};

struct serpent_sting_t : public hunter_ranged_t
{
  serpent_sting_t( hunter_t* player, util::string_view options_str )
    : hunter_ranged_t( "serpent_sting", player, player->rank.serpent_sting.find_rank()->spell() )
  {
    parse_options( options_str );

    // TODO Forever: Verify with ranks higher than 1. Only tested at low levels
    attack_power_mod.tick = 0.03;
  }
};

void hunter_t::init_spells()
{
  base_t::init_spells();

  parse_all_class_passives();
  parse_all_passive_talents();

  rank.aspect_of_the_hawk = { this,
    spell_container_t().set_spell( 13165 ),
    spell_container_t().set_spell( 14318 ),
    spell_container_t().set_spell( 14319 ),
    spell_container_t().set_spell( 14320 ),
    spell_container_t().set_spell( 14321 ),
    spell_container_t().set_spell( 14322 ),
    spell_container_t().set_spell( 25296 ),
  };
  rank.arcane_shot = { this,
    spell_container_t().set_spell(  3044 ),
    spell_container_t().set_spell( 14281 ),
    spell_container_t().set_spell( 14282 ),
    spell_container_t().set_spell( 14283 ),
    spell_container_t().set_spell( 14284 ),
    spell_container_t().set_spell( 14285 ),
    spell_container_t().set_spell( 14286 ),
    spell_container_t().set_spell( 14287 ),
  };
  rank.serpent_sting = { this,
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
}

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

  base_t::init_base_stats();

  base.attack_crit_chance = 0;
  // TODO Forever: Only checked on low level characters
  base.spell_crit_chance = 0.036;

  base.stats.attack_power        = 2.0 * level() - 20.0;
  base.stats.ranged_attack_power = 2.0 * level() - 20.0;

  // TODO Forever: Dodge, parry chance
}

void hunter_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    base_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  hunter_apl::hunter( this );
  use_default_action_list = true;
  base_t::init_action_list();
}

void hunter_t::init_finished()
{
  parse_player_effects();

  base_t::init_finished();
}

action_t* hunter_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "auto_shot" )          return new hunter_auto_shot_t( this, options_str );
  if ( name == "arcane_shot" )        return new arcane_shot_t( this, options_str );
  if ( name == "serpent_sting" )      return new serpent_sting_t( this, options_str );
  if ( name == "aspect_of_the_hawk" ) return new aspect_of_the_hawk_t( this, options_str );

  return base_t::create_action( name, options_str );
}

void hunter_t::create_buffs()
{
  buff.aspect_of_the_hawk = make_buff<stat_buff_t>( this, "aspect_of_the_hawk", rank.aspect_of_the_hawk.find_rank()->spell() );

  base_t::create_buffs();
}

// HUNTER MODULE INTERFACE ==================================================

struct hunter_module_t: public module_t
{
  hunter_module_t(): module_t( HUNTER ) {}

  player_t* create_player( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) const override
  {
    auto  player = new hunter_t( sim, name, r );
    return player;
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
