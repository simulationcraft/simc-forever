// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.hpp"
#include "action/parse_effects.hpp"
#include "class_modules/apl/warrior.hpp"

namespace warrior
{
// ==========================================================================
// Warrior
// ==========================================================================

constexpr unsigned STANCE_MASK_BATTLE    = 0x00010000;
constexpr unsigned STANCE_MASK_DEFENSIVE = 0x00020000;
constexpr unsigned STANCE_MASK_BERSERKER = 0x00040000;

struct warrior_t;

struct warrior_td_t : public actor_target_data_t
{
  struct dots_t
  {
    dot_t* deep_wounds = nullptr;
    dot_t* rend = nullptr;
  } dots;

  struct debuffs_t
  {
    buff_t* demoralizing_shout = nullptr;
    buff_t* disarm = nullptr;
    buff_t* hamstring = nullptr;
    buff_t* sunder_armor = nullptr;
    buff_t* thunder_clap = nullptr;
  } debuffs;

  warrior_td_t( player_t* target, warrior_t& p );
};

// utility to create target_effect_t compatible functions from warrior_td_t member references
template <typename T>
static std::function<int( actor_target_data_t* )> d_fn( T d )
{
  return [ d ]( actor_target_data_t* t ) {
    return std::invoke( d, static_cast<warrior_td_t*>( t )->debuffs )->check();
  };
}

struct warrior_t final : public parse_player_effects_t
{
  warrior_t( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) :
    parse_player_effects_t( sim, WARRIOR, name, r ) {}

  resource_e primary_resource() const override { return RESOURCE_RAGE; }

  void init_base_stats() override;
  void init_gains() override;
  void create_actions() override;
  void create_buffs() override;
  void init_spells() override;
  void init_special_effects() override;
  void activate() override;
  void assess_damage( school_e, result_amount_type, action_state_t* ) override;
  void parse_action_effects( action_t* );

  warrior_td_t* get_target_data( player_t* target ) const override;

  unsigned current_stance_mask() const;

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void init_action_list() override;

  std::string default_potion() const override   { return warrior_apl::potion( this ); }
  std::string default_flask() const override    { return warrior_apl::flask( this ); }
  std::string default_food() const override     { return warrior_apl::food( this ); }

  double rage_per_swing( const weapon_t& w ) const;

  struct talents_t
  {
    // Arms
    // Row 1
    player_talent_t improved_heroic_strike;
    player_talent_t deflection;
    player_talent_t improved_rend;
    // Row 2
    player_talent_t improved_charge;
    player_talent_t improved_tactical_mastery;
    player_talent_t improved_overpower;
    // Row 3
    player_talent_t anger_management;
    player_talent_t deep_wounds;
    // Row 4
    player_talent_t spearing_strike;
    player_talent_t two_handed_weapon_specialization;
    player_talent_t impale;
    // Row 5
    player_talent_t bloodthrill;
    player_talent_t sweeping_strikes;
    player_talent_t weaponmaster;
    // Row 6
    player_talent_t improved_slam;
    player_talent_t improved_hamstring;
    // Row 7
    player_talent_t mortal_strike;

    // Fury
    // Row 1
    player_talent_t booming_voice;
    player_talent_t cruelty;
    // Row 2
    player_talent_t lingering_rage;
    player_talent_t unbridled_wrath;
    // Row 3
    player_talent_t furious_precision;
    player_talent_t piercing_howl;
    player_talent_t blood_craze;
    // Row 4
    player_talent_t dual_wield_specialization;
    player_talent_t raging_blows;
    player_talent_t enrage;
    player_talent_t improved_execute;
    // Row 5
    player_talent_t improved_berserker_rage;
    player_talent_t death_wish;
    player_talent_t improved_intercept;
    // Row 6
    player_talent_t flurry;
    player_talent_t gore_drinker;
    // Row 7
    player_talent_t bloodthirst;

    // Protection
    // Row 1
    player_talent_t improved_bloodrage;
    player_talent_t shield_specialization;
    player_talent_t iron_will;
    // Row 2
    player_talent_t anticipation;
    player_talent_t improved_revenge;
    player_talent_t improved_thunder_clap;
    // Row 3
    player_talent_t last_stand;
    player_talent_t master_of_defense;
    player_talent_t improved_disarm;
    player_talent_t defiance;
    // Row 4
    player_talent_t improved_sunder_armor;
    player_talent_t vanguard;
    player_talent_t improved_shield_bash;
    // Row 5
    player_talent_t improved_shield_wall;
    player_talent_t concussion_blow;
    player_talent_t focused_rage;
    // Row 6
    player_talent_t bastion;
    // Row 7
    player_talent_t shield_slam;

  } talent;

  struct spells_t
  {
    const spell_data_t* battle_stance            = spell_data_t::nil();
    const spell_data_t* battle_stance_passive    = spell_data_t::nil();
    const spell_data_t* berserker_stance         = spell_data_t::nil();
    const spell_data_t* berserker_stance_passive = spell_data_t::nil();
    const spell_data_t* defensive_stance         = spell_data_t::nil();
    const spell_data_t* defensive_stance_passive = spell_data_t::nil();
    const spell_data_t* demoralizing_shout       = spell_data_t::nil();
    const spell_data_t* disarm                   = spell_data_t::nil();
    const spell_data_t* hamstring                = spell_data_t::nil();
    const spell_data_t* overpower_available      = spell_data_t::nil();
    const spell_data_t* retaliation              = spell_data_t::nil();
    const spell_data_t* shield_block             = spell_data_t::nil();
    const spell_data_t* sunder_armor             = spell_data_t::nil();
    const spell_data_t* thunder_clap             = spell_data_t::nil();
  } spell;

