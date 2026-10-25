// ==========================================================================
// Priest Sim File
// Contact: https://github.com/orgs/simulationcraft/teams/priest/members
// Wiki: https://github.com/simulationcraft/simc/wiki/Priests
// ==========================================================================

#include "simulationcraft.hpp"
#include "action/parse_effects.hpp"
#include "class_modules/apl/priest.hpp"

namespace priestspace
{
struct priest_t;

struct priest_td_t final : public actor_target_data_t
{
  struct dots_t
  {
    dot_t* devouring_plague = nullptr;
    dot_t* holy_fire = nullptr;
    dot_t* shadow_word_pain = nullptr;
  } dots;

  struct debuffs_t
  {
    buff_t* hex_of_weakness = nullptr;
    buff_t* shadow_weaving = nullptr;
    buff_t* touch_of_weakness = nullptr;
    buff_t* vampiric_embrace = nullptr;
  } debuffs;

  priest_td_t( player_t* target, priest_t& p );
};

// utility to create target_effect_t compatible functions from priest_td_t member references
template <typename T>
static std::function<int( actor_target_data_t* )> d_fn( T d )
{
  return [ d ]( actor_target_data_t* t ) { return std::invoke( d, static_cast<priest_td_t*>( t )->debuffs )->check(); };
}

struct priest_t final : public parse_player_effects_t
{
  priest_t( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) :
    parse_player_effects_t( sim, PRIEST, name, r ) {}

  resource_e primary_resource() const override { return RESOURCE_MANA; }

  void init_base_stats() override;
  void init_spells() override;
  void create_buffs() override;
  void parse_action_effects( action_t* );

  void init_special_effects() override;
  void create_actions() override;
  void activate() override;

  double composite_attribute_multiplier( attribute_e ) const override;

  priest_td_t* get_target_data( player_t* target ) const override;

  // Priest racials are not in the racial spell table, so check the spell's race mask instead
  // Rank 1 is checked first so other races and levels below rank 1 never reach find_rank
  const spell_data_t* find_racial_rank( util::span<const spell_container_t> ranks,
                                        rank_ptype_t field = &spell_container_t::spell ) const
  {
    if ( !( ranks.front().spell( this )->race_mask() & util::race_mask( race ) ) )
      return spell_data_t::not_found();

    return std::invoke( field, find_rank( ranks ), this );
  }

  std::string default_potion() const override   { return priest_apl::potion( this ); }
  std::string default_flask() const override    { return priest_apl::flask( this ); }
  std::string default_food() const override     { return priest_apl::food( this ); }

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void init_action_list() override;

  struct buffs_t
  {
    buff_t* dark_inspiration = nullptr;
    buff_t* holy_purpose = nullptr;
    buff_t* inner_focus = nullptr;
    buff_t* shadowform = nullptr;
    buff_t* touch_of_weakness = nullptr;
  } buff;

  struct talents_t
  {
    player_talent_t dark_inspiration;
    player_talent_t devouring_contagion;
    player_talent_t early_demise;
    player_talent_t holy_nova;
    player_talent_t improved_mind_flay;
    player_talent_t inner_focus;
    player_talent_t mind_flay;
    player_talent_t penance;
    player_talent_t power_in_light;
    player_talent_t searing_light;
    player_talent_t shadow_weaving;
    player_talent_t shadowform;
    player_talent_t vampiric_embrace;
    player_talent_t wand_specialization;
  } talent;

  struct spells_t
  {
    const spell_data_t* dark_inspiration = spell_data_t::nil();
    const spell_data_t* hex_of_weakness = spell_data_t::nil();
    const spell_data_t* holy_purpose = spell_data_t::nil();
    const spell_data_t* shadow_weaving = spell_data_t::nil();
    const spell_data_t* touch_of_weakness = spell_data_t::nil();
    const spell_data_t* touch_of_weakness_debuff = spell_data_t::nil();
  } spell;

