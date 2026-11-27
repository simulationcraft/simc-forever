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

// Order does not mean anything, just convenience.
enum paladin_seal_e
{
  SEAL_NONE = 0,
  SEAL_OF_RIGHTEOUSNESS,
  SEAL_OF_THE_CRUSADER,
  SEAL_OF_COMMAND,
  SEAL_OF_FURY,
  SEAL_OF_LIGHT,
  SEAL_OF_WISDOM,
  SEAL_OF_JUSTICE,
  SEAL_MAX
};

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
  block_result_e target_block_resolution( const action_state_t* ) const override;

  void init_special_effects() override;
  void create_actions() override;
  void assess_damage( school_e, result_amount_type, action_state_t* ) override;
  void activate() override;
  void reset() override;

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

  action_t* current_seal_judgement;
  buff_t* current_seal_buff;
  paladin_seal_e current_seal;
  paladin_seal_e previous_seal;

  struct buffs_t
  {
    buff_t* devotion_aura;
    buff_t* retribution_aura;

    absorb_buff_t* lights_fury;
    buff_t* redoubt;

    std::array<std::vector<buff_t*>, SEAL_MAX> seal_buffs;
    std::array<buff_t*, SEAL_MAX> seal_expression_buffs;
  } buff;

  struct talents_t
  {
    // Row 1 Holy
    player_talent_t spiritual_focus;
    player_talent_t divine_strength;

    // Row 1 Prot
    player_talent_t divine_intellect;
    player_talent_t redoubt;

    // Row 1 Ret
    player_talent_t parry;
    player_talent_t benediction;

    // Row 2 Holy
    player_talent_t healing_light;
    player_talent_t reverence;
    player_talent_t improved_seals;
    player_talent_t purifying_power;

    // Row 2 Prot
    player_talent_t precision;
    player_talent_t guardians_favor;
    player_talent_t sacred_duty;

    // Row 2 Ret
    player_talent_t improved_judgement;
    player_talent_t conviction;
    player_talent_t sanctified_judgement;

    // Row 3 Holy
    player_talent_t divine_favor;
    player_talent_t illumination;
    player_talent_t holy_conduit;

    // Row 3 Prot
    player_talent_t templars_bulwark;
    player_talent_t improved_hammer_of_justice;
    player_talent_t shield_specialization;
    player_talent_t toughness;

    // Row 3 Ret
    player_talent_t vindication;
    player_talent_t unyielding_faith;
    player_talent_t seal_of_command;
    player_talent_t pursuit_of_justice;

    // Row 4 Holy
    player_talent_t divine_precision;
    player_talent_t infusion_of_light;
    player_talent_t oath_of_light;

    // Row 4 Prot
    player_talent_t swift_judgement;
    player_talent_t improved_righteous_fury;
    player_talent_t anticipation;

    // Row 4 Ret
    player_talent_t eye_for_an_eye;
    player_talent_t sacred_arbiter;
    player_talent_t two_handed_weapon_specialization;

    // Row 5 Holy
    player_talent_t consecrated_ground;
    player_talent_t holy_shock;
    player_talent_t boundless_light;

    // Row 5 Prot
    player_talent_t stoicism;
    player_talent_t holy_shield;
    player_talent_t reckoning;

    // Row 5 Ret
    player_talent_t turn_evil;
    player_talent_t repentance;
    player_talent_t vengeance;

    // Row 6 Holy
    player_talent_t holy_power;

    // Row 6 Prot
    player_talent_t iron_creed;

    // Row 6 Ret
    player_talent_t champion_of_the_light;

    // Row 7 Holy
    player_talent_t lights_vigil;

    // Row 7 Prot
    player_talent_t shieldsworn;

    // Row 7 Ret
    player_talent_t unsealed_wrath;
  } talent;

  struct spells_t
  {
    const spell_data_t* lights_fury = spell_data_t::nil(); // Seal of Fury absorb
    const spell_data_t* consecration_debuff = spell_data_t::nil();
    const spell_data_t* redoubt_buff = spell_data_t::nil();
  } spell;

  struct actives_t
  {
    spell_t* retribution_aura = nullptr;
  } active;

  struct
  {
    spell_ranks_t blessing_of_might;
    spell_ranks_t blessing_of_wisdom;
    spell_ranks_t consecration_cast;
    spell_ranks_t consecration_damage;
    spell_ranks_t devotion_aura;
    spell_ranks_t exorcism;
    spell_ranks_t flash_of_light;
    spell_ranks_t hammer_of_justice;
    spell_ranks_t hammer_of_wrath;
    spell_ranks_t holy_light;
    spell_ranks_t holy_shield;
    spell_ranks_t holy_shock_cast;
    spell_ranks_t holy_shock_damage;
    spell_ranks_t holy_shock_heal;
    spell_ranks_t holy_strike;
    spell_ranks_t holy_wrath;
    spell_ranks_t lay_on_hands;
    spell_ranks_t lights_vigil_buff;
    spell_ranks_t lights_vigil_cast;
    spell_ranks_t lights_vigil_heal;
    spell_ranks_t retribution_aura;
    spell_ranks_t seal_of_command;
    spell_ranks_t seal_of_fury;
    spell_ranks_t seal_of_fury_damage;
    spell_ranks_t seal_of_light;
    spell_ranks_t seal_of_righteousness;
    spell_ranks_t seal_of_righteousness_damage;
    spell_ranks_t seal_of_the_crusader;
    spell_ranks_t seal_of_wisdom;
    spell_ranks_t seal_of_justice;
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
  paladin_action_t( Args&&... args )
    : ab( std::forward<Args>( args )... )
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

  static bool is_undead_or_demon( const player_t* target )
  {
    return target->race == RACE_UNDEAD || target->race == RACE_DEMON;
  }

  static target_filter_callback_t undead_and_demons_only()
  {
    return [ & ]( const action_t* action, player_t* target ) {
      return is_undead_or_demon( target );
    };
  }
};