  struct buffs_t
  {
    buff_t* battle_stance    = nullptr;
    buff_t* berserker_stance = nullptr;
    buff_t* defensive_stance = nullptr;

    buff_t* battle_shout = nullptr;
    buff_t* bloodrage    = nullptr;

    buff_t* cleave_queued        = nullptr;
    buff_t* heroic_strike_queued = nullptr;
    buff_t* overpower_available  = nullptr;
    buff_t* retaliation          = nullptr;
    buff_t* revenge_available    = nullptr;
    buff_t* shield_block         = nullptr;
  } buff;

  struct gains_t
  {
    gain_t* melee = nullptr;
  } gain;

  struct actives_t
  {
    action_t* cleave        = nullptr;
    action_t* heroic_strike = nullptr;
    action_t* retaliation   = nullptr;
  } active;

  struct
  {
    spell_ranks_t battle_shout;
    spell_ranks_t bloodrage;
    spell_ranks_t charge;
    spell_ranks_t cleave;
    spell_ranks_t demoralizing_shout;
    spell_ranks_t disarm;
    spell_ranks_t hamstring;
    spell_ranks_t heroic_strike;
    spell_ranks_t mocking_blow;
    spell_ranks_t overpower;
    spell_ranks_t rend;
    spell_ranks_t retaliation;
    spell_ranks_t revenge;
    spell_ranks_t shield_bash;
    spell_ranks_t shield_block;
    spell_ranks_t sunder_armor;
    spell_ranks_t taunt;
    spell_ranks_t thunder_clap;
  } rank;

private:
  target_specific_t<warrior_td_t> _target_data;
};

warrior_td_t::warrior_td_t( player_t* target, warrior_t& p ) : actor_target_data_t( target, &p )
{
  dots.deep_wounds = target->get_dot( "deep_wounds", &p );
  dots.rend        = target->get_dot( "rend", &p );

  debuffs.demoralizing_shout =
      buff_t::make_buff_fallback<stat_buff_t>( p.spell.demoralizing_shout->ok(), *this,
                                               "demoralizing_shout", p.spell.demoralizing_shout )
          ->set_default_value_from_effect( 1 );

  if ( p.spell.disarm->ok() )
    debuffs.disarm = make_buff( *this, "disarm", p.spell.disarm );

  if ( p.spell.hamstring->ok() )
    debuffs.hamstring = make_buff( *this, "hamstring", p.spell.hamstring )
                            ->set_default_value_from_effect( 2 );

  if ( p.spell.sunder_armor->ok() )
    debuffs.sunder_armor =
        make_buff<stat_buff_t>( *this, "sunder_armor", p.spell.sunder_armor )
            ->set_default_value_from_effect( 1 );

  if ( p.spell.thunder_clap->ok() )
    debuffs.thunder_clap = make_buff( *this, "thunder_clap", p.spell.thunder_clap );
}

template <class Base>
struct warrior_action_t : public parse_action_effects_t<Base>
{
private:
  using ab = parse_action_effects_t<Base>;

public:
  using base_t = warrior_action_t<Base>;

  template <typename... Args>
  warrior_action_t( Args&&... args ) : ab( std::forward<Args>( args )... )
  {
    if ( ab::data().ok() )
      p()->parse_action_effects( this );
  }

  warrior_t* p()
  { return debug_cast<warrior_t*>( this->player ); }

  const warrior_t* p() const
  { return debug_cast<const warrior_t*>( this->player ); }

  warrior_td_t* td( player_t* target ) const
  { return p()->get_target_data( target ); }

  bool stance_ready() const
  {
    if ( this->background )
      return true;

    const unsigned mask = this->data().stance_mask();
    if ( !mask )
      return true;

    return ( mask & p()->current_stance_mask() ) != 0;
  }

  bool ready() override
  { return stance_ready() && ab::ready(); }

  void impact( action_state_t* s ) override
  {
    ab::impact( s );

    if ( s->result == RESULT_DODGE )
      p()->buff.overpower_available->trigger();
  }

  void consume_resource() override
  {
    resource_e cr = this->current_resource();

    auto discountOnMiss = this->data().flags( spell_attribute::SX_DISCOUNT_ON_MISS );

    if (
        cr == RESOURCE_RAGE
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
    }
  }
};

using warrior_attack_t = warrior_action_t<melee_attack_t>;
using warrior_spell_t  = warrior_action_t<spell_t>;

// TODO: Party buff version
struct battle_shout_t : public warrior_spell_t
{
  battle_shout_t( warrior_t* player, util::string_view options_str ) :
    warrior_spell_t( "battle_shout", player, player->rank.battle_shout.find_rank()->spell() )
  {
    parse_options( options_str );

    harmful  = false;
    may_miss = false;

    target   = player;
  }

  void execute() override
  {
    warrior_spell_t::execute();

    // TODO: Legacy talent "Permanence" can extend the duration

    p()->buff.battle_shout->trigger();
  }
};

struct bloodrage_t : public warrior_spell_t
{
  bloodrage_t( warrior_t* player, util::string_view options_str ) :
    warrior_spell_t( "bloodrage", player, player->rank.bloodrage.find_rank()->spell() )
  {
    parse_options( options_str );

    harmful  = false;
    may_miss = false;

    target   = player;
  }

  void execute() override
  {
    warrior_spell_t::execute();

    p()->buff.bloodrage->trigger();
    p()->resource_loss( RESOURCE_HEALTH, 100 * data().cost( POWER_HEALTH ), nullptr, this );
  }
};

/////////////////////////////////////////////////////////////////////////////////////
//
// Stances
//
/////////////////////////////////////////////////////////////////////////////////////

struct warrior_stance_t : public warrior_spell_t
{
  buff_t* stance;
  std::array<buff_t*, 3> stances;

