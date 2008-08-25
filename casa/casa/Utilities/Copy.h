//# Copy.h: Copy objects from one C-style array to another.
//# Copyright (C) 1994-1997,1999-2002,2005
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
//# $Id: Copy.h 20229 2008-01-29 15:19:06Z gervandiepen $

#ifndef CASA_COPY_H
#define CASA_COPY_H

//# Includes
#include <casa/aips.h>
#include <casa/BasicSL/Complex.h>
#include <algorithm>

namespace casa { //# NAMESPACE CASA - BEGIN

  // <summary>
  // Copy objects from one C-style array to another.
  // </summary>

  // <use visibility=export>
  // <reviewed reviewer="Friso Olnon" date="1995/03/14" tests="tCopy" demos="">
  // </reviewed>

  // <synopsis>
  // Objset is used to fill a C-style array of objects.
  //
  // Objcopy and objmove are used to copy objects from one place to
  // another. Optionally a stride can be supplied.
  //
  // The functions are equivalent to C's <src>memcpy</src> and
  // <src>memmove</src>.
  // When possible C++ standard library functions are used to implement them
  //
  // Similar to memcpy and memmove, the difference between objcopy
  // and objmove is that objmove takes account of an overlap of source and
  // destination. In general, objcopy is slighty (but only slighty) faster.
  // </synopsis>

  // <example>
  // Setting and copying arrays of built-in types:
  // <srcblock>
  // // Create uInt array of 4 elements
  // uInt size=4;
  // int* ia = new int[size];
  // // Initialize all elements to value 99
  // objset(ia, 99, size);
  // // Change all odd elements to 66 -> [99 66 99 66]
  // objset(ia+1,66, uInt(5), uInt(2));
  //
  // // Create another 4-element uInt array
  // int* ia2 = new int[size];
  // // Copy array ia into array ia2 -> [99 66 99 66]
  // objmove(ia2, ia, size);
  // // Copy the even elements of ia to the odd elements of ia2
  // //                              -> [99 99 99 99]
  // objcopy(ia2+1, ia, uInt(size/2), uInt(2), uInt(2));
  // </srcblock>
  //
  // Setting and copying arrays of a randomly chosen type:
  // <srcblock>
  // // Create 4-element array of 3-element Block<int> objects 
  // uInt size=4;
  // Block<int>* ta = new Block<int>[size];
  // Block<int> set(3);
  // // Initialize the array -> [[123][123][123][123]]
  // set[0] = 1; set[1] = 2; set[2] = 3;
  // objset(ta, set, size);
  // // Change odd Blocks to [777]-> [[123][777][123][777]]
  // set[0] = set[1] = set[2] = 7;
  // objset(ta + 1, set, uInt(size/2), uInt(2));
  //
  // // Create another Block<int> array 
  // Block<int>* ta2 = new Block<int>[size];
  // // Copy the even elements of ta to the first elements of ta2
  // //                      -> [[123][123]...]
  // objcopy(ta2, ta, uInt(size/2), uInt(1), uInt(2));
  // </srcblock>
  // </example>
  
  // <group name=throw>
  // Throw the various AipsErrors when incorrect arguments used
  void objthrowmv1(const void *to, const void *from, const uInt n);
  void objthrowmv2(const void *to, const void *from, const uInt n,
                   const uInt toStride, const uInt fromStride);
  void objthrowcp1(const void *to, const void *from, const uInt n);
  void objthrowcp2(const void *to, const void *from, const uInt n,
                   const uInt toStride, const uInt fromStride);
  void objthrowfl1(const void *to, const uInt n);
  void objthrowfl2(const void *to, const uInt n,
		   const uInt toStride);
  // </group>
  
  // <summary> Test routines </summary>
  // <group name=test>
  // Test on how to handle the overlap in move
  void objtestmv(uInt &nLeft, uInt &startLeft, uInt &startRight,
                 const void *to, const void *from, const uInt n,
                 const uInt toStride, const uInt fromStride,
                 const void *toPn, const void *fromPn,
                 const uInt fromMto, const uInt toMfrom);
  // </group>
  