using paladin_absorb_t = paladin_action_t<absorb_t>;
using paladin_attack_t = paladin_action_t<melee_attack_t>;
using paladin_heal_t = paladin_action_t<heal_t>;
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


struct exorcism_t : public paladin_spell_t
{
  exorcism_t( paladin_t* player, util::string_view options_str )
    : paladin_spell_t( "exorcism", player, player->rank.exorcism.find_rank()->spell() )
  {
    parse_options( options_str );

    target_filter_callback = undead_and_demons_only();
  }

  bool target_ready( player_t* t )
  {
    if ( !is_undead_or_demon( t ) )
      return false;

    return paladin_spell_t::target_ready( t );
  }
};

struct holy_wrath_t : public paladin_spell_t
{
  timespan_t stun_duration;
  holy_wrath_t( paladin_t* player, util::string_view options_str )
    : paladin_spell_t( "holy_wrath", player, player->rank.holy_wrath.find_rank()->spell() ),
      stun_duration( player->find_spell( 429147 )->duration() )
  {
    parse_options( options_str );

    aoe = -1;

    target_filter_callback = undead_and_demons_only();
  }

  void impact( action_state_t* s ) override
  {
    base_t::impact( s );

    if ( result_is_hit( s->result ) )
    {
      s->target->buffs.stunned->trigger( stun_duration );
      s->target->stun();
    }
  }

  bool target_ready( player_t* t )
  {
    if ( !is_undead_or_demon( t ) )
      return false;

    return paladin_spell_t::target_ready( t );
  }
};

struct hammer_of_justice_t : public paladin_spell_t
{
  hammer_of_justice_t( paladin_t* player, util::string_view options_str )
    : paladin_spell_t( "hammer_of_justice", player, player->rank.hammer_of_justice.find_rank()->spell() )
  {
    parse_options( options_str );
  }

  void impact( action_state_t* s ) override
  {
    base_t::impact( s );
    if ( result_is_hit( s->result ) )
    {
      s->target->buffs.stunned->trigger( data().duration() );
      s->target->stun();
    }
  }
};

struct holy_light_t : public paladin_heal_t
{
  holy_light_t( paladin_t* player, util::string_view options_str )
    : paladin_heal_t( "holy_light", player, player->rank.holy_light.find_rank()->spell() )
  {
    parse_options( options_str );
  }
};

struct holy_strike_t : public paladin_attack_t
{
  holy_strike_t( paladin_t* player, util::string_view options_str )
    : paladin_attack_t( "holy_strike", player, player->rank.holy_strike.find_rank()->spell() )
  {
    parse_options( options_str );
  }

  void impact( action_state_t* s ) override
  {
    base_t::impact( s );

    if ( p()->talent.sacred_arbiter.enabled() )
    {
      // Refresh all judgement effects, kinda doesn't matter mostly except for command. (Procs)
    }
  }
};

struct paladin_seal_t : public paladin_spell_t
{
  buff_t* seal_buff;
  paladin_seal_e seal_type;

  paladin_seal_t( paladin_t* player, util::string_view name, spell_ranks_t rank, paladin_seal_e seal_type )
    : paladin_spell_t( name, player, rank.find_rank()->spell() ), seal_type( seal_type )
  {
  }

  void init() override
  {
    if ( initialized )
      return;

    base_t::init();

    create_buff();
  }

  virtual void initialize_procs()
  {
  }

