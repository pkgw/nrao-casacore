//# SubImage.h: A (masked) subset of an ImageInterface object
//# Copyright (C) 1998,1999,2000,2001,2003
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
//# $Id: SubImage.h 20739 2009-09-29 01:15:15Z Malte.Marquarding $

#ifndef IMAGES_SUBIMAGE_H
#define IMAGES_SUBIMAGE_H


//# Includes
#include <images/Images/ImageInterface.h>
#include <casa/Arrays/AxesSpecifier.h>

#include <memory>

namespace casa { //# NAMESPACE CASA - BEGIN

//# Forward Declarations
class IPosition;
class LattRegionHolder;
class Slicer;
template <class T> class SubLattice;
template <class T> class Array;
class LatticeNavigator;
template <class T> class LatticeIterInterface;
class String;


// <summary>
// A (masked) subset of an ImageInterface object.
// </summary>
//
// <use visibility=export>
//
// <reviewed reviewer="" date="" tests="tSubImage.cc">
// </reviewed>
//
// <prerequisite>
//   <li> <linkto class=ImageInterface>ImageInterface</linkto>
//   <li> <linkto class=SubLattice>SubLattice</linkto>
// </prerequisite>
//
// <synopsis> 
// Class SubImage has to be used to apply a region or mask to an image.
// Several functions are inherited from SubLattice and not declared
// in this class.
// <p>
// Using an <linkto class=AxesSpecifier>AxesSpecifier</linkto> object
// it is possible to remove some or all degenerate axes (i.e. axes
// with length 1) to get an image with a lower dimensionality.
// </synopsis> 
//
// <example>
// <srcblock>
// </srcblock>
// </example>
//
// <motivation>
// </motivation>
//
// <todo asof="1998/02/09">
// </todo>


template <class T> class SubImage: public ImageInterface<T>
{
public: 
  // The default constructor
  SubImage();

  // Create a SubImage from a Image.
  // This results in a SubImage without a real mask.
  // <br>The "const Image" version yields a non-writable SubImage,
  // while for the non-const version one has to specify if the SubImage
  // should be writable (if the original image is non-writable, the
  // SubImage is always set to non-writable).
  // <group>
  SubImage (ImageInterface<T>& image, AxesSpecifier=AxesSpecifier());
  SubImage (ImageInterface<T>& image, Bool writableIfPossible,
	    AxesSpecifier=AxesSpecifier());
  // </group>

  // Create a SubImage from the given Image and region.
  // <br>An exception is thrown if the image shape used in the region
  // differs from the shape of the image.
  // <group>
  SubImage (const ImageInterface<T>& image, const LattRegionHolder& region,
	    AxesSpecifier=AxesSpecifier());
  SubImage (ImageInterface<T>& image, const LattRegionHolder& region,
	    Bool writableIfPossible, AxesSpecifier=AxesSpecifier());
  // </group>
  
  // Create a SubImage from the given Image and slicer.
  // The slicer can be strided.
  // <br>An exception is thrown if the slicer exceeds the image shape.
  // <group>
  SubImage (const ImageInterface<T>& image, const Slicer& slicer,
	    AxesSpecifier=AxesSpecifier());
  SubImage (ImageInterface<T>& image, const Slicer& slicer,
	    Bool writableIfPossible, AxesSpecifier=AxesSpecifier());
  // </group>
  
  // Copy constructor (reference semantics).
  SubImage (const SubImage<T>& other);
    
  virtual ~SubImage();

  // Assignment (reference semantics).
  SubImage<T>& operator= (const SubImage<T>& other);

  // Make a copy of the object (reference semantics).
  // <group>
  virtual ImageInterface<T>* cloneII() const;
  // </group>

  // Get the image type (returns name of derived class).
  virtual String imageType() const;

  // Is the SubImage masked?
  // It is if its parent image or its region is masked.
  virtual Bool isMasked() const;

  // Does the image object have a pixelmask?
  // It does if its parent has a pixelmask.
  virtual Bool hasPixelMask() const;

  // Get access to the pixelmask in use (thus to the pixelmask of the parent).
  // An exception is thrown if the parent does not have a pixelmask.
  // <group>
  virtual const Lattice<Bool>& pixelMask() const;
  virtual Lattice<Bool>& pixelMask();
  // </group>

  // A SubImage is persistent if no region is applied to the parent image.
  // That is true if the region has the same shape as the parent image
  // and the region has no mask.
  virtual Bool isPersistent() const;

  // Is the SubImage paged to disk?
  virtual Bool isPaged() const;

  // Can the lattice data be referenced as an array section?
  virtual Bool canReferenceArray() const;

  // Is the SubImage writable?
  virtual Bool isWritable() const;

  // Get the region/mask object describing this subImage.
  virtual const LatticeRegion* getRegionPtr() const;

  // Returns the shape of the SubImage including all degenerate axes
  // (i.e. axes with a length of one).
  virtual IPosition shape() const;
  
  // Returns the number of axes in this SubImage. This includes all
  // degenerate axes.
  virtual uInt ndim() const;
  
  // Returns the total number of elements in this SubImage.
  virtual size_t nelements() const;
  
  // returns a value of "True" if this instance of Lattice and 'other' have 
  // the same shape, otherwise returns a value of "False".
  virtual Bool conform (const Lattice<T>& other) const;
  
  // This function returns the recommended maximum number of pixels to
  // include in the cursor of an iterator.
  virtual uInt advisedMaxPixels() const;

  // Get or put a single element in the lattice.
  // <group>
  virtual T getAt (const IPosition& where) const;
  virtual void putAt (const T& value, const IPosition& where);
  // </group>
  
  // Function which changes the shape of the SubImage.
  // Throws an exception as resizing a SubImage is not possible.
  virtual void resize(const TiledShape& newShape);

  // Return the name of the parent ImageInterface object. 
  virtual String name (Bool stripPath=False) const;
  
  // Check class invariants.
  virtual Bool ok() const;

  // Do the actual getting of an array of values.
  virtual Bool doGetSlice (Array<T>& buffer, const Slicer& section);

  // Do the actual getting of an array of values.
  virtual void doPutSlice (const Array<T>& sourceBuffer,
			   const IPosition& where,
			   const IPosition& stride);
  
  // Get a section of the mask.
  virtual Bool doGetMaskSlice (Array<Bool>& buffer, const Slicer& section);

  // This function is used by the LatticeIterator class to generate an
  // iterator of the correct type for this Lattice. Not recommended
  // for general use. 
  virtual LatticeIterInterface<T>* makeIter
                               (const LatticeNavigator& navigator,
				Bool useRef) const;

  // Get the best cursor shape.
  virtual IPosition doNiceCursorShape (uInt maxPixels) const;

  // Handle the (un)locking and syncing, etc.
  // <group>
  virtual Bool lock (FileLocker::LockType, uInt nattempts);
  virtual void unlock();
  virtual Bool hasLock (FileLocker::LockType) const;
  virtual void resync();
  virtual void flush();
  virtual void tempClose();
  virtual void reopen();
  // </group>

  // Factory method to create a SubImage from a region and a WCLELMask string.
  // Moved from ImageAnalysis
  // <src>outRegion</src> Pointer to the corresponding region. Pointer is
  // created internally by new(); it is the caller's responsibility to delete it.
  // <src>outMask</src> Pointer to corresponding mask. Pointer is created
  // internally via new(); it is the caller's responsibility to delete it.
  // <src>inImage</src> input image for which a subimage is desired.
  // <src>region</src> Input region record from which to make the subimage.
  // <src>mask</src> LEL mask description.
  // <src>os</src> Pointer to logger to which to log messages. If 0, no logging (except exceptions).
  // <src>writableIfPossible</src> make the subimage writable. If input image is not writable, this
  // will always be False.
  // <src>axesSpecifier</src> Specifier for output axes (duh).
  // <src>extendMask</src> If the mask has one
  // or more of degenerate axes whereas the corresponding axes of <src>inImage</src> are
  // not, extend the mask to match the shape of the input image.

  static SubImage<T> createSubImage(
	  ImageRegion*& outRegion, ImageRegion*& outMask,
      ImageInterface<T>& inImage, const Record& region,
      const String& mask, LogIO *const &os, Bool writableIfPossible,
      const AxesSpecifier& axesSpecifier=casa::AxesSpecifier(),
      const Bool extendMask=False
  );

  // variant on previous methods where caller doesn't have to worry
  // about creating pointers it does not need returned.
  static SubImage<T> createSubImage(
      ImageInterface<T>& inImage, const Record& region,
      const String& mask, LogIO *const &os, Bool writableIfPossible,
      const AxesSpecifier& axesSpecifier=casa::AxesSpecifier(),
      const Bool extendMask=False
  );

private:
  // Set the coordinates.
  // It removes world axes if the subimage has axes removed.
  void setCoords (const CoordinateSystem& coords);

  // Set the other members in the parent.
  void setMembers(const ImageBeamSet& hpBeams);

  // Helper
   void convertIPosition(Vector<Float>& x, const IPosition& pos) const;

  //# itsImagePtr points to the parent image.
  std::auto_ptr<ImageInterface<T> > itsImagePtr;
  std::auto_ptr<SubLattice<T> >    itsSubLatPtr;


  // Given an original image shape and coordinate system and a subimage shape and
  // coordinate system, get the corresponding per plane beam array. The subimage
  // coordinate system must have been made with origCoords.subimage() and both must
  // have the same types of axes in the same order. The shapes must have the same
  // number of dimensions and all members of <src>subShape</src> must be greater or
  // equal to 1 and less than the corresponding members of <src>origShape</src>.
  ImageBeamSet _beamsForSubImage(
  	const IPosition& subShape,
  	const CoordinateSystem& subCoords
  );


  //# Make members of parent class known.
public:
  using ImageInterface<T>::logger;
protected:
  using ImageInterface<T>::setCoordsMember;
};



} //# NAMESPACE CASA - END

#ifndef CASACORE_NO_AUTO_TEMPLATES
#include <images/Images/SubImage.tcc>
#endif //# CASACORE_NO_AUTO_TEMPLATES
#endif
