// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.hpp"
#include "action/parse_effects.hpp"
#include "class_modules/apl/druid.hpp"

namespace
{  // UNNAMED NAMESPACE
// ==========================================================================
// Druid
// ==========================================================================
struct druid_t;
struct druid_td_t final : public actor_target_data_t
{
  struct debuffs_t
  {
    buff_t* hurricane;
    buff_t* faerie_fire;
  } debuff;

  druid_td_t( player_t& target, druid_t& p );

  template <typename Buff = buff_t, typename... Args>
  inline buff_t* make_debuff( bool b, Args&&... args );
};

struct druid_t final : public parse_player_effects_t
{
  druid_t( sim_t* sim, std::string_view name, race_e r = RACE_NIGHT_ELF )
    : parse_player_effects_t( sim, DRUID, name, r ) {}

  struct talents_t
  {
    player_talent_t improved_entangling_roots;
  } talent;

  struct buffs_t
  {
    buff_t* innervate;
    buff_t* clearcasting;
  } buff;

  resource_e primary_resource() const override { return RESOURCE_MANA; }

  void init_base_stats() override;
  void init_spells() override;
  void init_action_list() override;
  void create_buffs() override;
  void init_special_effects() override;
  void init_finished() override;
  void parse_player_effects();
  void apply_action_effects( action_t* a );

  std::string default_potion() const override { return druid_apl::potion( this ); }
  std::string default_flask() const override  { return druid_apl::flask( this ); }
  std::string default_food() const override   { return druid_apl::food( this ); }

  target_specific_t<druid_td_t> target_data;

  action_t* create_action( std::string_view name, std::string_view options_str ) override;

  druid_td_t* get_target_data( player_t* target ) const override
  {
    assert( target );
    druid_td_t*& td = target_data[ target ];
    if ( !td )
      td = new druid_td_t( *target, const_cast<druid_t&>( *this ) );

    return td;
  }

};

// DRUID MODULE INTERFACE ===================================================
struct druid_module_t final : public module_t
{
  druid_module_t() : module_t( DRUID ) {}

  player_t* create_player( sim_t* sim, std::string_view name, race_e r = RACE_NONE ) const override
  {
    auto p = new druid_t( sim, name, r );
    return p;
  }

  bool valid() const override { return true; }

  void register_hotfixes() const override {}

  void register_actor_initializers( sim_t* ) const override {}
};

template <class Base>
struct druid_action_t : public parse_action_effects_t<Base>
{
  using action_base_t = parse_action_effects_t<Base>;
  using base_t = druid_action_t<Base>;

  template <typename... Args>
  druid_action_t( std::string_view n, druid_t* p, const spell_data_t* s = spell_data_t::nil() )
    : action_base_t( n, p, s ) 
  {
    if ( this->data().ok() )
    {
      p->apply_action_effects( this );
    }
  }

  druid_t* p()
  { return debug_cast<druid_t*>( this->player ); }

  const druid_t* p() const
  { return debug_cast<const druid_t*>( this->player ); }