  warrior_stance_t( warrior_t* player, util::string_view name, const spell_data_t* spell, buff_t* stance,
                    std::array<buff_t*, 3> stances, util::string_view options_str ) :
    warrior_spell_t( name, player, spell ), stance( stance ), stances( stances )
  {
    parse_options( options_str );

    harmful     = false;
    may_miss    = false;
    target      = player;
    trigger_gcd = 0_ms;

    cooldown           = player->get_cooldown( "stance_swap_cooldown", this );
    cooldown->category = true;
    cooldown->duration = std::max( spell->cooldown(), spell->category_cooldown() );
  }

  bool ready() override
  {
    if ( stance->check() )
      return false;

    return warrior_spell_t::ready();
  }

  void execute() override
  {
    warrior_spell_t::execute();

    for ( auto* b : stances )
      if ( b != stance )
        b->expire();

    stance->trigger();
  }
};

/////////////////////////////////////////////////////////////////////////////////////
//
// Melee
//
/////////////////////////////////////////////////////////////////////////////////////

struct warrior_melee_t : public warrior_attack_t
{
  warrior_melee_t( util::string_view name, warrior_t* player, weapon_t* w )
    : warrior_attack_t( name, player, spell_data_t::nil() )
  {
    weapon            = w;
    weapon_multiplier = 1.0;
    base_execute_time = w->swing_time;
    school            = SCHOOL_PHYSICAL;
    trigger_gcd       = 0_ms;

    background        = true;
    repeating         = true;
    may_glance        = true;
    may_crit          = true;
    may_dodge         = true;
    may_parry         = true;
    special           = false;
  }

  void execute() override
  {
    // try to trigger a queued heroic strike
    if ( p()->buff.heroic_strike_queued->check() )
    {
      if ( p()->resource_available( RESOURCE_RAGE, p()->active.heroic_strike->cost() ) )
      {
        p()->active.heroic_strike->execute_on_target( target );
        schedule_execute();
        return;
      }
      // didn't have enough rage, cancel the tracking queue buff
      p()->buff.heroic_strike_queued->expire();
    }

    // try to trigger a queued cleave
    if ( p()->buff.cleave_queued->check() )
    {
      if ( p()->resource_available( RESOURCE_RAGE, p()->active.cleave->cost() ) )
      {
        p()->active.cleave->execute_on_target( target );
        schedule_execute();
        return;
      }
      // didn't have enough rage, cancel the tracking queue buff
      p()->buff.cleave_queued->expire();
    }

    warrior_attack_t::execute();
  }

  void impact( action_state_t* s ) override
  {
    warrior_attack_t::impact( s );

    if ( result_is_hit( s->result ) )
    {
      double mult = s->result == RESULT_CRIT ? 2.0 : 1.0;
      p()->resource_gain( RESOURCE_RAGE, mult * p()->rage_per_swing( *weapon ), p()->gain.melee, this );
    }
  }
};

struct warrior_auto_attack_t : public warrior_attack_t
{
  warrior_auto_attack_t( warrior_t* p, util::string_view options_str )
    : warrior_attack_t( "auto_attack", p, spell_data_t::nil() )
  {
    parse_options( options_str );
    trigger_gcd           = 0_ms;
    ignore_false_positive = true;

    assert( p->equipped_weapons[ SLOT_MAIN_HAND ].type != WEAPON_NONE );

    p->main_hand_attack = new warrior_melee_t( "melee", p, &p->equipped_weapons[ SLOT_MAIN_HAND ] );
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

double warrior_t::rage_per_swing( const weapon_t& w ) const
{
  // log observations on low level warrior
  double mult = w.group() == WEAPON_2H ? 4.5 : 3.45;
  return w.swing_time.total_seconds() * mult;
}

/////////////////////////////////////////////////////////////////////////////////////
//
// Abilities
//
/////////////////////////////////////////////////////////////////////////////////////

struct rend_t : public warrior_attack_t
{
  rend_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "rend", player, player->rank.rend.find_rank()->spell() )
  {
    parse_options( options_str );

    dot_behavior = DOT_CLIP;

    // TODO: Verify this at higher levels
    attack_power_mod.tick = 0.016;
  }
};

struct hamstring_t : public warrior_attack_t
{
  hamstring_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "hamstring", player, player->rank.hamstring.find_rank()->spell() )
  {
    parse_options( options_str );
  }

  void impact( action_state_t* s ) override
  {
    warrior_attack_t::impact( s );

    if ( result_is_hit( s->result ) )
    {
      // NOTE: Movement speed slow is tracked but not applied to the target.
      td( s->target )->debuffs.hamstring->trigger();
    }
  }
};

struct sunder_armor_t : public warrior_attack_t
{
  sunder_armor_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "sunder_armor", player, player->rank.sunder_armor.find_rank()->spell() )
  {
    parse_options( options_str );
  }

  void impact( action_state_t* s ) override
  {
    warrior_attack_t::impact( s );

    if ( result_is_hit( s->result ) )
      td( s->target )->debuffs.sunder_armor->trigger();
  }
};

struct overpower_t : public warrior_attack_t
{
  overpower_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "overpower", player, player->rank.overpower.find_rank()->spell() )
  {
    parse_options( options_str );

    may_block = may_dodge = may_parry = false;
  }

  bool ready() override
  {
    if ( !data().ok() )
      return false;

    if ( !p()->buff.overpower_available->check() )
      return false;

    return warrior_attack_t::ready();
  }

  void execute() override
  {
    p()->buff.overpower_available->expire();

    warrior_attack_t::execute();
  }
};

