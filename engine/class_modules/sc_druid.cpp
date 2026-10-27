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
struct druid_t final : public parse_player_effects_t
{
  druid_t( sim_t* sim, std::string_view name, race_e r = RACE_NIGHT_ELF )
    : parse_player_effects_t( sim, DRUID, name, r ) {}

  resource_e primary_resource() const override { return RESOURCE_MANA; }

  void init_base_stats() override;
  void init_spells() override;
  void init_action_list() override;

  std::string default_potion() const override { return druid_apl::potion( this ); }
  std::string default_flask() const override  { return druid_apl::flask( this ); }
  std::string default_food() const override   { return druid_apl::food( this ); }

  action_t* create_action( std::string_view name, std::string_view options_str ) override;

  struct talents_t
  {
    player_talent_t improved_entangling_roots;
  } talent;
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
  druid_action_t( Args&&... args ) : action_base_t( std::forward<Args>( args )... ) {}

  druid_t* p()
  { return debug_cast<druid_t*>( this->player ); }

  const druid_t* p() const
  { return debug_cast<const druid_t*>( this->player ); }
};

using druid_spell_t = druid_action_t<spell_t>;
using druid_attack_t = druid_action_t<attack_t>;

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
};

struct hurricane_t : public druid_spell_t
{
  hurricane_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "hurricane", player, player->find_rank( hurricane_ranks )->spell( player ) )
  {
    parse_options( options_str );
    channeled   = true;
    tick_action = new hurricane_tick_t( player );
  }
};

// ==========================================================================
// Druid Attacks
// ==========================================================================
struct druid_melee_t : public druid_attack_t
{
  druid_melee_t( std::string_view name, player_t* player, weapon_t* w )
    : druid_attack_t( name, player, spell_data_t::nil() )
  {
    weapon_multiplier = 1.0;
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
};

struct auto_attack_t : public druid_attack_t
{
  auto_attack_t( druid_t* p, std::string_view options_str )
    : druid_attack_t( "auto_attack", p, spell_data_t::nil() )
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
}  // UNNAMED NAMESPACE

const module_t* module_t::druid()
{
  static druid_module_t m;
  return &m;
}
