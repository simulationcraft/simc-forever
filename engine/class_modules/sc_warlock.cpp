#include "simulationcraft.hpp"
#include "action/parse_effects.hpp"

#include "class_modules/apl/warlock.hpp"

namespace warlock
{
struct warlock_t final : public player_t
{
  warlock_t( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) :
    player_t( sim, WARLOCK, name, r ) {}

  resource_e primary_resource() const override { return RESOURCE_MANA; }

  std::string default_potion() const override   { return warlock_apl::potion( this ); }
  std::string default_flask() const override    { return warlock_apl::flask( this ); }
  std::string default_food() const override     { return warlock_apl::food( this ); }

  action_t* create_action( util::string_view name, util::string_view options_str ) override;
  void init_action_list() override;
  void init_base_stats() override;
  void init_spells() override;

  void create_pets() override;

  struct pets_t
  {
    pet_t* imp = nullptr;
  } pets;
};

struct warlock_module_t : public module_t
{
  warlock_module_t() : module_t( WARLOCK )
  { }

  player_t* create_player( sim_t* sim, util::string_view name, race_e r = RACE_NONE ) const override
  {
    auto p = new warlock_t( sim, name, r );
    return p;
  }

  bool valid() const override
  { return true; }

  void register_hotfixes() const override {}

  void register_actor_initializers( sim_t* ) const override {}
};

template <class Base>
struct warlock_action_t : public parse_action_effects_t<Base>
{
private:
  using ab = parse_action_effects_t<Base>;

public:
  using base_t = warlock_action_t<Base>;

  template <typename... Args>
  warlock_action_t( Args&&... args ) : ab( std::forward<Args>( args )... ) {}

  warlock_t* p()
  { return debug_cast<warlock_t*>( this->player ); }

