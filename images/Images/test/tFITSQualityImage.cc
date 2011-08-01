//# tFITSQualityImage.cc:  test the tFITSQualityImage class
//# Copyright (C) 1994,1995,1998,1999,2000,2001
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify it
//# under the terms of the GNU General Public License as published by the Free
//# Software Foundation; either version 2 of the License, or(at your option)
//# any later version.
//#
//# This program is distributed in the hope that it will be useful, but WITHOUT
//# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
//# more details.
//#
//# You should have received a copy of the GNU General Public License along
//# with this program; if not, write to the Free Software Foundation, Inc.,
//# 675 Massachusetts Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning AIPS++ should be addressed as follows:
//#        Internet email: aips2-request@nrao.edu.
//#        Postal address: AIPS++ Project Office
//#                        National Radio Astronomy Observatory
//#                        520 Edgemont Road
//#                        Charlottesville, VA 22903-2475 USA
//#
//# $Id: tFITSQualityImage.cc 20329 2008-06-06 07:59:22Z mkuemmel $

#include <casa/aips.h>
#include <casa/Arrays/Array.h>
#include <casa/Arrays/ArrayIO.h>
#include <casa/Arrays/Slicer.h>
#include <casa/Containers/Record.h>
#include <casa/Inputs/Input.h>
#include <casa/BasicMath/Math.h>
#include <casa/OS/Path.h>
#include <casa/BasicSL/String.h>
#include <casa/Exceptions/Error.h>
#include <casa/Logging/LogIO.h>

#include <images/Images/FITSQualityImage.h>
#include <images/Images/FITSImage.h>
#include <images/Images/FITSErrorImage.h>
#include <images/Images/ImageInterface.h>
#include <images/Images/ImageStatistics.h>
#include <coordinates/Coordinates/CoordinateSystem.h>

#include <casa/iostream.h>

#include <casa/namespace.h>
Bool allNear (const Array<Float>& data, const Array<Bool>& dataMask,
              const Array<Float>& fits, const Array<Bool>& fitsMask, Float tol=1.0e-5);

template <class T> void printArray (T array, Int size, String pre="printArray");