  void create_buff()
  {
    std::string buff_name = util::tokenize_fn( name_str + "_" + p()->dbc->spell_text( data().id() ).rank() );

    if ( auto found_buff = buff_t::find( p(), buff_name, p() ) )
    {
      seal_buff = found_buff;
      return;
    }

    seal_buff = make_buff( p(), buff_name, &data() );
    p()->buff.seal_buffs[ seal_type ].push_back( seal_buff );

    seal_buff->add_stack_change_callback( [ this ]( buff_t* b, int old, int _new ) {
      if ( _new )
      {
        p()->buff.seal_expression_buffs[ seal_type ]->trigger( 1, b->buff_duration() );
      }
      else
      {
        p()->buff.seal_expression_buffs[ seal_type ]->expire();
      }
    } );

    initialize_procs();
  }

  void execute() override
  {
    paladin_spell_t::execute();

    if ( seal_type != p()->current_seal )
    {
      p()->previous_seal = p()->current_seal;
      p()->current_seal  = seal_type;

      if ( p()->current_seal_buff )
      {
        p()->current_seal_buff->expire();
        p()->current_seal_buff = nullptr;
      }
    }

    if ( seal_buff )
    {
      seal_buff->trigger();
      p()->current_seal_buff = seal_buff;
    }
  }
};

struct seal_of_righteousness_t : public paladin_seal_t
{
  struct seal_of_righteousness_damage_t : public paladin_spell_t
  {
    seal_of_righteousness_damage_t( paladin_t* player )
      : paladin_spell_t( "seal_of_righteousness_damage", player, player->rank.seal_of_righteousness_damage.find_rank()->spell() )
    {
      background        = true;

      auto seal_spell = player->rank.seal_of_righteousness.find_rank()->spell();

      auto judgement_spell = player->find_spell( as<unsigned int>( seal_spell->effectN( 3 ).base_value() ) );

      base_dd_min = judgement_spell->effectN( 3 ).base_value() / 87;
      base_dd_max = judgement_spell->effectN( 3 ).base_value() / 25;

      spell_power_mod.direct = 0.1;

      // TODO: Weapon Scaling
    }
  };

  action_t* damage_action;
  seal_of_righteousness_t( paladin_t* player, util::string_view options_str )
    : paladin_seal_t( player, "seal_of_righteousness", player->rank.seal_of_righteousness, SEAL_OF_RIGHTEOUSNESS )
  {
    parse_options( options_str );

    damage_action = new seal_of_righteousness_damage_t( player );
    add_child( damage_action );
  }

  void initialize_procs() override
  {
    const auto driver    = new special_effect_t( p() );
    driver->name_str     = data().name_cstr();
    driver->spell_id     = data().id();
    driver->proc_chance_ = 1.0;

    p()->special_effects.push_back( driver );

    auto cb = new dbc_proc_callback_t( p(), *driver );
    cb->activate_with_buff( seal_buff, true );
    cb->proc_action = damage_action;
  }
};

struct seal_of_fury_t : public paladin_seal_t
{
  struct seal_of_fury_damage_t : public paladin_spell_t
  {
    double damage_percent_absorb;
    seal_of_fury_damage_t( paladin_t* player )
      : paladin_spell_t( "seal_of_fury_damage", player,
                         player->rank.seal_of_fury_damage.find_rank()->spell() ), damage_percent_absorb( player->rank.seal_of_fury.find_rank()->spell()->effectN( 2 ).percent() )
    {
      background = true;
      spell_power_mod.direct = 0.1;
    }

    void impact(action_state_t* s) override
    {
      paladin_spell_t::impact( s );

      const double damage = s->result_amount * damage_percent_absorb;
      p()->buff.lights_fury->trigger( 1, damage );
    }
  };

  action_t* damage_action;
  seal_of_fury_t( paladin_t* player, util::string_view options_str )
    : paladin_seal_t( player, "seal_of_fury", player->rank.seal_of_fury, SEAL_OF_FURY )
  {
    parse_options( options_str );

    damage_action = new seal_of_fury_damage_t( player );
    add_child( damage_action );
  }

  void initialize_procs() override
  {
    const auto driver    = new special_effect_t( p() );
    driver->name_str     = data().name_cstr();
    driver->spell_id     = data().id();
    driver->proc_chance_ = 1.0;

    p()->special_effects.push_back( driver );

    auto cb = new dbc_proc_callback_t( p(), *driver );
    cb->activate_with_buff( seal_buff, true );
    cb->proc_action = damage_action;
  }
};

struct devotion_aura_t : public paladin_spell_t
{
  devotion_aura_t( paladin_t* player, util::string_view options_str ) : paladin_spell_t( "devotion_aura", player, player->rank.devotion_aura.find_rank()->spell() )
  {
    parse_options( options_str );
    harmful = false;
    set_target( player );
  }

  void execute() override
  {
    paladin_spell_t::execute();

    // TODO: Disable other auras
    p()->buff.retribution_aura->trigger();
    p()->buff.devotion_aura->trigger();
  }
};

