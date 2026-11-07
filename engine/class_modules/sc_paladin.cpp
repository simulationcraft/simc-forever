// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================
/*
  TODO: reimplement Holy if anyone ever becomes interested in maintaining it
*/
#include "simulationcraft.hpp"
#include "action/dbc_proc_callback.hpp"
#include "action/parse_effects.hpp"
#include "item/special_effect.hpp"
#include "class_modules/apl/paladin.hpp"
#include <algorithm>

// ==========================================================================
// Paladin
// ==========================================================================
namespace paladin
{
struct paladin_t;

struct paladin_td_t final : public actor_target_data_t
{
  struct debuffs_t
  {
    buff_t* consecration              = nullptr;
    buff_t* vindication               = nullptr;
    buff_t* judgement_of_light        = nullptr;
    buff_t* judgement_of_wisdom       = nullptr;
    buff_t* judgement_of_the_crusader = nullptr;
    buff_t* judgement_of_justice      = nullptr; // NYI
    buff_t* judgement_of_fury         = nullptr; // NYI - Taunt
  } debuffs;

  paladin_td_t( player_t* target, paladin_t& p );
};

// utility to create target_effect_t compatible functions from paladin_td_t member references
template <typename T>
static std::function<int( actor_target_data_t* )> d_fn( T d )
{
  return [ d ]( actor_target_data_t* t ) { return std::invoke( d, static_cast<paladin_td_t*>( t )->debuffs )->check(); };
}

struct paladin_t final : public parse_player_effects_t
{
private:
  using base_t = parse_player_effects_t;

public:
  paladin_t( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) :
    parse_player_effects_t( sim, PALADIN, name, r ) {}

  
  resource_e primary_resource() const override
  {
    return RESOURCE_MANA;
  }

  void init_base_stats() override;
  void init_spells() override;
  void create_buffs() override;
  void parse_action_effects( action_t* );

  void init_special_effects() override;
  void create_actions() override;
  void activate() override;

  double composite_attribute_multiplier( attribute_e ) const override;

  paladin_td_t* get_target_data( player_t* target ) const override;
  paladin_td_t* find_target_data( player_t* target ) const;

  std::string default_potion() const override
  {
    return paladin_apl::potion( this );
  }
  std::string default_flask() const override
  {
    return paladin_apl::flask( this );
  }
  std::string default_food() const override
  {
    return paladin_apl::food( this );
  }

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void init_action_list() override;

  struct buffs_t
  {
  } buff;

  struct talents_t
  {
    player_talent_t spiritual_focus;
    player_talent_t divine_strength;
  } talent;

  struct spells_t
  {
  } spell;

  struct actives_t
  {
  } active;

  struct
  {
    spell_ranks_t blessing_of_might;
    spell_ranks_t blessing_of_wisdom;
    spell_ranks_t consecration;
    spell_ranks_t devotion_aura;
    spell_ranks_t exorcism;
    spell_ranks_t flash_of_light;
    spell_ranks_t hammer_of_justice;
    spell_ranks_t hammer_of_wrath;
    spell_ranks_t holy_light;
    spell_ranks_t holy_shield;
    spell_ranks_t holy_shock;
    spell_ranks_t holy_strike;
    spell_ranks_t holy_wrath;
    spell_ranks_t judgement;
    spell_ranks_t lay_on_hands;
    spell_ranks_t lights_vigil;
    spell_ranks_t retribution_aura;
    spell_ranks_t seal_of_command;
    spell_ranks_t seal_of_fury;
    spell_ranks_t seal_of_justice;
    spell_ranks_t seal_of_light;
    spell_ranks_t seal_of_righteousness;
    spell_ranks_t seal_of_the_crusader;
    spell_ranks_t seal_of_wisdom;
  } rank;

private:
  target_specific_t<paladin_td_t> _target_data;
};

// Paladin Actions
template <class Base>
struct paladin_action_t : public parse_action_effects_t<Base>
{
private:
  using ab = parse_action_effects_t<Base>;

public:
  using base_t = paladin_action_t<Base>;

  template <typename... Args>
  paladin_action_t( Args&&... args ) : ab( std::forward<Args>( args )... )
  {
  }

  paladin_t* p()
  {
    return debug_cast<paladin_t*>( this->player );
  }

