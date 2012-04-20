//# SubImage.cc: A subset of a Image
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
//# $Id: SubImage.tcc 20739 2009-09-29 01:15:15Z Malte.Marquarding $

#include <images/Images/SubImage.h>

#include <images/Images/ImageExpr.h>
#include <images/Images/ExtendImage.h>
#include <images/Regions/WCBox.h>
#include <images/Regions/WCLELMask.h>
#include <coordinates/Coordinates/CoordinateUtil.h>
#include <lattices/Lattices/LattRegionHolder.h>
#include <lattices/Lattices/LCMask.h>
#include <lattices/Lattices/SubLattice.h>
#include <lattices/Lattices/LatticeRegion.h>
#include <casa/Arrays/IPosition.h>
#include <casa/Arrays/Vector.h>
#include <casa/Utilities/Assert.h>
#include <casa/Exceptions/Error.h>

#include <memory>

#include <casa/Arrays.h>


namespace casa { //# NAMESPACE CASA - BEGIN

template<class T>
SubImage<T>::SubImage()
: itsImagePtr  (0),
  itsSubLatPtr (0)
{}

template<class T>
SubImage<T>::SubImage (
	ImageInterface<T>& image,
	AxesSpecifier axesSpec
)
	: itsImagePtr(image.cloneII()) {
	itsSubLatPtr.reset(new SubLattice<T> (image, axesSpec));
	setCoords (image.coordinates());
	setMembers (*itsImagePtr);
}

template<class T>
SubImage<T>::SubImage (ImageInterface<T>& image,
		       Bool writableIfPossible,
		       AxesSpecifier axesSpec)
: itsImagePtr (image.cloneII())
{
	itsSubLatPtr.reset(
		new SubLattice<T> (image, writableIfPossible, axesSpec)
	);
	setCoords (image.coordinates());
	setMembers (*itsImagePtr);
}

template<class T>
SubImage<T>::SubImage (
	const ImageInterface<T>& image,
	const LattRegionHolder& region,
	AxesSpecifier axesSpec
)
	: itsImagePtr (image.cloneII()) {
	itsSubLatPtr.reset(
		new SubLattice<T> (
			image,
			region.toLatticeRegion(image.coordinates(),
			image.shape()),
			axesSpec)
		);
	const Slicer& slicer = itsSubLatPtr->getRegionPtr()->slicer();

	Vector<Float> blc, inc;
	convertIPosition(blc, slicer.start());
	convertIPosition(inc, slicer.stride());

	setCoords (image.coordinates().subImage (blc, inc, slicer.length().asVector()));
	setMembers (*itsImagePtr);
}

template<class T>
SubImage<T>::SubImage (ImageInterface<T>& image,
		       const LattRegionHolder& region,
		       Bool writableIfPossible,
		       AxesSpecifier axesSpec)
: itsImagePtr (image.cloneII())
{
    LatticeRegion latReg = region.toLatticeRegion(
				 image.coordinates(),
				 image.shape()
    );
    itsSubLatPtr.reset(
    	new SubLattice<T> (
    		image,
    		latReg,
    		writableIfPossible,
    		axesSpec
		)
    );


    const Slicer& slicer = itsSubLatPtr->getRegionPtr()->slicer();

	Vector<Float> blc, inc;
	convertIPosition(blc, slicer.start());
	convertIPosition(inc, slicer.stride());
	setCoords (image.coordinates().subImage (blc, inc, slicer.length().asVector()));
	setMembers (*itsImagePtr);
}

template<class T>
SubImage<T>::SubImage (
	const ImageInterface<T>& image,
	const Slicer& slicer,
	AxesSpecifier axesSpec
)
	: itsImagePtr (image.cloneII()) {
	itsSubLatPtr.reset(new SubLattice<T> (image, slicer, axesSpec));
	const Slicer& refslicer = itsSubLatPtr->getRegionPtr()->slicer();
	Vector<Float> blc, inc;
	convertIPosition(blc, refslicer.start());
	convertIPosition(inc, refslicer.stride());
	setCoords (image.coordinates().subImage (blc, inc, refslicer.length().asVector()));
	setMembers (*itsImagePtr);
}

template<class T>
SubImage<T>::SubImage (
	ImageInterface<T>& image,
	const Slicer& slicer,
	Bool writableIfPossible,
	AxesSpecifier axesSpec
)
	: itsImagePtr (image.cloneII()) {
	itsSubLatPtr.reset(
		new SubLattice<T> (
			image, slicer, writableIfPossible,
			axesSpec
		)
	);
	const Slicer& refslicer = itsSubLatPtr->getRegionPtr()->slicer();
	Vector<Float> blc, inc;
	convertIPosition(blc, refslicer.start());
	convertIPosition(inc, refslicer.stride());
	setCoords(
		image.coordinates().subImage(
			blc, inc, refslicer.length().asVector()
		)
	);
	setMembers (*itsImagePtr);
}

template<class T>
SubImage<T>::SubImage (const SubImage<T>& other)
: ImageInterface<T> (other),
  itsImagePtr (other.itsImagePtr->cloneII())
{
	itsSubLatPtr.reset(new SubLattice<T> (*other.itsSubLatPtr));
}

template<class T>
SubImage<T>::~SubImage() {}

template<class T>
SubImage<T>& SubImage<T>::operator= (const SubImage<T>& other)
{
  if (this != &other) {
    ImageInterface<T>::operator= (other);
    itsImagePtr.reset(other.itsImagePtr->cloneII());
    itsSubLatPtr.reset(new SubLattice<T> (*other.itsSubLatPtr));
  }
  return *this;
}

template<class T>
ImageInterface<T>* SubImage<T>::cloneII() const
{
  return new SubImage<T> (*this);
}

template<class T>
void SubImage<T>::setMembers (const ImageInterface<T>& image)
{
  setImageInfo (image.imageInfo());
  setMiscInfoMember (image.miscInfo());
  setUnitMember (image.units());
  logger().addParent (image.logger());
}

template<class T>
String SubImage<T>::imageType() const
{
  return "SubImage";
}

template<class T>
void SubImage<T>::setCoords (const CoordinateSystem& coords)
{
  const AxesMapping& axesMap = itsSubLatPtr->getAxesMap();
  AlwaysAssert (!axesMap.isReordered(), AipsError);
  if (!axesMap.isRemoved()) {
    setCoordsMember (coords);
  } else {
    const IPosition& map = axesMap.getToNew();
    const uInt naxes = map.nelements();
    Vector<Double> pixels(naxes), world(naxes);
    pixels = 0;
    coords.toWorld (world, pixels);
    CoordinateSystem crd(coords);
    for (Int i=naxes; i>0; ) {
      i--;
      if (map(i) < 0) {
	crd.removeWorldAxis (i, world(i));
      }
    }

// Actually drop any coordinates which have their axes fully removed

    CoordinateSystem crdOut;
    CoordinateUtil::dropRemovedAxes(crdOut, crd);
    setCoordsMember (crdOut);
  }
}

template<class T> SubImage<T> SubImage<T>::createSubImage(
	ImageRegion*& outRegion, ImageRegion*& outMask,
	ImageInterface<T>& inImage, const Record& region,
	const String& mask, LogIO *const &os,
	Bool writableIfPossible, const AxesSpecifier& axesSpecifier,
	const Bool extendMask
) {
	// The ImageRegion pointers must be null on entry
	// either pointer may be null on exit
	std::auto_ptr<ImageRegion> outMaskMgr(0);
    if (! mask.empty()) {
    	String mymask = mask;
    	for (uInt i=0; i<2; i++) {
    		try {
    			outMaskMgr.reset(ImageRegion::fromLatticeExpression(mymask));
    			break;
    		}
    		catch (AipsError x) {
    			if (i == 0) {
    				// not an LEL expression, perhaps it's a clean mask image name
    				mymask += ">=0.5";
    				continue;
    			}
    			LogIO *myos = os;
    			std::auto_ptr<LogIO> localLogMgr(0);
    			if (! myos) {
    				myos = new LogIO();
    				localLogMgr.reset(myos);
    			}
    			*myos << LogOrigin("SubImage", __FUNCTION__);
    			*myos << "Input mask specification is incorrect: "
    				<< x.getMesg() << LogIO::EXCEPTION;
    		}
    	}
	}
    if (
		extendMask && outMaskMgr.get() != 0
		&& outMaskMgr->asWCRegionPtr()->type() == "WCLELMask"
		&& ! dynamic_cast<const WCLELMask *>(
			outMaskMgr->asWCRegionPtr()
		)->getImageExpr()->shape().isEqual(inImage.shape())
	) {
		try {
			const WCRegion *wcptr = outMaskMgr->asWCRegionPtr();
			const WCLELMask *mymask = dynamic_cast<const WCLELMask *>(wcptr);
			const ImageExpr<Bool> *const imEx = mymask->getImageExpr();
			ExtendImage<Bool> exIm(*imEx, inImage.shape(), inImage.coordinates());
			outMaskMgr.reset(new ImageRegion(LCMask(exIm)));
		}
		catch (AipsError x) {
			LogIO *myos = os;
			std::auto_ptr<LogIO> localLogMgr(0);
			if (! myos) {
				myos = new LogIO();
				localLogMgr.reset(myos);
			}
			*myos << LogOrigin("SubImage", __FUNCTION__);
			*myos << "Unable to extend mask: " << x.getMesg() << LogIO::EXCEPTION;
		}
	}
	SubImage<T> subImage;
	// We can get away with no region processing if the region record
	// is empty and the user is not dropping degenerate axes
	if (region.nfields() == 0 && axesSpecifier.keep()) {
		subImage = (outMaskMgr.get() == 0)
			? SubImage<T>(inImage, True)
			: SubImage<T>(
				inImage, *outMaskMgr,
				writableIfPossible
			);
	}
	else {
		std::auto_ptr<ImageRegion> outRegionMgr(
			ImageRegion::fromRecord(
				os, inImage.coordinates(),
				inImage.shape(), region
			)
		);
		if (outMaskMgr.get() == 0) {
            subImage = SubImage<T>(
				inImage, *outRegionMgr,
				writableIfPossible, axesSpecifier
			);
		}
		else {
            SubImage<T> subImage0(
				inImage, *outMaskMgr, writableIfPossible
			);
			subImage = SubImage<T>(
				subImage0, *outRegionMgr,
				writableIfPossible, axesSpecifier
			);
		}
		outRegion = outRegionMgr.release();
	}
	outMask = outMaskMgr.release();
	return subImage;
}

template<class T> SubImage<T> SubImage<T>::createSubImage(
	ImageInterface<T>& inImage, const Record& region,
	const String& mask, LogIO *const &os,
	Bool writableIfPossible, const AxesSpecifier& axesSpecifier,
	const Bool extendMask
) {
	ImageRegion *pRegion = 0;
	ImageRegion *pMask = 0;
	return createSubImage(
		pRegion, pMask, inImage, region,
		mask, os, writableIfPossible, axesSpecifier,
		extendMask
	);
	delete pRegion;
	delete pMask;
}

template <class T>
Bool SubImage<T>::ok() const
{
  return itsSubLatPtr->ok();
}

template<class T>
Bool SubImage<T>::isMasked() const
{
  return itsSubLatPtr->isMasked();
}

template<class T>
Bool SubImage<T>::isPersistent() const
{
  return itsSubLatPtr->isPersistent();
}

template<class T>
Bool SubImage<T>::isPaged() const
{
  return itsSubLatPtr->isPaged();
}

template<class T>
Bool SubImage<T>::canReferenceArray() const
{
  return itsSubLatPtr->canReferenceArray();
}

template<class T>
Bool SubImage<T>::isWritable() const
{
  return itsSubLatPtr->isWritable();
}

template<class T>
Bool SubImage<T>::hasPixelMask() const
{
  return itsSubLatPtr->hasPixelMask();
}

template<class T>
const Lattice<Bool>& SubImage<T>::pixelMask() const
{
  return itsSubLatPtr->pixelMask();
}
template<class T>
Lattice<Bool>& SubImage<T>::pixelMask()
{
  return itsSubLatPtr->pixelMask();
}

template<class T>
const LatticeRegion* SubImage<T>::getRegionPtr() const
{
    return itsSubLatPtr->getRegionPtr();
}

template<class T>
IPosition SubImage<T>::shape() const
{
  return itsSubLatPtr->shape();
}

template<class T>
uInt SubImage<T>::ndim() const
{
  return itsSubLatPtr->ndim();
}

template<class T>
size_t SubImage<T>::nelements() const
{
  return itsSubLatPtr->nelements();
}

template<class T>
Bool SubImage<T>::conform (const Lattice<T>& other) const
{
  return shape().isEqual (other.shape());
}

template<class T>
void SubImage<T>::resize (const TiledShape&)
{
  throw (AipsError ("SubImage::resize is not possible"));
}

template<class T>
String SubImage<T>::name (Bool stripPath) const
{
  return itsImagePtr->name (stripPath);
}
  
template<class T>
Bool SubImage<T>::doGetSlice (Array<T>& buffer,
			      const Slicer& section)
{
  return itsSubLatPtr->doGetSlice (buffer, section);
}

template<class T>
void SubImage<T>::doPutSlice (const Array<T>& sourceBuffer,
			      const IPosition& where, 
			      const IPosition& stride)
{
  itsSubLatPtr->doPutSlice (sourceBuffer, where, stride);
}

template<class T>
Bool SubImage<T>::doGetMaskSlice (Array<Bool>& buffer,
				  const Slicer& section)
{
  return itsSubLatPtr->doGetMaskSlice (buffer, section);
}

template<class T>
uInt SubImage<T>::advisedMaxPixels() const
{
  return itsSubLatPtr->advisedMaxPixels();
}

template<class T>
IPosition SubImage<T>::doNiceCursorShape (uInt maxPixels) const
{
  return itsSubLatPtr->niceCursorShape (maxPixels);
}

template<class T>
T SubImage<T>::getAt (const IPosition& where) const
{
  return itsSubLatPtr->getAt (where);
}

template<class T>
void SubImage<T>::putAt (const T& value, const IPosition& where)
{
  itsSubLatPtr->putAt (value, where);
}

template<class T>
LatticeIterInterface<T>* SubImage<T>::makeIter
                               (const LatticeNavigator& navigator,
				Bool useRef) const
{
  return itsSubLatPtr->makeIter (navigator, useRef);
}

template<class T>
Bool SubImage<T>::lock (FileLocker::LockType type, uInt nattempts)
{
  return itsSubLatPtr->lock (type, nattempts);
}
template<class T>
void SubImage<T>::unlock()
{
  itsSubLatPtr->unlock();
  itsImagePtr->unlock();
}
template<class T>
Bool SubImage<T>::hasLock (FileLocker::LockType type) const
{
  return itsSubLatPtr->hasLock (type);
}
template<class T>
void SubImage<T>::resync()
{
  itsSubLatPtr->resync();
  itsImagePtr->resync();
}
template<class T>
void SubImage<T>::flush()
{
  itsImagePtr->flush();
}
template<class T>
void SubImage<T>::tempClose()
{
  itsSubLatPtr->tempClose();
  itsImagePtr->tempClose();
  logger().tempClose();
}
template<class T>
void SubImage<T>::reopen()
{
  itsImagePtr->reopen();
}

template<class T>
void SubImage<T>::convertIPosition(Vector<Float>& x, const IPosition& pos) const
{
  x.resize(pos.nelements());
  for (uInt i=0; i<x.nelements(); i++) x[i] = Float(pos(i));
}

} //# NAMESPACE CASA - END