struct retribution_aura_damage_t : public paladin_spell_t
{
  retribution_aura_damage_t( paladin_t* player )
    : paladin_spell_t( "retribution_aura_damage", player, player->rank.retribution_aura.find_rank()->spell() )
  {
    background = true;
    may_miss = may_dodge = may_block = may_crit = may_glance = may_parry = false; // TOCHECK

    if ( data().ok() )
      base_dd_min = base_dd_max = data().effectN( 1 ).base_value();
  }
};

struct retribution_aura_t : public paladin_spell_t
{
  retribution_aura_t( paladin_t* player, util::string_view options_str ) : paladin_spell_t( "retribution_aura", player, player->rank.retribution_aura.find_rank()->spell() )
  {
    parse_options( options_str );
    harmful = false;
    set_target( player );
  }

  void execute() override
  {
    paladin_spell_t::execute();

    // TODO: Disable other auras
    p()->buff.devotion_aura->expire();
    p()->buff.retribution_aura->trigger();
  }
};

struct consecration_t : public paladin_spell_t
{
  // Consecration damage is weird because it does flat damage to all targets which is then increased for the first X
  // targets that it hits.
  // The way we handle this is for the first X targets, we use the SP scaled effect DA min + DA max + SP coeff and then
  // add the flat damage after. For the remaining targets, we use the flat damage DA min + DA max + 0 SP coeff.
  struct consecration_damage_t : public paladin_spell_t
  {

    double flat_damage_dd_min;
    double flat_damage_dd_max;
    double flat_damage_spell_power_mod;
    double increased_damage_dd_min;
    double increased_damage_dd_max;
    double increased_damage_spell_power_mod;
    int increased_damage_enemies;

    consecration_damage_t( paladin_t* player, int num_enemies )
      : paladin_spell_t( "consecration_damage", player, player->rank.consecration_damage.find_rank()->spell() ), increased_damage_enemies( num_enemies )
    {
      background = true;

      may_miss = may_crit = may_dodge = may_block = may_glance = may_parry = false;

      // uncapped but deals increased damage to the first 4 enemies to enter
      aoe = -1;

      // we're going to override this in the functions
      base_dd_min = base_dd_max = spell_power_mod.direct = 0;

      if (data().ok())
      {
        flat_damage_dd_min = data().effectN( 1 ).min( player, player->level() );
        flat_damage_dd_max = data().effectN( 1 ).max( player, player->level() );
        flat_damage_spell_power_mod = data().effectN( 1 ).sp_coeff();
        increased_damage_dd_min = data().effectN( 2 ).min( player, player->level() );
        increased_damage_dd_max = data().effectN( 2 ).max( player, player->level() );
        increased_damage_spell_power_mod = data().effectN( 2 ).sp_coeff();
      }
    }

    void impact(action_state_t* s) override
    {
      paladin_spell_t::impact( s );

      // chain target is 0-based and spell data is 1-based
      if ( s->chain_target < increased_damage_enemies )
      {
        p()->get_target_data( s->target )->debuffs.consecration->trigger();
      }
    }

    double spell_direct_power_coefficient( const action_state_t* s ) const override
    {
      // chain target is 0-based and spell data is 1-based
      if ( s->chain_target >= increased_damage_enemies )
      {
        return flat_damage_spell_power_mod;
      }

      return increased_damage_spell_power_mod;
    }

    double base_da_min( const action_state_t* s ) const override
    {
      // chain target is 0-based and spell data is 1-based
      if ( s->chain_target >= increased_damage_enemies )
      {
        return flat_damage_dd_min;
      }

      return increased_damage_dd_min;
    }

    double base_da_max( const action_state_t* s ) const override
    {
      // chain target is 0-based and spell data is 1-based
      if ( s->chain_target >= increased_damage_enemies )
      {
        return flat_damage_dd_max;
      }

      return increased_damage_dd_max;
    }

    double bonus_da(const action_state_t* s) const override
    {
      // chain target is 0-based and spell data is 1-based
      if ( s->chain_target >= increased_damage_enemies )
      {
        return 0;
      }

      // we add the flat damage as bonus DA so that it isn't affected by the SP coefficient for
      // the second damage effect.
      return sim->averaged_range( flat_damage_dd_min, flat_damage_dd_max );
    }
  };

  consecration_damage_t* damage_action;
  consecration_t( paladin_t* player, util::string_view options_str )
    : paladin_spell_t( "consecration", player, player->rank.consecration_cast.find_rank()->spell() )
  {
    parse_options( options_str );

    dot_duration = 0_ms;

    if (data().ok())
    {
      damage_action = new consecration_damage_t( player,  as<int>(data().effectN( 3 ).base_value()) );
      add_child( damage_action );
    }
  }