struct revenge_t : public warrior_attack_t
{
  revenge_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "revenge", player, player->rank.revenge.find_rank()->spell() )
  {
    parse_options( options_str );

    // Revenge scales with 22.5% of attack power in Defensive Stance.
    attack_power_mod.direct = 0.225;
  }

  bool ready() override
  {
    if ( !data().ok() )
      return false;

    if ( !p()->buff.revenge_available->check() )
      return false;

    return warrior_attack_t::ready();
  }

  void execute() override
  {
    p()->buff.revenge_available->expire();

    warrior_attack_t::execute();
  }
};

// APL action to queue up cleave
struct cleave_queue_t : public action_t
{
  cleave_queue_t( warrior_t* p, util::string_view options_str ) :
    action_t( ACTION_OTHER, "cleave_queue", p )
  {
    parse_options( options_str );

    trigger_gcd           = 0_ms;
    harmful               = false;
    ignore_false_positive = true;
  }

  warrior_t* p() const
  { return debug_cast<warrior_t*>( player ); }

  bool ready() override
  {
    if ( !p()->active.cleave->data().ok() )
      return false;

    if ( p()->buff.cleave_queued->check() || p()->buff.heroic_strike_queued->check() )
      return false;

    if ( !p()->resource_available( RESOURCE_RAGE, p()->active.cleave->cost() ) )
      return false;

    return action_t::ready();
  }

  void execute() override
  {
    p()->buff.cleave_queued->trigger();
  }
};

struct cleave_t : public warrior_attack_t
{
  cleave_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "cleave", player, player->rank.cleave.find_rank()->spell() )
  {
    parse_options( options_str );

    trigger_gcd = 0_ms;
  }

  void execute() override
  {
    warrior_attack_t::execute();

    p()->buff.cleave_queued->expire();
  }
};

// TODO: Use spell 20240 if it becomes available in the client data. It is the historical
// hidden Retaliation damage spell, but it is absent from the current 70707 spell data.
struct retaliation_attack_t : public warrior_attack_t
{
  retaliation_attack_t( warrior_t* player ) :
    warrior_attack_t( "retaliation_attack", player, spell_data_t::nil() )
  {
    weapon            = &player->equipped_weapons[ SLOT_MAIN_HAND ];
    weapon_multiplier = 1.0;
    school            = SCHOOL_PHYSICAL;
    trigger_gcd       = 0_ms;

    background = true;
    may_crit   = true;
    may_dodge  = true;
    may_parry  = true;
    special    = true;
  }
};

struct retaliation_t : public warrior_spell_t
{
  retaliation_t( warrior_t* player, util::string_view options_str ) :
    warrior_spell_t( "retaliation", player, player->rank.retaliation.find_rank()->spell() )
  {
    parse_options( options_str );

    harmful  = false;
    may_miss = false;
    target   = player;

    add_child( p()->active.retaliation );
  }

  bool ready() override
  {
    if ( !data().ok() )
      return false;

    return warrior_spell_t::ready();
  }

  void execute() override
  {
    warrior_spell_t::execute();

    p()->buff.retaliation->trigger();
  }
};

// APL action to queue up heroic strike
// Checks to make sure warrior has enough rage to queue it up
// The actual HS execute in the melee swing will check rage again to make sure that rage
// wasn't consumed mid-swing
// TODO: Is there a better way to handle On Next Swing?
// TODO: look into generalizing this pattern for cleave, hunter (raptor strike), and druid (maul)
struct heroic_strike_queue_t : public action_t
{
  heroic_strike_queue_t( warrior_t* p, util::string_view options_str )
    : action_t( ACTION_OTHER, "heroic_strike_queue", p )
  {
    parse_options( options_str );

    trigger_gcd = 0_ms;
    harmful = false;
    ignore_false_positive = true;
  }

  warrior_t* p() const
  { return debug_cast<warrior_t*>( player ); }

  bool ready() override
  {
    // already queued
    if ( p()->buff.heroic_strike_queued->check() || p()->buff.cleave_queued->check() )
      return false;

    // not enough rage
    if ( !p()->resource_available( RESOURCE_RAGE, p()->active.heroic_strike->cost() ) )
      return false;

    return action_t::ready();
  }

  void execute() override
  {
    p()->buff.heroic_strike_queued->trigger();
  }
};

struct heroic_strike_t : public warrior_attack_t
{
  heroic_strike_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "heroic_strike", player, player->rank.heroic_strike.find_rank()->spell() )
  {
    parse_options( options_str );

    trigger_gcd = 0_ms;
  }

  void execute() override
  {
    warrior_attack_t::execute();

    p()->buff.heroic_strike_queued->expire();
  }
};

struct charge_t : public warrior_attack_t
{
  charge_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "charge", player, player->rank.charge.find_rank()->spell() )
  {
    parse_options( options_str );

    harmful = false;
    may_miss = false;
  }

  bool ready() override
  {
    return !player->in_combat && warrior_attack_t::ready();
  }
};

struct taunt_t : public warrior_spell_t
{
  taunt_t( warrior_t* player, util::string_view options_str ) :
    warrior_spell_t( "taunt", player, player->rank.taunt.find_rank()->spell() )
  {
    parse_options( options_str );
  }

  void execute() override
  {
    warrior_spell_t::execute();

    target->taunt( player );
  }
};

