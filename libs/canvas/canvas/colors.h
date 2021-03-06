/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_canvas_colors_h__
#define __ardour_canvas_colors_h__

#include <cairomm/context.h>

#include "canvas/visibility.h"
#include "canvas/types.h"

namespace ArdourCanvas 
{

struct LIBCANVAS_API HSV;
struct LIBCANVAS_API HSVA;

extern LIBCANVAS_API Color hsv_to_color (double h, double s, double v, double a = 1.0);
extern LIBCANVAS_API Color hsv_to_color (const HSV&, double a = 1.0);
extern LIBCANVAS_API Color hsva_to_color (const HSVA&);
extern LIBCANVAS_API void  color_to_hsv (Color color, double& h, double& s, double& v);

extern LIBCANVAS_API void  color_to_rgba (Color, double& r, double& g, double& b, double& a);
extern LIBCANVAS_API Color rgba_to_color (double r, double g, double b, double a);

uint32_t LIBCANVAS_API contrasting_text_color (uint32_t c);

struct LIBCANVAS_API HSV 
{
	HSV ();
	HSV (double h, double s, double v);
	HSV (Color);
	virtual ~HSV() {}

	double h;
	double s;
	double v;

	bool is_gray() const { return s == 0; }

	operator Color() const { return hsv_to_color (*this); }

	HSV operator+ (const HSV&) const;
	HSV operator- (const HSV&) const;
	HSV operator* (double) const;

	HSV darker (double factor = 1.3) const { return shade (factor); }
	HSV lighter (double factor = 0.7) const { return shade (factor); }

	HSV shade (double factor) const;
	HSV mix (const HSV& other, double amt) const;

	void print (std::ostream&) const;

  protected:
	virtual void clamp();
};



struct LIBCANVAS_API HSVA : public HSV
{
	HSVA ();
	HSVA (double h, double s, double v, double a);
	HSVA (Color);

	double a;

	operator Color() const { return hsva_to_color (*this); }

	HSVA operator+ (const HSVA&) const;
	HSVA operator- (const HSVA&) const;

	void print (std::ostream&) const;

  protected:
	void clamp ();
};

}

std::ostream& operator<<(std::ostream& o, const ArdourCanvas::HSV& hsv);
std::ostream& operator<<(std::ostream& o, const ArdourCanvas::HSVA& hsva);

#endif /* __ardour_canvas_colors_h__ */
