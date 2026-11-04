#include "class_modules/apl/druid.hpp"

#include "player/action_priority_list.hpp"
#include "player/player.hpp"

namespace druid_apl
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

// druid_apl_start
void druid( player_t* p )
{
  action_priority_list_t* default_  = p->get_action_priority_list( "default" );
  action_priority_list_t* balance   = p->get_action_priority_list( "balance" );
  action_priority_list_t* precombat = p->get_action_priority_list( "precombat" );

  precombat->add_action( "snapshot_stats" );
  precombat->add_action( "moonkin_form" );
  precombat->add_action( "entangling_roots" );

  default_->add_action( "call_action_list,name=balance" );

  balance->add_action( "faerie_fire,if=time<1" );
  balance->add_action( "hurricane,if=time<10" );
  balance->add_action( "innervate" );
  balance->add_action( "starfire,if=buff.clearcasting.react|buff.eclipse.react" );
  balance->add_action( "moonfire,if=!ticking" );
  balance->add_action( "insect_swarm,if=!ticking" );
  balance->add_action( "wrath" );
  balance->add_action( "auto_attack" );
}
// druid_apl_end
}  // namespace druid_apl