  struct actives_t
  {
    action_t* touch_of_weakness = nullptr;
  } active;

private:
  target_specific_t<priest_td_t> _target_data;
};

priest_td_t::priest_td_t( player_t* target, priest_t& p ) : actor_target_data_t( target, &p )
{
  dots.devouring_plague = target->get_dot( "devouring_plague", &p );
  dots.holy_fire        = target->get_dot( "holy_fire", &p );
  dots.shadow_word_pain = target->get_dot( "shadow_word_pain", &p );

  debuffs.hex_of_weakness   = make_buff( *this, "hex_of_weakness", p.spell.hex_of_weakness );
  debuffs.shadow_weaving    = make_buff( *this, "shadow_weaving", p.spell.shadow_weaving );
  debuffs.touch_of_weakness = make_buff( *this, "touch_of_weakness", p.spell.touch_of_weakness_debuff );
  debuffs.vampiric_embrace  = make_buff( *this, "vampiric_embrace", p.talent.vampiric_embrace );
}

struct priest_module_t final : public module_t
{
  priest_module_t() : module_t( PRIEST ) {}

  player_t* create_player( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) const override
  {
    auto p = new priest_t( sim, name, r );
    return p;
  }

  bool valid() const override
  {
    return true;
  }

  void register_hotfixes() const override {}

  void register_actor_initializers( sim_t* ) const override {}
};

template <class Base>
struct priest_action_t : public parse_action_effects_t<Base>
{
private:
  using ab = parse_action_effects_t<Base>;

public:
  using base_t = priest_action_t<Base>;

  template <typename... Args>
  priest_action_t( Args&&... args ) : ab( std::forward<Args>( args )... )
  {
    if ( ab::data().ok() )
      p()->parse_action_effects( this );
  }

  priest_t* p()
  { return debug_cast<priest_t*>( this->player ); }

  const priest_t* p() const
  { return debug_cast<const priest_t*>( this->player ); }

