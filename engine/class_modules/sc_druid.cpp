// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "action/parse_effects.hpp"
#include "class_modules/apl/druid.hpp"
#include "dbc/spell_data.hpp"

#include "simulationcraft.hpp"

namespace
{  // UNNAMED NAMESPACE

// Finds an action with the given name. If no action exists, a new one will
// be created.
//
// Use this with secondary background actions to ensure the player only has
// one copy of the action.
// Shamelessly borrowed from the mage module
template <typename Action, typename Actor, typename... Args>
action_t* get_action( std::string_view name, Actor* actor, Args&&... args )
{
  action_t* a = actor->find_action( name );
  if ( !a )
    a = new Action( name, actor, std::forward<Args>( args )... );
  assert( dynamic_cast<Action*>( a ) && a->name_str == name && a->background );
  return a;
}

// ==========================================================================
// Druid
// ==========================================================================
enum form_e : unsigned
{
  NONE_FORM    = 0x00000000,
  CAT_FORM     = 0x00000001,
  CASTER_FORM  = 0x00000002,
  MOONKIN_FORM = 0x40000000,

  ANY_FORM = CAT_FORM | MOONKIN_FORM | CASTER_FORM
};

struct druid_t;
struct druid_td_t final : public actor_target_data_t
{
  struct debuffs_t
  {
    buff_t* hurricane;
    buff_t* faerie_fire;
  } debuff;

  // Combo points belong to the target they were generated on
  int combo_points = 0;

  druid_td_t( player_t& target, druid_t& p );

  template <typename Buff = buff_t, typename... Args>
  inline buff_t* make_debuff( bool b, Args&&... args );
};

struct druid_t final : public parse_player_effects_t
{
  druid_t( sim_t* sim, std::string_view name, race_e r = RACE_NIGHT_ELF )
    : parse_player_effects_t( sim, DRUID, name, r )
  {
  }

  struct talents_t
  {
    player_talent_t improved_entangling_roots;
    player_talent_t insect_swarm;
    player_talent_t overgrowth;
    player_talent_t natures_grace;
    player_talent_t eclipse;
    player_talent_t moonkin_form;
  } talent;

  struct buffs_t
  {
    buff_t* cat_form;
    buff_t* moonkin_form;
    buff_t* innervate;
    buff_t* clearcasting;
    buff_t* natures_grace;
    buff_t* eclipse;
    buff_t* moonkin_aura;
    buff_t* prowl;
    buff_t* poised;
    buff_t* mark_of_the_wild;
  } buff;

  struct
  {
    spell_ranks_t cat_form;
    spell_ranks_t claw;
    spell_ranks_t entangling_roots;
    spell_ranks_t faerie_fire;
    spell_ranks_t hurricane;
    spell_ranks_t innervate;
    spell_ranks_t insect_swarm;
    spell_ranks_t mark_of_the_wild;
    spell_ranks_t moonfire;
    spell_ranks_t omen_of_clarity;
    spell_ranks_t prowl;
    spell_ranks_t rake;
    spell_ranks_t rip;
    spell_ranks_t shred;
    spell_ranks_t starfire;
    spell_ranks_t wrath;
  } rank;

  struct background_actions_t
  {
    action_t* mainhand_melee;
    action_t* offhand_melee;
    action_t* cat_melee;
  } background_actions;

  form_e form                  = CASTER_FORM;
  weapon_t caster_form_weapon  = equipped_weapons[ SLOT_MAIN_HAND ];
  weapon_t caster_form_offhand = equipped_weapons[ SLOT_OFF_HAND ];
  weapon_t cat_weapon;
  melee_attack_t* caster_melee_attack = nullptr;
  melee_attack_t* cat_melee_attack    = nullptr;

  void reset() override;
  void init_base_stats() override;
  void init_spells() override;
  void init_action_list() override;
  void create_buffs() override;
  void init_special_effects() override;
  void init_finished() override;
  void parse_player_effects();
  void apply_action_effects( action_t* a );
  void create_actions() override;
  double resource_loss( resource_e resource_type, double amount, gain_t* g = nullptr, action_t* a = nullptr ) override;
  std::unique_ptr<expr_t> create_expression( util::string_view name ) override;

  static constexpr int max_combo_points = 5;
  // Target that currently holds the player's combo points
  player_t* combo_target = nullptr;

  int combo_points( player_t* t ) const;
  void gain_combo_points( player_t* t, int amount, gain_t* g );
  void spend_combo_points( player_t* t, gain_t* g );

  resource_e primary_resource() const override
  {
    return RESOURCE_MANA;
  }

