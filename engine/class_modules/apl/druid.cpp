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

//druid_apl_start
void druid( player_t* p )
{
  action_priority_list_t* default_ = p->get_action_priority_list( "default" );
  action_priority_list_t* precombat = p->get_action_priority_list( "precombat" );

  precombat->add_action( "snapshot_stats" );
  precombat->add_action( "entangling_roots" );

  default_->add_action( "faerie_fire,if=time<1" );
  default_->add_action( "hurricane,if=time<10" );
  default_->add_action( "innervate" );
  default_->add_action( "moonfire,if=!ticking" );
  default_->add_action( "starfire" );
  default_->add_action( "wrath" );
  default_->add_action( "auto_attack" );
}
//druid_apl_end
}  // namespace druid_apl