  void execute() override
  {
    paladin_spell_t::execute();

    make_event<ground_aoe_event_t>( *sim, p(),
                                    ground_aoe_params_t()
                                        .target( target )
                                        .x( p()->x_position )
                                        .y( p()->y_position )
                                        .duration( data().duration() )
                                        .action( damage_action ),
                                    // immediate pulse
                                    true );
  }
};

void paladin_t::activate()
{
  base_t::activate();
}

void paladin_t::reset()
{
  base_t::reset();

  current_seal_judgement = nullptr;
  previous_seal     = SEAL_NONE;
  current_seal      = SEAL_NONE;
  current_seal_buff = nullptr;
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
  if ( name == "holy_strike" )
    return new holy_strike_t( this, options_str );

  if ( name == "consecration" )
    return new consecration_t( this, options_str );

  if ( name == "exorcism" )
    return new exorcism_t( this, options_str );
  if ( name == "holy_wrath" )
    return new holy_wrath_t( this, options_str );

  if ( name == "seal_of_righteousness" )
    return new seal_of_righteousness_t( this, options_str );
  if ( name == "seal_of_fury" )
    return new seal_of_fury_t( this, options_str );

  // auras
  if ( name == "devotion_aura" )
    return new devotion_aura_t( this, options_str );
  if ( name == "retribution_aura" )
    return new retribution_aura_t( this, options_str );

  // utility
  if ( name == "hammer_of_justice" )
    return new hammer_of_justice_t( this, options_str );

  // heals
  if ( name == "holy_light" )
    return new holy_light_t( this, options_str );

  return base_t::create_action( name, options_str );
}

void paladin_t::create_buffs()
{
  base_t::create_buffs();

  buff.seal_expression_buffs[ SEAL_OF_LIGHT ] =
      make_buff( this, "seal_of_light", rank.seal_of_light.find_rank()->spell() );
  buff.seal_expression_buffs[ SEAL_OF_FURY ] =
      make_buff( this, "seal_of_fury", rank.seal_of_fury.find_rank()->spell() );
  buff.seal_expression_buffs[ SEAL_OF_RIGHTEOUSNESS ] =
      make_buff( this, "seal_of_righteousness", rank.seal_of_righteousness.find_rank()->spell() );
  buff.seal_expression_buffs[ SEAL_OF_JUSTICE ] =
      make_buff( this, "seal_of_justice", rank.seal_of_justice.find_rank()->spell() );
  buff.seal_expression_buffs[ SEAL_OF_WISDOM ] =
      make_buff( this, "seal_of_wisdom", rank.seal_of_wisdom.find_rank()->spell() );
  buff.seal_expression_buffs[ SEAL_OF_THE_CRUSADER ] =
      make_buff( this, "seal_of_the_crusader", rank.seal_of_the_crusader.find_rank()->spell() );

  buff.devotion_aura = make_buff( this, "devotion_aura", rank.devotion_aura.find_rank()->spell() );
  buff.retribution_aura = make_buff( this, "retribution_aura", rank.retribution_aura.find_rank()->spell() );

  buff.lights_fury = make_buff<absorb_buff_t>( this, "lights_fury", spell.lights_fury );
  buff.lights_fury->set_absorb_source( get_stats( "lights_fury" ) )
      ->set_absorb_gain( get_gain( "lights_fury" ) )
      ->set_cumulative( true )
      ->set_cooldown( timespan_t::zero() );

  buff.redoubt = make_buff( this, "redoubt", spell.redoubt_buff )->set_reverse( true );

  parse_effects( buff.devotion_aura );
  parse_effects( buff.redoubt, IGNORE_STACKS );
}

void paladin_t::create_actions()
{
  base_t::create_actions();

  if ( rank.retribution_aura.find_rank()->spell()->ok() )
  {
    active.retribution_aura = new retribution_aura_damage_t( this );
  }
}