  void parse_power_in_light()
  {
    if ( !p()->talent.power_in_light.ok() )
      return;

    const auto& eff = p()->talent.power_in_light->effectN( 1 );

    add_parse_entry( this->target_multiplier_effects )
      .set_func( []( actor_target_data_t* td ) {
        const auto& dots = static_cast<priest_td_t*>( td )->dots;
        return dots.holy_fire->is_ticking() || dots.shadow_word_pain->is_ticking();
      } )
      .set_value( eff.percent() )
      .set_eff( &eff );
  }
};

using priest_spell_t = priest_action_t<spell_t>;

// Hex of Weakness

constexpr std::array hex_of_weakness_ranks {
  spell_container_t().set_spell(  9035 ),
  spell_container_t().set_spell( 19281 ),
  spell_container_t().set_spell( 19282 ),
  spell_container_t().set_spell( 19283 ),
  spell_container_t().set_spell( 19284 ),
  spell_container_t().set_spell( 19285 ),
};

// Touch of Weakness

// spell: self buff, debuff: damage and attack power debuff on the attacker
constexpr std::array touch_of_weakness_ranks {
  spell_container_t().set_spell(  2652 ).set_debuff(  2943 ),
  spell_container_t().set_spell( 19261 ).set_debuff( 19249 ),
  spell_container_t().set_spell( 19262 ).set_debuff( 19251 ),
  spell_container_t().set_spell( 19264 ).set_debuff( 19252 ),
  spell_container_t().set_spell( 19265 ).set_debuff( 19253 ),
  spell_container_t().set_spell( 19266 ).set_debuff( 19254 ),
};

void priest_t::init_base_stats()
{
  parse_player_effects_t::init_base_stats();

  // original classic crit rates according to
  // * https://github.com/wowsims/classic/blob/master/sim/core/base_stats.go#L106
  // TODO: Verify in-game
  base.attack_crit_chance = get_passive_player_value( 0.03, "all_crit" );
  base.spell_crit_chance = get_passive_player_value( 0.008, "all_crit" );
}

void priest_t::init_spells()
{
  parse_player_effects_t::init_spells();

  parse_all_class_passives();
  parse_all_passive_talents();

  talent.dark_inspiration    = find_talent_spell( talent_tree::CLASS, "Dark Inspiration" );
  talent.devouring_contagion = find_talent_spell( talent_tree::CLASS, "Devouring Contagion" );
  talent.early_demise        = find_talent_spell( talent_tree::CLASS, "Early Demise" );
  talent.holy_nova           = find_talent_spell( talent_tree::CLASS, "Holy Nova" );
  talent.improved_mind_flay  = find_talent_spell( talent_tree::CLASS, "Improved Mind Flay" );
  talent.inner_focus         = find_talent_spell( talent_tree::CLASS, "Inner Focus" );
  talent.mind_flay           = find_talent_spell( talent_tree::CLASS, "Mind Flay" );
  talent.penance             = find_talent_spell( talent_tree::CLASS, "Penance" );
  talent.power_in_light      = find_talent_spell( talent_tree::CLASS, "Power in Light" );
  talent.shadow_weaving      = find_talent_spell( talent_tree::CLASS, "Shadow Weaving" );
  talent.searing_light       = find_talent_spell( talent_tree::CLASS, "Searing Light" );
  talent.shadowform          = find_talent_spell( talent_tree::CLASS, "Shadowform" );
  talent.vampiric_embrace    = find_talent_spell( talent_tree::CLASS, "Vampiric Embrace" );

  // Wand only, all schools because wand damage can be any school
  // TODO: Apply to the wand auto attack once implemented
  talent.wand_specialization = find_talent_spell( talent_tree::CLASS, "Wand Specialization" );
  deregister_passive_spell( talent.wand_specialization );

  spell.dark_inspiration         = find_spell( 1326168 );
  spell.hex_of_weakness          = find_racial_rank( hex_of_weakness_ranks );
  spell.holy_purpose             = talent.searing_light->effectN( 2 ).trigger();
  spell.shadow_weaving           = talent.shadow_weaving->effectN( 1 ).trigger();
  spell.touch_of_weakness        = find_racial_rank( touch_of_weakness_ranks );
  spell.touch_of_weakness_debuff = find_racial_rank( touch_of_weakness_ranks, &spell_container_t::debuff );
}

void priest_t::create_buffs()
{
  parse_player_effects_t::create_buffs();

  buff.dark_inspiration = make_buff( this, "dark_inspiration", spell.dark_inspiration )
    ->set_default_value( talent.dark_inspiration->effectN( 1 ).percent() )
    ->add_invalidate( CACHE_SPIRIT );

  buff.holy_purpose = make_buff( this, "holy_purpose", spell.holy_purpose );

  // Cooldown starts when the buff is consumed
  buff.inner_focus = make_buff( this, "inner_focus", talent.inner_focus )
    ->set_cooldown( 0_ms )
    ->set_stack_change_callback( [ this ]( buff_t*, int, int new_ ) {
      if ( !new_ )
        get_cooldown( "inner_focus" )->start( talent.inner_focus->cooldown() );
    } );

  buff.shadowform = make_buff( this, "shadowform", talent.shadowform );

  buff.touch_of_weakness = make_buff( this, "touch_of_weakness", spell.touch_of_weakness );

  parse_effects( buff.shadowform );
  parse_target_effects( d_fn( &priest_td_t::debuffs_t::shadow_weaving ), spell.shadow_weaving );
}

priest_td_t* priest_t::get_target_data( player_t* target ) const
{
  priest_td_t*& td = _target_data[ target ];
  if ( !td )
  {
    td = new priest_td_t( target, const_cast<priest_t&>( *this ) );
  }
  return td;
}

void priest_t::init_special_effects()
{
  using trigger_type = dbc_proc_callback_t::trigger_fn_type;

  if ( talent.shadow_weaving.ok() )
  {
    const auto driver    = new special_effect_t( this );
    driver->name_str     = talent.shadow_weaving->name_cstr();
    driver->spell_id     = talent.shadow_weaving->id();
    driver->proc_chance_ = talent.shadow_weaving->effectN( 1 ).percent();
    special_effects.push_back( driver );

    callbacks.register_callback_trigger_function(
      driver->spell_id, trigger_type::CONDITION, []( auto, auto, auto, const action_state_t* s, auto ) {
        return dbc::is_school( s->action->get_school(), SCHOOL_SHADOW );
      } );

    callbacks.register_callback_execute_function( driver->spell_id, [ this ]( auto, auto, player_t* t, auto ) {
      get_target_data( t )->debuffs.shadow_weaving->trigger();
    } );

    new dbc_proc_callback_t( this, *driver );
  }

  if ( talent.searing_light.ok() )
  {
    const auto driver    = new special_effect_t( this );
    driver->name_str     = talent.searing_light->name_cstr();
    driver->spell_id     = talent.searing_light->id();
    driver->proc_chance_ = talent.searing_light->effectN( 2 ).percent();
    special_effects.push_back( driver );

    callbacks.register_callback_trigger_function(
      driver->spell_id, trigger_type::CONDITION, [ this ]( auto, auto, auto, const action_state_t* s, auto ) {
        return s->result_type == result_amount_type::DMG_OVER_TIME &&
               s->action->get_dot( s->target ) == get_target_data( s->target )->dots.holy_fire;
      } );

    callbacks.register_callback_execute_function( driver->spell_id, [ this ]( auto, auto, auto, auto ) {
      buff.holy_purpose->trigger();
    } );

    new dbc_proc_callback_t( this, *driver );
  }

  if ( spell.touch_of_weakness->ok() )
  {
    const auto driver = new special_effect_t( this );
    driver->name_str  = spell.touch_of_weakness->name_cstr();
    driver->spell_id  = spell.touch_of_weakness->id();
    special_effects.push_back( driver );

    // Proc events are delayed, so simultaneous melee hits can queue more than one before the buff expires
    callbacks.register_callback_execute_function( driver->spell_id, [ this ]( auto, auto, auto, action_state_t* s ) {
      if ( !buff.touch_of_weakness->check() )
        return;

      active.touch_of_weakness->execute_on_target( s->action->player );
      buff.touch_of_weakness->expire();
    } );

    auto cb = new dbc_proc_callback_t( this, *driver );
    cb->activate_with_buff( buff.touch_of_weakness );
  }

  parse_player_effects_t::init_special_effects();
}

void priest_t::activate()
{
  parse_player_effects_t::activate();

  if ( talent.devouring_contagion.ok() )
  {
    auto range   = talent.devouring_contagion->effectN( 2 ).base_value();
    auto targets = as<size_t>( talent.devouring_contagion->effectN( 3 ).base_value() );

    register_on_kill_callback( [ this, range, targets ]( player_t* t ) {
      if ( sim->event_mgr.canceled )
        return;

      auto source = get_target_data( t )->dots.devouring_plague;
      if ( !source->is_ticking() )
        return;

      auto tl = sim->target_non_sleeping_list.data();
      if ( sim->distance_targeting_enabled )
        range::erase_remove( tl, [ t, range ]( player_t* e ) { return e->get_player_distance( *t ) > range; } );

      // prioritize undotted over dotted
      rng().shuffle( tl.begin(), tl.end() );
      std::stable_partition( tl.begin(), tl.end(), [ this ]( player_t* e ) {
        return !get_target_data( e )->dots.devouring_plague->is_ticking();
      } );

      for ( size_t i = 0; i < std::min( targets, tl.size() ); i++ )
        source->copy( tl[ i ], DOT_COPY_START );
    } );
  }
}

// parse_effects does not parse party buffs with range TODO improve core to allow that?
double priest_t::composite_attribute_multiplier( attribute_e attr ) const
{
  double m = parse_player_effects_t::composite_attribute_multiplier( attr );

  if ( attr == ATTR_SPIRIT )
    m *= 1.0 + buff.dark_inspiration->check_value();

  return m;
}

void priest_t::parse_action_effects( action_t* action )
{
  auto _a = dynamic_cast<parse_action_base_t*>( action );
  assert( _a );

  _a->parse_effects( buff.holy_purpose, CONSUME_BUFF );
  _a->parse_effects( buff.inner_focus, CONSUME_BUFF );
  _a->parse_effects( buff.shadowform );
}

// Smite

constexpr std::array smite_ranks {
  spell_container_t().set_spell(   585 ),
  spell_container_t().set_spell(   591 ),
  spell_container_t().set_spell(   598 ),
  spell_container_t().set_spell(   984 ),
  spell_container_t().set_spell(  1004 ),
  spell_container_t().set_spell(  6060 ),
  spell_container_t().set_spell( 10933 ),
  spell_container_t().set_spell( 10934 ),
};
struct smite_t : public priest_spell_t
{
  smite_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "smite", player, player->find_rank( smite_ranks )->spell( player ) )
  {
    parse_options( options_str );
    parse_power_in_light();
  }
};

