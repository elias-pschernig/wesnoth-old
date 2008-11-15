/* $Id$ */
/*
   Copyright (C) 2003 - 2008 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file unit_display.cpp */

#include "global.hpp"
#include "unit_display.hpp"

#include "game_preferences.hpp"
#include "log.hpp"
#include "mouse_events.hpp"


#define LOG_DP LOG_STREAM(info, display)

static void teleport_unit_between( const map_location& a, const map_location& b, unit& temp_unit)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || (disp->fogged(a) && disp->fogged(b))) {
		return;
	}
	disp->scroll_to_tiles(a,b,game_display::ONSCREEN,true);

	if (!disp->fogged(a)) { // teleport
		disp->place_temporary_unit(temp_unit,a);
		unit_animator animator;
		animator.add_animation(&temp_unit,"pre_teleport",a);
		animator.start_animations();
		animator.wait_for_end();
	}
	if (!disp->fogged(b)) { // teleport
		disp->place_temporary_unit(temp_unit,b);
		disp->scroll_to_tiles(b,a,game_display::ONSCREEN,true);
		unit_animator animator;
		animator.add_animation(&temp_unit,"post_teleport",b);
		animator.start_animations();
		animator.wait_for_end();
	}
	temp_unit.set_standing(b);
	disp->update_display();
	events::pump();
}

static void move_unit_between(const map_location& a, const map_location& b, unit& temp_unit)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || (disp->fogged(a) && disp->fogged(b))) {
		return;
	}


	disp->place_temporary_unit(temp_unit,a);
	temp_unit.set_facing(a.get_relative_dir(b));
	unit_animator animator;
	animator.replace_anim_if_invalid(&temp_unit,"movement",a);
	animator.start_animations();
        animator.pause_animation();
	disp->scroll_to_tiles(a,b,game_display::ONSCREEN);
        animator.restart_animation();
	int target_time = animator.get_animation_time_potential();
	target_time += 150;
	target_time -= target_time%150;
	if(  target_time - animator.get_animation_time_potential() < 100 ) target_time +=150;
	animator.wait_until(target_time);
	map_location arr[6];
	get_adjacent_tiles(a, arr);
	unsigned int i;
	for (i = 0; i < 6; i++) {
		disp->invalidate(arr[i]);
	}
	get_adjacent_tiles(b, arr);
	for (i = 0; i < 6; i++) {
		disp->invalidate(arr[i]);
	}
}