void paladin_t::assess_damage( school_e school, result_amount_type type, action_state_t* s )
{
  parse_player_effects_t::assess_damage( school, type, s );

  if ( active.retribution_aura && buff.retribution_aura->check() && type == result_amount_type::DMG_DIRECT &&
       s->action->player->is_enemy() && !s->action->special && s->action->result_is_hit( s->result ) )
  {
    active.retribution_aura->execute_on_target( s->action->player );
  }

  if ( s->block_result == BLOCK_RESULT_BLOCKED && buff.redoubt->check() )
    buff.redoubt->decrement();
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

  auto CT = [ & ]( std::string_view n ) { return find_talent_spell( talent_tree::CLASS, n ); };

    // Row 1 Holy
  talent.spiritual_focus = CT( "Spiritual Focus" );
  talent.divine_strength = CT( "Divine Strength" );

  // Row 1 Prot
  talent.divine_intellect = CT( "Divine Intellect" );
  talent.redoubt          = CT( "Redoubt" );

  // Row 1 Ret
  talent.parry       = CT( "Parry" );
  talent.benediction = CT( "Benediction" );

  // Row 2 Holy
  talent.healing_light   = CT( "Healing Light" );
  talent.reverence       = CT( "Reverence" );
  talent.improved_seals  = CT( "Improved Seals" );
  talent.purifying_power = CT( "Purifying Power" );

  // Row 2 Prot
  talent.precision       = CT( "Precision" );
  talent.guardians_favor = CT( "Guardian's Favor" );
  talent.sacred_duty     = CT( "Sacred Duty" );

  // Row 2 Ret
  talent.improved_judgement   = CT( "Improved Judgement" );
  talent.conviction           = CT( "Conviction" );
  talent.sanctified_judgement = CT( "Sanctified Judgement" );

  // Row 3 Holy
  talent.divine_favor = CT( "Divine Favor" );
  talent.illumination = CT( "Illumination" );
  talent.holy_conduit = CT( "Holy Conduit" );

  // Row 3 Prot
  talent.templars_bulwark           = CT( "Templar's Bulwark" );
  talent.improved_hammer_of_justice = CT( "Improved Hammer of Justice" );
  talent.shield_specialization      = CT( "Shield Specialization" );
  talent.toughness                  = CT( "Toughness" );

  // Row 3 Ret
  talent.vindication        = CT( "Vindication" );
  talent.unyielding_faith   = CT( "Unyielding Faith" );
  talent.seal_of_command    = CT( "Seal of Command" );
  talent.pursuit_of_justice = CT( "Pursuit of Justice" );

  // Row 4 Holy
  talent.divine_precision  = CT( "Divine Precision" );
  talent.infusion_of_light = CT( "Infusion of Light" );
  talent.oath_of_light     = CT( "Oath of Light" );

  // Row 4 Prot
  talent.swift_judgement         = CT( "Swift Judgement" );
  talent.improved_righteous_fury = CT( "Improved Righteous Fury" );
  talent.anticipation            = CT( "Anticipation" );

  // Row 4 Ret
  talent.eye_for_an_eye                   = CT( "Eye for an Eye" );
  talent.sacred_arbiter                   = CT( "Sacred Arbiter" );
  talent.two_handed_weapon_specialization = CT( "Two-Handed Weapon Specialization" );

  // Row 5 Holy
  talent.consecrated_ground = CT( "Consecrated Ground" );
  talent.holy_shock         = CT( "Holy Shock" );
  talent.boundless_light    = CT( "Boundless Light" );

  // Row 5 Prot
  talent.stoicism    = CT( "Stoicism" );
  talent.holy_shield = CT( "Holy Shield" );
  talent.reckoning   = CT( "Reckoning" );

  // Row 5 Ret
  talent.turn_evil  = CT( "Turn Evil" );
  talent.repentance = CT( "Repentance" );
  talent.vengeance  = CT( "Vengeance" );

  // Row 6 Holy
  talent.holy_power = CT( "Holy Power" );

  // Row 6 Prot
  talent.iron_creed = CT( "Iron Creed" );

  // Row 6 Ret
  talent.champion_of_the_light = CT( "Champion of the Light" );

  // Row 7 Holy
  talent.lights_vigil = CT( "Light's Vigil" );

  // Row 7 Prot
  talent.shieldsworn = CT( "Shieldsworn" );

  // Row 7 Ret
  talent.unsealed_wrath = CT( "Unsealed Wrath" );

  rank.blessing_of_might = {
      this,
      spell_container_t().set_spell( 19740 ),
      spell_container_t().set_spell( 19834 ),
      spell_container_t().set_spell( 19835 ),
      spell_container_t().set_spell( 19836 ),
      spell_container_t().set_spell( 19837 ),
      spell_container_t().set_spell( 19838 ),
      spell_container_t().set_spell( 25291 ),
  };
  rank.blessing_of_wisdom = {
      this,
      spell_container_t().set_spell( 19742 ),
      spell_container_t().set_spell( 19850 ),
      spell_container_t().set_spell( 19852 ),
      spell_container_t().set_spell( 19853 ),
      spell_container_t().set_spell( 19854 ),
      spell_container_t().set_spell( 25290 ),
  };
  rank.consecration_cast = {
      this,
      spell_container_t().set_spell( 26573 ),
      spell_container_t().set_spell( 20116 ),
      spell_container_t().set_spell( 20922 ),
      spell_container_t().set_spell( 20923 ),
      spell_container_t().set_spell( 20924 ),
  };
  rank.consecration_damage = {
      this,
      spell_container_t().set_spell( 1280345 ),
      spell_container_t().set_spell( 1280346 ),
      spell_container_t().set_spell( 1280347 ),
      spell_container_t().set_spell( 1280348 ),
      spell_container_t().set_spell( 1280349 ),
  };
  rank.devotion_aura = {
      this,
      spell_container_t().set_spell( 465 ),
      spell_container_t().set_spell( 10290 ),
      spell_container_t().set_spell( 643 ),
      spell_container_t().set_spell( 10291 ),
      spell_container_t().set_spell( 1032 ),
      spell_container_t().set_spell( 10292 ),
      spell_container_t().set_spell( 10293 ),
  };
  rank.exorcism = {
      this,
      spell_container_t().set_spell( 879 ),
      spell_container_t().set_spell( 5614 ),
      spell_container_t().set_spell( 5615 ),
      spell_container_t().set_spell( 10312 ),
      spell_container_t().set_spell( 10313 ),
      spell_container_t().set_spell( 10314 ),
  };
  rank.flash_of_light = {
      this,
      spell_container_t().set_spell( 19750 ),
      spell_container_t().set_spell( 19939 ),
      spell_container_t().set_spell( 19940 ),
      spell_container_t().set_spell( 19941 ),
      spell_container_t().set_spell( 19942 ),
      spell_container_t().set_spell( 19943 ),
  };
  rank.hammer_of_justice = {
      this,
      spell_container_t().set_spell( 853 ),
      spell_container_t().set_spell( 5588 ),
      spell_container_t().set_spell( 5589 ),
      spell_container_t().set_spell( 10308 ),
  };
  rank.hammer_of_wrath = {
      this,
      spell_container_t().set_spell( 24275 ),
      spell_container_t().set_spell( 24274 ),
      spell_container_t().set_spell( 24239 ),
  };
  rank.holy_light = {
      this,
      spell_container_t().set_spell( 635 ),
      spell_container_t().set_spell( 639 ),
      spell_container_t().set_spell( 647 ),
      spell_container_t().set_spell( 1026 ),
      spell_container_t().set_spell( 1042 ),
      spell_container_t().set_spell( 3472 ),
      spell_container_t().set_spell( 10328 ),
      spell_container_t().set_spell( 10329 ),
      spell_container_t().set_spell( 25292 ),
  };
  rank.holy_shield = {
      this,
      spell_container_t().set_spell( 20925 ),
      spell_container_t().set_spell( 20927 ),
      spell_container_t().set_spell( 20928 ),
      spell_container_t().set_spell( 1239570 ),
  };
  rank.holy_shock_cast = {
      this,
      spell_container_t().set_spell( 1311606 ),
      spell_container_t().set_spell( 20473 ),
      spell_container_t().set_spell( 20929 ),
      spell_container_t().set_spell( 20930 ),
  };
  rank.holy_shock_damage = {
      this,
      spell_container_t().set_spell( 1311604 ),
      spell_container_t().set_spell( 25912 ),
      spell_container_t().set_spell( 25911 ),
      spell_container_t().set_spell( 25902 ),
  };
  rank.holy_shock_heal = {
      this,
      spell_container_t().set_spell( 1311605 ),
      spell_container_t().set_spell( 25914 ),
      spell_container_t().set_spell( 25913 ),
      spell_container_t().set_spell( 25903 ),
  };
  rank.holy_strike = {
      this,
      spell_container_t().set_spell( 679 ),
      spell_container_t().set_spell( 678 ),
      spell_container_t().set_spell( 1866 ),
      spell_container_t().set_spell( 680 ),
      spell_container_t().set_spell( 2495 ),
      spell_container_t().set_spell( 5569 ),
      spell_container_t().set_spell( 10332 ),
      spell_container_t().set_spell( 10333 ),
  };
  rank.holy_wrath = {
      this,
      spell_container_t().set_spell( 2812 ),
      spell_container_t().set_spell( 10318 ),
  };
  rank.lay_on_hands = {
      this,
      spell_container_t().set_spell( 633 ),
      spell_container_t().set_spell( 2800 ),
      spell_container_t().set_spell( 10310 ),
  };
  rank.lights_vigil_buff = {
      this,
      spell_container_t().set_spell( 1310909 ),
      spell_container_t().set_spell( 1311593 ),
      spell_container_t().set_spell( 1311597 ),
  };
  rank.lights_vigil_cast = {
      this,
      spell_container_t().set_spell( 1310911 ),
      spell_container_t().set_spell( 1311590 ),
      spell_container_t().set_spell( 1311595 ),
  };
  rank.lights_vigil_heal = {
      this,
      spell_container_t().set_spell( 1310912 ),
      spell_container_t().set_spell( 1311591 ),
      spell_container_t().set_spell( 1311596 ),
  };
  rank.retribution_aura = {
      this,
      spell_container_t().set_spell( 7294 ),
      spell_container_t().set_spell( 10298 ),
      spell_container_t().set_spell( 10299 ),
      spell_container_t().set_spell( 10300 ),
      spell_container_t().set_spell( 10301 ),
  };
  rank.seal_of_command = {
      this,
      spell_container_t().set_spell( 20375 ),
      spell_container_t().set_spell( 20915 ),
      spell_container_t().set_spell( 20918 ),
      spell_container_t().set_spell( 20919 ),
      spell_container_t().set_spell( 20920 ),
  };
  rank.seal_of_fury = {
      this,
      spell_container_t().set_spell( 1311649 ),
      spell_container_t().set_spell( 1311656 ),
      spell_container_t().set_spell( 20163 ),
      spell_container_t().set_spell( 20419 ),
      spell_container_t().set_spell( 20421 ),
      spell_container_t().set_spell( 20422 ),
      spell_container_t().set_spell( 20423 ),
  };
  rank.seal_of_fury_damage = {
      this,
      spell_container_t().set_spell( 1311647 ),
      spell_container_t().set_spell( 1311654 ),
      spell_container_t().set_spell( 20231 ),
      spell_container_t().set_spell( 20415 ),
      spell_container_t().set_spell( 20416 ),
      spell_container_t().set_spell( 20417 ),
      spell_container_t().set_spell( 20418 ),
  };
  rank.seal_of_light = {
      this,
      spell_container_t().set_spell( 20165 ),
      spell_container_t().set_spell( 10298 ),
      spell_container_t().set_spell( 10299 ),
      spell_container_t().set_spell( 10300 ),
  };
  rank.seal_of_righteousness = {
      this,
      spell_container_t().set_spell( 20154 ),
      spell_container_t().set_spell( 20287 ),
      spell_container_t().set_spell( 20288 ),
      spell_container_t().set_spell( 20289 ),
      spell_container_t().set_spell( 20290 ),
      spell_container_t().set_spell( 20291 ),
      spell_container_t().set_spell( 20292 ),
      spell_container_t().set_spell( 20293 ),
  };
  rank.seal_of_righteousness_damage = {
      this,
      spell_container_t().set_spell( 25742 ),
      spell_container_t().set_spell( 25741 ),
      spell_container_t().set_spell( 25740 ),
      spell_container_t().set_spell( 25739 ),
      spell_container_t().set_spell( 25738 ),
      spell_container_t().set_spell( 25737 ),
      spell_container_t().set_spell( 25736 ),
      spell_container_t().set_spell( 25735 ),
  };
  rank.seal_of_the_crusader = {
      this,
      spell_container_t().set_spell( 21082 ),
      spell_container_t().set_spell( 20162 ),
      spell_container_t().set_spell( 20305 ),
      spell_container_t().set_spell( 20306 ),
      spell_container_t().set_spell( 20307 ),
      spell_container_t().set_spell( 20308 ),
  };
  rank.seal_of_wisdom = {
      this,
      spell_container_t().set_spell( 20166 ),
      spell_container_t().set_spell( 20356 ),
      spell_container_t().set_spell( 20357 ),
  };
  rank.seal_of_justice = {
      this,
      spell_container_t().set_spell( 20164 )
  };

  spell.lights_fury = find_spell( 1310927 );

  if (rank.consecration_cast.find_rank()->spell()->ok())
  {
    spell.consecration_debuff = find_spell( 1280353 );
  }

  if ( talent.redoubt->ok() )
  {
    spell.redoubt_buff = find_spell( 20128 );
  }

  parse_all_class_passives();
  parse_all_passive_talents();
}

  void paladin_t::parse_action_effects( action_t* action )
{
  auto _a = dynamic_cast<parse_action_base_t*>( action );
  assert( _a );
}