struct mocking_blow_t : public warrior_attack_t
{
  mocking_blow_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "mocking_blow", player, player->rank.mocking_blow.find_rank()->spell() )
  {
    parse_options( options_str );

    // Mocking Blow has fixed spell damage and no attack-power coefficient.
  }

  bool ready() override
  {
    if ( !data().ok() )
      return false;

    return warrior_attack_t::ready();
  }

  void impact( action_state_t* s ) override
  {
    warrior_attack_t::impact( s );

    if ( result_is_hit( s->result ) )
      s->target->taunt( player );
  }
};

struct shield_bash_t : public warrior_attack_t
{
  shield_bash_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "shield_bash", player, player->rank.shield_bash.find_rank()->spell() )
  {
    parse_options( options_str );

    is_interrupt = true;
  }

  bool ready() override
  {
    if ( !player->has_shield_equipped() )
      return false;

    return warrior_attack_t::ready();
  }
};

struct shield_block_t : public warrior_spell_t
{
  shield_block_t( warrior_t* player, util::string_view options_str ) :
    warrior_spell_t( "shield_block", player, player->rank.shield_block.find_rank()->spell() )
  {
    parse_options( options_str );

    harmful  = false;
    may_miss = false;
    target   = player;
  }

  bool ready() override
  {
    if ( !data().ok() )
      return false;

    if ( !player->has_shield_equipped() )
      return false;

    return warrior_spell_t::ready();
  }

  void execute() override
  {
    warrior_spell_t::execute();

    p()->buff.shield_block->trigger();
  }
};

struct disarm_t : public warrior_attack_t
{
  disarm_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "disarm", player, player->rank.disarm.find_rank()->spell() )
  {
    parse_options( options_str );
  }

  bool ready() override
  {
    if ( !data().ok() )
      return false;

    return warrior_attack_t::ready();
  }

  void impact( action_state_t* s ) override
  {
    warrior_attack_t::impact( s );

    if ( result_is_hit( s->result ) )
      td( s->target )->debuffs.disarm->trigger();
  }
};

struct demoralizing_shout_t : public warrior_spell_t
{
  demoralizing_shout_t( warrior_t* player, util::string_view options_str ) :
    warrior_spell_t( "demoralizing_shout", player, player->rank.demoralizing_shout.find_rank()->spell() )
  {
    parse_options( options_str );

    aoe = -1;
  }

  bool ready() override
  {
    if ( !data().ok() )
      return false;

    return warrior_spell_t::ready();
  }

  void impact( action_state_t* s ) override
  {
    warrior_spell_t::impact( s );

    if ( result_is_hit( s->result ) )
      td( s->target )->debuffs.demoralizing_shout->trigger();
  }
};

struct thunder_clap_t : public warrior_attack_t
{
  thunder_clap_t( warrior_t* player, util::string_view options_str ) :
    warrior_attack_t( "thunder_clap", player, player->rank.thunder_clap.find_rank()->spell() )
  {
    parse_options( options_str );

    aoe = data().max_targets();

    // TODO: Verify this at higher levels
    attack_power_mod.direct = 0.05;
  }

  void impact( action_state_t* s ) override
  {
    warrior_attack_t::impact( s );

    if ( result_is_hit( s->result ) )
    {
      // NOTE: Currently tracked for uptime only. The attack speed slow is not applied because
      // enemy targets do not parse player effects. This only affects target swing timing/DTPS.
      td( s->target )->debuffs.thunder_clap->trigger();
    }
  }
};

/////////////////////////////////////////////////////////////////////////////////////
//
// Stats, Actions, Oh my
//
/////////////////////////////////////////////////////////////////////////////////////

void warrior_t::init_base_stats()
{
  base.attack_power_per_strength = 2.0;

  parse_player_effects_t::init_base_stats();


  base.parry_per_strength = 0.0;

  // TODO - can this base stuff be moved into core?

  base.all_crit           = 0.0;
  base.spell_crit_chance  = 0.0;

  base.dodge_per_agility = base.attack_crit_per_agility;
  auto base_dodge = ( dbc->race_base( race ).agility + dbc->attribute_base( type, level() ).agility )
               * base.dodge_per_agility;

  base.attack_crit_chance = get_passive_player_value( 0.0, "attack_crit" );
  base.parry              = get_passive_player_value( 0.05, "parry" );
  base.block              = get_passive_player_value( 0.05, "block" );
  base.dodge              = get_passive_player_value( base_dodge, "dodge" );

  base.stats.attack_power = 3.0 * level() - 20.0;
}

void warrior_t::init_gains()
{
  parse_player_effects_t::init_gains();

  gain.melee = get_gain( "melee" );
}

void warrior_t::create_actions()
{
  active.cleave        = new cleave_t( this, "" );
  active.heroic_strike = new heroic_strike_t( this, "" );
  active.retaliation   = new retaliation_attack_t( this );

  parse_player_effects_t::create_actions();
}

