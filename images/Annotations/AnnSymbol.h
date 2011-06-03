//# Copyright (C) 1998,1999,2000,2001
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This library is free software; you can redistribute it and/or modify it
//# under the terms of the GNU Library General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or (at your
//# option) any later version.
//#
//# This library is distributed in the hope that it will be useful, but WITHOUT
//# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
//# License for more details.
//#
//# You should have received a copy of the GNU Library General Public License
//# along with this library; if not, write to the Free Software Foundation,
//# Inc., 675 Massachusetts Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning AIPS++ should be addressed as follows:
//#        Internet email: aips2-request@nrao.edu.
//#        Postal address: AIPS++ Project Office
//#                        National Radio Astronomy Observatory
//#                        520 Edgemont Road
//#                        Charlottesville, VA 22903-2475 USA
//#

#ifndef ANNOTATIONS_ANNSYMBOL_H
#define ANNOTATIONS_ANNSYMBOL_H

#include <images/Annotations/AnnotationBase.h>

#include <measures/Measures/MDirection.h>

namespace casa {

// <summary>Represents a symbol annotation</summary>

// <use visibility=export>

// <reviewed reviewer="" date="yyyy/mm/dd">
// </reviewed>

// <synopsis>

// Represents an ascii symbol annotation
// </synopsis>

class AnnSymbol: public AnnotationBase {
public:

	// allowed symbols
	enum Symbol {
		POINT,
		PIXEL,
		CIRCLE,
		TRIANGLE_DOWN,
		TRIANGLE_UP,
		TRIANGLE_LEFT,
		TRIANGLE_RIGHT,
		TRI_DOWN,
		TRI_UP,
		TRI_LEFT,
		TRI_RIGHT,
		SQUARE,
		PENTAGON,
		STAR,
		HEXAGON1,
		HEXAGON2,
		PLUS,
		X,
		DIAMOND,
		THIN_DIAMOND,
		VLINE,
		HLINE,
		UNKOWN
	};

	/*
	AnnSymbol(
		const Quantity& x, const Quantity& y,
		const String& dirRefFrameString,
		const CoordinateSystem& csys,
		const Symbol symbol
	);
	*/

	AnnSymbol(
		const Quantity& x, const Quantity& y,
		const String& dirRefFrameString,
		const CoordinateSystem& csys,
		const Char symbolChar
	);

	MDirection getDirection() const;

	Symbol getSymbol() const;

	static Symbol charToSymbol(const Char c);

protected:
	Vector<Quantity> _inputDirection;
	Symbol _symbol;

private:
	void _init(const Quantity& x, const Quantity& y);

};



}

#endif
