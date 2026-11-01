#include "class_modules/apl/rogue.hpp"

#include "player/action_priority_list.hpp"
#include "player/player.hpp"

namespace rogue_apl
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
//rogue_apl_start
void rogue( player_t* p )
{
  action_priority_list_t* precombat = p->get_action_priority_list( "precombat" );
  action_priority_list_t* default_  = p->get_action_priority_list( "default" );

  default_->add_action( "auto_attack" );

  default_->add_action( "eviscerate,if=combo_points>=5" );
  default_->add_action( "sinister_strike" );
}
//rogue_apl_end
}  // namespace rogue_apl
