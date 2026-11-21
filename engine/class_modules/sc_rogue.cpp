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
  void init_spells() override;

  double current_cp() const
  {
    return resources.current[ RESOURCE_COMBO_POINT ];
  }

  struct
  {
    spell_ranks_t backstab;
    spell_ranks_t eviscerate;
    spell_ranks_t sinister_strike;
  } rank;
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

  bool consumes_combo_points() const
  {
    return ab::base_costs[ RESOURCE_COMBO_POINT ] > 0;
  }

  void spend_combo_points()
  {
    if ( !consumes_combo_points() )
      return;

    double cp_loss = p()->current_cp();

    ab::stats->consume_resource( RESOURCE_COMBO_POINT, cp_loss );

    p()->resource_loss( RESOURCE_COMBO_POINT, cp_loss, nullptr, this );

    p()->sim->print_log( "{} consumes {} {} for {} ({})", *p(), cp_loss, util::resource_type_string( RESOURCE_COMBO_POINT ),
                         *this, p()->current_cp() );
  }

  bool ready() override
  {
    if ( consumes_combo_points() )
      return ab::ready() && p()->current_cp() > 0;

    return ab::ready();
  }

  void consume_resource() override
  {
    resource_e cr = this->current_resource();

    auto discountOnMiss = this->data().flags( spell_attribute::SX_DISCOUNT_ON_MISS );

    // TODO: Move this into core? I basically copied this from warrior
    if (
        cr == RESOURCE_ENERGY
        && this->cost() > 0
        && discountOnMiss
        && !this->proc
        && !this->result_is_hit( this->execute_state->result ) )
    {
      this->last_resource_cost = 0.2 * this->cost();

      this->player->resource_loss( cr, this->last_resource_cost, nullptr, this );
      this->stats->consume_resource( cr, this->last_resource_cost );

      this->sim->print_log("{} consumes {} {} for {} ({})",
          *this->player, this->last_resource_cost, cr, *this, this->player->resources.current[ cr ] );
    }
    else
    {
      ab::consume_resource();

      spend_combo_points();
    }
  }
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

struct sinister_strike_t : public rogue_attack_t
{
  sinister_strike_t( rogue_t* player, util::string_view options_str )
    :  rogue_attack_t( "sinister_strike", player, player->rank.sinister_strike.find_rank()->spell() )
  {
    parse_options( options_str );
  }
};

struct backstab_t : public rogue_attack_t
{
  backstab_t( rogue_t* player, util::string_view options_str )
    :  rogue_attack_t( "backstab", player, player->rank.backstab.find_rank()->spell() )
  {
    parse_options( options_str );
  }

  bool ready() override
  {
    return base_t::ready() && player->position() == POSITION_BACK;
  }
};

struct eviscerate_t : public rogue_attack_t
{
  eviscerate_t( rogue_t* player, util::string_view options_str )
    :  rogue_attack_t( "eviscerate", player, player->rank.eviscerate.find_rank()->spell() )
  {
    parse_options( options_str );

    amount_delta           = 0.0; // combo point damage is flat so we handle spread in base_da_min/max
    spell_power_mod.direct = 0.0; // nullify the 1 from spell data
  }

  double cp_bonus() const
  {
    return p()->current_cp() * data().effectN( 1 ).pp_combo_points();
  }

  // TODO Forever: Verify the 3% per combo point. Looks right from low level logs
  double attack_direct_power_coefficient( const action_state_t* ) const override
  {
    return 0.03 * p()->current_cp();
  }

  // Eviscerate doesn't apply the base damage spread to the extra damage from combo points

  double base_da_min( const action_state_t* ) const override
  {
    const auto& e = data().effectN( 1 );
    return e.base_value() * ( 1 - e.m_delta() / 2 ) + cp_bonus();
  }

  double base_da_max( const action_state_t* ) const override
  {
    const auto& e = data().effectN( 1 );
    return e.base_value() * ( 1 + e.m_delta() / 2 ) + cp_bonus();
  }
};

void rogue_t::init_spells()
{
  player_t::init_spells();

  rank.backstab = { this,
    spell_container_t().set_spell(    53 ),
    spell_container_t().set_spell(  2589 ),
    spell_container_t().set_spell(  2590 ),
    spell_container_t().set_spell(  2591 ),
    spell_container_t().set_spell(  8721 ),
    spell_container_t().set_spell( 11279 ),
    spell_container_t().set_spell( 11280 ),
    spell_container_t().set_spell( 11281 ),
    spell_container_t().set_spell( 25300 ),
  };

  rank.eviscerate = { this,
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

  rank.sinister_strike = { this,
    spell_container_t().set_spell(  1752 ),
    spell_container_t().set_spell(  1757 ),
    spell_container_t().set_spell(  1758 ),
    spell_container_t().set_spell(  1759 ),
    spell_container_t().set_spell(  1760 ),
    spell_container_t().set_spell(  8621 ),
    spell_container_t().set_spell( 11293 ),
    spell_container_t().set_spell( 11294 ),
  };
}

action_t* rogue_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "auto_attack" ) return new rogue_auto_attack_t( this, options_str );

  // builders
  if ( name == "sinister_strike" ) return new sinister_strike_t( this, options_str );
  if ( name == "backstab" ) return new backstab_t( this, options_str );

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