  std::string default_potion() const override
  {
    return druid_apl::potion( this );
  }
  std::string default_flask() const override
  {
    return druid_apl::flask( this );
  }
  std::string default_food() const override
  {
    return druid_apl::food( this );
  }

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

void druid_t::reset()
{
  parse_player_effects_t::reset();

  form = CASTER_FORM;

  if ( combo_target )
    get_target_data( combo_target )->combo_points = 0;
  combo_target = nullptr;
}

void druid_t::init_base_stats()
{

  base.attack_power_per_strength = 2.0;
  base.stats.attack_power        = -20.0;
  base.attack_crit_chance = get_passive_player_value( .0074, "attack_crit" );
  base.spell_crit_chance  = get_passive_player_value( .018, "spell_crit" );

  parse_player_effects_t::init_base_stats();
}

void druid_t::init_spells()
{
  parse_player_effects_t::init_spells();

  parse_all_class_passives();
  parse_all_passive_talents();

  talent.improved_entangling_roots = find_talent_spell( talent_tree::CLASS, "Improved Entangling Roots" );
  talent.insect_swarm              = find_talent_spell( talent_tree::CLASS, "Insect Swarm" );
  talent.overgrowth                = find_talent_spell( talent_tree::CLASS, "Overgrowth" );
  talent.natures_grace             = find_talent_spell( talent_tree::CLASS, "Nature's Grace" );
  talent.eclipse                   = find_talent_spell( talent_tree::CLASS, "Eclipse" );
  talent.moonkin_form              = find_talent_spell( talent_tree::CLASS, "Moonkin Form" );

  if ( talent.moonkin_form->ok() )
  {
    // The omen of clarity mods are on the talent, not the aura.
    // TODO Confirm if omen is modified by the talent being picked alone
    parse_passive_effects( talent.moonkin_form, true );
  }

  rank.cat_form = {
      this,
      spell_container_t().set_spell( 768 ).set_buff( 3025 ),
  };
  rank.claw = {
      this,
      spell_container_t().set_spell( 1082 ),
      spell_container_t().set_spell( 3029 ),
      spell_container_t().set_spell( 5201 ),
      spell_container_t().set_spell( 9849 ),
      spell_container_t().set_spell( 9850 ),
  };
  rank.entangling_roots = {
      this,
      spell_container_t().set_spell( 339 ),  // 19975 is also a rank 1 spell id, it is used by nature's grasp
      spell_container_t().set_spell( 1062 ),
      spell_container_t().set_spell( 5195 ),
      spell_container_t().set_spell( 5196 ),
      spell_container_t().set_spell( 9852 ),
      spell_container_t().set_spell( 9853 ),
  };
  rank.faerie_fire = {
      this,
      spell_container_t().set_spell( 770 ),
      spell_container_t().set_spell( 778 ),
      spell_container_t().set_spell( 9749 ),
      spell_container_t().set_spell( 9907 ),
  };
  rank.hurricane = {
      this,
      spell_container_t().set_spell( 16914 ).set_debuff( 1278965 ),
      spell_container_t().set_spell( 17401 ).set_debuff( 1278968 ),
      spell_container_t().set_spell( 17402 ).set_debuff( 1278759 ),
  };
  rank.innervate = {
      this,
      spell_container_t().set_spell( 29166 ),
  };
  rank.insect_swarm = {
      this,
      talent.insect_swarm,
      spell_container_t().set_spell( 5570 ),
      spell_container_t().set_spell( 24974 ),
      spell_container_t().set_spell( 24975 ),
      spell_container_t().set_spell( 24976 ),
      spell_container_t().set_spell( 24977 ),
  };
  rank.mark_of_the_wild = {
      this,
      spell_container_t().set_spell( 1126 ),
      spell_container_t().set_spell( 5232 ),
      spell_container_t().set_spell( 6756 ),
      spell_container_t().set_spell( 5234 ),
      spell_container_t().set_spell( 8907 ),
      spell_container_t().set_spell( 9884 ),
      spell_container_t().set_spell( 9885 ),
  };
  rank.moonfire = {
      this,
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
  rank.omen_of_clarity = {
      this,
      spell_container_t().set_spell( 16864 ).set_buff( 16870 ),
  };
  rank.prowl = {
      this,
      spell_container_t().set_spell( 5215 ),
      spell_container_t().set_spell( 6783 ),
      spell_container_t().set_spell( 9913 ),
  };
  rank.rake = {
      this,
      spell_container_t().set_spell( 1822 ),
      spell_container_t().set_spell( 1823 ),
      spell_container_t().set_spell( 1824 ),
      spell_container_t().set_spell( 9904 ),
  };
  rank.rip = {
      this,
      spell_container_t().set_spell( 1079 ),
      spell_container_t().set_spell( 9492 ),
      spell_container_t().set_spell( 9493 ),
      spell_container_t().set_spell( 9752 ),
      spell_container_t().set_spell( 9894 ),
      spell_container_t().set_spell( 9896 ),
  };
  rank.shred = {
      this,
      spell_container_t().set_spell( 5221 ),
      spell_container_t().set_spell( 6800 ),
      spell_container_t().set_spell( 8992 ),
      spell_container_t().set_spell( 9829 ),
      spell_container_t().set_spell( 9830 ),
  };
  rank.starfire = {
      this,
      spell_container_t().set_spell( 2912 ),
      spell_container_t().set_spell( 8949 ),
      spell_container_t().set_spell( 8950 ),
      spell_container_t().set_spell( 8951 ),
      spell_container_t().set_spell( 9875 ),
      spell_container_t().set_spell( 9876 ),
      spell_container_t().set_spell( 25298 ),
  };
  rank.wrath = {
      this,
      spell_container_t().set_spell( 5176 ),
      spell_container_t().set_spell( 5177 ),
      spell_container_t().set_spell( 5178 ),
      spell_container_t().set_spell( 5179 ),
      spell_container_t().set_spell( 5180 ),
      spell_container_t().set_spell( 6780 ),
      spell_container_t().set_spell( 8905 ),
      spell_container_t().set_spell( 9912 ),
  };
}

// DRUID MODULE INTERFACE ===================================================
struct druid_module_t final : public module_t
{
  druid_module_t() : module_t( DRUID )
  {
  }

  player_t* create_player( sim_t* sim, std::string_view name, race_e r = RACE_NONE ) const override
  {
    auto p = new druid_t( sim, name, r );
    return p;
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

template <class Base>
struct druid_action_t : public parse_action_effects_t<Base>
{
  using action_base_t = parse_action_effects_t<Base>;
  using base_t        = druid_action_t<Base>;

  unsigned form_mask = 0;

  template <typename... Args>
  druid_action_t( std::string_view n, druid_t* p, const spell_data_t* s = spell_data_t::nil() )
    : action_base_t( n, p, s )
  {
    form_mask = this->data().stance_mask();

    if ( this->data().ok() )
    {
      p->apply_action_effects( this );
    }
  }

  druid_t* p()
  {
    return debug_cast<druid_t*>( this->player );
  }

  const druid_t* p() const
  {
    return debug_cast<const druid_t*>( this->player );
  }

  bool ready() override
  {
    if ( !form_mask || ( form_mask & p()->form ) == p()->form )
    {
      return action_base_t::ready();
    }
    if ( this->sim->debug )
    {
      this->sim->print_debug( "{} ready() failed due to wrong form. form={:#010x} form_mask={:#010x}", this->name(),
                              static_cast<unsigned>( this->p()->form ), form_mask );
    }
    return false;
  }

  double gain_energize_resource( resource_e resource_type, double amount, gain_t* g ) override
  {
    if ( resource_type == RESOURCE_COMBO_POINT )
    {
      this->p()->gain_combo_points( this->target, static_cast<int>( amount ), g );
    }

    return action_base_t::gain_energize_resource( resource_type, amount, g );
  }

  std::unique_ptr<expr_t> create_expression( util::string_view name ) override
  {
    if ( name == "combo_points" )
      return make_fn_expr( name, [ this ]() { return this->p()->combo_points( this->target ); } );

    return action_base_t::create_expression( name );
  }

  void execute() override
  {
    action_base_t::execute();

    if ( !this->data().flags( SX_NO_STEALTH_BREAK ) )
      this->p()->buff.prowl->expire();

    if ( this->data().affected_by( this->p()->buff.clearcasting->data() ) )
    {
      this->p()->buff.clearcasting->expire();
    }
  }

  void impact( action_state_t* s ) override
  {
    action_base_t::impact( s );

    // Nature's Grace: non-periodic spell criticals. impact() is only the direct component, ticks go through tick()
    if constexpr ( std::is_base_of_v<spell_t, Base> )
    {
      if ( s->result == RESULT_CRIT && this->spell_power_mod.direct > 0 )
        this->p()->buff.natures_grace->trigger();
    }
  }
};

using druid_spell_t        = druid_action_t<spell_t>;
using druid_melee_attack_t = druid_action_t<melee_attack_t>;

// ==========================================================================
// Druid Spells
// ==========================================================================
struct druid_form_t : public druid_spell_t
{
  druid_form_t( std::string_view n, druid_t* player, const spell_data_t* s, form_e f, std::string_view options_str )
    : druid_spell_t( n, player, s ), form( f )
  {
    parse_options( options_str );
    harmful = false;

    // Castable from every form except itself
    form_mask = ANY_FORM & ~form;
  }

  void execute() override
  {
    druid_spell_t::execute();

    shapeshift();
  }

protected:
  form_e form;

  buff_t* get_form_buff( form_e f ) const
  {
    switch ( f )
    {
      case CAT_FORM:
        return p()->buff.cat_form;
      case MOONKIN_FORM:
        return p()->buff.moonkin_form;
      default:
        return nullptr;
    }
  }

  action_t* get_form_mh_attack( form_e f ) const
  {
    switch ( f )
    {
      case CAT_FORM:
        return p()->background_actions.cat_melee;
      case MOONKIN_FORM:
      case CASTER_FORM:
        return p()->background_actions.mainhand_melee;
      default:
        return nullptr;
    }
  }

  action_t* get_form_oh_attack( form_e f ) const
  {
    switch ( f )
    {
      case MOONKIN_FORM:
      case CASTER_FORM:
        return p()->caster_form_offhand.type != WEAPON_NONE ? p()->background_actions.offhand_melee : nullptr;
      default:
        return nullptr;
    }
  }

  weapon_t get_form_weapon( form_e f ) const
  {
    switch ( f )
    {
      case CAT_FORM:
        return p()->cat_weapon;
      default:
        return p()->caster_form_weapon;
    }
  }

  weapon_t get_form_offhand_weapon( form_e f ) const
  {
    switch ( f )
    {
      case CASTER_FORM:
      case MOONKIN_FORM:
        return p()->caster_form_offhand;
      default:
        return WEAPON_NONE;
    }
  }

  void shapeshift()
  {
    auto old_form = p()->form;

    if ( auto old_buff = get_form_buff( old_form ) )
      old_buff->expire();

    p()->form = form;

    if ( auto new_buff = get_form_buff( form ) )
      new_buff->trigger();

    p()->main_hand_attack                   = static_cast<attack_t*>( get_form_mh_attack( form ) );
    p()->off_hand_attack                    = static_cast<attack_t*>( get_form_oh_attack( form ) );
    p()->equipped_weapons[ SLOT_MAIN_HAND ] = get_form_weapon( form );
    p()->equipped_weapons[ SLOT_OFF_HAND ]  = get_form_offhand_weapon( form );
  }
};

struct cat_form_t : public druid_form_t
{
  cat_form_t( druid_t* player, std::string_view options_str )
    : druid_form_t( "cat_form", player, player->rank.cat_form.find_rank()->spell(), CAT_FORM, options_str )
  {
  }
};

struct moonkin_form_t : public druid_form_t
{
  moonkin_form_t( druid_t* player, std::string_view options_str )
    : druid_form_t( "moonkin_form", player, player->talent.moonkin_form, MOONKIN_FORM, options_str )
  {
  }
};

// Flags the druid as stealthed so openers can be modeled from stealth. Only castable out of combat in cat form.
// TODO: stealth level (effect 1), movement speed reduction (effect 2), Feral Instinct, and the 10s cooldown starting on
// stealth break instead of on cast
struct prowl_t : public druid_spell_t
{
  prowl_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "prowl", player, player->rank.prowl.find_rank()->spell() )
  {
    parse_options( options_str );
    harmful        = false;
    use_off_gcd = true;

  }

  bool ready() override
  {
    if ( p()->buff.prowl->check() )
      return false;

    return druid_spell_t::ready();
  }

  void execute() override
  {
    druid_spell_t::execute();

    p()->buff.prowl->trigger();
  }
};

struct wrath_t : public druid_spell_t
{
  wrath_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "wrath", player, player->rank.wrath.find_rank()->spell() )
  {
    parse_options( options_str );

    eclipse_stacks = as<int>( p()->talent.eclipse->effectN( 3 ).base_value() );
  }

  void execute() override
  {
    druid_spell_t::execute();

    // TODO: confirm that the charges are granted on cast, and that the proc can't miss
    if ( p()->talent.eclipse->ok() )
      p()->buff.eclipse->trigger( eclipse_stacks );
  }

private:
  int eclipse_stacks;
};

struct moonfire_t : public druid_spell_t
{
  moonfire_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "moonfire", player, player->rank.moonfire.find_rank()->spell() )
  {
    parse_options( options_str );
  }
};

// The talent is a single point, but the spell itself still has ranks
struct insect_swarm_t : public druid_spell_t
{
  insect_swarm_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "insect_swarm", player, player->rank.insect_swarm.find_rank()->spell() )
  {
    parse_options( options_str );
  }
};

struct starfire_t : public druid_spell_t
{
  starfire_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "starfire", player, player->rank.starfire.find_rank()->spell() )
  {
    parse_options( options_str );

    eclipse_reduction =
        player->talent.eclipse->ok() ? timespan_t::from_millis( p()->talent.eclipse->effectN( 2 ).base_value() ) : 0_s;
  }