int main (int argc, const char* argv[])
{
try {

   LogIO os(LogOrigin("tFITSQualityImage", "main()", WHERE));

// Get inputs

   Input inputs(1);
   inputs.create("in", "",  "Input FITS file");
   inputs.create("hdu_sci", "1", "HDU number");
   inputs.create("hdu_err", "2", "HDU number");
   inputs.create("print",   "F", "Print some data");
   inputs.create("size",    "5", "Size to print");
//
   inputs.readArguments(argc, argv);
   String in = inputs.getString("in");
   const uInt hdu_sci = inputs.getInt("hdu_sci");
   const uInt hdu_err = inputs.getInt("hdu_err");
   const Bool print   = inputs.getBool("print");
   const Int size     = inputs.getInt("size");
//
   if (in.empty()) {
     in = "mexinputtest.fits";
   }   
   Path p(in);

   // create the image
   FITSQualityImage fitsQI(in, hdu_sci, hdu_err);

   {
	   if (fitsQI.name(True)!=in){
		   String msg = String("Image name stored in object is wrong!");
           throw(AipsError(msg));
	   }
   }
   {
	   // make sure the last axis has two pixels
	   uInt ndim       = fitsQI.ndim();
	   IPosition shape = fitsQI.shape();
	   if (shape(ndim-1)!=2) {
		   String msg = String("Last dimension should be 2 but is: ") + String::toString(shape(ndim-1));
           throw(AipsError(msg));

	   }
   }
   {
	   // make sure a quality coordinate axis exists
	   CoordinateSystem cSys = fitsQI.coordinates();
	   Int qCoord = cSys.findCoordinate(Coordinate::QUALITY);
	   if (qCoord < 0){
		   String msg = String("The image does not contain a quality coordinate axis!");
           throw(AipsError(msg));
	   }
   }
   {
	   // check the image type
	   if (fitsQI.imageType()!="FITSQualityImage"){
		   String msg = String("The image has wrong type!");
		   throw(AipsError(msg));
	   }
   }
   {
	   // make sure the object is masked
	   if (!fitsQI.isMasked()){
		   String msg = String("The object MUST be masked!");
		   throw(AipsError(msg));
	   }
   }
   {
	   // make sure the object has a pixel mask
	   if (!fitsQI.hasPixelMask()){
		   String msg = String("The object MUST have a pixel mask!");
		   throw(AipsError(msg));
	   }
   }
   {
	   // make sure the region pointer returned is 0
	   if (fitsQI.getRegionPtr()!=0){
		   String msg = String("The object MUST return a 0 as region pointer!");
		   throw(AipsError(msg));
	   }
   }
   {
	   // make sure the object is persistent
	   if (!fitsQI.isPersistent()){
		   String msg = String("The object MUST be persistent!");
		   throw(AipsError(msg));
	   }
   }
   {
	   // make sure the object is paged
	   if (!fitsQI.isPaged()){
		   String msg = String("The object MUST be aged!");
		   throw(AipsError(msg));
	   }
   }
   {
	   // make sure the object is not writable
	   if (fitsQI.isWritable()){
		   String msg = String("The object MUST NOT be writable!");
		   throw(AipsError(msg));
	   }
   }
   {
	   // make sure the object is OK
	   if (!fitsQI.ok()){
		   String msg = String("The object MUST be OK!");
		   throw(AipsError(msg));
	   }
   }

   // open the data error extensions independently
   FITSImage fitsDataImg  = FITSImage(in, 0, hdu_sci);
   FITSImage fitsErrorImg = FITSImage(in, 0, hdu_err);

   //fitsQI.tempClose();

   {
	   Array<Float> mmData;
	   Array<Bool>  mmMask;

	   // dimension the start and end points
	   // target data and error values
	   IPosition start (fitsQI.ndim(), 0);
	   IPosition end   (fitsQI.shape()-1);
	   IPosition stride(fitsQI.ndim(), 1);

	   // get a slicer and get the values and mask
	   Slicer mmSection(start, end, stride, Slicer::endIsLast);
	   fitsQI.doGetSlice(mmData, mmSection);
	   fitsQI.doGetMaskSlice(mmMask, mmSection);
	   if (print){
		   printArray (mmData,size, "Data = ");
		   printArray (mmMask,size, "Mask = ");
	   }

	   Array<Float> fitsDData;
	   Array<Bool>  fitsDMask;
	   Array<Float> fitsEData;
	   Array<Bool>  fitsEMask;

	   // dimension the start and end points
	   // for the individual extensions
	   IPosition fStart (fitsQI.ndim()-1, 0);
	   IPosition fStride(fitsQI.ndim()-1, 1);
	   IPosition fEnd (fitsQI.ndim()-1);
	   for (uInt i=0; i<fitsQI.ndim()-1; i++){
		   fEnd(i) = end(i);
	   }

	   // get a slicer for the individual extensions
	   // get the data and masks from the individual extensions
	   Slicer fitsSection(fStart, fEnd, fStride, Slicer::endIsLast);
	   fitsDataImg.doGetSlice(fitsDData, fitsSection);
	   fitsDataImg.doGetMaskSlice(fitsDMask, fitsSection);
	   if (print){
		   printArray (fitsDData,size, "fData = ");
		   printArray (fitsDMask,size, "fMask = ");
	   }
	   fitsErrorImg.doGetSlice(fitsEData, fitsSection);
	   fitsErrorImg.doGetMaskSlice(fitsEMask, fitsSection);
	   if (print){
		   printArray (fitsEData,size, "feData = ");
		   printArray (fitsEMask,size, "feMask = ");
	   }


	   Array<Float> tmpData;
	   Array<Bool>  tmpMask;

	   // extract the data values from the quality
	   // array and compare to the individual extension
	   start(fitsQI.ndim()-1)=0;
	   end(fitsQI.ndim()-1)=0;
	   tmpData.reference(mmData(start, end).nonDegenerate());
	   tmpMask.reference(mmMask(start, end).nonDegenerate());
	   if (print){
		   printArray (tmpData,size, "tmpData: ");
		   printArray (tmpMask,size, "tmpMask: ");
	   }
	   if (!allNear (tmpData, tmpMask, fitsDData, fitsDMask)){
		   String msg = String("The data arrays are not the same!");
		   throw(AipsError(msg));
	   }

	   // extract the error values from the quality
	   // array and compare to the individual extension array
	   start(fitsQI.ndim()-1)=1;
	   end(fitsQI.ndim()-1)=1;
	   tmpData.reference(mmData(start, end).nonDegenerate());
	   tmpMask.reference(mmMask(start, end).nonDegenerate());
	   if (print){
		   printArray (tmpData,size, "tmpData: ");
		   printArray (tmpMask,size, "tmpMask: ");
	   }
	   if (!allNear (tmpData, tmpMask, fitsEData, fitsEMask)){
		   String msg = String("The error arrays are not the same!");
		   throw(AipsError(msg));
	   }
   }
   {
	   Array<Float> mmData;
	   Array<Bool>  mmMask;

	   // dimension the start and end points
	   // target only data values
	   IPosition start (fitsQI.ndim(), 0);
	   IPosition end   (fitsQI.shape()-1);
	   IPosition stride(fitsQI.ndim(), 1);
	   end(fitsQI.ndim()-1) = 0;

	   // get a slicer and get the values and mask
	   Slicer mmSection(start, end, stride, Slicer::endIsLast);
	   fitsQI.doGetSlice(mmData, mmSection);
	   fitsQI.doGetMaskSlice(mmMask, mmSection);
	   if (print){
		   printArray (mmData,size, "DataII = ");
		   printArray (mmMask,size, "MaskII = ");
	   }

	   Array<Float> fitsDData;
	   Array<Bool>  fitsDMask;

	   // dimension the start and end points
	   // for the individual extension
	   IPosition fStart (fitsQI.ndim()-1, 0);
	   IPosition fStride(fitsQI.ndim()-1, 1);
	   IPosition fEnd (fitsQI.ndim()-1);
	   for (uInt i=0; i<fitsQI.ndim()-1; i++){
		   fEnd(i) = end(i);
	   }

	   // get a slicer for the individual extension
	   // get the data and masks from the individual extensions
	   Slicer fitsSection(fStart, fEnd, fStride, Slicer::endIsLast);
	   fitsDataImg.doGetSlice(fitsDData, fitsSection);
	   fitsDataImg.doGetMaskSlice(fitsDMask, fitsSection);
	   if (print){
		   printArray (fitsDData,size, "fDataII = ");
		   printArray (fitsDMask,size, "fMaskII = ");
	   }


	   Array<Float> tmpData;
	   Array<Bool>  tmpMask;

	   // extract the error values from the quality
	   // array and compare to the individual extension array
	   start(fitsQI.ndim()-1)=0;
	   end(fitsQI.ndim()-1)=0;
	   tmpData.reference(mmData(start, end).nonDegenerate());
	   tmpMask.reference(mmMask(start, end).nonDegenerate());
	   if (print){
		   printArray (tmpData,size, "tmpDataII: ");
		   printArray (tmpMask,size, "tmpMaskII: ");
	   }
	   if (!allNear (tmpData, tmpMask, fitsDData, fitsDMask)){
		   String msg = String("The data II arrays are not the same!");
		   throw(AipsError(msg));
	   }
   }
   {
	   Array<Float> mmData;
	   Array<Bool>  mmMask;

	   // dimension the start and end points
	   // target only error values
	   IPosition start (fitsQI.ndim(), 0);
	   IPosition end   (fitsQI.shape()-1);
	   IPosition stride(fitsQI.ndim(), 1);
	   start(fitsQI.ndim()-1) = 1;
	   end(fitsQI.ndim()-1)   = 1;

	   // get a slicer and get the values and mask
	   Slicer mmSection(start, end, stride, Slicer::endIsLast);
	   fitsQI.doGetSlice(mmData, mmSection);
	   fitsQI.doGetMaskSlice(mmMask, mmSection);
	   if (print){
		   printArray (mmData,size, "DataIII = ");
		   printArray (mmMask,size, "MaskIII = ");
	   }

	   Array<Float> fitsEData;
	   Array<Bool>  fitsEMask;

	   // dimension the start and end points
	   // for the individual extensions
	   IPosition fStart (fitsQI.ndim()-1, 0);
	   IPosition fStride(fitsQI.ndim()-1, 1);
	   IPosition fEnd (fitsQI.ndim()-1);
	   for (uInt i=0; i<fitsQI.ndim()-1; i++){
		   fEnd(i) = end(i);
	   }

	   // get a slicer for the individual extensions
	   // get the data and masks from the individual extensions
	   Slicer fitsSection(fStart, fEnd, fStride, Slicer::endIsLast);
	   fitsErrorImg.doGetSlice(fitsEData, fitsSection);
	   fitsErrorImg.doGetMaskSlice(fitsEMask, fitsSection);
	   if (print){
		   printArray (fitsEData,size, "feDataIII = ");
		   printArray (fitsEMask,size, "feMaskIII = ");
	   }


	   Array<Float> tmpData;
	   Array<Bool>  tmpMask;

	   // extract the data values from the quality
	   // array and compare to the individual extension
	   start(fitsQI.ndim()-1)=0;
	   end(fitsQI.ndim()-1)=0;
	   tmpData.reference(mmData(start, end).nonDegenerate());
	   tmpMask.reference(mmMask(start, end).nonDegenerate());
	   if (print){
		   printArray (tmpData,size, "tmpDataIII: ");
		   printArray (tmpMask,size, "tmpMaskIII: ");
	   }
	   if (!allNear (tmpData, tmpMask, fitsEData, fitsEMask)){
		   String msg = String("The error III arrays are not the same!");
		   throw(AipsError(msg));
	   }
   }

   {
	   // test assignment
	   FITSQualityImage  secImg = fitsQI;

	   Array<Float> mmData;
	   Array<Bool>  mmMask;
	   Array<Float> mmDataII;
	   Array<Bool>  mmMaskII;

	   // dimension the start and end points
	   // target data and error values
	   IPosition start (fitsQI.ndim(), 0);
	   IPosition end   (fitsQI.shape()-1);
	   IPosition stride(fitsQI.ndim(), 1);

	   // generate a slicer
	   Slicer mmSection(start, end, stride, Slicer::endIsLast);

	   // get the values and mask of the original image
	   fitsQI.doGetSlice(mmData, mmSection);
	   fitsQI.doGetMaskSlice(mmMask, mmSection);

	   // get the values and mask of the original image
	   secImg.doGetSlice(mmDataII, mmSection);
	   secImg.doGetMaskSlice(mmMaskII, mmSection);
	   if (print){
		   printArray (mmData,size,   "Data orig. = ");
		   printArray (mmMask,size,   "Mask orig. = ");
		   printArray (mmDataII,size, "Data assig.= ");
		   printArray (mmMaskII,size, "Mask assig.= ");
	   }
	   if (!allNear (mmData, mmMask, mmDataII, mmMaskII)){
		   String msg = String("The assigned image has different values than the original!");
		   throw(AipsError(msg));
	   }
   }
   {
	   // test the clone method
	   ImageInterface<Float>* pFitsMM = fitsQI.cloneII();
	   Array<Float> fCloneArray = pFitsMM->get();
	   Array<Bool>  fCloneMask  = pFitsMM->getMask();
	   CoordinateSystem fCloneCS = pFitsMM->coordinates();

	   Array<Float> fOrigArray = fitsQI.get();
	   Array<Bool>  fOrigMask  = fitsQI.getMask();
	   CoordinateSystem fOrigCS = fitsQI.coordinates();
	   if (print){
		   printArray (fOrigArray,size,  "Data orig. = ");
		   printArray (fOrigMask,size,   "Mask orig. = ");
		   printArray (fCloneArray,size, "Data clone = ");
		   printArray (fCloneMask,size,  "Mask clone = ");
	   }

	   if (!allNear (fOrigArray, fOrigMask, fCloneArray, fCloneMask)){
		   String msg = String("The cloned image has different values than the original!");
		   throw(AipsError(msg));
	   }

	   if (!fCloneCS.near(fOrigCS)){
		   String msg = String("The cloned image has different coord-sys than the original!");
		   throw(AipsError(msg));
	   }

	   delete pFitsMM;
   }

   cerr << "ok " << endl;

} catch (AipsError x) {
   cerr << "aipserror: error " << x.getMesg() << endl;
   return 1;
}
  return 0;
}

