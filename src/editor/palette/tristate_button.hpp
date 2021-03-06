/*
   Copyright (C) 2003 - 2013 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef TRISTATE_BUTTON_H_INCLUDED
#define TRISTATE_BUTTON_H_INCLUDED

#include "widgets/widget.hpp"

#include "exceptions.hpp"
#include "editor/palette/common_palette.hpp"

namespace gui {

class tristate_button : public widget
{

public:

	struct error : public game::error {
        error()
            : game::error("GUI1 tristate button error")
            {}
    };

	enum PRESSED_STATE { LEFT, RIGHT, BOTH, NONE };

	enum SPACE_CONSUMPTION { DEFAULT_SPACE, MINIMUM_SPACE };

	tristate_button(CVideo& video,
			editor::common_palette* palette,
			std::string button_image="",
			SPACE_CONSUMPTION spacing=DEFAULT_SPACE,
			const bool auto_join=true);

	/** Default implementation, but defined out-of-line for efficiency reasons. */
	virtual ~tristate_button();

	void set_pressed(PRESSED_STATE new_pressed_state);

	//	void set_active(bool active);

	bool pressed();
	PRESSED_STATE pressed_state() const;

	void set_label(const std::string& val);

	bool hit(int x, int y) const;
	virtual void enable(bool new_val=true);
	void release();

	void set_item_image(
			const surface& image)
	{
		itemImage_ = image;
	}

	void set_item_id(const std::string& id) {
		item_id_ = id;
	}

	void draw() {
		widget::draw();
	}

protected:

	virtual void handle_event(const SDL_Event& event);
	virtual void mouse_motion(const SDL_MouseMotionEvent& event);
	virtual void mouse_down(const SDL_MouseButtonEvent& event);
	virtual void mouse_up(const SDL_MouseButtonEvent& event);

	virtual void draw_contents();

private:

	void calculate_size();

	std::string label_;

	surface baseImage_, touchedBaseImage_, activeBaseImage_,
		itemImage_,
	//	normalImage_, activeImage_,
		pressedDownImage_, pressedUpImage_, pressedBothImage_,
		pressedBothActiveImage_, pressedDownActiveImage_, pressedUpActiveImage_,
		touchedDownImage_, touchedUpImage_, touchedBothImage_;
	//	disabledImage_, pressedDownDisabledImage_, pressedUpDisabledImage_, pressedBothDisabledImage_;

	SDL_Rect textRect_;

	enum STATE { UNINIT,
		NORMAL, ACTIVE,
		PRESSED_ACTIVE_LEFT, PRESSED_ACTIVE_RIGHT, PRESSED_ACTIVE_BOTH,
		TOUCHED_RIGHT, TOUCHED_BOTH_RIGHT, TOUCHED_BOTH_LEFT, TOUCHED_LEFT,
		PRESSED_LEFT, PRESSED_RIGHT, PRESSED_BOTH
	};


	STATE state_;

	bool pressed_;

	SPACE_CONSUMPTION spacing_;

	int base_height_, base_width_;

	editor::common_palette* palette_;

	std::string item_id_;

}; //end class button

}

#endif
