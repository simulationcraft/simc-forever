// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "action/parse_effects.hpp"
#include "class_modules/apl/rogue.hpp"

#include "simulationcraft.hpp"

namespace
{  // UNNAMED NAMESPACE

struct rogue_t;

struct rogue_td_t : public actor_target_data_t
{
  // Combo points belong to the target they were generated on
  int combo_points = 0;

  rogue_td_t( player_t& target, rogue_t& p );
};

struct rogue_t final : public parse_player_effects_t
{
  rogue_t( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) : parse_player_effects_t( sim, ROGUE, name, r )
  {
  }

  resource_e primary_resource() const override
  {
    return RESOURCE_ENERGY;
  }

  void init_base_stats() override;
  void create_buffs() override;
  void init_spells() override;

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void init_action_list() override;

  void parse_action_effects( action_t* );

  std::string default_potion() const override
  {
    return rogue_apl::potion( this );
  }
  std::string default_flask() const override
  {
    return rogue_apl::flask( this );
  }
  std::string default_food() const override
  {
    return rogue_apl::food( this );
  }

  static constexpr int max_combo_points = 5;
  // Target that currently holds the player's combo points
  player_t* combo_target = nullptr;

  int combo_points( player_t* t ) const;
  void gain_combo_points( player_t* t, int amount, gain_t* g );
  void spend_combo_points( player_t* t, gain_t* g );

  target_specific_t<rogue_td_t> target_data;
  rogue_td_t* get_target_data( player_t* target ) const override
  {
    assert( target );
    rogue_td_t*& td = target_data[ target ];
    if ( !td )
      td = new rogue_td_t( *target, const_cast<rogue_t&>( *this ) );

    return td;
  }

  struct talents_t
  {
    // Assassination
    // Row 1
    player_talent_t improved_gouge;
    player_talent_t remorseless_attacks;
    player_talent_t malice;
    // Row 2
    player_talent_t ruthlessness;
    player_talent_t improved_expose_armor;
    player_talent_t improved_slice_and_dice;
    // Row 3
    player_talent_t vigor;
    player_talent_t blood_spatter;
    player_talent_t lethality;
    // Row 4
    player_talent_t vile_poisons;
    player_talent_t cold_blood;
    player_talent_t improved_poisons;
    // Row 5
    player_talent_t relentless_strikes;
    player_talent_t venom;
    player_talent_t improved_kidney_shot;
    // Row 6
    player_talent_t seal_fate;
    // Row 7
    player_talent_t mutilate;

    // Combat
    // Row 1
    player_talent_t coup_de_grace;
    player_talent_t efficient_strikes;
    player_talent_t lightning_reflexes;
    // Row 2
    player_talent_t puncturing_wounds;
    player_talent_t deflection;
    player_talent_t precision;
    player_talent_t improved_sprint;
    // Row 3
    player_talent_t endurance;
    player_talent_t riposte;
    player_talent_t composure;
    // Row 4
    player_talent_t improved_kick;
    player_talent_t flawless_execution;
    player_talent_t weapon_expertise;
    player_talent_t aggression;
    // Row 5
    player_talent_t dual_wield_specialization;
    player_talent_t blade_flurry;
    player_talent_t hack_and_slash;
    // Row 6
    player_talent_t flash_of_steel;
    // Row 7
    player_talent_t adrenaline_rush;

    // Subtlety
    // Row 1
    player_talent_t setup;
    player_talent_t master_of_deception;
    player_talent_t opportunity;
    // Row 2
    player_talent_t dirty_deeds;
    player_talent_t camouflage;
    player_talent_t improved_ambush;
    player_talent_t dirty_tricks;
    // Row 3
    player_talent_t initiative;
    player_talent_t premeditation;
    player_talent_t serrated_blades;
    // Row 4
    player_talent_t elusiveness;
    player_talent_t ghostly_strike;
    player_talent_t quick_getaway;
    // Row 5
    player_talent_t heightened_senses;
    player_talent_t preparation;
    player_talent_t hemorrhage;
    // Row 6
    player_talent_t boot_knife;
    // Row 7
    player_talent_t death_mark;
  } talent;