// Shadow Word: Pain

constexpr std::array shadow_word_pain_ranks {
  spell_container_t().set_spell(   589 ),
  spell_container_t().set_spell(   594 ),
  spell_container_t().set_spell(   970 ),
  spell_container_t().set_spell(   992 ),
  spell_container_t().set_spell(  2767 ),
  spell_container_t().set_spell( 10892 ),
  spell_container_t().set_spell( 10893 ),
  spell_container_t().set_spell( 10894 ),
};
struct shadow_word_pain_t : public priest_spell_t
{
  shadow_word_pain_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "shadow_word_pain", player, player->find_rank( shadow_word_pain_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

// Mind Blast

constexpr std::array mind_blast_ranks {
  spell_container_t().set_spell(  8092 ),
  spell_container_t().set_spell(  8102 ),
  spell_container_t().set_spell(  8103 ),
  spell_container_t().set_spell(  8104 ),
  spell_container_t().set_spell(  8105 ),
  spell_container_t().set_spell(  8106 ),
  spell_container_t().set_spell( 10945 ),
  spell_container_t().set_spell( 10946 ),
  spell_container_t().set_spell( 10947 ),
};
struct mind_blast_t : public priest_spell_t
{
  mind_blast_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "mind_blast", player, player->find_rank( mind_blast_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }

  void execute() override
  {
    priest_spell_t::execute();

    if ( p()->talent.dark_inspiration.ok() )
      p()->buff.dark_inspiration->trigger();
  }
};

// Devouring Plague

// TODO: Heal the caster for the damage dealt
constexpr std::array devouring_plague_ranks {
  spell_container_t().set_spell(  2944 ),
  spell_container_t().set_spell( 19276 ),
  spell_container_t().set_spell( 19277 ),
  spell_container_t().set_spell( 19278 ),
  spell_container_t().set_spell( 19279 ),
  spell_container_t().set_spell( 19280 ),
};
struct devouring_plague_t : public priest_spell_t
{
  devouring_plague_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "devouring_plague", player, player->find_rank( devouring_plague_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

// Mind Flay

constexpr std::array mind_flay_ranks {
  spell_container_t().set_spell( 15407 ),
  spell_container_t().set_spell( 17311 ),
  spell_container_t().set_spell( 17312 ),
  spell_container_t().set_spell( 17313 ),
  spell_container_t().set_spell( 17314 ),
  spell_container_t().set_spell( 18807 ),
};
struct mind_flay_t : public priest_spell_t
{
  mind_flay_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "mind_flay", player, player->talent.mind_flay.ok()
                                         ? player->find_rank( mind_flay_ranks )->spell( player )
                                         : spell_data_t::not_found() )
  {
    parse_options( options_str );
    channeled = true;

    if ( player->talent.improved_mind_flay.ok() )
      update_flags |= STATE_MUL_SPELL_TA;
  }

  double composite_ta_multiplier( const action_state_t* s ) const override
  {
    double m = priest_spell_t::composite_ta_multiplier( s );

    if ( p()->talent.improved_mind_flay.ok() )
    {
      const dot_t* dot = find_dot( s->target );
      int ticks        = dot ? std::max( dot->current_tick - 1, 0 ) : 0;
      m *= std::pow( 1.0 + p()->talent.improved_mind_flay->effectN( 1 ).percent(), ticks );
    }

    return m;
  }

  void last_tick( dot_t* d ) override
  {
    priest_spell_t::last_tick( d );

    if ( p()->talent.dark_inspiration.ok() && d->current_tick == d->num_ticks() )
      p()->buff.dark_inspiration->trigger();
  }
};

// Shadow Word: Death

// TODO: Backlash damage, effect 2
constexpr std::array shadow_word_death_ranks {
  spell_container_t().set_spell( 1309595 ),
  spell_container_t().set_spell( 1309633 ),
  spell_container_t().set_spell( 1309635 ),
  spell_container_t().set_spell( 1309636 ),
};
struct shadow_word_death_t : public priest_spell_t
{
  shadow_word_death_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "shadow_word_death", player, player->find_rank( shadow_word_death_ranks )->spell( player ) )
  {
    parse_options( options_str );

    if ( player->talent.early_demise.ok() )
    {
      const auto& crit = player->talent.early_demise->effectN( 1 );
      auto health_pct  = player->talent.early_demise->effectN( 2 ).base_value();

      add_parse_entry( target_crit_chance_effects )
        .set_func( [ health_pct ]( actor_target_data_t* td ) { return td->target->health_percentage() <= health_pct; } )
        .set_value( crit.percent() )
        .set_eff( &crit );
    }
  }
};

// Holy Fire

constexpr std::array holy_fire_ranks {
  spell_container_t().set_spell( 14914 ),
  spell_container_t().set_spell( 15262 ),
  spell_container_t().set_spell( 15263 ),
  spell_container_t().set_spell( 15264 ),
  spell_container_t().set_spell( 15265 ),
  spell_container_t().set_spell( 15266 ),
  spell_container_t().set_spell( 15267 ),
  spell_container_t().set_spell( 15261 ), // Rank 8 is 15261 for some reason
};
struct holy_fire_t : public priest_spell_t
{
  holy_fire_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "holy_fire", player, player->find_rank( holy_fire_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

// Penance TODO: Check Penance vs Inner Focus - Right now only first bolt gets the crit bonus

// driver: channel, attack: damage bolt
constexpr std::array penance_ranks {
  spell_container_t().set_spell(  402174 ).set_driver(  402261 ).set_attack(  402284 ),
  spell_container_t().set_spell( 1240720 ).set_driver( 1240734 ).set_attack( 1240727 ),
  spell_container_t().set_spell( 1240721 ).set_driver( 1240736 ).set_attack( 1240730 ),
  spell_container_t().set_spell( 1316995 ).set_driver( 1316994 ).set_attack( 1316993 ),
};

struct penance_damage_t : public priest_spell_t
{
  penance_damage_t( priest_t* player ) :
    priest_spell_t( "penance_damage", player, player->find_rank( penance_ranks )->attack( player ) )
  {
    background = true;
    parse_power_in_light();
  }
};

struct penance_t : public priest_spell_t
{
  penance_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "penance", player, player->talent.penance.ok()
                                       ? player->find_rank( penance_ranks )->spell( player )
                                       : spell_data_t::not_found() )
  {
    parse_options( options_str );
    channeled = true;
    may_miss = may_crit = false;

    parse_effect_data( player->find_rank( penance_ranks )->driver( player )->effectN( 2 ) );
    tick_action = new penance_damage_t( player );
  }
};

// Holy Nova

// TODO: Party heal
constexpr std::array holy_nova_ranks {
  spell_container_t().set_spell( 15237 ),
  spell_container_t().set_spell( 15430 ),
  spell_container_t().set_spell( 15431 ),
  spell_container_t().set_spell( 27799 ),
  spell_container_t().set_spell( 27800 ),
  spell_container_t().set_spell( 27801 ),
};
struct holy_nova_t : public priest_spell_t
{
  holy_nova_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "holy_nova", player, player->talent.holy_nova.ok()
                                         ? player->find_rank( holy_nova_ranks )->spell( player )
                                         : spell_data_t::not_found() )
  {
    parse_options( options_str );
    aoe = -1;
  }
};

// Starshards

constexpr std::array starshards_ranks {
  spell_container_t().set_spell( 10797 ),
  spell_container_t().set_spell( 19296 ),
  spell_container_t().set_spell( 19299 ),
  spell_container_t().set_spell( 19302 ),
  spell_container_t().set_spell( 19303 ),
  spell_container_t().set_spell( 19304 ),
  spell_container_t().set_spell( 19305 ),
};
struct starshards_t : public priest_spell_t
{
  starshards_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "starshards", player, player->find_racial_rank( starshards_ranks ) )
  {
    parse_options( options_str );
    channeled = true;
  }
};

// Dark Sacrifice

// TODO: Health cost, effect 1
constexpr std::array dark_sacrifice_ranks {
  spell_container_t().set_spell( 1277324 ),
  spell_container_t().set_spell( 1277325 ),
  spell_container_t().set_spell( 1277326 ),
  spell_container_t().set_spell( 1277327 ),
  spell_container_t().set_spell( 1277328 ),
};
struct dark_sacrifice_t : public priest_spell_t
{
  // TODO: Check if spirit is snapshot on cast or updates dynamically
  double spirit_per_tick = 0.0;