void warrior_t::create_buffs()
{
  buff.cleave_queued        = make_buff( this, "cleave_queued" );
  buff.heroic_strike_queued = make_buff( this, "heroic_strike_queued" );
  buff.overpower_available  = make_buff( this, "overpower", spell.overpower_available );
  buff.retaliation          = make_buff( this, "retaliation", spell.retaliation );
  // Revenge is usable for 5 seconds after a block, dodge, or parry.
  buff.revenge_available = make_buff( this, "revenge_available" )->set_duration( 5_s );
  buff.shield_block      = make_buff( this, "shield_block", spell.shield_block );

  buff.battle_stance    = make_buff( this, "battle_stance", spell.battle_stance );
  buff.defensive_stance = make_buff( this, "defensive_stance", spell.defensive_stance );
  buff.berserker_stance = make_buff( this, "berserker_stance", spell.berserker_stance );

  buff.battle_shout = make_buff<stat_buff_t>( this, "battle_shout", rank.battle_shout.find_rank()->spell() );

  buff.bloodrage = make_buff( this, "bloodrage",
                              rank.bloodrage.find_rank()->buff() )
                        ->set_tick_callback( [ this ]( buff_t*, int, timespan_t ) {
                            resource_gain( RESOURCE_RAGE, 1.0, get_gain( "bloodrage" ) ); } );

  parse_effects( spell.battle_stance_passive, PARSE_PASSIVE,
                 [ this ] { return buff.battle_stance->check() > 0; } );
  parse_effects( spell.defensive_stance_passive, PARSE_PASSIVE,
                 [ this ] { return buff.defensive_stance->check() > 0; } );
  parse_effects( spell.berserker_stance_passive, PARSE_PASSIVE,
                 [ this ] { return buff.berserker_stance->check() > 0; } );
  parse_effects( buff.shield_block, IGNORE_STACKS );

  register_precombat_begin( buff.battle_stance );

  parse_player_effects_t::create_buffs();
}

unsigned warrior_t::current_stance_mask() const
{
  unsigned mask = 0;

  if ( buff.battle_stance->check() )
    mask |= STANCE_MASK_BATTLE;
  if ( buff.defensive_stance->check() )
    mask |= STANCE_MASK_DEFENSIVE;
  if ( buff.berserker_stance->check() )
    mask |= STANCE_MASK_BERSERKER;

  return mask;
}

warrior_td_t* warrior_t::get_target_data( player_t* target ) const
{
  warrior_td_t*& td = _target_data[ target ];
  if ( !td )
    td = new warrior_td_t( target, const_cast<warrior_t&>( *this ) );

  return td;
}