  const warlock_t* p() const
  { return debug_cast<const warlock_t*>( this->player ); }
};

using warlock_spell_t  = warlock_action_t<spell_t>;

// Shadow Bolt

constexpr std::array shadow_bolt_ranks {
  spell_container_t().set_spell(   686 ),
  spell_container_t().set_spell(   695 ),
  spell_container_t().set_spell(   705 ),
  spell_container_t().set_spell(  1088 ),
  spell_container_t().set_spell(  1106 ),
  spell_container_t().set_spell(  7641 ),
  spell_container_t().set_spell( 11659 ),
  spell_container_t().set_spell( 11660 ),
  spell_container_t().set_spell( 11661 ),
  spell_container_t().set_spell( 25307 ),
};
struct shadow_bolt_t : public warlock_spell_t
{
  shadow_bolt_t( warlock_t* player, util::string_view options_str ) :
    warlock_spell_t( "shadow_bolt", player, player->find_rank( shadow_bolt_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

// Immolate

constexpr std::array immolate_ranks {
  spell_container_t().set_spell(   348 ),
  spell_container_t().set_spell(   707 ),
  spell_container_t().set_spell(  1094 ),
  spell_container_t().set_spell(  2941 ),
  spell_container_t().set_spell( 11665 ),
  spell_container_t().set_spell( 11667 ),
  spell_container_t().set_spell( 11668 ),
  spell_container_t().set_spell( 25309 ),
};
struct immolate_t : public warlock_spell_t
{
  immolate_t( warlock_t* player, util::string_view options_str ) :
    warlock_spell_t( "immolate", player, player->find_rank( immolate_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

// Corruption

constexpr std::array corruption_ranks {
  spell_container_t().set_spell(   172 ),
  spell_container_t().set_spell(  6222 ),
  spell_container_t().set_spell(  6223 ),
  spell_container_t().set_spell(  7648 ),
  spell_container_t().set_spell( 11671 ),
  spell_container_t().set_spell( 11672 ),
  spell_container_t().set_spell( 25311 ),
};
struct corruption_t : public warlock_spell_t
{
  corruption_t( warlock_t* player, util::string_view options_str ) :
    warlock_spell_t( "corruption", player, player->find_rank( corruption_ranks )->spell( player ) )
  {
    parse_options( options_str );
  }
};

struct summon_imp_t final : public warlock_spell_t
{
  summon_imp_t( warlock_t* player, std::string_view options_str ) :
    warlock_spell_t( "summon_imp", player, player->find_spell( 688 ) )
  {
    parse_options( options_str );
    harmful = false;
  }

  void execute() override
  {
    warlock_spell_t::execute();
    p()->pets.imp->summon();
  }
};

///////////////////////////////////////////
//
// Demons
//
///////////////////////////////////////////

namespace pets {
  struct warlock_pet_t : public pet_t
  {
    warlock_pet_t( sim_t* sim, warlock_t* owner, std::string_view pet_name, pet_e pet_type, bool guardian = false ) :
      pet_t( sim, owner, pet_name, pet_type, guardian, false )
    {
    }

    const warlock_t* o() const
    { return static_cast<warlock_t*>( owner ); }

    warlock_t* o()
    { return static_cast<warlock_t*>( owner ); }
  };

  template <class Base>
  struct warlock_pet_action_t : parse_action_effects_t<Base>
  {
  private:
    using ab = parse_action_effects_t<Base>;
  public:
    using base_t = warlock_pet_action_t<Base>;

    template <typename... Args>
    warlock_pet_action_t( Args&&... args ) : ab( std::forward<Args>( args )... ) {}

    warlock_pet_t* p()
    { return debug_cast<warlock_pet_t*>( this->player ); }

    const warlock_pet_t* p() const
    { return debug_cast<const warlock_pet_t*>( this->player ); }

    warlock_t* o()
    { return debug_cast<warlock_t*>( p()->owner ); }

    const warlock_t* o() const
    { return debug_cast<const warlock_t*>( p()->owner ); }
  };

  using warlock_pet_attack_t = warlock_pet_action_t<melee_attack_t>;
  using warlock_pet_spell_t  = warlock_pet_action_t<spell_t>;

  namespace imp {
    struct imp_pet_t : warlock_pet_t
    {
      imp_pet_t( sim_t* sim, warlock_t* owner ) :
        warlock_pet_t( sim, owner, "imp", PET_IMP )
      {
        // from level 6-10 imp character sheet
        owner_coeff.armor = 0.35;
        owner_coeff.ap_from_sp = 0.17;

        // TODO: Imp HP looks to come from a table
        owner_coeff.health = 1.5;
        stamina_per_owner = 0;

        // matches in-game stats but int might not actually do anything on the pet
        intellect_per_owner = 0.3;
      }

      resource_e primary_resource() const override { return RESOURCE_MANA; }

      // copies owner's crit which matches imp char sheet
      double pet_crit() const override
      { return current_pet_stats.composite_spell_crit; }

      double resource_regen_per_second( resource_e r ) const override
      {
        if ( r != RESOURCE_MANA )
        {
          return pet_t::resource_regen_per_second( r );
        }

        // TODO - this is at level 7, probably need a table for this
        return recent_cast() ? 1.65 : 2.73;
      }

      void init_base_stats() override
      {
        warlock_pet_t::init_base_stats();

        // TODO: figure this out - probably another table of int/mana at every level
        // fit to obsevations level 3-8 but starts to break down at level 9
        double mana = 29 + (9.5 * level());
        resources.base[ RESOURCE_MANA ] = std::floor(mana);
      }

      void init_action_list() override
      {
        action_list_str = "firebolt";
        warlock_pet_t::init_action_list();
      }

      action_t* create_action( std::string_view, std::string_view ) override;
    };

    constexpr std::array firebolt_ranks {
      spell_container_t().set_spell(  3110 ),  // 1
      spell_container_t().set_spell(  7799 ),  // 8
      spell_container_t().set_spell(  7800 ),  // 18
      spell_container_t().set_spell(  7801 ),  // 28
      spell_container_t().set_spell(  7802 ),  // 38
      spell_container_t().set_spell( 11762 ),  // 48
      spell_container_t().set_spell( 11763 ),  // 58
    };
    // TODO: There's some funky delay on beta. Some casts are back-to-back, some have a 0.5-1.0s gap even when
    // the imp has mana
    struct firebolt_t : warlock_pet_spell_t
    {
      firebolt_t( std::string_view n, imp_pet_t* p, std::string_view options_str  ) :
        warlock_pet_spell_t( n, p, p->find_rank( firebolt_ranks )->spell( player ) )
      {
        parse_options( options_str );

        // beta casts have been wonky with gaps in between casts
        //ability_lag.mean   = 500_ms;
        //ability_lag.stddev = 100_ms;
      }
    };

    action_t* imp_pet_t::create_action( std::string_view name, std::string_view options_str )
    {
      if ( name == "firebolt" ) return new firebolt_t( name, this, options_str );

      return warlock_pet_t::create_action( name, options_str );
    }
  }
}

action_t* warlock_t::create_action( util::string_view name, util::string_view options_str )
{
  if ( name == "shadow_bolt" )
    return new shadow_bolt_t( this, options_str );
  if ( name == "immolate" )
    return new immolate_t( this, options_str );
  if ( name == "corruption" )
    return new corruption_t( this, options_str );

  if ( name == "summon_imp" )
    return new summon_imp_t( this, options_str );

  return player_t::create_action( name, options_str );
}

void warlock_t::create_pets()
{
  player_t::create_pets();

  pets.imp = new pets::imp::imp_pet_t( sim, this );
}

void warlock_t::init_base_stats()
{
  player_t::init_base_stats();

  base.attack_crit_chance = get_passive_player_value(0.02, "all_crit");
  base.spell_crit_chance = get_passive_player_value(0.01701, "all_crit");
}

void warlock_t::init_spells()
{
  player_t::init_spells();

  parse_all_class_passives();
  parse_all_passive_talents();
}

void warlock_t::init_action_list()
{
  if ( !action_list_str.empty() )
  {
    player_t::init_action_list();
    return;
  }

  clear_action_priority_lists();
  warlock_apl::warlock( this );
  use_default_action_list = true;
  player_t::init_action_list();
}

}  // namespace warlock

const module_t* module_t::warlock()
{
  static warlock::warlock_module_t m;
  return &m;
}