  dark_sacrifice_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "dark_sacrifice", player, player->find_racial_rank( dark_sacrifice_ranks ) )
  {
    parse_options( options_str );
    harmful = false;
    target = player;
  }

  void execute() override
  {
    spirit_per_tick = player->cache.spirit() / ( dot_duration / base_tick_time );

    priest_spell_t::execute();
  }

  double composite_energize_amount( const action_state_t* s ) const override
  {
    return priest_spell_t::composite_energize_amount( s ) + spirit_per_tick;
  }
};

// Vampiric Embrace

// TODO: Party heal
struct vampiric_embrace_t : public priest_spell_t
{
  vampiric_embrace_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "vampiric_embrace", player, player->talent.vampiric_embrace.ok()
                                                ? player->talent.vampiric_embrace.spell()
                                                : spell_data_t::not_found() )
  {
    parse_options( options_str );
  }

  void impact( action_state_t* s ) override
  {
    priest_spell_t::impact( s );

    if ( result_is_hit( s->result ) )
      p()->get_target_data( s->target )->debuffs.vampiric_embrace->trigger();
  }
};

// Hex of Weakness

struct hex_of_weakness_t : public priest_spell_t
{
  hex_of_weakness_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "hex_of_weakness", player, player->spell.hex_of_weakness )
  {
    parse_options( options_str );
  }

  void impact( action_state_t* s ) override
  {
    priest_spell_t::impact( s );

    if ( result_is_hit( s->result ) )
      p()->get_target_data( s->target )->debuffs.hex_of_weakness->trigger();
  }
};