void warrior_t::init_spells()
{
  parse_player_effects_t::init_spells();

  rank.battle_shout = { this,
    spell_container_t().set_spell(  5242 ),
    spell_container_t().set_spell(  6192 ),
    spell_container_t().set_spell(  6673 ),
    spell_container_t().set_spell( 11549 ),
    spell_container_t().set_spell( 11550 ),
    spell_container_t().set_spell( 11551 ),
    spell_container_t().set_spell( 25289 ),
  };

  rank.bloodrage = { this,
    spell_container_t().set_spell(  2687 ).set_buff( 29131 ),
  };

  rank.rend = { this,
    spell_container_t().set_spell(   772 ),
    spell_container_t().set_spell(  6546 ),
    spell_container_t().set_spell(  6547 ),
    spell_container_t().set_spell(  6548 ),
    spell_container_t().set_spell( 11572 ),
    spell_container_t().set_spell( 11573 ),
    spell_container_t().set_spell( 11574 ),
  };

  rank.heroic_strike = { this,
    spell_container_t().set_spell(    78 ),
    spell_container_t().set_spell(   284 ),
    spell_container_t().set_spell(   285 ),
    spell_container_t().set_spell(  1608 ),
    spell_container_t().set_spell( 11564 ),
    spell_container_t().set_spell( 11565 ),
    spell_container_t().set_spell( 11566 ),
    spell_container_t().set_spell( 11567 ),
    spell_container_t().set_spell( 25286 ),
  };

  rank.charge = { this,
    spell_container_t().set_spell(   100 ),
    spell_container_t().set_spell(  6178 ),
    spell_container_t().set_spell( 11578 ),
  };

  rank.cleave = { this,
    spell_container_t().set_spell(   845 ),
    spell_container_t().set_spell(  7369 ),
    spell_container_t().set_spell( 11608 ),
    spell_container_t().set_spell( 11609 ),
    spell_container_t().set_spell( 20569 ),
  };

  rank.demoralizing_shout = { this,
    spell_container_t().set_spell(  1160 ),
    spell_container_t().set_spell(  6190 ),
    spell_container_t().set_spell( 11554 ),
    spell_container_t().set_spell( 11555 ),
    spell_container_t().set_spell( 11556 ),
  };

  rank.disarm = { this,
    spell_container_t().set_spell( 676 ),
  };

  rank.hamstring = { this,
    spell_container_t().set_spell( 1715 ),
    spell_container_t().set_spell( 7372 ),
    spell_container_t().set_spell( 7373 ),
  };

  rank.mocking_blow = { this,
    spell_container_t().set_spell(   694 ),
    spell_container_t().set_spell(  7400 ),
    spell_container_t().set_spell(  7402 ),
    spell_container_t().set_spell( 20559 ),
    spell_container_t().set_spell( 20560 ),
  };

  rank.overpower = { this,
    spell_container_t().set_spell(  7384 ),
    spell_container_t().set_spell(  7887 ),
    spell_container_t().set_spell( 11584 ),
    spell_container_t().set_spell( 11585 ),
  };

  rank.retaliation = { this,
    spell_container_t().set_spell( 20230 ),
  };

  rank.revenge = { this,
    spell_container_t().set_spell(  6572 ),
    spell_container_t().set_spell(  6574 ),
    spell_container_t().set_spell(  7379 ),
    spell_container_t().set_spell( 11600 ),
    spell_container_t().set_spell( 11601 ),
    spell_container_t().set_spell( 25288 ),
  };

  rank.shield_bash = { this,
    spell_container_t().set_spell(   72 ),
    spell_container_t().set_spell( 1671 ),
    spell_container_t().set_spell( 1672 ),
  };

  rank.shield_block = { this,
    spell_container_t().set_spell( 2565 ),
  };

  rank.sunder_armor = { this,
    spell_container_t().set_spell(  7386 ),
    spell_container_t().set_spell(  7405 ),
    spell_container_t().set_spell(  8380 ),
    spell_container_t().set_spell( 11596 ),
    spell_container_t().set_spell( 11597 ),
  };

  rank.taunt = { this,
    spell_container_t().set_spell( 355 ),
  };

  rank.thunder_clap = { this,
    spell_container_t().set_spell(  6343 ),
    spell_container_t().set_spell(  8198 ),
    spell_container_t().set_spell(  8204 ),
    spell_container_t().set_spell(  8205 ),
    spell_container_t().set_spell( 11580 ),
    spell_container_t().set_spell( 11581 ),
  };

  parse_all_class_passives();
  parse_all_passive_talents();

  spell.battle_stance            = find_spell( 2457 );
  spell.battle_stance_passive    = find_spell( 21156 );
  spell.berserker_stance         = find_spell( 2458 );
  spell.berserker_stance_passive = find_spell( 7381 );
  spell.defensive_stance         = find_spell( 71 );
  spell.defensive_stance_passive = find_spell( 7376 );
  spell.demoralizing_shout       = rank.demoralizing_shout.find_rank()->spell();
  spell.disarm                   = rank.disarm.find_rank()->spell();
  spell.hamstring                = rank.hamstring.find_rank()->spell();
  spell.overpower_available      = find_spell( 1282733 );
  spell.retaliation              = rank.retaliation.find_rank()->spell();
  spell.shield_block             = rank.shield_block.find_rank()->spell();
  spell.sunder_armor             = rank.sunder_armor.find_rank()->spell();
  spell.thunder_clap             = rank.thunder_clap.find_rank()->spell();

  // Arms
  talent.improved_heroic_strike           = find_talent_spell( talent_tree::CLASS, "Improved Heroic Strike" );
  talent.deflection                       = find_talent_spell( talent_tree::CLASS, "Deflection" );
  talent.improved_rend                    = find_talent_spell( talent_tree::CLASS, "Improved Rend" );
  talent.improved_charge                  = find_talent_spell( talent_tree::CLASS, "Improved Charge" );
  talent.improved_tactical_mastery        = find_talent_spell( talent_tree::CLASS, "Improved Tactical Mastery" );
  talent.improved_overpower               = find_talent_spell( talent_tree::CLASS, "Improved Overpower" );
  talent.anger_management                 = find_talent_spell( talent_tree::CLASS, "Anger Management" );
  talent.deep_wounds                      = find_talent_spell( talent_tree::CLASS, "Deep Wounds" );
  talent.spearing_strike                  = find_talent_spell( talent_tree::CLASS, "Spearing Strike" );
  talent.two_handed_weapon_specialization = find_talent_spell( talent_tree::CLASS, "Two-Handed Weapon Specialization" );
  talent.impale                           = find_talent_spell( talent_tree::CLASS, "Impale" );
  talent.bloodthrill                      = find_talent_spell( talent_tree::CLASS, "Bloodthrill" );
  talent.sweeping_strikes                 = find_talent_spell( talent_tree::CLASS, "Sweeping Strikes" );
  talent.weaponmaster                     = find_talent_spell( talent_tree::CLASS, "Weaponmaster" );
  talent.improved_slam                    = find_talent_spell( talent_tree::CLASS, "Improved Slam" );
  talent.improved_hamstring               = find_talent_spell( talent_tree::CLASS, "Improved Hamstring" );
  talent.mortal_strike                    = find_talent_spell( talent_tree::CLASS, "Mortal Strike" );

  // Fury
  talent.booming_voice             = find_talent_spell( talent_tree::CLASS, "Booming Voice" );
  talent.cruelty                   = find_talent_spell( talent_tree::CLASS, "Cruelty" );
  talent.lingering_rage            = find_talent_spell( talent_tree::CLASS, "Lingering Rage" );
  talent.unbridled_wrath           = find_talent_spell( talent_tree::CLASS, "Unbridled Wrath" );
  talent.furious_precision         = find_talent_spell( talent_tree::CLASS, "Furious Precision" );
  talent.piercing_howl             = find_talent_spell( talent_tree::CLASS, "Piercing Howl" );
  talent.blood_craze               = find_talent_spell( talent_tree::CLASS, "Blood Craze" );
  talent.dual_wield_specialization = find_talent_spell( talent_tree::CLASS, "Dual Wield Specialization" );
  talent.raging_blows              = find_talent_spell( talent_tree::CLASS, "Raging Blows" );
  talent.enrage                    = find_talent_spell( talent_tree::CLASS, "Enrage" );
  talent.improved_execute          = find_talent_spell( talent_tree::CLASS, "Improved Execute" );
  talent.improved_berserker_rage   = find_talent_spell( talent_tree::CLASS, "Improved Berserker Rage" );
  talent.death_wish                = find_talent_spell( talent_tree::CLASS, "Death Wish" );
  talent.improved_intercept        = find_talent_spell( talent_tree::CLASS, "Improved Intercept" );
  talent.flurry                    = find_talent_spell( talent_tree::CLASS, "Flurry" );
  talent.gore_drinker              = find_talent_spell( talent_tree::CLASS, "Gore Drinker" );
  talent.bloodthirst               = find_talent_spell( talent_tree::CLASS, "Bloodthirst" );

  // Protection
  talent.improved_bloodrage    = find_talent_spell( talent_tree::CLASS, "Improved Bloodrage" );
  talent.shield_specialization = find_talent_spell( talent_tree::CLASS, "Shield Specialization" );
  talent.iron_will             = find_talent_spell( talent_tree::CLASS, "Iron Will" );
  talent.anticipation          = find_talent_spell( talent_tree::CLASS, "Anticipation" );
  talent.improved_revenge      = find_talent_spell( talent_tree::CLASS, "Improved Revenge" );
  talent.improved_thunder_clap = find_talent_spell( talent_tree::CLASS, "Improved Thunder Clap" );
  talent.last_stand            = find_talent_spell( talent_tree::CLASS, "Last Stand" );
  talent.master_of_defense     = find_talent_spell( talent_tree::CLASS, "Master of Defense" );
  talent.improved_disarm       = find_talent_spell( talent_tree::CLASS, "Improved Disarm" );
  talent.defiance              = find_talent_spell( talent_tree::CLASS, "Defiance" );
  talent.improved_sunder_armor = find_talent_spell( talent_tree::CLASS, "Improved Sunder Armor" );
  talent.vanguard              = find_talent_spell( talent_tree::CLASS, "Vanguard" );
  talent.improved_shield_bash  = find_talent_spell( talent_tree::CLASS, "Improved Shield Bash" );
  talent.improved_shield_wall  = find_talent_spell( talent_tree::CLASS, "Improved Shield Wall" );
  talent.concussion_blow       = find_talent_spell( talent_tree::CLASS, "Concussion Blow" );
  talent.focused_rage          = find_talent_spell( talent_tree::CLASS, "Focused Rage" );
  talent.bastion               = find_talent_spell( talent_tree::CLASS, "Bastion" );
  talent.shield_slam           = find_talent_spell( talent_tree::CLASS, "Shield Slam" );
}

