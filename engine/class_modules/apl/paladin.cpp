#include "class_modules/apl/paladin.hpp"

#include "player/action_priority_list.hpp"
#include "player/player.hpp"

namespace paladin_apl
{
std::string potion( const player_t* )
{
  return "disabled";
}

std::string flask( const player_t* )
{
  return "disabled";
}

std::string food( const player_t* )
{
  return "disabled";
}

std::string rune( const player_t* )
{
  return "disabled";
}

std::string temporary_enchant( const player_t* )
{
  return "disabled";
}

// paladin_apl_start
void paladin( player_t* p )
{
  action_priority_list_t* precombat = p->get_action_priority_list( "precombat" );
  action_priority_list_t* default_  = p->get_action_priority_list( "default" );

  precombat->add_action( "retribution_aura" );

  default_->add_action( "auto_attack" );
  default_->add_action( "consecration" );
  default_->add_action( "seal_of_fury,if=!buff.seal_of_fury.up" );
  default_->add_action( "holy_strike" );
  // default_->add_action( "hammer_of_justice" );
  default_->add_action( "holy_wrath" );
  default_->add_action( "exorcism" );

  default_->add_action( "holy_light,if=target.health.deficit>=crit_heal" );
}
// paladin_apl_end
}  // namespace paladin_apl
