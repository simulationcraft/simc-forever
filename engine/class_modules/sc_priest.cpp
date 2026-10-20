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
  struct debuffs_t
  {
    buff_t* shadow_weaving = nullptr;
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

  priest_td_t* get_target_data( player_t* target ) const override;

  std::string default_potion() const override   { return priest_apl::potion( this ); }
  std::string default_flask() const override    { return priest_apl::flask( this ); }
  std::string default_food() const override     { return priest_apl::food( this ); }

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void init_action_list() override;

  struct buffs_t
  {
    buff_t* inner_focus = nullptr;
    buff_t* shadowform = nullptr;
  } buff;

  struct talents_t
  {
    player_talent_t early_demise;
    player_talent_t inner_focus;
    player_talent_t mind_flay;
    player_talent_t shadow_weaving;
    player_talent_t shadowform;
    player_talent_t wand_specialization;
  } talent;

  struct spells_t
  {
    const spell_data_t* shadow_weaving = spell_data_t::nil();
  } spell;

private:
  target_specific_t<priest_td_t> _target_data;
};

priest_td_t::priest_td_t( player_t* target, priest_t& p ) : actor_target_data_t( target, &p )
{
  debuffs.shadow_weaving = make_buff( *this, "shadow_weaving", p.spell.shadow_weaving );
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
};

using priest_spell_t = priest_action_t<spell_t>;

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

  talent.early_demise   = find_talent_spell( talent_tree::CLASS, "Early Demise" );
  talent.inner_focus    = find_talent_spell( talent_tree::CLASS, "Inner Focus" );
  talent.mind_flay      = find_talent_spell( talent_tree::CLASS, "Mind Flay" );
  talent.shadow_weaving = find_talent_spell( talent_tree::CLASS, "Shadow Weaving" );
  talent.shadowform     = find_talent_spell( talent_tree::CLASS, "Shadowform" );

  // Wand only, all schools because wand damage can be any school
  // TODO: Apply to the wand auto attack once implemented
  talent.wand_specialization = find_talent_spell( talent_tree::CLASS, "Wand Specialization" );
  deregister_passive_spell( talent.wand_specialization );

  spell.shadow_weaving = talent.shadow_weaving->effectN( 1 ).trigger();
}

void priest_t::create_buffs()
{
  parse_player_effects_t::create_buffs();

  // Cooldown starts when the buff is consumed
  buff.inner_focus = make_buff( this, "inner_focus", talent.inner_focus )
    ->set_cooldown( 0_ms )
    ->set_stack_change_callback( [ this ]( buff_t*, int, int new_ ) {
      if ( !new_ )
        get_cooldown( "inner_focus" )->start( talent.inner_focus->cooldown() );
    } );

  buff.shadowform = make_buff( this, "shadowform", talent.shadowform );

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

  parse_player_effects_t::init_special_effects();
}

void priest_t::parse_action_effects( action_t* action )
{
  auto _a = dynamic_cast<parse_action_base_t*>( action );
  assert( _a );

  _a->parse_effects( buff.inner_focus, CONSUME_BUFF );
  _a->parse_effects( buff.shadowform );
}

// Smite

constexpr std::array smite_ranks {
  //            spell
  spell_rank_t{   585 },
  spell_rank_t{   591 },
  spell_rank_t{   598 },
  spell_rank_t{   984 },
  spell_rank_t{  1004 },
  spell_rank_t{  6060 },
  spell_rank_t{ 10933 },
  spell_rank_t{ 10934 },
};
struct smite_t : public priest_spell_t
{
  smite_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "smite", player, player->find_spell( player->find_rank( smite_ranks ).spell_id ) )
  {
    parse_options( options_str );
  }
};

// Shadow Word: Pain

constexpr std::array shadow_word_pain_ranks {
  //            spell
  spell_rank_t{   589 },
  spell_rank_t{   594 },
  spell_rank_t{   970 },
  spell_rank_t{   992 },
  spell_rank_t{  2767 },
  spell_rank_t{ 10892 },
  spell_rank_t{ 10893 },
  spell_rank_t{ 10894 },
};
struct shadow_word_pain_t : public priest_spell_t
{
  shadow_word_pain_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "shadow_word_pain", player, player->find_spell( player->find_rank( shadow_word_pain_ranks ).spell_id ) )
  {
    parse_options( options_str );
  }
};

// Mind Blast

constexpr std::array mind_blast_ranks {
  //            spell
  spell_rank_t{  8092 },
  spell_rank_t{  8102 },
  spell_rank_t{  8103 },
  spell_rank_t{  8104 },
  spell_rank_t{  8105 },
  spell_rank_t{  8106 },
  spell_rank_t{ 10945 },
  spell_rank_t{ 10946 },
  spell_rank_t{ 10947 },
};
struct mind_blast_t : public priest_spell_t
{
  mind_blast_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "mind_blast", player, player->find_spell( player->find_rank( mind_blast_ranks ).spell_id ) )
  {
    parse_options( options_str );
  }
};

// Devouring Plague

// TODO: Heal the caster for the damage dealt
constexpr std::array devouring_plague_ranks {
  //            spell
  spell_rank_t{  2944 },
  spell_rank_t{ 19276 },
  spell_rank_t{ 19277 },
  spell_rank_t{ 19278 },
  spell_rank_t{ 19279 },
  spell_rank_t{ 19280 },
};
struct devouring_plague_t : public priest_spell_t
{
  devouring_plague_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "devouring_plague", player, player->find_spell( player->find_rank( devouring_plague_ranks ).spell_id ) )
  {
    parse_options( options_str );
  }
};

// Mind Flay

constexpr std::array mind_flay_ranks {
  //            spell
  spell_rank_t{ 15407 },
  spell_rank_t{ 17311 },
  spell_rank_t{ 17312 },
  spell_rank_t{ 17313 },
  spell_rank_t{ 17314 },
  spell_rank_t{ 18807 },
};
struct mind_flay_t : public priest_spell_t
{
  mind_flay_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "mind_flay", player, player->talent.mind_flay.ok()
                                         ? player->find_spell( player->find_rank( mind_flay_ranks ).spell_id )
                                         : spell_data_t::not_found() )
  {
    parse_options( options_str );
    channeled = true;
  }
};

// Shadow Word: Death

// TODO: Backlash damage, effect 2
constexpr std::array shadow_word_death_ranks {
  //              spell
  spell_rank_t{ 1309595 },
  spell_rank_t{ 1309633 },
  spell_rank_t{ 1309635 },
  spell_rank_t{ 1309636 },
};
struct shadow_word_death_t : public priest_spell_t
{
  shadow_word_death_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "shadow_word_death", player, player->find_spell( player->find_rank( shadow_word_death_ranks ).spell_id ) )
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
  //            spell
  spell_rank_t{ 14914 },
  spell_rank_t{ 15262 },
  spell_rank_t{ 15263 },
  spell_rank_t{ 15264 },
  spell_rank_t{ 15265 },
  spell_rank_t{ 15266 },
  spell_rank_t{ 15267 },
  spell_rank_t{ 15261 }, // Rank 8 is 15261 for some reason
};
struct holy_fire_t : public priest_spell_t
{
  holy_fire_t( priest_t* player, util::string_view options_str ) :
    priest_spell_t( "holy_fire", player, player->find_spell( player->find_rank( holy_fire_ranks ).spell_id ) )
  {
    parse_options( options_str );
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
  if ( name == "inner_focus" )
    return new inner_focus_t( this, options_str );
  if ( name == "shadowform" )
    return new shadowform_t( this, options_str );

  return parse_player_effects_t::create_action( name, options_str );
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