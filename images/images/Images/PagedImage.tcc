//# PagedImage.cc: defines the PagedImage class
//# Copyright (C) 1994,1995,1996,1997,1998,1999,2000,2001,2003
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
//# $Id: PagedImage.tcc 19940 2007-02-27 05:35:22Z Malte.Marquarding $

#include <images/Images/PagedImage.h>
#include <images/Regions/ImageRegion.h>
#include <images/Regions/RegionHandlerTable.h>
#include <images/Images/ImageInfo.h>
#include <lattices/Lattices/ArrayLattice.h>
#include <lattices/Lattices/LatticeNavigator.h>
#include <lattices/Lattices/LatticeStepper.h>
#include <lattices/Lattices/LatticeIterator.h>
#include <lattices/Lattices/PagedArrIter.h>
#include <lattices/Lattices/LatticeExprNode.h>
#include <lattices/Lattices/LatticeExpr.h>
#include <lattices/Lattices/LatticeRegion.h>
#include <casa/Logging/LogIO.h>
#include <casa/Logging/LogMessage.h>

#include <casa/Arrays/Array.h>
#include <casa/Arrays/ArrayMath.h>
#include <casa/Arrays/ArrayLogical.h>
#include <casa/Arrays/LogiArray.h>
#include <casa/Exceptions/Error.h>
#include <casa/OS/File.h>
#include <casa/OS/Path.h>
#include <casa/Arrays/IPosition.h>
#include <casa/Arrays/Slicer.h>
#include <tables/Tables/SetupNewTab.h>
#include <tables/Tables/TableLock.h>
#include <tables/Tables/Table.h>
#include <tables/Tables/TableDesc.h>
#include <tables/Tables/TableRecord.h>
#include <tables/Tables/TableInfo.h>
#include <tables/Tables/TableColumn.h>
#include <casa/BasicSL/String.h>
#include <casa/Utilities/Assert.h>
#include <casa/Quanta/UnitMap.h>

#include <casa/iostream.h>
#include <casa/sstream.h>


