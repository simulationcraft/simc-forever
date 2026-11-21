#include "class_modules/apl/hunter.hpp"

#include "player/action_priority_list.hpp"
#include "player/player.hpp"

namespace hunter_apl
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

//hunter_apl_start
void hunter( player_t* p )
{
  action_priority_list_t* precombat = p->get_action_priority_list( "precombat" );
  action_priority_list_t* default_  = p->get_action_priority_list( "default" );

  precombat->add_action( "aspect_of_the_hawk" );

  default_->add_action( "auto_shot" );
  default_->add_action( "serpent_sting,if=!ticking" );
  default_->add_action( "arcane_shot,if=dot.serpent_sting.ticking" );
}
//hunter_apl_end
}  // namespace hunter_apl
