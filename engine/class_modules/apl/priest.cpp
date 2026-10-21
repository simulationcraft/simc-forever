#include "class_modules/apl/priest.hpp"

#include "player/action_priority_list.hpp"
#include "player/player.hpp"

namespace priest_apl
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

//priest_apl_start
void priest( player_t* p )
{
  action_priority_list_t* precombat = p->get_action_priority_list( "precombat" );
  action_priority_list_t* default_  = p->get_action_priority_list( "default" );

  precombat->add_action( "shadowform" );

  default_->add_action( "shadow_word_pain,if=!ticking" );
  default_->add_action( "devouring_plague,if=!ticking" );
  default_->add_action( "mind_blast" );
  default_->add_action( "shadow_word_death" );
  default_->add_action( "penance" );
  default_->add_action( "starshards" );
  default_->add_action( "mind_flay" );
  default_->add_action( "smite" );
}
//priest_apl_end
}  // namespace priest_apl