  timespan_t execute_time_flat_modifier() const override
  {
    auto t = druid_spell_t::execute_time_flat_modifier();

    if ( p()->buff.eclipse->up() )
      t += eclipse_reduction;

    return t;
  }

  void execute() override
  {
    druid_spell_t::execute();

    if ( p()->buff.eclipse->up() )
      p()->buff.eclipse->decrement();
  }

private:
  timespan_t eclipse_reduction;
};

struct entangling_roots_t : public druid_spell_t
{
  entangling_roots_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "entangling_roots", player, player->rank.entangling_roots.find_rank()->spell() )
  {
    parse_options( options_str );

    if ( p()->talent.overgrowth->ok() )
      aoe = 1 + as<int>( p()->talent.overgrowth->effectN( 1 ).base_value() );

    // TODO: verify periodic can crit
    break_chance = break_chance / ( 1.0 + p()->talent.improved_entangling_roots->effectN( 1 ).percent() );
  }

  void tick( dot_t* d ) override
  {
    druid_spell_t::tick( d );

    if ( p()->sim->rng().roll( break_chance ) )
    {
      d->cancel();
    }
  }

  // TODO: Didn't get nearly enough data from beta to determine if this is the right value
  // 20% generates a geometric distribution with a mean of 5 hits
  // It is unlikely that roots is used in a real apl, but some model for the dot and break is needed for testing
  double break_chance = .2;
};