Bool allNear (const Array<Float>& data, const Array<Bool>& dataMask,
              const Array<Float>& fits, const Array<Bool>& fitsMask,
              Float tol)
{
   Bool deletePtrData, deletePtrDataMask, deletePtrFITS, deletePtrFITSMask;
   const Float* pData = data.getStorage(deletePtrData);
   const Float* pFITS = fits.getStorage(deletePtrFITS);
   const Bool* pDataMask = dataMask.getStorage(deletePtrDataMask);
   const Bool* pFITSMask = fitsMask.getStorage(deletePtrFITSMask);
//
   for (uInt i=0; i<data.nelements(); i++) {
      if (pDataMask[i] != pFITSMask[i]) {
         cerr << "masks differ" << endl;
         return False;
      }
      if (pDataMask[i]) { 
         if (!near(pData[i], pFITS[i], tol)) {
            cerr << "data differ, tol = " << tol << endl;
            cerr << pData[i] << ", " << pFITS[i] << endl;
            return False;
         }
      }
   }
//
   data.freeStorage(pData, deletePtrData);
   dataMask.freeStorage(pDataMask, deletePtrDataMask);
   fits.freeStorage(pFITS, deletePtrFITS);
   fitsMask.freeStorage(pFITSMask, deletePtrFITSMask);
   return True;
}

template <class T> void printArray (T array, Int size, String pre)
{
	T tmpArray;
	IPosition start (array.ndim(), 0);
	IPosition end   (array.shape()-1);
	for (uInt i=0; i<array.ndim(); i++)
		if (end(i) > size-1) end(i) = size-1;
	tmpArray.reference(array(start, end));
	cerr << "\n" << pre << tmpArray;
}