// Touch of Weakness

struct touch_of_weakness_damage_t : public priest_spell_t
{
  touch_of_weakness_damage_t( priest_t* player ) :
    priest_spell_t( "touch_of_weakness_damage", player, player->spell.touch_of_weakness_debuff )
  {
    background = true;
  }

  void impact( action_state_t* s ) override
  {
    priest_spell_t::impact( s );

    if ( result_is_hit( s->result ) )
      p()->get_target_data( s->target )->debuffs.touch_of_weakness->trigger();
  }
};

struct touch_of_weakness_t : public priest_spell_t
{
  touch_of_weakness_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "touch_of_weakness", player, player->spell.touch_of_weakness )
  {
    parse_options( options_str );
    harmful = false;
    target = player;
  }

  void execute() override
  {
    priest_spell_t::execute();

    p()->buff.touch_of_weakness->trigger();
  }

  bool ready() override
  {
    if ( p()->buff.touch_of_weakness->check() )
      return false;

    return priest_spell_t::ready();
  }
};

// Inner Focus

struct inner_focus_t : public priest_spell_t
{
  inner_focus_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "inner_focus", player, player->talent.inner_focus.ok()
                                           ? player->talent.inner_focus.spell()
                                           : spell_data_t::not_found() )
  {
    parse_options( options_str );
    harmful = false;
    target = player;
    cooldown->duration = 0_ms;
  }

  void execute() override
  {
    priest_spell_t::execute();

    p()->buff.inner_focus->trigger();
  }

  bool ready() override
  {
    if ( p()->buff.inner_focus->check() )
      return false;

    return priest_spell_t::ready();
  }
};