  const paladin_t* p() const
  {
    return debug_cast<const paladin_t*>( this->player );
  }
};

using paladin_attack_t = paladin_action_t<melee_attack_t>;
using paladin_spell_t  = paladin_action_t<spell_t>;

struct paladin_melee_t : public paladin_attack_t
{
  paladin_melee_t( util::string_view name, player_t* player, weapon_t* w )
    : paladin_attack_t( name, player, spell_data_t::nil() )
  {
    weapon            = w;
    weapon_multiplier = 1.0;
    base_execute_time = w->swing_time;
    school            = SCHOOL_PHYSICAL;
    trigger_gcd       = 0_ms;

    background = true;
    repeating  = true;
    may_glance = true;
    may_crit   = true;
    may_dodge  = true;
    may_parry  = true;
    special    = false;
  }

  void impact( action_state_t* state ) override
  {
    paladin_attack_t::impact( state );
  }
};

struct paladin_auto_attack_t : public paladin_attack_t
{
  paladin_auto_attack_t( paladin_t* p, util::string_view options_str )
    : paladin_attack_t( "auto_attack", p, spell_data_t::nil() )
  {
    parse_options( options_str );
    trigger_gcd           = 0_ms;
    ignore_false_positive = true;

    assert( p->equipped_weapons[ SLOT_MAIN_HAND ].type != WEAPON_NONE );

    p->main_hand_attack = new paladin_melee_t( "melee", p, &p->equipped_weapons[ SLOT_MAIN_HAND ] );
  }

  void execute() override
  {
    player->main_hand_attack->schedule_execute();
  }

  bool ready() override
  {
    if ( player->is_moving() )
    {
      return false;
    }
    return player->main_hand_attack->execute_event == nullptr;
  }
};



void paladin_t::activate()
{
  base_t::activate();
}

void paladin_t::init_base_stats()
{
  base.attack_power_per_strength = 2.0;

  base_t::init_base_stats();

  base.stats.attack_power = 2.0 * level() - 20.0;

  // original classic crit rates according to
  // * https://github.com/wowsims/forever/blob/master/sim/core/base_stats_auto_gen.go#L57
  // TODO: Verify in-game
  base.attack_crit_chance = get_passive_player_value( 0.007, "all_crit" );
  base.spell_crit_chance  = get_passive_player_value( 0.035, "all_crit" );
}

action_t* paladin_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "auto_attack" )
    return new paladin_auto_attack_t( this, options_str );

  return base_t::create_action( name, options_str );
}

void paladin_t::create_buffs()
{
  base_t::create_buffs();
}

void paladin_t::create_actions()
{
  base_t::create_actions();
}

void paladin_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    base_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  paladin_apl::paladin( this );
  use_default_action_list = true;

  base_t::init_action_list();
}

// parse_effects does not parse party buffs with range TODO improve core to allow that?
double paladin_t::composite_attribute_multiplier( attribute_e attr ) const
{
  double m = base_t::composite_attribute_multiplier( attr );
  return m;
}

void paladin_t::init_spells()
{
  base_t::init_spells();
}

  void paladin_t::parse_action_effects( action_t* action )
{
  auto _a = dynamic_cast<parse_action_base_t*>( action );
  assert( _a );
}

paladin_td_t* paladin_t::get_target_data( player_t* target ) const
{
  paladin_td_t*& td = _target_data[ target ];
  if ( !td )
  {
    td = new paladin_td_t( target, const_cast<paladin_t&>( *this ) );
  }
  return td;
}

paladin_td_t* paladin_t::find_target_data( player_t* target ) const
{
  paladin_td_t*& td = _target_data[ target ];

  return td;
}

void paladin_t::init_special_effects()
{
  base_t::init_special_effects();
}

paladin_td_t::paladin_td_t( player_t* target, paladin_t& p ) : actor_target_data_t( target, &p )
{
}

// PALADIN MODULE INTERFACE =================================================

struct paladin_module_t : public module_t
{
  paladin_module_t() : module_t( PALADIN ) {}

  player_t* create_player( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) const override
  {
    auto p = new paladin_t( sim, name, r );
    return p;
  }

  bool valid() const override
  {
    return true;
  }

  void register_actor_initializers( sim_t* ) const override {}

  void register_hotfixes() const override {}
};
}  // end namespace paladin

const module_t* module_t::paladin()
{
  static paladin::paladin_module_t m;
  return &m;
}
