//# Link.cc: Singly linked list primitive
//# Copyright (C) 1993,1994,1995
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
//# $Id: Link.tcc 21561 2015-02-16 06:57:35Z gervandiepen $

#ifndef CASA_LINK_TCC
#define CASA_LINK_TCC

#include <casacore/casa/Containers/Link.h>

namespace casacore { //# NAMESPACE CASACORE - BEGIN

template<class t> Link<t> *Link<t>::unlink(Link<t> *) {
  Link<t> *nxt = Next;
  if (Prev) (*Prev).Next = Next;
  if (nxt) (*nxt).Prev = Prev;
  Next = Prev = 0;
  return nxt;
}

//
// Destructor needs to be out-of-line because of recursive call to delete
template<class t> Link<t>::~Link() {
  if (Next != 0) delete Next;}


} //# NAMESPACE CASACORE - END


#endif