struct hurricane_tick_t : public druid_spell_t
{
  hurricane_tick_t( druid_t* player )
    : druid_spell_t( "hurricane_tick", player, player->rank.hurricane.find_rank()->debuff() )
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
    : druid_spell_t( "hurricane", player, player->rank.hurricane.find_rank()->spell() )
  {
    parse_options( options_str );
    channeled   = true;
    tick_action = new hurricane_tick_t( player );
    // debuff applies instantly, but the first tick is delayed by 1 second
  }
};

struct faerie_fire_t : public druid_spell_t
{
  faerie_fire_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "faerie_fire", player, player->rank.faerie_fire.find_rank()->spell() )
  {
    parse_options( options_str );

    if ( p()->buff.cat_form->data().ok() )
    {
      // TODO see about putting flat cdr effects in core
      cat_cooldown = timespan_t::from_millis( p()->buff.cat_form->data().effectN( 4 ).base_value() );
      cat_gcd_mod  = timespan_t::from_millis( p()->buff.cat_form->data().effectN( 5 ).base_value() );
    }

  }

  timespan_t gcd() const override
  {
    auto g = druid_spell_t::gcd();

    if ( p()->buff.cat_form->check() )
      g -= cat_gcd_mod;

    return g;
  }

  // Faerie Fire has no cooldown of its own, so cooldown_t::start ignores a default duration. Pass it explicitly.
  void update_ready( timespan_t cd = timespan_t::min() ) override
  {
    if ( cat_cooldown > 0_ms && p()->buff.cat_form->check() )
      cd = cat_cooldown;

    druid_spell_t::update_ready( cd );
  }

  void impact( action_state_t* s ) override
  {
    druid_spell_t::impact( s );

    p()->get_target_data( s->target )->debuff.faerie_fire->trigger();
  }