  // <summary> Copy methods </summary>
  // <group name=copy>
  // The general function to copy <src>n</src> objects from one place
  // to another if overlap between <src>to</src> and <src>from</src> fields
  // is possible. Strides may be specified, i.e. you may copy from every
  // <src>fromStride</src>-th position into every <src>toStride</src>-th
  // one.
  //
  // The function will call <src>std::copy()</src> when possible.
  // Objmove works correctly if the source and destination overlap in any way.
  //
  // An exception will be thrown if the source or the destination does not
  // exist (and <em>n</em> is non-zero) or if the strides are non-positive.
  // <thrown>
  //  <li> AipsError
  // </thrown>
  //
  // <group>
  template<class T> void objmove(T* to, const T* from, uInt n) {
    objthrowmv1(to,from,n);
    (to<from || to >= from+n) ? std::copy(from,from+n,to)
      : std::copy_backward(from,from+n,to+n); }
  template<class T> void objmove(T* to, const T* from, uInt n,
				 uInt toStride, uInt fromStride) {
    if (!n) return;
    objthrowmv2(to,from,n,toStride,fromStride);
    if (toStride*fromStride == 1) { objmove(to, from, n);
    return; }
    uInt nLeft, startLeft, startRight;
    uInt fromMto=0; uInt toMfrom=0;
    if (toStride > fromStride && from > to)
      fromMto = (from-to)/(toStride-fromStride);
    else if (toStride < fromStride && from < to)
      toMfrom = (to-from)/(fromStride-toStride);
    objtestmv(nLeft, startLeft, startRight, to, from, n,
	      toStride, fromStride, to+n*toStride, from+n*fromStride,
	      fromMto, toMfrom);
    n -= nLeft;
    if (nLeft) {
      const T* fromPtr = from + startLeft*fromStride;
      T* toPtr = to + startLeft*toStride;
      while (nLeft--) { *toPtr = *fromPtr;
      fromPtr += fromStride;
      toPtr += toStride; }; };
    // Do the moves from the right.
    if (n) {
      const T* fromPtr = from + startRight*fromStride;
      T* toPtr = to + startRight*toStride;
      while (n--) { fromPtr -= fromStride; toPtr -= toStride;
      *toPtr = *fromPtr; }; };
  }
  // </group> 
  
  //# To support a container of const void*.
  //# I think that nowadays handled ok
  ///  inline void objmove(const void** to, const void*const *from, uInt n)
  ///    { memmove(to, from, n*sizeof(void*)); }
  ///  inline void objmove(const char** to, const char*const *from, uInt n)
  ///    { memmove(to, from, n*sizeof(char*)); }
  
  //# To support a container of void*.
  ///  inline void objmove(void** to, void*const *from, uInt n)
  ///    { memmove(to, from, n*sizeof(void*)); }
  ///  inline void objmove(char** to, char*const *from, uInt n)
  ///    { memmove(to, from, n*sizeof(char*)); }
  
  // The non-general function to copy <src>n</src> objects from one place
  // to another. Strides may be specified, i.e. you may copy from every
  // <src>fromStride</src>-th position into every <src>toStride</src>-th
  // one.
  //
  // Objcopy does not take an overlap of source and destination into account.
  // Objmove should be used if that is an issue.
  //
  // An exception will be thrown if the source or the destination does not
  // exist or if the strides are non-positive.
  // <thrown>
  //  <li> AipsError
  // </thrown>
  //
  // <group>
  template<class T> void objcopy(T* to, const T* from, uInt n) {
    objthrowcp1(to,from,n); std::copy(from, from+n, to); }
  template<class T> void objcopy(T* to, const T* from, uInt n, uInt toStride,
				 uInt fromStride) {
    objthrowcp2(to,from,n,toStride,fromStride); while (n--) {
      *to = *from; to += toStride; from += fromStride; } }
  // </group> 
  
  //# To support a container of const void*.
  //# Not necessary I think wnb
  ///  inline void objcopy(const void** to, const void*const *from, uInt n)
  ///    { memcpy(to, from, n*sizeof(void*)); }
  ///  inline void objcopy(const char** to, const char*const *from, uInt n)
  ///    { memcpy(to, from, n*sizeof(char*)); }
  
  //# To support a container of void*.
  ///  inline void objcopy(void** to, void*const *from, uInt n)
  ///    { memcpy(to, from, n*sizeof(void*)); }
  ///  inline void objcopy(char** to, char*const *from, uInt n)
  ///    { memcpy(to, from, n*sizeof(char*)); }
  
  // Fill <src>n</src> elements of an array of objects with the given
  // value, optionally with a stride. Note that the fillValue is passed
  // by value.
  //
  // An exception will be thrown if the destination array does not exist
  // or if the stride is non-positive.
  //
  // <thrown>
  //  <li> AipsError
  // </thrown>
  //
  // <group>
  template<class T> void objset(T* to, const T fillValue, uInt n) {
    objthrowfl1(to,n); std::fill_n(to, n, fillValue); }
  
  template<class T> void objset(T* to, const T fillValue, uInt n,
				uInt toStride) {
    objthrowfl2(to,n,toStride); 
    while (n--){*to = fillValue; to += toStride; }; }
  
  // </group>
  
  // </group>
  
} //# NAMESPACE CASA - END

#ifndef CASACORE_NO_AUTO_TEMPLATES
#include <casa/Utilities/Copy.tcc>
#endif //# CASACORE_NO_AUTO_TEMPLATES
#endif
