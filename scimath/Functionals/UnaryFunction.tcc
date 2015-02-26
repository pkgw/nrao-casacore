//# UnaryFunction.cc: A one dimensional unary function
//# Copyright (C) 2002
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
//# $Id: UnaryFunction.tcc 21563 2015-02-16 07:05:15Z gervandiepen $

#ifndef SCIMATH_UNARYFUNCTION_TCC
#define SCIMATH_UNARYFUNCTION_TCC

#include <casacore/scimath/Functionals/UnaryFunction.h>
#include <casacore/casa/BasicSL/Constants.h>
#include <casacore/casa/BasicMath/Math.h>

namespace casacore { //# NAMESPACE CASACORE - BEGIN

//# Constructors

//# Operators
template<class T> 
T UnaryFunction<T>::eval(typename Function<T>::FunctionArg x) const {
  T tmp(abs(x[0] - param_p[CENTER]));
  if (tmp == param_p[WIDTH]) return T(0.5)*param_p[HEIGHT];
  if (tmp <  param_p[WIDTH]) return param_p[HEIGHT];
  return T(0.0);
}

//# Member functions

} //# NAMESPACE CASACORE - END


#endif