namespace casa { //# NAMESPACE CASA - BEGIN

template <class T> 
PagedImage<T>::PagedImage (const TiledShape& shape, 
			   const CoordinateSystem& coordinateInfo, 
			   Table& table, uInt rowNumber)
: ImageInterface<T>(RegionHandlerTable(getTable, this)),
  map_p         (shape, table, "map", rowNumber),
  regionPtr_p   (0)
{
  attach_logtable();
  logSink() << LogOrigin("PagedImage<T>", 
			 "PagedImage(const TiledShape& shape,  "
			 "const CoordinateSystem& coordinateInfo, "
			 "Table& table, uInt rowNumber)", WHERE);
  
  logSink() << LogIO::DEBUGGING 
	    << "Creating an image in row " << rowNumber 
	    << " of an existing table called"
	    << " '" << name() << "'" << endl
	    << "The image shape is " << shape.shape() << endl;
  logSink() << LogIO::NORMAL;
  AlwaysAssert(setCoordinateInfo(coordinateInfo), AipsError);
  setTableType();
}

template <class T> 
PagedImage<T>::PagedImage (const TiledShape& shape, 
			   const CoordinateSystem& coordinateInfo, 
			   const String& filename, 
			   TableLock::LockOption lockMode,
			   uInt rowNumber)
: ImageInterface<T>(RegionHandlerTable(getTable, this)),
  regionPtr_p   (0)
{
  makePagedImage (shape, coordinateInfo, filename,
		  TableLock(lockMode), rowNumber);
}

template <class T> 
PagedImage<T>::PagedImage (const TiledShape& shape, 
			   const CoordinateSystem& coordinateInfo, 
			   const String& filename, 
			   const TableLock& lockOptions,
			   uInt rowNumber)
: ImageInterface<T>(RegionHandlerTable(getTable, this)),
  regionPtr_p   (0)
{
  makePagedImage (shape, coordinateInfo, filename, lockOptions, rowNumber);
}


template <class T> 
void PagedImage<T>::makePagedImage (const TiledShape& shape, 
				    const CoordinateSystem& coordinateInfo, 
				    const String& filename, 
				    const TableLock& lockOptions,
				    uInt rowNumber)
{
  logSink() << LogOrigin("PagedImage<T>", 
			 "PagedImage(const TiledShape& shape, "
			 "const CoordinateSystem& coordinateInfo, "
			 "const TableLock& lockoptions, "
			 "const String& filename, "
			 "uInt rowNumber)", WHERE);
  logSink() << LogIO::DEBUGGING
	    << "Creating an image in row " << rowNumber 
	    << " of a new table called"
	    << " '" << filename << "'" << endl
	    << "The image shape is " << shape.shape() << endl;
  SetupNewTable newtab (filename, TableDesc(), Table::New);
  Table tab(newtab, lockOptions);
  map_p = PagedArray<T> (shape, tab, "map", rowNumber);
  attach_logtable();
  logSink() << LogIO::NORMAL;
  AlwaysAssert(setCoordinateInfo(coordinateInfo), AipsError);
  setTableType();
}

template <class T> 
PagedImage<T>::PagedImage (const TiledShape& shape, 
			   const CoordinateSystem& coordinateInfo, 
			   const String& filename, 
			   uInt rowNumber)
: ImageInterface<T>(RegionHandlerTable(getTable, this)),
  regionPtr_p   (0)
{
  logSink() << LogOrigin("PagedImage<T>", 
			 "PagedImage(const TiledShape& shape,  "
			 "const CoordinateSystem& coordinateInfo, "
			 "const String& filename, "
			 "uInt rowNumber)", WHERE);
  logSink() << LogIO::DEBUGGING
	    << "Creating an image in row " << rowNumber 
	    << " of a new table called"
	    << " '" << filename << "'" << endl
	    << "The image shape is " << shape.shape() << endl;
  SetupNewTable newtab (filename, TableDesc(), Table::New);
  Table tab(newtab);
  map_p = PagedArray<T> (shape, tab, "map", rowNumber);
  attach_logtable();
  logSink() << LogIO::NORMAL;
  AlwaysAssert(setCoordinateInfo(coordinateInfo), AipsError);
  setTableType();
}

template <class T> 
PagedImage<T>::PagedImage (Table& table, MaskSpecifier spec, uInt rowNumber)
: ImageInterface<T>(RegionHandlerTable(getTable, this)),
  map_p         (table, "map", rowNumber),
  regionPtr_p   (0)
{
  attach_logtable();
  logSink() << LogOrigin("PagedImage<T>", 
			 "PagedImage(Table& table, "
			 "MaskSpecifier, "
			 "uInt rowNumber)", WHERE);
  logSink() << LogIO::DEBUGGING
	    << "Reading an image from row " << rowNumber 
	    << " of a table called"
	    << " '" << name() << "'" << endl
	    << "The image shape is " << map_p.shape() << endl;

  restoreAll (table.keywordSet());
  applyMaskSpecifier (spec);
}

template <class T> 
PagedImage<T>::PagedImage (const String& filename, MaskSpecifier spec,
			   uInt rowNumber)
: ImageInterface<T>(RegionHandlerTable(getTable, this)),
  regionPtr_p   (0)
{
  logSink() << LogOrigin("PagedImage<T>", 
			 "PagedImage(const String& filename, "
			 "MaskSpecifier, "
			 "uInt rowNumber)", WHERE);
  logSink() << LogIO::DEBUGGING
	    << "Reading an image from row " << rowNumber 
	    << " of a file called"
	    << " '" << filename << "'" << endl;
  Table tab(filename);
  map_p = PagedArray<T>(tab, "map", rowNumber);
  attach_logtable();
  logSink() << LogIO::DEBUGGING << "The image shape is " << map_p.shape() << endl;
  logSink() << LogIO::DEBUGGING;
//
  restoreAll (tab.keywordSet());
  applyMaskSpecifier (spec);
}

template <class T> 
PagedImage<T>::PagedImage (const String& filename,
			   const TableLock& lockOptions,
			   MaskSpecifier spec, uInt rowNumber)
: ImageInterface<T>(RegionHandlerTable(getTable, this)),
  regionPtr_p   (0)
{
  makePagedImage (filename, lockOptions, spec, rowNumber);
}

template <class T> 
PagedImage<T>::PagedImage (const String& filename,
			   TableLock::LockOption lockMode,
			   MaskSpecifier spec, uInt rowNumber)
: ImageInterface<T>(RegionHandlerTable(getTable, this)),
  regionPtr_p   (0)
{
  makePagedImage (filename, TableLock(lockMode), spec, rowNumber);
}

template <class T> 
void PagedImage<T>::makePagedImage (const String& filename,
				    const TableLock& lockOptions,
				    const MaskSpecifier& spec,
				    uInt rowNumber)
{
  logSink() << LogOrigin("PagedImage<T>", 
			 "PagedImage(const String& filename, "
			 "const TableLock& lockOptions, "
			 "MaskSpecifier, "
			 "uInt rowNumber)", WHERE);
  logSink() << LogIO::DEBUGGING
	    << "Reading an image from row " << rowNumber 
	    << " of a file called"
	    << " '" << filename << "'" << endl;
  Table tab(filename, lockOptions);
  map_p = PagedArray<T>(tab, "map", rowNumber);
  attach_logtable();
  logSink() << LogIO::DEBUGGING << "The image shape is " << map_p.shape() << endl;
  logSink() << LogIO::DEBUGGING;
//
  restoreAll (tab.keywordSet());
  applyMaskSpecifier (spec);
}

template <class T> 
PagedImage<T>::PagedImage (const PagedImage<T>& other)
: ImageInterface<T>(other),
  map_p            (other.map_p),
  regionPtr_p      (0)
{
  if (other.regionPtr_p != 0) {
    regionPtr_p = new LatticeRegion (*other.regionPtr_p);
  }
}

template <class T> 
PagedImage<T>::~PagedImage()
{
  // Close the logger here in case the image table is going to be deleted.
  delete regionPtr_p;
  logger().tempClose();
}

template <class T> 
PagedImage<T>& PagedImage<T>::operator=(const PagedImage<T>& other)
{
  if (this != &other) {
    ImageInterface<T>::operator= (other);
    map_p = other.map_p;
    delete regionPtr_p;
    regionPtr_p = 0;
    if (other.regionPtr_p != 0) {
      regionPtr_p = new LatticeRegion (*other.regionPtr_p);
    }
  } 
  return *this;
}

template <class T> 
ImageInterface<T>* PagedImage<T>::cloneII() const
{
  return new PagedImage<T> (*this);
}

template <class T>
void PagedImage<T>::restoreAll (const TableRecord& rec)
{
  // Restore the coordinates.
  CoordinateSystem* restoredCoords = CoordinateSystem::restore(rec, "coords");
  AlwaysAssert(restoredCoords != 0, AipsError);
  setCoordsMember (*restoredCoords);
  delete restoredCoords;
  // Restore the image info.
  restoreImageInfo (rec);
  // Restore the units.
  restoreUnits (rec);
  // Restore the miscinfo.
  restoreMiscInfo (rec);
}

template<class T>
String PagedImage<T>::imageType() const
{
  return "PagedImage";
}

template<class T>
Bool PagedImage<T>::isPersistent() const
{
  return True;
}

template<class T>
Bool PagedImage<T>::isPaged() const
{
  return True;
}

template <class T> 
Bool PagedImage<T>::isWritable() const
{
  return map_p.isWritable();
}

template<class T>
void PagedImage<T>::doReopenRW()
{
  table().reopenRW();
}

template<class T>
Bool PagedImage<T>::hasPixelMask() const
{
  return (regionPtr_p != 0  &&  regionPtr_p->hasMask());
}

template<class T>
const Lattice<Bool>& PagedImage<T>::pixelMask() const
{
  if (regionPtr_p == 0) {
    throw (AipsError ("PagedImage::pixelMask - no pixelmask used"));
  }
  return *regionPtr_p;
}
template<class T>
Lattice<Bool>& PagedImage<T>::pixelMask()
{
  if (regionPtr_p == 0) {
    throw (AipsError ("PagedImage::pixelMask - no pixelmask used"));
  }
  return *regionPtr_p;
}

template<class T>
const LatticeRegion* PagedImage<T>::getRegionPtr() const
{
  return regionPtr_p;
}

template<class T>
void PagedImage<T>::setDefaultMask (const String& regionName)
{
  // Reopen for write when needed and possible.
  reopenRW();
  // Use the new region as the image's mask.
  applyMask (regionName);
  // Store the new default name.
  ImageInterface<T>::setDefaultMask (regionName);
}

template <class T> 
void PagedImage<T>::useMask (MaskSpecifier spec)
{
  applyMaskSpecifier (spec);
}

template<class T>
void PagedImage<T>::applyMaskSpecifier (const MaskSpecifier& spec)
{
  // Use default mask if told to do so.
  // If it does not exist, use no mask.
  String name = spec.name();
  if (spec.useDefault()) {
    name = getDefaultMask();
    if (! hasRegion (name, RegionHandler::Masks)) {
      name = "";
    }
  }
  applyMask (name);
}

template<class T>
void PagedImage<T>::applyMask (const String& maskName)
{
  // No region if no mask name is given.
  if (maskName.empty()) {
    delete regionPtr_p;
    regionPtr_p = 0;
    return;
  }
  // Reconstruct the ImageRegion object.
  // Turn the region into lattice coordinates.
  ImageRegion* regPtr = getImageRegionPtr (maskName, RegionHandler::Masks);
  LatticeRegion* latReg = new LatticeRegion
                          (regPtr->toLatticeRegion (coordinates(), shape()));
  delete regPtr;
  // The mask has to cover the entire image.
  if (latReg->shape() != shape()) {
    delete latReg;
    throw (AipsError ("PagedImage::setDefaultMask - region " + maskName +
		      " does not cover the full image"));
  }
  // Replace current by new mask.
  delete regionPtr_p;
  regionPtr_p = latReg;
}


template <class T> 
void PagedImage<T>::rename (const String& newName)
{
  table().rename (newName, Table::New);
}

template <class T> 
String PagedImage<T>::name (Bool stripPath) const 
{
  return map_p.name (stripPath);
}

template <class T> 
uInt PagedImage<T>::rowNumber() const
{
  return map_p.rowNumber();
}

template <class T> 
IPosition PagedImage<T>::shape() const
{
  return map_p.shape();
}

template<class T> 
void PagedImage<T>::resize (const TiledShape& newShape)
{
  if (newShape.shape().nelements() != coordinates().nPixelAxes()) {
    throw(AipsError("PagedImage<T>::resize: coordinate info is "
		    "the incorrect shape."));
  }
  map_p.resize (newShape);
}

template <class T> 
Bool PagedImage<T>::setCoordinateInfo (const CoordinateSystem& coords)
{
  logSink() << LogOrigin ("PagedImage<T>", "setCoordinateInfo(const "
			  "CoordinateSystem& coords)",  WHERE);
  
  Bool ok = ImageInterface<T>::setCoordinateInfo(coords);
  if (ok) {
    reopenRW();
    Table& tab = table();
    if (tab.isWritable()) {
      // Update the coordinates
      if (tab.keywordSet().isDefined("coords")) {
	tab.rwKeywordSet().removeField("coords");
      }
      if (!(coordinates().save(tab.rwKeywordSet(), "coords"))) {
	logSink() << LogIO::SEVERE << "Error saving coordinates in table"
		  << LogIO::POST;
	ok = False;
      }
    } else {
      logSink() << LogIO::SEVERE
		<< "Table is not writable: not saving coordinates to disk."
		<< LogIO::POST;
    }
  }
  return ok;
}

  
template <class T> 
Bool PagedImage<T>::doGetSlice(Array<T>& buffer, const Slicer& theSlice)
{
  return map_p.doGetSlice(buffer, theSlice);
}

template <class T> 
void PagedImage<T>::doPutSlice(const Array<T>& sourceBuffer, 
                               const IPosition& where, 
                               const IPosition& stride)
{
    //  if (throughmask_p || !mask_p) {
  map_p.putSlice(sourceBuffer,where,stride);
    //  } else if (mask_p) {
    //    Array<T> map;
    //Array<Bool> mask;
    //IPosition shape(sourceBuffer.shape());
    //mask_p->getSlice(mask, where, shape, stride, True);
    //map_p.getSlice(map, where, shape, stride, True);
    // use maskedarrays to do all the work.
    //map(mask==False) = sourceBuffer;
    //map_p.putSlice(map,where,stride);
    //  } else {
    //    throw(AipsError("PagedImage<T>::putSlice - throughmask==False but no "
    //		    "mask exists."));
    //  }
}


// apply a function to all elements of the map
template <class T> 
void PagedImage<T>::apply(T (*function)(T)) 
{
    map_p.apply(function);
}

// apply a function to all elements of a const map;
template <class T> 
void PagedImage<T>::apply(T (*function)(const T&))
{
    map_p.apply(function);
}

template <class T> 
void PagedImage<T>::apply(const Functional<T,T>& function)
{
    map_p.apply(function);
}


template <class T> 
T PagedImage<T>::getAt(const IPosition& where) const
{
   return map_p(where);
}

template <class T> 
void PagedImage<T>::putAt(const T& value, const IPosition& where) {
    map_p.putAt (value, where);
}

template<class T> 
void PagedImage<T>::restoreMiscInfo (const TableRecord& rec)
{
  if (rec.isDefined("miscinfo")  &&
      rec.dataType("miscinfo") == TpRecord) {
    setMiscInfoMember (rec.asRecord ("miscinfo"));
  }
}

template<class T> 
Bool PagedImage<T>::setMiscInfo (const RecordInterface& newInfo)
{
  setMiscInfoMember (newInfo);
  Table& tab = table();
  reopenRW();
  if (! tab.isWritable()) {
    return False;
  }
  if (tab.keywordSet().isDefined("miscinfo")) {
    tab.rwKeywordSet().removeField("miscinfo");
  }
  tab.rwKeywordSet().defineRecord("miscinfo", newInfo);
  return True;
}

template <class T> 
LatticeIterInterface<T>* PagedImage<T>::makeIter
                                   (const LatticeNavigator& navigator,
				    Bool useRef) const
{
  return map_p.makeIter (navigator, useRef);
}

template <class T> 
Bool PagedImage<T>::ok() const
{
  Int okay = (map_p.ndim() == coordinates().nPixelAxes());
  return okay  ?  True : False;
}


template <class T> 
PagedImage<T>& PagedImage<T>::operator+= (const Lattice<T>& other)
{
  logSink() << LogOrigin("PagedImage<T>", 
			 "operator+=(const Lattice<T>& other)", WHERE) <<
    LogIO::DEBUGGING << "Adding other to our pixels" << endl;
  
  check_conformance(other);
  logSink() << LogIO::POST;
  logSink() << LogIO::NORMAL;
/*  
  IPosition cursorShape(this->niceCursorShape(this->advisedMaxPixels() - 1));
  LatticeIterator<T> toiter(*this, cursorShape);
  RO_LatticeIterator<T> otheriter(other, cursorShape);
  for (toiter.reset(), otheriter.reset(); !toiter.atEnd();
       toiter++, otheriter++) {
    toiter.rwCursor() += otheriter.cursor();
  }
  // Mask is not handled in such a loop; therefore use LEL.
*/

  LatticeExpr<T> expr(*this + other);
  copyData (expr);
  ///  copyData (LatticeExprNode(*this)+LatticeExprNode(other));
  return *this;
}


template<class T> 
void PagedImage<T>::attach_logtable()
{
  open_logtable();
  logSink() << LogIO::NORMAL;
}

template<class T> 
void PagedImage<T>::open_logtable()
{
  // Open logtable as readonly if main table is not writable.
  Table& tab = table();
  setLogMember (LoggerHolder (name() + "/logtable", tab.isWritable()));
  // Insert the keyword if possible and if it does not exist yet.
  if (tab.isWritable()  &&  ! tab.keywordSet().isDefined ("logtable")) {
    tab.rwKeywordSet().defineTable("logtable", Table(name() + "/logtable"));
  }
}

template<class T> 
Bool PagedImage<T>::setUnits(const Unit& newUnits) 
{
  setUnitMember (newUnits);
  reopenRW();
  Table& tab = table();
  if (! tab.isWritable()) {
    return False;
  }
  if (tab.keywordSet().isDefined("units")) {
    tab.rwKeywordSet().removeField("units");
  }
  tab.rwKeywordSet().define("units", newUnits.getName());
  return True;
}

template<class T> 
void PagedImage<T>::restoreUnits (const TableRecord& rec)
{
  Unit retval;
  String unitName;
  if (rec.isDefined("units")) {
    if (rec.dataType("units") != TpString) {
      LogIO os;
      os << LogOrigin("PagedImage<T>", "units()", WHERE) <<
	"'units' keyword in image table is not a string! Units not restored." 
		<< LogIO::SEVERE << LogIO::POST;
    } else {
      rec.get("units", unitName);
    }
  }
  if (! unitName.empty()) {
    // OK, non-empty unit, see if it's valid, if not try some known things to
    // make a valid unit out of it.
    if (! UnitVal::check(unitName)) {
      // Beam and Pixel are the most common undefined units
      UnitMap::putUser("Pixel",UnitVal(1.0),"Pixel unit");
      UnitMap::putUser("Beam",UnitVal(1.0),"Beam area");
    }
    if (! UnitVal::check(unitName)) {
      // OK, maybe we need FITS
      UnitMap::addFITS();
    }
    if (!UnitVal::check(unitName)) {
      LogIO os;
      UnitMap::putUser(unitName, UnitVal::UnitVal(1.0, UnitDim::Dnon), unitName);
      os << LogIO::WARN << "FITS unit \"" << unitName << "\" unknown to CASA - will treat it as non-dimensional."
	 << LogIO::POST;
      retval.setName(unitName);
      retval.setValue(UnitVal::UnitVal(1.0, UnitDim::Dnon));
    } else {
      retval = Unit(unitName);
    }
  }
  setUnitMember (retval);
}


template<class T> 
void PagedImage<T>::removeRegion (const String& name,
				  RegionHandler::GroupType type,
				  Bool throwIfUnknown)
{
  reopenRW();
  // Remove the default mask if it is the region to be removed.
  if (name == getDefaultMask()) {
    setDefaultMask ("");
  }
  ImageInterface<T>::removeRegion (name, type, throwIfUnknown);
}


template<class T> 
void PagedImage<T>::check_conformance(const Lattice<T>& other)
{
  if (! this->conform(other)) {
    logSink() << "this and other do not conform (" << this->shape() 
	      << " != " << other.shape() << ")" << LogIO::EXCEPTION;
  }
}

template<class T> 
uInt PagedImage<T>::maximumCacheSize() const
{
  return map_p.maximumCacheSize();
}

template<class T> 
void PagedImage<T>::setMaximumCacheSize(uInt howManyPixels)
{
  map_p.setMaximumCacheSize(howManyPixels);
  if (regionPtr_p != 0) {
    regionPtr_p->setMaximumCacheSize(howManyPixels);
  }
}

template<class T> 
void PagedImage<T>::setCacheSizeFromPath(const IPosition& sliceShape, 
    	                                 const IPosition& windowStart,
                                         const IPosition& windowLength,
                                         const IPosition& axisPath)
{
  map_p.setCacheSizeFromPath(sliceShape, windowStart, windowLength, axisPath);
  if (regionPtr_p != 0) {
    regionPtr_p->setCacheSizeFromPath(sliceShape, windowStart,
				      windowLength, axisPath);
  }
}

template<class T>
void PagedImage<T>::setCacheSizeInTiles (uInt howManyTiles)  
{  
  map_p.setCacheSizeInTiles (howManyTiles);
  if (regionPtr_p != 0) {
    regionPtr_p->setCacheSizeInTiles (howManyTiles);
  }
}


template<class T> 
void PagedImage<T>::clearCache()
{
  map_p.clearCache();
  if (regionPtr_p != 0) {
    regionPtr_p->clearCache();
  }
}

template<class T> 
void PagedImage<T>::showCacheStatistics(ostream& os) const
{
  os << "Pixel statistics : ";
  map_p.showCacheStatistics(os);
  if (regionPtr_p != 0) {
    os << "Pixelmask statistics : ";
    regionPtr_p->showCacheStatistics(os);
  }
}

template<class T> 
uInt PagedImage<T>::advisedMaxPixels() const
{
  return map_p.advisedMaxPixels();
}

template<class T> 
IPosition PagedImage<T>::doNiceCursorShape(uInt maxPixels) const
{
  return map_p.niceCursorShape(maxPixels);
}

template<class T> 
void PagedImage<T>::setTableType()
{
  TableInfo& info(table().tableInfo());
  const String reqdType = info.type(TableInfo::PAGEDIMAGE);
  if (info.type() != reqdType) {
    info.setType(reqdType);
  }
  const String reqdSubType = info.subType(TableInfo::PAGEDIMAGE);
  if (info.subType() != reqdSubType) {
    info.setSubType(reqdSubType);
  }
}


template<class T>
Table& PagedImage<T>::getTable (void* imagePtr, Bool writable)
{
  PagedImage<T>* im = static_cast<PagedImage<T>*>(imagePtr);
  if (writable) {
    im->reopenRW();
  }
  return im->map_p.table();
}

template<class T>
Bool PagedImage<T>::lock (FileLocker::LockType type, uInt nattempts)
{
  return map_p.lock (type, nattempts);
}
template<class T>
void PagedImage<T>::unlock()
{
  map_p.unlock();
  logger().unlock();
  if (regionPtr_p != 0) {
    regionPtr_p->unlock();
  }
}
template<class T>
Bool PagedImage<T>::hasLock (FileLocker::LockType type) const
{
  return map_p.hasLock (type);
}

template<class T>
void PagedImage<T>::resync()
{
  map_p.resync();
  logger().resync();
  if (regionPtr_p != 0
  &&  !regionPtr_p->hasLock (FileLocker::Read)) {
    regionPtr_p->resync();
  }
}

template<class T>
void PagedImage<T>::flush()
{
  map_p.flush();
  logger().flush();
  if (regionPtr_p != 0) {
    regionPtr_p->flush();
  }
}

template<class T>
void PagedImage<T>::tempClose()
{
  map_p.tempClose();
  logger().tempClose();
  if (regionPtr_p != 0) {
    regionPtr_p->tempClose();
  }
}

template<class T>
void PagedImage<T>::reopen()
{
  map_p.reopen();
  if (regionPtr_p != 0) {
    regionPtr_p->reopen();
  }
}

template<class T>
Bool PagedImage<T>::setImageInfo (const ImageInfo& info) 
{
  logSink() << LogOrigin ("PagedImage<T>", "setImageInfo(const "
			  "ImageInfo& info)",  WHERE);
  Bool ok = ImageInterface<T>::setImageInfo(info);
  if (ok) {
    reopenRW();
    Table& tab = table();
    if (tab.isWritable()) {

// Update the ImageInfo

      if (tab.keywordSet().isDefined("imageinfo")) {
	tab.rwKeywordSet().removeField("imageinfo");
      }
      TableRecord rec;
      String error;
      if (imageInfo().toRecord(error, rec)) {
         tab.rwKeywordSet().defineRecord("imageinfo", rec);
      } else {
	logSink() << LogIO::SEVERE << "Error saving ImageInfo in table because " 
          + error << LogIO::POST;
	ok = False;
      }
    } else {
      logSink() << LogIO::SEVERE
		<< "Table is not writable: not saving ImageInfo to disk."
		<< LogIO::POST;
    }
  }
  return ok;
}

template<class T>
void PagedImage<T>::restoreImageInfo (const TableRecord& rec)
{
  if (rec.isDefined("imageinfo")) {
    String error;
    ImageInfo info;
    Bool ok = info.fromRecord (error, rec.asRecord("imageinfo"));
    if (!ok) {
      logSink() << LogIO::WARN << "Failed to restore the ImageInfo because " 
	+ error << LogIO::POST;
    } else {
      setImageInfoMember (info);
    }
  }
}

} //# NAMESPACE CASA - END