private:
  timespan_t cat_cooldown = 0_ms;
  timespan_t cat_gcd_mod  = 0_ms;
};

struct mark_of_the_wild_t : public druid_spell_t
{
  mark_of_the_wild_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "mark_of_the_wild", player, player->rank.mark_of_the_wild.find_rank()->spell() )
  {
    parse_options( options_str );
    harmful    = false;
    background = sim->overrides.mark_of_the_wild != 0;
  }

  void execute() override
  {
    druid_spell_t::execute();

    p()->buff.mark_of_the_wild->trigger();
  }
};

struct innervate_t : public druid_spell_t
{
  innervate_t( druid_t* player, std::string_view options_str )
    : druid_spell_t( "innervate", player, player->rank.innervate.find_rank()->spell() )
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

// ==========================================================================
// Druid Attacks
// ==========================================================================
struct shred_t : public druid_melee_attack_t
{
  shred_t( druid_t* player, std::string_view options_str )
    : druid_melee_attack_t( "shred", player, player->rank.shred.find_rank()->spell() )
  {
    parse_options( options_str );

    // TODO get a log or level to 20 and see how the refund on miss works + the weapon scaling
  }
};

struct claw_t : public druid_melee_attack_t
{
  claw_t( druid_t* player, std::string_view options_str )
    : druid_melee_attack_t( "claw", player, player->rank.claw.find_rank()->spell() )
  {
    parse_options( options_str );
  }
};

struct rake_t : public druid_melee_attack_t
{
  rake_t( druid_t* player, std::string_view options_str )
    : druid_melee_attack_t( "rake", player, player->rank.rake.find_rank()->spell() )
  {
    parse_options( options_str );

    attack_power_mod.direct = 0.065;
    attack_power_mod.tick   = 0.065;
  }
};

struct druid_combo_attack_t : public druid_melee_attack_t
{
  druid_combo_attack_t( std::string_view n, druid_t* player, const spell_data_t* s )
    : druid_melee_attack_t( n, player, s )
  {
  }