// Shadowform

struct shadowform_t : public priest_spell_t
{
  shadowform_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "shadowform", player, player->talent.shadowform.ok()
                                          ? player->talent.shadowform.spell()
                                          : spell_data_t::not_found() )
  {
    parse_options( options_str );
    harmful = false;
    target = player;
  }

  void execute() override
  {
    priest_spell_t::execute();

    p()->buff.shadowform->trigger();
  }

  bool ready() override
  {
    if ( p()->buff.shadowform->check() )
      return false;

    return priest_spell_t::ready();
  }
};

action_t* priest_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "smite" )
    return new smite_t( this, options_str );
  if ( name == "shadow_word_pain" )
    return new shadow_word_pain_t( this, options_str );
  if ( name == "mind_blast" )
    return new mind_blast_t( this, options_str );
  if ( name == "devouring_plague" )
    return new devouring_plague_t( this, options_str );
  if ( name == "mind_flay" )
    return new mind_flay_t( this, options_str );
  if ( name == "shadow_word_death" )
    return new shadow_word_death_t( this, options_str );
  if ( name == "holy_fire" )
    return new holy_fire_t( this, options_str );
  if ( name == "penance" )
    return new penance_t( this, options_str );
  if ( name == "holy_nova" )
    return new holy_nova_t( this, options_str );
  if ( name == "starshards" )
    return new starshards_t( this, options_str );
  if ( name == "dark_sacrifice" )
    return new dark_sacrifice_t( this, options_str );
  if ( name == "vampiric_embrace" )
    return new vampiric_embrace_t( this, options_str );
  if ( name == "hex_of_weakness" )
    return new hex_of_weakness_t( this, options_str );
  if ( name == "touch_of_weakness" )
    return new touch_of_weakness_t( this, options_str );
  if ( name == "inner_focus" )
    return new inner_focus_t( this, options_str );
  if ( name == "shadowform" )
    return new shadowform_t( this, options_str );

  return parse_player_effects_t::create_action( name, options_str );
}

void priest_t::create_actions()
{
  if ( spell.touch_of_weakness->ok() )
    active.touch_of_weakness = new touch_of_weakness_damage_t( this );

  parse_player_effects_t::create_actions();
}

void priest_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    parse_player_effects_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  priest_apl::priest( this );
  use_default_action_list = true;
  parse_player_effects_t::init_action_list();
}

}  // namespace priestspace

const module_t* module_t::priest()
{
  static priestspace::priest_module_t m;
  return &m;
}