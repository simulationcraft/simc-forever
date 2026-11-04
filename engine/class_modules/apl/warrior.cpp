#include "class_modules/apl/warrior.hpp"

#include "player/action_priority_list.hpp"
#include "player/player.hpp"

namespace warrior_apl
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

//warrior_apl_start
void warrior( player_t* p )
{
  action_priority_list_t* precombat = p->get_action_priority_list( "precombat" );
  action_priority_list_t* default_  = p->get_action_priority_list( "default" );

  precombat->add_action( "bloodrage" );
  precombat->add_action( "battle_shout" );
  precombat->add_action( "charge" );

  default_->add_action( "auto_attack" );

  default_->add_action( "battle_shout,if=!buff.battle_shout.up" );
  default_->add_action( "demoralizing_shout,if=!debuff.demoralizing_shout.up" );
  default_->add_action( "bloodrage,if=rage<80" );
  default_->add_action( "rend,if=!ticking" );
  default_->add_action( "overpower,if=buff.overpower.up" );
  default_->add_action( "revenge,if=buff.revenge_available.up" );
  default_->add_action( "mocking_blow" );
  default_->add_action( "disarm" );
  default_->add_action( "shield_block" );
  default_->add_action( "sunder_armor,if=debuff.sunder_armor.stack<5|debuff.sunder_armor.remains<5" );
  default_->add_action( "shield_bash" );
  default_->add_action( "thunder_clap,if=spell_targets>=2" );
  default_->add_action( "heroic_strike,if=rage>=30" );
}
//warrior_apl_end

}  // namespace warrior_apl