  struct buffs_t
  {
    buff_t* slice_and_dice = nullptr;
  } buff;

  struct
  {
    spell_ranks_t sinister_strike;
    spell_ranks_t backstab;
    spell_ranks_t eviscerate;
    spell_ranks_t slice_and_dice;
  } rank;
};

rogue_td_t::rogue_td_t( player_t& target, rogue_t& p ) : actor_target_data_t( &target, &p )
{
}

template <class Base>
struct rogue_action_t : public parse_action_effects_t<Base>
{
  template <typename... Args>
  rogue_action_t( Args&&... args ) : ab( std::forward<Args>( args )... )
  {
    if ( ab::data().ok() )
      p()->parse_action_effects( this );
  }

  rogue_t* p()
  {
    return debug_cast<rogue_t*>( this->player );
  }

  const rogue_t* p() const
  {
    return debug_cast<const rogue_t*>( this->player );
  }

  double gain_energize_resource( resource_e resource_type, double amount, gain_t* g ) override
  {
    if ( resource_type == RESOURCE_COMBO_POINT )
    {
      this->p()->gain_combo_points( this->target, static_cast<int>( amount ), g );
    }

    return ab::gain_energize_resource( resource_type, amount, g );
  }

  std::unique_ptr<expr_t> create_expression( util::string_view name ) override
  {
    if ( name == "combo_points" )
      return make_fn_expr( name, [ this ]() { return this->p()->combo_points( this->target ); } );

    return ab::create_expression( name );
  }

  // TODO: Verify if we need to put the Energy refund back here

protected:
  /// typedef for rogue_action_t<action_base_t>
  using base_t = rogue_action_t<Base>;

private:
  /// typedef for the templated action type, eg. spell_t, attack_t, heal_t
  using ab = parse_action_effects_t<Base>;
};

using rogue_attack_t = rogue_action_t<melee_attack_t>;
using rogue_spell_t  = rogue_action_t<spell_t>;

struct rogue_combo_attack_t : public rogue_attack_t
{
  rogue_combo_attack_t( std::string_view n, rogue_t* player, const spell_data_t* s ) : rogue_attack_t( n, player, s )
  {
  }

  bool ready() override
  {
    if ( p()->combo_points( target ) < 1 )
      return false;

    return rogue_attack_t::ready();
  }

  void execute() override
  {
    combo_points = p()->combo_points( target );

    rogue_attack_t::execute();
  }

  void consume_resource() override
  {
    if ( this->data().flags( SX_DISCOUNT_ON_MISS ) && this->result_is_miss( execute_state->result ) )
    {
      resource_e cr = current_resource();

      last_resource_cost = cost();

      last_resource_cost *= .2;

      player->resource_loss( cr, last_resource_cost, nullptr, this );

      sim->print_log( "{} consumes {} {} for {} ({})", *player, last_resource_cost, cr, *this,
                      player->resources.current[ cr ] );

      stats->consume_resource( cr, last_resource_cost );
    }
    else
    {
      rogue_attack_t::consume_resource();

      p()->spend_combo_points( target, gain );
    }
  }

protected:
  int combo_points = 0;
};

// Abilities

struct rogue_auto_attack_t : public auto_attack_t
{
  rogue_auto_attack_t( rogue_t* p, util::string_view options_str ) : auto_attack_t( options_str, p )
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
    : rogue_attack_t( "sinister_strike", player, player->rank.sinister_strike.find_rank()->spell() )
  {
    parse_options( options_str );
  }
};

struct backstab_t : public rogue_attack_t
{
  backstab_t( rogue_t* player, util::string_view options_str )
    : rogue_attack_t( "backstab", player, player->rank.backstab.find_rank()->spell() )
  {
    parse_options( options_str );
  }

  bool ready() override
  {
    return base_t::ready() && player->position() == POSITION_BACK;
  }
};

struct eviscerate_t : public rogue_combo_attack_t
{
  eviscerate_t( rogue_t* player, util::string_view options_str )
    : rogue_combo_attack_t( "eviscerate", player, player->rank.eviscerate.find_rank()->spell() )
  {
    parse_options( options_str );

    amount_delta           = 0.0;  // combo point damage is flat so we handle spread in base_da_min/max
    spell_power_mod.direct = 0.0;  // nullify the 1 from spell data
  }