  void execute() override
  {
    action_base_t::execute();
    
    if ( this->data().affected_by( this->p()->buff.clearcasting->data() ) )
    {
      this->p()->buff.clearcasting->expire();
    }
  }
};

using druid_spell_t = druid_action_t<spell_t>;
using druid_melee_attack_t = druid_action_t<melee_attack_t>;

void druid_t::init_base_stats()
{
  parse_player_effects_t::init_base_stats();

  // TODO: plug in crit
}

void druid_t::init_spells()
{
  parse_player_effects_t::init_spells();

  parse_all_class_passives();
  parse_all_passive_talents();

  talent.improved_entangling_roots = find_talent_spell( talent_tree::CLASS, "Improved Entangling Roots" );
}

// ==========================================================================
// Druid Spells
// ==========================================================================
constexpr std::array wrath_ranks {
  spell_container_t().set_spell( 5176 ),
  spell_container_t().set_spell( 5177 ),
  spell_container_t().set_spell( 5178 ),
  spell_container_t().set_spell( 5179 ),
  spell_container_t().set_spell( 5180 ),
  spell_container_t().set_spell( 6780 ),
  spell_container_t().set_spell( 8905 ),
  spell_container_t().set_spell( 9912 ),
};

struct wrath_t : public druid_spell_t
{
  wrath_t( druid_t* player, std::string_view options_str ) :
    druid_spell_t( "wrath", player, player->find_rank( wrath_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

constexpr std::array moonfire_ranks {
  spell_container_t().set_spell( 8921 ),
  spell_container_t().set_spell( 8924 ),
  spell_container_t().set_spell( 8925 ),
  spell_container_t().set_spell( 8926 ),
  spell_container_t().set_spell( 8927 ),
  spell_container_t().set_spell( 8928 ),
  spell_container_t().set_spell( 8929 ),
  spell_container_t().set_spell( 9833 ),
  spell_container_t().set_spell( 9834 ),
  spell_container_t().set_spell( 9835 ),
};
struct moonfire_t : public druid_spell_t
{
  moonfire_t( druid_t* player, std::string_view options_str ) :
    druid_spell_t( "moonfire", player, player->find_rank( moonfire_ranks )->spell( player ) )
  {
    parse_options( options_str );
    // TODO moonfire dot reporting
  }
};

constexpr std::array starfire_ranks {
  spell_container_t().set_spell(  2912 ),
  spell_container_t().set_spell(  8949 ),
  spell_container_t().set_spell(  8950 ),
  spell_container_t().set_spell(  8951 ),
  spell_container_t().set_spell(  9875 ),
  spell_container_t().set_spell(  9876 ),
  spell_container_t().set_spell( 25298 ),
};
struct starfire_t : public druid_spell_t
{
  starfire_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "starfire", player, player->find_rank( starfire_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

constexpr std::array entangling_roots_ranks{
    spell_container_t().set_spell( 339 ), // 19975 is also a rank 1 spell id, it is used by nature's grasp
    spell_container_t().set_spell( 1062 ),
    spell_container_t().set_spell( 5195 ),
    spell_container_t().set_spell( 5196 ),
    spell_container_t().set_spell( 9852 ),
    spell_container_t().set_spell( 9853 ),
};
struct entangling_roots_t : public druid_spell_t
{
  entangling_roots_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "entangling_roots", player, player->find_rank( entangling_roots_ranks )->spell( player ) )
  {
    parse_options( options_str );
    // TODO: verify periodic can crit
    break_chance = break_chance / ( 1.0 + p()->talent.improved_entangling_roots->effectN( 1 ).percent() );
  }

  void tick( dot_t* d ) override
  {
    druid_spell_t::tick( d );
  }

  // TODO: Didn't get nearly enough data from beta to determine if this is the right value
  // 20% generates a geometric distribution with a mean of 5 hits
  // It is unlikely that roots is used in a real apl, but some model for the dot and break is needed for testing
  double break_chance = .2;
};

constexpr std::array hurricane_ranks{
    spell_container_t().set_spell( 16914 ).set_debuff( 1278965 ),
    spell_container_t().set_spell( 17401 ).set_debuff( 1278968 ),
    spell_container_t().set_spell( 17402 ).set_debuff( 1278759 ),
};
struct hurricane_tick_t : public druid_spell_t
{
  hurricane_tick_t( druid_t* player )
    : druid_spell_t( "hurricane_tick", player, player->find_rank( hurricane_ranks )->debuff( player ) )
  {
    background = dual = true;
    aoe               = -1;
  }

  void impact( action_state_t* s ) override
  {
    druid_spell_t::impact( s );

    // TODO: the damage spell has no duration; confirm how long the slow lasts after the last tick
    p()->get_target_data( s->target )->debuff.hurricane->trigger( 1_s );
  }
};

struct hurricane_t : public druid_spell_t
{
  hurricane_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "hurricane", player, player->find_rank( hurricane_ranks )->spell( player ) )
  {
    parse_options( options_str );
    channeled   = true;
    tick_action = new hurricane_tick_t( player );
    // debuff applies instantly, but the first tick is delayed by 1 second
  }
};

constexpr std::array faerie_fire_ranks{
    spell_container_t().set_spell( 770 ),
    spell_container_t().set_spell( 778 ),
    spell_container_t().set_spell( 9749 ),
    spell_container_t().set_spell( 9907 ),
};
struct faerie_fire_t : public druid_spell_t
{
  faerie_fire_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "faerie_fire", player, player->find_rank( faerie_fire_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }

  void impact( action_state_t* s ) override
  {
    druid_spell_t::impact( s );

    p()->get_target_data( s->target )->debuff.faerie_fire->trigger();
  }
};

constexpr std::array innervate_ranks{
    spell_container_t().set_spell( 29166 ),
}; 
 struct innervate_t : public druid_spell_t
{
  innervate_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "innervate", player, player->find_rank( innervate_ranks)->spell( player) )
  {
    parse_options( options_str );
    harmful = false;
    // TODO: verify against live data
    // assumed that the regen buff applies only to spirit regen
  }

  void impact( action_state_t* s ) override
  {
    druid_spell_t::impact( s );

    p()->buff.innervate->trigger();
  }
};

constexpr std::array omen_of_clarity_ranks{
    spell_container_t().set_spell( 16864 ).set_buff( 16870 ),
};

// ==========================================================================
// Druid Attacks
// ==========================================================================
struct druid_melee_t : public druid_melee_attack_t
{
  druid_melee_t( std::string_view name, druid_t* player, weapon_t* w )
    : druid_melee_attack_t( name, player, spell_data_t::nil() )
  {
    weapon_multiplier = 1.0;
    school            = SCHOOL_PHYSICAL;
    trigger_gcd       = 0_ms;

    background = repeating = allow_class_ability_procs = not_a_proc = true;
    may_glance = may_crit = may_dodge = may_parry  = true;
    special    = false;
  }
};

struct auto_attack_t : public druid_melee_attack_t
{
  auto_attack_t( druid_t* p, std::string_view options_str )
    : druid_melee_attack_t( "auto_attack", p, spell_data_t::nil() )
  {
    parse_options( options_str );
    trigger_gcd           = 0_ms;

    assert( p->equipped_weapons[ SLOT_MAIN_HAND ].type != WEAPON_NONE );

    p->main_hand_attack = new druid_melee_t( "auto_attack_mh", p, &p->equipped_weapons[ SLOT_MAIN_HAND ] );
    p->main_hand_attack->weapon = &p->equipped_weapons[ SLOT_MAIN_HAND ];
    p->main_hand_attack->base_execute_time = p->equipped_weapons[ SLOT_MAIN_HAND ].swing_time;

    if ( p->equipped_weapons[ SLOT_OFF_HAND ].type != WEAPON_NONE )
    {
      p->off_hand_attack         = new druid_melee_t( "auto_attack_oh", p, &p->equipped_weapons[ SLOT_OFF_HAND ] );
      p->off_hand_attack->weapon = &p->equipped_weapons[ SLOT_OFF_HAND ];
      p->off_hand_attack->base_execute_time = p->equipped_weapons[ SLOT_OFF_HAND ].swing_time;
    }
  }

  void execute() override
  {
    player->main_hand_attack->schedule_execute();
    if ( player->off_hand_attack )
    {
      player->off_hand_attack->schedule_execute();
    }
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

action_t* druid_t::create_action( std::string_view name, std::string_view options_str )
{
  if ( name == "auto_attack" )
    return new auto_attack_t( this, options_str );
  if ( name == "wrath" )
    return new wrath_t( this, options_str );
  if ( name == "moonfire" )
    return new moonfire_t( this, options_str );
  if ( name == "starfire" )
    return new starfire_t( this, options_str );
  if ( name == "entangling_roots" )
    return new entangling_roots_t( this, options_str );
  if ( name == "hurricane" )
    return new hurricane_t( this, options_str );
  if ( name == "faerie_fire" )
    return new faerie_fire_t( this, options_str );
  if ( name == "innervate" )
    return new innervate_t( this, options_str );

  return parse_player_effects_t::create_action( name, options_str );
}

void druid_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    parse_player_effects_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  druid_apl::druid( this );
  use_default_action_list = true;
  parse_player_effects_t::init_action_list();
}

template <typename Buff, typename... Args>
buff_t* druid_td_t::make_debuff( bool b, Args&&... args )
{
  return buff_t::make_buff_fallback<Buff>( target->is_enemy() && b, std::forward<Args>( args )... );
}
inline druid_td_t::druid_td_t( player_t& target, druid_t& p ) : actor_target_data_t( &target, &p )
{
  auto const hurricane_debuff = p.find_rank( hurricane_ranks );
  debuff.hurricane = make_debuff( hurricane_debuff->spell( &p )->ok(), *this, "hurricane",
                                  hurricane_debuff->debuff( &p ) )
    ->set_default_value (hurricane_debuff->debuff( &p )->effectN(2).percent() );

  auto const faerie_fire_debuff = p.find_rank( faerie_fire_ranks );
  debuff.faerie_fire =
      make_debuff<stat_buff_t>( faerie_fire_debuff->spell( &p )->ok(), *this, "faerie_fire", faerie_fire_debuff->spell( &p ))
    ->set_default_value_from_effect( 1 );
}

template <typename Buff, typename... Args>
static inline buff_t* make_fallback( Args&&... args )
{
  return buff_t::make_buff_fallback<Buff>( true, std::forward<Args>( args )... );
};

void druid_t::create_buffs()
{
  player_t::create_buffs();

  auto const innervate_buff = find_rank( innervate_ranks )->spell( this );
  buff.innervate            = make_buff_fallback( innervate_buff->ok(), this, "innervate", innervate_buff );

  auto const omen_of_clarity_buff = find_rank( omen_of_clarity_ranks )->buff( this );
  buff.clearcasting = make_buff_fallback( omen_of_clarity_buff->ok(), this, "clearcasting", omen_of_clarity_buff );
};

void druid_t::init_special_effects()
{
  auto const omen_of_clarity = find_rank( omen_of_clarity_ranks )->spell( this );
  if ( omen_of_clarity->ok() )
  {
    const auto driver = new special_effect_t( this );
    driver->name_str  = omen_of_clarity->name_cstr();
    driver->spell_id  = omen_of_clarity->id();
    driver->ppm_ = 2.0; // TODO 2 PPM is an educated guess based on old comments from classic. Must be reverified but beta logs were not big enough.
    special_effects.push_back( driver );

    callbacks.register_callback_execute_function( driver->spell_id, [ this ]( auto, auto, auto, auto ) {
      buff.clearcasting->trigger();
    } );

    new dbc_proc_callback_t( this, *driver );
  }

  parse_player_effects_t::init_special_effects();
}

void druid_t::init_finished()
{
  parse_player_effects();

  player_t::init_finished();
}

void druid_t::parse_player_effects() 
{
  parse_effects( buff.innervate );
};

void druid_t::apply_action_effects( action_t* a )
{
  auto action = dynamic_cast<parse_action_base_t*>( a );
  assert( action );

  action->parse_effects( buff.clearcasting );
}

}  // UNNAMED NAMESPACE

const module_t* module_t::druid()
{
  static druid_module_t m;
  return &m;
}