namespace unit_display
{

bool unit_visible_on_path( const std::vector<map_location>& path, const unit& u, const unit_map& units, const std::vector<team>& teams)
{
	game_display* disp = game_display::get_singleton();
	assert(disp);
	for(size_t i = 0; i+1 < path.size(); ++i) {
		const bool invisible = teams[u.side()-1].is_enemy(int(disp->viewing_team()+1)) &&
	             u.invisible(path[i],units,teams) &&
		         u.invisible(path[i+1],units,teams);
		if(!invisible) {
			return true;
		}
	}

	return false;
}

void move_unit(const std::vector<map_location>& path, unit& u, const std::vector<team>& teams)
{
	game_display* disp = game_display::get_singleton();
	assert(!path.empty());
	assert(disp);
	// One hex path (strange), nothing to do
	if (path.size()==1) return;

	const unit_map& units = disp->get_units();

	bool invisible = teams[u.side()-1].is_enemy(int(disp->viewing_team()+1)) &&
		u.invisible(path[0],units,teams);

	if(!invisible) {
		// Scroll to the path, but only if it fully fits on screen.
		// If it does not fit we might be able to do a better scroll later.
		disp->scroll_to_tiles(path, game_display::ONSCREEN, true, true);
	}

	bool was_hidden = u.get_hidden();
	// Original unit is usually hidden (but still on map, so count is correct)
	unit temp_unit = u;
	u.set_hidden(true);
	temp_unit.set_hidden(false);
        disp->draw();
	for(size_t i = 0; i+1 < path.size(); ++i) {

		invisible = teams[temp_unit.side()-1].is_enemy(int(disp->viewing_team()+1)) &&
				temp_unit.invisible(path[i],units,teams) &&
				temp_unit.invisible(path[i+1],units,teams);

		if(!invisible) {
			if (!disp->tile_on_screen(path[i]) || !disp->tile_on_screen(path[i+1])) {
				// prevent the unit from dissappearing if we scroll here with i == 0
				disp->place_temporary_unit(temp_unit,path[i]);
				// scroll in as much of the remaining path as possible
				std::vector<map_location> remaining_path;
				for(size_t j = i; j < path.size(); j++) {
					remaining_path.push_back(path[j]);
				}
				disp->scroll_to_tiles(remaining_path);
			}

			if( !tiles_adjacent(path[i], path[i+1])) {
				teleport_unit_between(path[i],path[i+1],temp_unit);
			} else {
				move_unit_between(path[i],path[i+1],temp_unit);
			}
		}
	}
	disp->remove_temporary_unit();
	u.set_facing(path[path.size()-2].get_relative_dir(path[path.size()-1]));
	u.set_standing(path[path.size()-1]);

	u.set_hidden(was_hidden);
	disp->invalidate_unit_after_move(path[0], path[path.size()-1]);

	events::mouse_handler* mousehandler = events::mouse_handler::get_singleton();
	if (mousehandler) {
		mousehandler->invalidate_reachmap();
	}
}

void unit_die(const map_location& loc, unit& loser,
		const attack_type* attack,const attack_type* secondary_attack, unit* winner)
{
	game_display* disp = game_display::get_singleton();
	if(!disp ||disp->video().update_locked() || disp->fogged(loc) || preferences::show_combat() == false) {
		return;
	}
	unit_animator animator;
	// hide the hp/xp bars of the loser (useless and prevent bars around an erased unit)
	animator.add_animation(&loser,"death",loc,0,false,false,"",0,unit_animation::KILL,attack,secondary_attack,0);
	// but show the bars of the winner (avoid blinking and show its xp gain)
	animator.add_animation(winner,"victory",loc.get_direction(loser.facing()),0,true,false,"",0,
			unit_animation::KILL,secondary_attack,attack,0);
	animator.start_animations();
	animator.wait_for_end();

	events::mouse_handler* mousehandler = events::mouse_handler::get_singleton();
	if (mousehandler) {
		mousehandler->invalidate_reachmap();
	}
}


void unit_attack(
                 const map_location& a, const map_location& b, int damage,
                 const attack_type& attack, const attack_type* secondary_attack,
		  int swing,std::string hit_text,bool drain,std::string att_text)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || preferences::show_combat() == false) return;
	unit_map& units = disp->get_units();
	disp->select_hex(map_location::null_location);
	const bool hide = disp->video().update_locked() || (disp->fogged(a) && disp->fogged(b));

	if(!hide) {
		// scroll such that there is at least half a hex spacing around fighters
		disp->scroll_to_tiles(a,b,game_display::ONSCREEN,true,0.5);
	}

	log_scope("unit_attack");

	const unit_map::iterator att = units.find(a);
	assert(att != units.end());
	unit& attacker = att->second;

	const unit_map::iterator def = units.find(b);
	assert(def != units.end());
	unit& defender = def->second;

	att->second.set_facing(a.get_relative_dir(b));
	def->second.set_facing(b.get_relative_dir(a));


	unit_animator animator;
	const map_location leader_loc = under_leadership(units,a);
	unit_map::iterator leader = units.end();

	{
		std::string text ;
		if(damage) text = lexical_cast<std::string>(damage);
		if(!hit_text.empty()) {
			text.insert(text.begin(),hit_text.size()/2,' ');
			text = text + "\n" + hit_text;
		}

		std::string text_2 ;
		if(drain && damage) text_2 = lexical_cast<std::string>(std::min<int>(damage,defender.hitpoints())/2);
		if(!att_text.empty()) {
			text_2.insert(text_2.begin(),att_text.size()/2,' ');
			text_2 = text_2 + "\n" + att_text;
		}

		unit_animation::hit_type hit_type;
		if(damage >= defender.hitpoints()) {
			hit_type = unit_animation::KILL;
		} else if(damage > 0) {
			hit_type = unit_animation::HIT;
		}else {
			hit_type = unit_animation::MISS;
		}
		animator.add_animation(&attacker,"attack",att->first,damage,true,false,text_2,display::rgb(0,255,0),hit_type,&attack,secondary_attack,swing);
		animator.add_animation(&defender,"defend",def->first,damage,true,false,text  ,display::rgb(255,0,0),hit_type,&attack,secondary_attack,swing);

		if(leader_loc.valid() && leader_loc != att->first && leader_loc != def->first){
			leader = units.find(leader_loc);
			leader->second.set_facing(leader_loc.get_relative_dir(a));
			assert(leader != units.end());
			animator.add_animation(&leader->second,"leading",leader_loc,damage,true,false,"",0,hit_type,&attack,secondary_attack,swing);
		}
	}

	animator.start_animations();
	animator.wait_for_end();

	if(leader_loc.valid() && leader_loc != att->first && leader_loc != def->first) leader->second.set_standing(leader_loc);
	att->second.set_standing(a);
	def->second.set_standing(b);
}


void unit_recruited(map_location& loc)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() ||disp->fogged(loc)) return;
	unit_map::iterator u = disp->get_units().find(loc);
	if(u == disp->get_units().end()) return;

	u->second.set_hidden(true);
	disp->scroll_to_tile(loc,game_display::ONSCREEN);
	disp->draw();
	u->second.set_hidden(false);
	u->second.set_facing(static_cast<map_location::DIRECTION>(rand()%map_location::NDIRECTIONS));
	unit_animator animator;
	animator.add_animation(&u->second,"recruited",loc);
	animator.start_animations();
	animator.wait_for_end();
	u->second.set_standing(loc);
	if (loc==disp->mouseover_hex()) disp->invalidate_unit();
}

void unit_healing(unit& healed,map_location& healed_loc, std::vector<unit_map::iterator> healers, int healing)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || disp->fogged(healed_loc)) return;
	if(healing==0) return;
	// This is all the pretty stuff.
	disp->scroll_to_tile(healed_loc, game_display::ONSCREEN);
	disp->display_unit_hex(healed_loc);
	unit_animator animator;

	for(std::vector<unit_map::iterator>::iterator heal_anim_it = healers.begin(); heal_anim_it != healers.end(); ++heal_anim_it) {
		(*heal_anim_it)->second.set_facing((*heal_anim_it)->first.get_relative_dir(healed_loc));
		animator.add_animation(&(*heal_anim_it)->second,"healing",(*heal_anim_it)->first,healing);
	}
	if (healing < 0) {
		animator.add_animation(&healed,"poisoned",healed_loc,-healing,false,false,lexical_cast<std::string>(-healing), display::rgb(255,0,0));
	} else {
		animator.add_animation(&healed,"healed",healed_loc,healing,false,false,lexical_cast<std::string>(healing), display::rgb(0,255,0));
	}
	animator.start_animations();
	animator.wait_for_end();

	healed.set_standing(healed_loc);
	for(std::vector<unit_map::iterator>::iterator heal_sanim_it = healers.begin(); heal_sanim_it != healers.end(); ++heal_sanim_it) {
		(*heal_sanim_it)->second.set_standing((*heal_sanim_it)->first);
	}

}

} // end unit_display namespace