  double cp_bonus() const
  {
    return combo_points * data().effectN( 1 ).pp_combo_points();
  }

  // TODO Forever: Verify the 3% per combo point. Looks right from low level logs
  double attack_direct_power_coefficient( const action_state_t* ) const override
  {
    return 0.03 * combo_points;
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

struct slice_and_dice_t : public rogue_combo_attack_t
{
  slice_and_dice_t( rogue_t* player, util::string_view options_str )
    : rogue_combo_attack_t( "slice_and_dice", player, player->rank.slice_and_dice.find_rank()->spell() )
  {
    parse_options( options_str );
    harmful = false;
  }

  timespan_t get_triggered_duration( const int cp )
  {
    return ( timespan_t::from_seconds( cp * 3 ) + p()->buff.slice_and_dice->data().duration() ) *
           ( 1.0 + p()->talent.improved_slice_and_dice->effectN( 1 ).percent() );
  }

  void execute() override
  {
    rogue_combo_attack_t::execute();

    p()->buff.slice_and_dice->trigger( get_triggered_duration( combo_points ) );
  }
};

void rogue_t::init_base_stats()
{
  base.attack_power_per_strength = 1.0;
  base.attack_power_per_agility  = 1.0;

  parse_player_effects_t::init_base_stats();

  // TODO - can this base stuff be moved into core?

  base.all_crit          = 0.0;
  base.spell_crit_chance = 0.0;

  base.dodge_per_agility = 2.0 * base.attack_crit_per_agility;
  auto base_dodge =
      ( dbc->race_base( race ).agility + dbc->attribute_base( type, level() ).agility ) * base.dodge_per_agility;

  base.attack_crit_chance = get_passive_player_value( 0.0, "attack_crit" );
  base.parry              = get_passive_player_value( 0.0, "parry" );
  base.dodge              = get_passive_player_value( base_dodge, "dodge" );

  base.stats.attack_power = 2.0 * level() - 20.0;
}

void rogue_t::create_buffs()
{
  buff.slice_and_dice = make_buff( this, "slice_and_dice", rank.slice_and_dice.find_rank()->spell() )
                            ->set_constant_behavior( buff_constant_behavior::NEVER_CONSTANT );

  parse_effects( buff.slice_and_dice );

  parse_player_effects_t::create_buffs();
}

void rogue_t::init_spells()
{
  parse_player_effects_t::init_spells();

  rank.sinister_strike = {
      this,
      spell_container_t().set_spell( 1752 ),
      spell_container_t().set_spell( 1757 ),
      spell_container_t().set_spell( 1758 ),
      spell_container_t().set_spell( 1759 ),
      spell_container_t().set_spell( 1760 ),
      spell_container_t().set_spell( 8621 ),
      spell_container_t().set_spell( 11293 ),
      spell_container_t().set_spell( 11294 ),
  };

  rank.eviscerate = {
      this,
      spell_container_t().set_spell( 2098 ),
      spell_container_t().set_spell( 6760 ),
      spell_container_t().set_spell( 6761 ),
      spell_container_t().set_spell( 6762 ),
      spell_container_t().set_spell( 8623 ),
      spell_container_t().set_spell( 8624 ),
      spell_container_t().set_spell( 11299 ),
      spell_container_t().set_spell( 11300 ),
      spell_container_t().set_spell( 31016 ),
  };

  rank.backstab = {
      this,
      spell_container_t().set_spell( 53 ),
      spell_container_t().set_spell( 2589 ),
      spell_container_t().set_spell( 2590 ),
      spell_container_t().set_spell( 2591 ),
      spell_container_t().set_spell( 8721 ),
      spell_container_t().set_spell( 11279 ),
      spell_container_t().set_spell( 11280 ),
      spell_container_t().set_spell( 11281 ),
      spell_container_t().set_spell( 25300 ),
  };

  rank.slice_and_dice = { this, spell_container_t().set_spell( 5171 ), spell_container_t().set_spell( 6774 ) };

  parse_all_class_passives();
  parse_all_passive_talents();

  // Assassination
  talent.improved_gouge          = find_talent_spell( talent_tree::CLASS, "Improved Gouge" );
  talent.remorseless_attacks     = find_talent_spell( talent_tree::CLASS, "Remorseless Attacks" );
  talent.malice                  = find_talent_spell( talent_tree::CLASS, "Malice" );
  talent.ruthlessness            = find_talent_spell( talent_tree::CLASS, "Ruthlessness" );
  talent.improved_expose_armor   = find_talent_spell( talent_tree::CLASS, "Improved Expose Armor" );
  talent.improved_slice_and_dice = find_talent_spell( talent_tree::CLASS, "Improved Slice and Dice" );
  talent.vigor                   = find_talent_spell( talent_tree::CLASS, "Vigor" );
  talent.blood_spatter           = find_talent_spell( talent_tree::CLASS, "Blood Spatter" );
  talent.lethality               = find_talent_spell( talent_tree::CLASS, "Lethality" );
  talent.vile_poisons            = find_talent_spell( talent_tree::CLASS, "Vile Poisons" );
  talent.cold_blood              = find_talent_spell( talent_tree::CLASS, "Cold Blood" );
  talent.improved_poisons        = find_talent_spell( talent_tree::CLASS, "Improved Poisons" );
  talent.relentless_strikes      = find_talent_spell( talent_tree::CLASS, "Relentless Strikes" );
  talent.venom                   = find_talent_spell( talent_tree::CLASS, "Venom" );
  talent.improved_kidney_shot    = find_talent_spell( talent_tree::CLASS, "Improved Kidney Shot" );
  talent.seal_fate               = find_talent_spell( talent_tree::CLASS, "Seal Fate" );
  talent.mutilate                = find_talent_spell( talent_tree::CLASS, "Mutilate" );

  // Combat
  talent.coup_de_grace             = find_talent_spell( talent_tree::CLASS, "Coup de Grace" );
  talent.efficient_strikes         = find_talent_spell( talent_tree::CLASS, "Efficient Strikes" );
  talent.lightning_reflexes        = find_talent_spell( talent_tree::CLASS, "Lightning Reflexes" );
  talent.puncturing_wounds         = find_talent_spell( talent_tree::CLASS, "Puncturing Wounds" );
  talent.deflection                = find_talent_spell( talent_tree::CLASS, "Deflection" );
  talent.precision                 = find_talent_spell( talent_tree::CLASS, "Precision" );
  talent.improved_sprint           = find_talent_spell( talent_tree::CLASS, "Improved Sprint" );
  talent.endurance                 = find_talent_spell( talent_tree::CLASS, "Endurance" );
  talent.riposte                   = find_talent_spell( talent_tree::CLASS, "Riposte" );
  talent.composure                 = find_talent_spell( talent_tree::CLASS, "Composure" );
  talent.improved_kick             = find_talent_spell( talent_tree::CLASS, "Improved Kick" );
  talent.flawless_execution        = find_talent_spell( talent_tree::CLASS, "Flawless Execution" );
  talent.weapon_expertise          = find_talent_spell( talent_tree::CLASS, "Weapon Expertise" );
  talent.aggression                = find_talent_spell( talent_tree::CLASS, "Aggression" );
  talent.dual_wield_specialization = find_talent_spell( talent_tree::CLASS, "Dual Wield Specialization" );
  talent.blade_flurry              = find_talent_spell( talent_tree::CLASS, "Blade Flurry" );
  talent.hack_and_slash            = find_talent_spell( talent_tree::CLASS, "Hack and Slash" );
  talent.flash_of_steel            = find_talent_spell( talent_tree::CLASS, "Flash of Steel" );
  talent.adrenaline_rush           = find_talent_spell( talent_tree::CLASS, "Adrenaline Rush" );

  // Subtlety
  talent.setup               = find_talent_spell( talent_tree::CLASS, "Setup" );
  talent.master_of_deception = find_talent_spell( talent_tree::CLASS, "Master of Deception" );
  talent.opportunity         = find_talent_spell( talent_tree::CLASS, "Opportunity" );
  talent.dirty_deeds         = find_talent_spell( talent_tree::CLASS, "Dirty Deeds" );
  talent.camouflage          = find_talent_spell( talent_tree::CLASS, "Camouflage" );
  talent.improved_ambush     = find_talent_spell( talent_tree::CLASS, "Improved Ambush" );
  talent.dirty_tricks        = find_talent_spell( talent_tree::CLASS, "Dirty Tricks" );
  talent.initiative          = find_talent_spell( talent_tree::CLASS, "Initiative" );
  talent.premeditation       = find_talent_spell( talent_tree::CLASS, "Premeditation" );
  talent.serrated_blades     = find_talent_spell( talent_tree::CLASS, "Serrated Blades" );
  talent.elusiveness         = find_talent_spell( talent_tree::CLASS, "Elusiveness" );
  talent.ghostly_strike      = find_talent_spell( talent_tree::CLASS, "Ghostly Strike" );
  talent.quick_getaway       = find_talent_spell( talent_tree::CLASS, "Quick Getaway" );
  talent.heightened_senses   = find_talent_spell( talent_tree::CLASS, "Heightened Senses" );
  talent.preparation         = find_talent_spell( talent_tree::CLASS, "Preparation" );
  talent.hemorrhage          = find_talent_spell( talent_tree::CLASS, "Hemorrhage" );
  talent.boot_knife          = find_talent_spell( talent_tree::CLASS, "Boot Knife" );
  talent.death_mark          = find_talent_spell( talent_tree::CLASS, "Death Mark" );

  // Improved Slice and Dice applies to the total duration, not the base duration
  deregister_passive_spell( talent.improved_slice_and_dice );
}

action_t* rogue_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "auto_attack" )
    return new rogue_auto_attack_t( this, options_str );