void warrior_t::init_special_effects()
{
  parse_player_effects_t::init_special_effects();
}

void warrior_t::parse_action_effects( action_t* action )
{
  auto _a = dynamic_cast<parse_action_base_t*>( action );
  assert( _a );

  // TODO: parse action-affecting warrior buffs here as they are implemented.
}

action_t* warrior_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "auto_attack" ) return new warrior_auto_attack_t( this, options_str );

  if ( name == "battle_stance" )
    return new warrior_stance_t( this, "battle_stance", spell.battle_stance, buff.battle_stance,
                                 { buff.battle_stance, buff.defensive_stance, buff.berserker_stance }, options_str );
  if ( name == "defensive_stance" )
    return new warrior_stance_t( this, "defensive_stance", spell.defensive_stance, buff.defensive_stance,
                                 { buff.battle_stance, buff.defensive_stance, buff.berserker_stance }, options_str );
  if ( name == "berserker_stance" )
    return new warrior_stance_t( this, "berserker_stance", spell.berserker_stance, buff.berserker_stance,
                                 { buff.battle_stance, buff.defensive_stance, buff.berserker_stance }, options_str );

  if ( name == "battle_shout" ) return new battle_shout_t( this, options_str );
  if ( name == "demoralizing_shout" ) return new demoralizing_shout_t( this, options_str );
  if ( name == "disarm" ) return new disarm_t( this, options_str );
  if ( name == "bloodrage" ) return new bloodrage_t( this, options_str );
  if ( name == "charge" ) return new charge_t( this, options_str );
  if ( name == "taunt" ) return new taunt_t( this, options_str );
  if ( name == "mocking_blow" ) return new mocking_blow_t( this, options_str );
  if ( name == "shield_bash" ) return new shield_bash_t( this, options_str );
  if ( name == "shield_block" ) return new shield_block_t( this, options_str );

  if ( name == "rend" ) return new rend_t( this, options_str );
  if ( name == "hamstring" ) return new hamstring_t( this, options_str );
  if ( name == "sunder_armor" ) return new sunder_armor_t( this, options_str );
  if ( name == "overpower" ) return new overpower_t( this, options_str );
  if ( name == "revenge" ) return new revenge_t( this, options_str );
  if ( name == "retaliation" ) return new retaliation_t( this, options_str );
  if ( name == "cleave" ) return new cleave_queue_t( this, options_str );
  if ( name == "heroic_strike" ) return new heroic_strike_queue_t( this, options_str );


  if ( name == "thunder_clap" ) return new thunder_clap_t( this, options_str );

  return parse_player_effects_t::create_action( name, options_str );
}

void warrior_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    parse_player_effects_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  warrior_apl::warrior( this );
  use_default_action_list = true;
  parse_player_effects_t::init_action_list();
}

void warrior_t::activate()
{
  parse_player_effects_t::activate();
}

void warrior_t::assess_damage( school_e school, result_amount_type type, action_state_t* s )
{
  parse_player_effects_t::assess_damage( school, type, s );

  if ( s->result == RESULT_DODGE || s->result == RESULT_PARRY ||
       s->block_result == BLOCK_RESULT_BLOCKED )
    buff.revenge_available->trigger();

  if ( s->block_result == BLOCK_RESULT_BLOCKED && buff.shield_block->check() )
    buff.shield_block->decrement();

  if ( buff.retaliation->check() && s->action && s->action->player != this &&
       s->action->type == ACTION_ATTACK && s->action->school == SCHOOL_PHYSICAL )
  {
    active.retaliation->execute_on_target( s->action->player );
    buff.retaliation->decrement();
  }
}

struct warrior_module_t : public module_t
{
  warrior_module_t() : module_t( WARRIOR ) {}

  player_t* create_player( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) const override
  {
    auto p = new warrior_t( sim, name, r );
    return p;
  }

  bool valid() const override
  {
    return true;
  }

  void register_hotfixes() const override {}

  void register_actor_initializers( sim_t* ) const override {}
};
}  // namespace warrior

const module_t* module_t::warrior()
{
  static warrior::warrior_module_t m;
  return &m;
}