  bool ready() override
  {
    if ( p()->combo_points( target ) < 1 )
      return false;

    return druid_melee_attack_t::ready();
  }

  void execute() override
  {
    combo_points = p()->combo_points( target );

    druid_melee_attack_t::execute();

    p()->spend_combo_points( target, gain );
  }

protected:
  int combo_points = 0;
};

struct rip_t : public druid_combo_attack_t
{
  rip_t( druid_t* player, std::string_view options_str )

    : druid_combo_attack_t( "rip", player, player->rank.rip.find_rank()->spell() )

  {
    parse_options( options_str );

    // TODO confirm refresh behavior
  }

  double base_ta( const action_state_t* s ) const override
  {
    return druid_combo_attack_t::base_ta( s ) + combo_points * data().effectN( 1 ).pp_combo_points();
  }

  // Per a community source: the total AP scaling is 6% per combo point, capped at 24% (4 combo points).
  double attack_tick_power_coefficient( const action_state_t* ) const override
  {
    constexpr double ap_per_combo_point = 0.06;
    constexpr int max_scaling_points    = 4;

    auto const base_ticks = data().duration() / data().effectN( 1 ).period();

    return ap_per_combo_point * std::min( combo_points, max_scaling_points ) / base_ticks;
  }
};

struct druid_melee_t : public druid_melee_attack_t
{
  druid_melee_t( std::string_view name, druid_t* p ) : druid_melee_attack_t( name, p, spell_data_t::nil() )
  {
    weapon_multiplier = 1.0;
    school            = SCHOOL_PHYSICAL;
    trigger_gcd       = 0_ms;

    background = repeating = allow_class_ability_procs = not_a_proc = true;
    may_glance = may_crit = may_dodge = may_parry = true;
    special                                       = false;
  }
};

struct cat_melee_t : public druid_melee_attack_t
{
  cat_melee_t( std::string_view name, druid_t* p ) : druid_melee_attack_t( name, p, spell_data_t::nil() )
  {
    form_mask = form_e::CAT_FORM;

    weapon_multiplier = 1.0;
    school            = SCHOOL_PHYSICAL;
    trigger_gcd       = 0_ms;

    background = repeating = allow_class_ability_procs = not_a_proc = true;
    may_glance = may_crit = may_dodge = may_parry = true;
    special                                       = false;

    // Speculative implementation via notes from the druid discord
    // https://discord.com/channels/253205420225724416/1548424199184588931/1552873876537737236
    double const swing_time_ms = 1000.0;
    double const variance      = 0.4 / 2;  // TODO this can be driven by SpellShapeshiftForm.db2
    double const high_variance = 1.0 + variance / 2.0;
    double const low_variance  = 1.0 - variance / 2.0;
    double const base_damage   = p->equipped_weapons[ SLOT_MAIN_HAND ].dps * swing_time_ms * 0.001;

    auto const top_damage = std::max( 1.0, util::ceil( base_damage * high_variance ) );
    auto const low_damage = std::max( 1.0, util::floor( base_damage * low_variance ) );
        

    p->cat_weapon            = weapon_t( WEAPON_FIST, ( 1 + 1 ) / 2, timespan_t::from_millis( swing_time_ms ) );
    p->cat_weapon.slot       = SLOT_MAIN_HAND;
    p->cat_weapon.min_dmg    = low_damage;
    p->cat_weapon.max_dmg    = top_damage;
    p->cat_weapon.swing_time = timespan_t::from_millis( swing_time_ms );

    weapon            = &p->cat_weapon;
    base_execute_time = p->cat_weapon.swing_time;
  };
};

struct auto_attack_t : public druid_melee_attack_t
{
  auto_attack_t( druid_t* p, std::string_view options_str )
    : druid_melee_attack_t( "auto_attack", p, spell_data_t::nil() )
  {
    parse_options( options_str );
    trigger_gcd = 0_ms;

    assert( p->equipped_weapons[ SLOT_MAIN_HAND ].type != WEAPON_NONE );

    p->main_hand_attack                    = static_cast<attack_t*>( p->background_actions.mainhand_melee );
    p->main_hand_attack->weapon            = &p->equipped_weapons[ SLOT_MAIN_HAND ];
    p->main_hand_attack->base_execute_time = p->equipped_weapons[ SLOT_MAIN_HAND ].swing_time;
    p->caster_form_weapon                  = p->equipped_weapons[ SLOT_MAIN_HAND ];

    if ( p->equipped_weapons[ SLOT_OFF_HAND ].type != WEAPON_NONE )
    {
      p->off_hand_attack                    = static_cast<attack_t*>( p->background_actions.offhand_melee );
      p->off_hand_attack->weapon            = &p->equipped_weapons[ SLOT_OFF_HAND ];
      p->off_hand_attack->base_execute_time = p->equipped_weapons[ SLOT_OFF_HAND ].swing_time;
      p->caster_form_offhand                = p->equipped_weapons[ SLOT_OFF_HAND ];
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

int druid_t::combo_points( player_t* t ) const
{
  return t ? get_target_data( t )->combo_points : 0;
}

void druid_t::gain_combo_points( player_t* t, int amount, gain_t* )
{
  if ( combo_target && combo_target != t )
  {
    auto old_td = get_target_data( combo_target );
    sim->print_log( "{} loses {} combo_points on {}", *this, old_td->combo_points, *combo_target );
    resource_loss( RESOURCE_COMBO_POINT, old_td->combo_points, get_gain( "combo_points_lost" ) );
    old_td->combo_points = 0;
  }

  combo_target = t;

  auto td      = get_target_data( t );
  td->combo_points = std::min( td->combo_points + amount, max_combo_points );
}

void druid_t::spend_combo_points( player_t* t, gain_t* g )
{
  auto td = get_target_data( t );

  sim->print_log( "{} spends {} combo_points on {}", *this, td->combo_points, *t );

  resource_loss( RESOURCE_COMBO_POINT, td->combo_points, g );
  td->combo_points = 0;
}

std::unique_ptr<expr_t> druid_t::create_expression( util::string_view name )
{
  if ( name == "combo_points" )
    return make_fn_expr( name, [ this ]() { return combo_points( target ); } );

  return parse_player_effects_t::create_expression( name );
}

double druid_t::resource_loss( resource_e resource_type, double amount, gain_t* g, action_t* a )
{
  if ( resource_type == RESOURCE_ENERGY && buff.poised->check() && a && a->data().affected_by( buff.poised->data() ) )
    buff.poised->expire();
  return parse_player_effects_t::resource_loss( resource_type, amount, g, a );
}

action_t* druid_t::create_action( std::string_view name, std::string_view options_str )
{
  if ( name == "auto_attack" )
    return new auto_attack_t( this, options_str );
  if ( name == "wrath" )
    return new wrath_t( this, options_str );
  if ( name == "moonfire" )
    return new moonfire_t( this, options_str );
  if ( name == "insect_swarm" )
    return new insect_swarm_t( this, options_str );
  if ( name == "starfire" )
    return new starfire_t( this, options_str );
  if ( name == "entangling_roots" )
    return new entangling_roots_t( this, options_str );
  if ( name == "hurricane" )
    return new hurricane_t( this, options_str );
  if ( name == "faerie_fire" )
    return new faerie_fire_t( this, options_str );
  if ( name == "mark_of_the_wild" )
    return new mark_of_the_wild_t( this, options_str );
  if ( name == "innervate" )
    return new innervate_t( this, options_str );
  if ( name == "moonkin_form" )
    return new moonkin_form_t( this, options_str );
  if ( name == "cat_form" )
    return new cat_form_t( this, options_str );
  if ( name == "prowl" )
    return new prowl_t( this, options_str );
  if ( name == "claw" )
    return new claw_t( this, options_str );
  if ( name == "rake" )
    return new rake_t( this, options_str );
  if ( name == "rip" )
    return new rip_t( this, options_str );
  if ( name == "shred" )
    return new shred_t( this, options_str );

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
  auto const hurricane_debuff = p.rank.hurricane.find_rank();
  debuff.hurricane = make_debuff( hurricane_debuff->spell()->ok(), *this, "hurricane", hurricane_debuff->debuff() )
                         ->set_default_value( hurricane_debuff->debuff()->effectN( 2 ).percent() );

  auto const faerie_fire_debuff = p.rank.faerie_fire.find_rank();
  debuff.faerie_fire =
      make_debuff<stat_buff_t>( faerie_fire_debuff->spell()->ok(), *this, "faerie_fire", faerie_fire_debuff->spell() )
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

  auto const innervate_buff = rank.innervate.find_rank()->spell();
  buff.innervate            = make_buff_fallback( innervate_buff->ok(), this, "innervate", innervate_buff );

  auto const mark_of_the_wild_spell = rank.mark_of_the_wild.find_rank()->spell();
  auto const mark_of_the_wild_buff = debug_cast<stat_buff_t*>(
      make_buff_fallback<stat_buff_t>( mark_of_the_wild_spell->ok(), this, "mark_of_the_wild", mark_of_the_wild_spell ) );

  buff.mark_of_the_wild = mark_of_the_wild_buff;

  auto const omen_of_clarity_buff = rank.omen_of_clarity.find_rank()->buff();
  buff.clearcasting = make_buff_fallback( omen_of_clarity_buff->ok(), this, "clearcasting", omen_of_clarity_buff );

  auto const natures_grace_buff = talent.natures_grace->ok()
                                      ? find_spell( talent.natures_grace->effectN( 1 ).trigger_spell_id() )
                                      : spell_data_t::not_found();
  buff.natures_grace = make_buff_fallback( natures_grace_buff->ok(), this, "natures_grace", natures_grace_buff );

  auto const eclipse_buff = find_spell( 408255 );
  buff.eclipse            = make_buff_fallback( talent.eclipse->ok(), this, "eclipse", eclipse_buff );

  auto const cat_form_spell = rank.cat_form.find_rank()->buff();
  buff.cat_form             = make_buff_fallback<stat_buff_t>( cat_form_spell->ok(), this, "cat_form", cat_form_spell )
                                  ->set_stack_change_callback( [ this ]( buff_t*, int, int new_ ) {
                        current.attack_power_per_agility = new_ ? 1.0 : base.attack_power_per_agility;
                        invalidate_cache( CACHE_ATTACK_POWER );
                                  } );

  auto const prowl_spell = rank.prowl.find_rank()->spell();
  buff.prowl             = make_buff_fallback( prowl_spell->ok(), this, "prowl", prowl_spell );

  auto const poised_passive = find_spell( 1328829 );
  auto const poised_buff    = find_spell( 1328827 );
  buff.poised = make_buff_fallback( poised_passive->ok() && true_level >= static_cast<int>( poised_passive->level() ), this, "poised",
                                    poised_buff );
  register_precombat_begin( buff.poised );

  buff.moonkin_form =
      make_buff_fallback( talent.moonkin_form->ok(), this, "moonkin_form", talent.moonkin_form )
          ->set_period( 0_ms )
          ->set_expire_callback( [ this ]( buff_t*, int, timespan_t ) { buff.moonkin_aura->expire(); } )
          ->set_stack_change_callback( [ this ]( buff_t*, int, int ) { buff.moonkin_aura->trigger(); } );

  buff.moonkin_aura =
      make_buff_fallback( talent.moonkin_form->ok(), this, "moonkin_aura", talent.moonkin_form->effectN( 3 ).trigger() )
          ->set_default_value_from_effect( 1, .01 )
          ->set_pct_buff_type_from_effect( 1 );
};

void druid_t::init_special_effects()
{
  auto const omen_of_clarity = rank.omen_of_clarity.find_rank()->spell();
  if ( omen_of_clarity->ok() )
  {
    const auto driver = new special_effect_t( this );
    driver->name_str  = omen_of_clarity->name_cstr();
    driver->spell_id  = omen_of_clarity->id();
    // TODO 2 PPM is an educated guess based on old comments from classic. Must be reverified but beta logs were not big
    // enough. scale ppm by proc chance to pick up modifiers (Moonkin Form).
    driver->ppm_ = 2.0 * omen_of_clarity->proc_chance();
    special_effects.push_back( driver );

    callbacks.register_callback_execute_function(
        driver->spell_id, [ this ]( auto, auto, auto, auto ) { buff.clearcasting->trigger(); } );

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
  parse_effects( buff.moonkin_aura );
  parse_effects( buff.natures_grace );  // buffs spell haste and gcd, confirm the double dip on gcd speed
  parse_effects( buff.cat_form );
};

void druid_t::apply_action_effects( action_t* a )
{
  auto action = dynamic_cast<parse_action_base_t*>( a );
  assert( action );

  action->parse_effects( buff.clearcasting );
  action->parse_effects( buff.natures_grace );
  action->parse_effects( buff.cat_form );
  action->parse_effects( buff.poised ); 
}

void druid_t::create_actions()
{
  background_actions.mainhand_melee = get_action<druid_melee_t>( "auto_attack_mh", this );
  background_actions.offhand_melee  = get_action<druid_melee_t>( "auto_attack_oh", this );
  background_actions.cat_melee      = get_action<cat_melee_t>( "paw_attack", this );

  parse_player_effects_t::create_actions();
}

}  // UNNAMED NAMESPACE

const module_t* module_t::druid()
{
  static druid_module_t m;
  return &m;
}