  // builders
  if ( name == "sinister_strike" )
    return new sinister_strike_t( this, options_str );
  if ( name == "backstab" )
    return new backstab_t( this, options_str );

  // finishers
  if ( name == "eviscerate" )
    return new eviscerate_t( this, options_str );
  if ( name == "slice_and_dice" )
    return new slice_and_dice_t( this, options_str );

  return parse_player_effects_t::create_action( name, options_str );
}

void rogue_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    parse_player_effects_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  rogue_apl::rogue( this );
  use_default_action_list = true;
  parse_player_effects_t::init_action_list();
}

void rogue_t::parse_action_effects( action_t* action )
{
  auto _a = dynamic_cast<parse_action_base_t*>( action );
  assert( _a );

  // TODO: parse action-affecting rogue buffs here as they are implemented.
}

int rogue_t::combo_points( player_t* t ) const
{
  return t ? get_target_data( t )->combo_points : 0;
}

void rogue_t::gain_combo_points( player_t* t, int amount, gain_t* )
{
  if ( combo_target && combo_target != t )
  {
    auto old_td = get_target_data( combo_target );
    sim->print_log( "{} loses {} combo_points on {}", *this, old_td->combo_points, *combo_target );
    resource_loss( RESOURCE_COMBO_POINT, old_td->combo_points, get_gain( "combo_points_lost" ) );
    old_td->combo_points = 0;
  }

  combo_target = t;

  auto td          = get_target_data( t );
  td->combo_points = std::min( td->combo_points + amount, max_combo_points );
}

void rogue_t::spend_combo_points( player_t* t, gain_t* g )
{
  auto td = get_target_data( t );

  sim->print_log( "{} spends {} combo_points on {}", *this, td->combo_points, *t );

  resource_loss( RESOURCE_COMBO_POINT, td->combo_points, g );
  td->combo_points = 0;
}

// ROGUE MODULE INTERFACE ===================================================

class rogue_module_t : public module_t
{
public:
  rogue_module_t() : module_t( ROGUE )
  {
  }

  player_t* create_player( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) const override
  {
    return new rogue_t( sim, name, r );
  }

  bool valid() const override
  {
    return true;
  }

  void register_hotfixes() const override
  {
  }

  void register_actor_initializers( sim_t* ) const override
  {
  }
};

}  // UNNAMED NAMESPACE

const module_t* module_t::rogue()
{
  static rogue_module_t m;
  return &m;
}