// TODO: Verify
// This is mostly yoinked from Warrior in the Midnight branch of simc
block_result_e paladin_t::target_block_resolution( const action_state_t* s ) const
{
  // Only direct physical attacks may be blocked
  if ( s->result_type == result_amount_type::DMG_DIRECT && s->action->get_school() == SCHOOL_PHYSICAL &&
       s->action->type == ACTION_ATTACK )
  {
    auto block_chance = cache.block();

    if ( rng().roll( block_chance ) )
    {
      return block_result_e::BLOCK_RESULT_BLOCKED;
    }
  }

  return block_result_e::BLOCK_RESULT_UNBLOCKED;
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

  if ( talent.redoubt->ok() )
  {
    const auto driver = new special_effect_t( this );
    driver->name_str = "redoubt";
    driver->spell_id = talent.redoubt->id();

    special_effects.push_back( driver );

    auto cb = new dbc_proc_callback_t( this, *driver );
    cb->proc_buff = buff.redoubt;
  }
}

paladin_td_t::paladin_td_t( player_t* target, paladin_t& p ) : actor_target_data_t( target, &p )
{
  debuffs.consecration = make_buff( *this, "consecration", p.spell.consecration_debuff )
                             ->set_duration( 1_s )
                             // ->set_quiet( true )
                             ->set_refresh_behavior( buff_refresh_behavior::DURATION )
                             ->disable_ticking( true );
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
