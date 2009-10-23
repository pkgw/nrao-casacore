//# tSubImage.cc: Test program for class SubImage
//# Copyright (C) 1998,1999,2000,2001,2003
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify it
//# under the terms of the GNU General Public License as published by the Free
//# Software Foundation; either version 2 of the License, or (at your option)
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
//# $Id: $

#include <images/Images/ImageAnalysis.h>
#include <images/Images/ImageFitter.h>
#include <images/Images/ImageMetaData.h>
#include <images/Images/ImageStatistics.h>
#include <images/Regions/RegionManager.h>
#include <coordinates/Coordinates/CoordinateSystem.h>
#include <casa/Quanta/Quantum.h>
#include <casa/Quanta/Unit.h>
#include <casa/Quanta/UnitMap.h>
#include <casa/Quanta/MVAngle.h>
#include <casa/Quanta/MVTime.h>
#include <components/ComponentModels/Flux.h>
#include <images/Images/FITSImage.h>
#include <images/Images/MIRIADImage.h>
#include <images/IO/FitterEstimatesFileParser.h>
// #include <casa/Arrays/ArrayUtil.h>

#include <components/ComponentModels/SpectralModel.h>
#include <images/Regions/WCUnion.h>
#include <images/Regions/WCBox.h>
#include <lattices/Lattices/LCRegion.h>
#include <coordinates/Coordinates/CoordinateUtil.h>
// #include <casa/Arrays/Vector.h>
// #include <casa/Arrays/IPosition.h>
#include <casa/Utilities/Assert.h>
// #include <casa/Exceptions/Error.h>

#include <components/ComponentModels/SkyComponent.h>
#include <components/ComponentModels/ComponentShape.h>

#include <components/ComponentModels/GaussianShape.h>

namespace casa {

    ImageFitter::ImageFitter(
        const String& imagename, const String& box, const String& region,
        const uInt chanInp, const String& stokes,
        const String& maskInp, const Vector<Float>& includepix,
        const Vector<Float>& excludepix, const String& residualInp,
        const String& modelInp, const String& estimatesFilename
    ) : chan(chanInp), stokesString(stokes), mask(maskInp),
		residual(residualInp),model(modelInp), includePixelRange(includepix),
		excludePixelRange(excludepix), estimates(), fixed(0) {
        itsLog = new LogIO();
        *itsLog << LogOrigin("ImageFitter", "constructor");
        _construct(imagename, box, region, estimatesFilename);
    }

    ImageFitter::~ImageFitter() {
        delete itsLog;
        delete image;
    }

    ComponentList ImageFitter::fit() {
        *itsLog << LogOrigin("ImageFitter", "fit");
        Array<Float> residPixels;
        Array<Bool> residMask;
        Bool converged;
        uInt ngauss = estimates.nelements() > 0 ? estimates.nelements() : 1;
        Vector<String> models(ngauss);
        models.set("gaussian");
        ImageAnalysis myImage(image);
        Bool fit = True;
        Bool deconvolve = False;
        Bool list = True;
        // TODO make param passed to fitsky a ComponentList so this crap doesn't
        // have to be done.
        String errmsg;
        Record estimatesRecord;
        estimates.toRecord(errmsg, estimatesRecord);
        Record rec = Record(imRegion.toRecord(""));
        results = myImage.fitsky(
            residPixels, residMask, converged, rec,
            chan, stokesString, mask, models,
            estimatesRecord, fixed, includePixelRange,
            excludePixelRange, fit, deconvolve, list,
            residual, model
        );
        Flux<Double> flux;
        Vector<Quantity> fluxQuant;
        *itsLog << LogIO::NORMAL << _resultsToString() << LogIO::POST;
        for(uInt k=0; k<results.nelements(); k++) {
            SkyComponent skyComp = results.component(k);
            flux = skyComp.flux();
            cout << "flux val " << flux.value(Stokes::I) << endl;
            results.getFlux(fluxQuant, k);
            cout << " flux from comp list " << fluxQuant << endl;
            Vector<String> polarization = results.getStokes(k);
            cout << "stokes from comp list " << polarization << endl;
            const ComponentShape* compShape = results.getShape(k);
            String compType = ComponentType::name(compShape->type());
            cout << "component type " << compType << endl;
            MDirection mdir = results.getRefDirection(k);

            Quantity lat = mdir.getValue().getLat("rad");
            Quantity longitude = mdir.getValue().getLong("rad");

            Vector<Double> world(4,0), pixel(4,0);
            image->coordinates().toWorld(world, pixel);

            world[0] = longitude.getValue();
            world[1] = lat.getValue();
            if (image->coordinates().toPixel(pixel, world)) {
        	    cout << "max pixel position " << pixel << endl;
            }
            else {
        	    cerr << "unable to convert world to pixel" << endl;
            }

            Quantity elat = compShape->refDirectionErrorLat();
            Quantity elong = compShape->refDirectionErrorLong();
            cout << " RA " << MVTime(lat).string(MVTime::TIME, 11) << " DEC "
                << MVAngle(longitude).string(MVAngle::ANGLE_CLEAN, 11) << endl;
            cout << "RA error " << MVTime(elat).string(MVTime::TIME, 11) << " Dec error " 
                << MVAngle(elong).string(MVAngle::ANGLE, 11) << endl;

            cout << "RA error rads" << elat << " Dec error rad " << elong << endl;

            if (compShape->type() == ComponentType::GAUSSIAN) {
                // print gaussian stuff
                Quantity bmaj = (static_cast<const GaussianShape *>(compShape))->majorAxis();
                Quantity bmin = (static_cast<const GaussianShape *>(compShape))->minorAxis();
                Quantity bpa  = (static_cast<const GaussianShape *>(compShape))->positionAngle();
                Quantity emaj = (static_cast<const GaussianShape *>(compShape))->majorAxisError();
                Quantity emin = (static_cast<const GaussianShape *>(compShape))->minorAxisError();
                Quantity epa  = (static_cast<const GaussianShape *>(compShape))->positionAngleError();
                cout << "bmaj " << bmaj << " bmin " << bmin << " bpa " << bpa << endl;
                cout << "emaj " << emaj << " emin " << emin << " epa " << epa << endl;
            }
        }
        return results;
   /*
    // this needs to be cleaned up, it was done as an intro to the casa dev system

    ImageInterface<Float>* imagePtr = &image;
    SubImage<Float> subim;
    if (doRegion) {
        subim = SubImage<Float>(image, imRegion, False);
        imagePtr = &subim;
    }
    LogIO logio;
    ImageStatistics<Float> stats(*imagePtr, logio, True, False);
    IPosition minpos, maxpos;
    stats.getMinMaxPos(minpos, maxpos);
    cout << " min pos " << minpos << " maxpos " << maxpos << endl; 
    Array<Double> sumsquared;
    stats.getStatistic (sumsquared, LatticeStatsBase::SUMSQ);
    cout << "sumsq " << sumsquared << endl;
    // get single channel
    IPosition start(4, 0, 0, 0, 0);
    IPosition end = imagePtr->shape() - 1;
    IPosition stride(4, 1, 1, 1, 1);
    cout << "start " << start << " end " << end << endl;
    // channel 38
    start[3] = chan;
    end[3] = chan; 
    cout << "start " << start << " end " << end << endl;

    Slicer sl(start, end, stride, Slicer::endIsLast);
    SubImage<Float> subim2(*imagePtr, sl, False);
    
    stats.setNewImage(subim2);
    stats.getMinMaxPos(minpos, maxpos);
    cout << " min pos " << minpos << " maxpos " << maxpos << endl; 

    stats.getStatistic (sumsquared, LatticeStatsBase::SUMSQ);
    cout << "sumsq " << sumsquared << endl;


//    GaussianFitter myGF(image, regionRecord);
//    const IPosition latticeShape(image.shape());
//    RegionManager *regionManager_p;
//    regionManager_p = new casa::RegionManager();
*/
    }

    void ImageFitter::_construct(
        const String& imagename, const String& box, const String& region,
        const String& estimatesFilename
    ) {
        if (imagename.empty()) {
            *itsLog << "imagename cannot be blank" << LogIO::EXCEPTION;
        }

        // Register the functions to create a FITSImage or MIRIADImage object.
        FITSImage::registerOpenFunction();
        MIRIADImage::registerOpenFunction();

        ImageUtilities::openImage(image, imagename, *itsLog);
        if (image == 0) {
            throw(AipsError("Unable to open image " + imagename));
        }
        _doRegion(box, region);
        _checkImageParameterValidity();
        if(estimatesFilename.empty()) {
        	*itsLog << LogIO::NORMAL << "No estimates file specified, so will attempt to find and fit one gaussian."
        		<< LogIO::POST;
        }
        else {
        	FitterEstimatesFileParser parser(estimatesFilename, *image);
        	estimates = parser.getEstimates();
        	fixed = parser.getFixed();
        	Record rec;
        	String errmsg;
        	estimates.toRecord(errmsg, rec);
        	*itsLog << LogIO::NORMAL << "File " << estimatesFilename << " has " << estimates.nelements()
        		<< " specified, so will attempt to fit that many gaussians " << LogIO::POST;
        }
    }

    void ImageFitter::_checkImageParameterValidity() const {
        *itsLog << LogOrigin("ImageFitter", "_checkImageParameterValidity");
        String error;
        ImageMetaData imageProps(*image);
        if (imageProps.hasPolarizationAxis() && imageProps.hasSpectralAxis()) {
            if (! imageProps.areChannelAndStokesValid(error, chan, stokesString)) {
                *itsLog << error << LogIO::EXCEPTION;
            }
        }
    } 

    void ImageFitter::_doRegion(const String& box, const String& region) {
        if (box == "") {
            // box not specified, check for saved region
            if (region == "") {
                // neither region nor box specified, use entire 2-D plane
                IPosition imShape = image->shape();
                Vector<Int> dirNums = ImageMetaData(*image).directionAxesNumbers();
                Vector<Int> dirShape(imShape[dirNums[0]], imShape[dirNums[1]]);
                *itsLog << LogIO::NORMAL << "Neither box nor region specified, "
                    << "so entire plane of " << dirShape[0] << " x "
                    << dirShape[1] << "  will be used" << LogIO::POST;
                ostringstream boxStream;
                boxStream << "0, 0, " << dirShape[0] << ", " << dirShape[1];
                _processBox(String(boxStream));
            }
            else {
                // get the ImageRegion from the specified region
                imRegion = image->getRegion(region);
            }

        }
        else if (box.freq(",") != 3) {
            *itsLog << "box not specified correctly" << LogIO::EXCEPTION;
        }
        else {
            if (region != "") {
                *itsLog << "both box and region specified, box will be used" << LogIO::WARN;
            }
            // we have been given a box by the user and it is specified correctly
            _processBox(box);
        }
    } 

    void ImageFitter::_processBox(const String& box) {
        Vector<String> boxParts = stringToVector(box);
        if (boxParts.size() != 4) {
            return;
        }
        IPosition imShape = image->shape(); 
        Vector<Double> blc(imShape.nelements());
        Vector<Double> trc(imShape.nelements());

        for (uInt i=0; i<imShape.nelements(); ++i) {
            blc[i] = 0;
            trc[i] = imShape[i] - 1;
        }
    
        Vector<Int> dirNums = ImageMetaData(*image).directionAxesNumbers();
        blc[dirNums[0]] = String::toDouble(boxParts[0]);
        blc[dirNums[1]] = String::toDouble(boxParts[1]);
        trc[dirNums[0]] = String::toDouble(boxParts[2]);
        trc[dirNums[1]] = String::toDouble(boxParts[3]);

        LCBox lcBox(blc, trc, imShape);
        WCBox wcBox(lcBox, image->coordinates());
        imRegion = ImageRegion(wcBox);
    }

    String ImageFitter::_resultsToString() const {
    	ostringstream summary;
    	for (uInt i = 0; i < results.nelements(); i++) {
    		summary << "Fit on " << image->name(True) << " region " << i << endl;
    		summary << _positionToString(i) << endl;
    		summary << _sizeToString(i) << endl;
    		summary << _fluxToString(i) << endl;
    		summary << _spectrumToString(i) << endl;
    	}
    	return summary.str();
    }

    String ImageFitter::_positionToString(const uInt compNumber) const  {
    	ostringstream position;
    	MDirection mdir = results.getRefDirection(compNumber);

    	Quantity lat = mdir.getValue().getLat("rad");
    	String dec = MVAngle(lat).string(MVAngle::ANGLE_CLEAN, 8);

    	Quantity longitude = mdir.getValue().getLong("rad");
    	String ra = MVTime(longitude).string(MVTime::TIME, 9);
    	const ComponentShape* compShape = results.getShape(compNumber);

    	Quantity ddec = compShape->refDirectionErrorLat();
    	ddec.convert("rad");

    	Quantity dra = compShape->refDirectionErrorLong();

    	dra.convert("rad");

    	// choose a unified error for both axes
    	Double delta = 0;
    	if ( dra.getValue() == 0 && ddec.getValue() == 0 ) {
    		delta = 0;
    	}
    	else if ( dra.getValue() == 0 ) {
    		delta = fabs(ddec.getValue());
    	}
    	else if ( ddec.getValue() == 0 ) {
    		delta = fabs(dra.getValue());
    	}
    	else {
    		delta = sqrt ( dra.getValue()*dra.getValue() + ddec.getValue()*ddec.getValue() );
    	}

		// Add error estimates to ra/dec strings if an error is given (either >0)

    	uInt precision = 1;
    	if ( delta != 0 ) {
    		dra.convert("s");

    		ddec.convert("arcsec");
    		Double drasec  = _round(dra.getValue());
    		Double ddecarcsec = _round(ddec.getValue());
    		Vector<Double> dravec(2), ddecvec(2);
    		dravec.set(drasec);
    		ddecvec.set(ddecarcsec);
    		precision = _precision(dravec,ddecvec);
    		ra = MVTime(longitude).string(MVTime::TIME, 6+precision);
    		dec =  MVAngle(lat).string(MVAngle::ANGLE, 6+precision);
    	}
    	position << "Position ---" << endl;
    	position << "       --- ra:    " << ra << " +/- " << std::fixed
    		<< setprecision(precision) << dra << " (" << dra.getValue("arcsec")
    		<< " arcsec)" << endl;
    	position << "       --- dec: " << dec << " +/- " << ddec << endl;
       	Vector<Double> world(4,0), pixel(4,0);
    	image->coordinates().toWorld(world, pixel);

        world[0] = longitude.getValue();
        world[1] = lat.getValue();
        if (image->coordinates().toPixel(pixel, world)) {
        	DirectionCoordinate dCoord = image->coordinates().directionCoordinate(
        		ImageMetaData(*image).directionCoordinateNumber()
        	);
        	Vector<Double> increment = dCoord.increment();
        	Double raPixErr = dra.getValue("rad")/increment[0];
        	Double decPixErr = ddec.getValue("rad")/increment[1];
        	Vector<Double> raPix(2), decPix(2);
         	raPix.set(_round(raPixErr));
        	decPix.set(_round(decPixErr));
        	precision = _precision(raPix, decPix);

        	position << setprecision(precision);
        	position << "       --- ra:   " << pixel[0] << " +/- " << raPixErr << " pixels" << endl;
        	position << "       --- dec:  " << pixel[1] << " +/- " << decPixErr << " pixels" << endl;
        }
        else {
        	position << "unable to determine max in pixels" << endl;
        }
    	return position.str();
    }

    String ImageFitter::_sizeToString(const uInt compNumber) const  {
    	ostringstream size;
        const ComponentShape* compShape = results.getShape(compNumber);

    	if (compShape->type() == ComponentType::GAUSSIAN) {
    		// print gaussian stuff
    		Quantity maj = (static_cast<const GaussianShape *>(compShape))->majorAxis();
    		Quantity min = (static_cast<const GaussianShape *>(compShape))->minorAxis();
    		Quantity pa  = (static_cast<const GaussianShape *>(compShape))->positionAngle();
    		Quantity emaj = (static_cast<const GaussianShape *>(compShape))->majorAxisError();
    		Quantity emin = (static_cast<const GaussianShape *>(compShape))->minorAxisError();
    		Quantity epa  = (static_cast<const GaussianShape *>(compShape))->positionAngleError();
    		Vector<Quantum<Double> > beam = image->imageInfo().restoringBeam();
    		Bool hasBeam = beam.nelements() == 3;
    		size << "Image component size";
    		if (hasBeam) {
    			size << " (convolved with beam)";
    		}
    		size << " ---" << endl;
    		size << _gaussianToString(maj, min, pa, emaj, emin, epa) << endl;
    		if (hasBeam) {
        		size << "Clean beam size ---" << endl;
    			size << _gaussianToString(beam[0], beam[1], beam[2], 0, 0, 0, False) << endl;
    			size << "Image component size (deconvolved from beam) ---" << endl;
    			// NOTE fit components change here to their deconvolved counterparts
    			Quantity femaj = emaj/maj;
    			Quantity femin = emin/min;
    			if (ImageUtilities::deconvolveFromBeam(maj, min, pa, *itsLog, beam)) {
    				size << "    Component is a point source" << endl;
    			}
    			else {
    				if (pa.getValue("deg") < 0) {
    					pa += Quantity(180, "deg");
    				}
    				emaj *= femaj;
    				emin *= femin;
    				size << _gaussianToString(maj, min, pa, emaj, emin, epa);
    			}
    		}
    	}
    	return size.str();
    }

    Double ImageFitter::_round(Double number) const {
    	Double sign = 1;
        if (number < 0) {
            sign = -1;
            number = -number;
        }
        Double lgr = log10(number);
        // shift number into range 32-320
        Int i = (lgr >= 0) ? int(lgr + 0.5) : int(lgr - 0.5);
        Double temp = number * pow(10.0, (2-i));
        return sign*(temp + 0.5)*pow(10.0, (i-2));
    }

    uInt ImageFitter::_precision(
    	const Vector<Double>& pair1, const Vector<Double>& pair2
    ) const {
    	Double val1 = pair1[0];
    	Double err1 = pair1[1];
    	Double value = val1;
    	Double error = fabs(err1);
    	uInt sign = 0;
    	if (pair2.size() == 0) {
				if (val1 < 0) {
                sign = 1;
                val1 = fabs(val1);
            }
    	}
    	else {
    		Double val2 = pair2[0];
    		Double err2 = pair2[1];
            if (val1 < 0 or val2 < 0) {
                sign = 1;
                val1 = fabs(val1);
                val2 = fabs(val2);
            }
            value = max(val1, val2);
            error = (err1 == 0 || err2 == 0)
            	? max(fabs(err1), fabs(err1))
            	: min(fabs(err1), fabs(err2));
    	}

    	// Here are a few general safeguards
    	// If we are dealing with a value smaller than the estimated error
    	// (e.g., 0.6 +/- 12) , the roles in formatting need to be
    	// reversed.
    	if ( value < error ) {
    		value = max(value,0.1*error);
    		// TODO be cool and figure out a way to swap without using a temporary variable
    		Double tmp = value;
    		value = error;
    		error = tmp;
    	}

		// A value of precisely 0 formats as if it were 1.  If the error is
    	// precisely 0, we print to 3 significant digits

    	if ( value == 0 ) {
    		value = 1;
    	}
    	if ( error == 0 ) {
    		error = 0.1*value;
    	}

		// Arithmetically we have to draw the limit somewhere.  It is
		// unlikely that numbers (and errors) < 1e-10 are reasonably
		// printed using this limited technique.
    	value = max(value,1e-10);
    	error = max(error,1e-8);

    	// Print only to two significant digits in the error
    	error = 0.1*error;

    	// Generate format

    	uInt before = max(int(log10(value))+1,1);     // In case value << 1 ==> log10 < 0
    	uInt after = 0;
    	//String format="%"+str(sign+before)+".0f"
    	uInt width = sign + before;
    	if ( log10(error) < 0 ) {
    		after = int(fabs(log10(error)))+1;
    		//format="%"+str(sign+before+1+after)+"."+str(after)+"f"
    		width = sign + before + after + 1;
    	}
    	return after;
    }

    String ImageFitter::_gaussianToString(
    	Quantity major, Quantity minor, Quantity posangle,
    	Quantity majorErr, Quantity minorErr, Quantity posanErr,
    	Bool includeUncertainties
    ) const {
    	// Inputs all as angle quanta
    	Vector<String> angUnits(5);
    	angUnits[0] = "deg";
    	angUnits[1] = "arcmin";
    	angUnits[2] = "arcsec";
    	angUnits[3] = "marcsec";
    	angUnits[4] = "uarcsec";
    	// First force position angle to be between 0 and 180 deg
    	if(posangle.getValue() < 0) {
    		posangle + Quantity(180, "deg");
    	}

    	String prefUnits;
    	Quantity vmax(max(fabs(major.getValue("arcsec")), fabs(minor.getValue("arcsec"))), "arcsec");

       	for (uInt i=0; i<angUnits.size(); i++) {
        	prefUnits = angUnits[i];
        	if(vmax.getValue(prefUnits) > 1) {
        		break;
        	}
        }
       	major.convert(prefUnits);
       	minor.convert(prefUnits);
       	majorErr.convert(prefUnits);
       	minorErr.convert(prefUnits);

    	Double vmaj = major.getValue();
    	Double vmin = minor.getValue();

    	// Formatting as "value +/- err" for symmetric errors

    	Double dmaj = majorErr.getValue();
    	Double dmin = minorErr.getValue();
    	// position angle is always in degrees cuz users like that
    	Double pa  = posangle.getValue("deg");
    	Double dpa = posanErr.getValue("deg");

    	Vector<Double> majVec(2), minVec(2), paVec(2);
    	majVec[0] = vmaj;
    	majVec[1] = dmaj;
    	minVec[0] = vmin;
    	minVec[1] = dmin;
    	paVec[0] = pa;
    	paVec[1] = dpa;
    	uInt precision1 = _precision(majVec, minVec);
    	uInt precision2 = _precision(paVec, Vector<Double>(0));

    	ostringstream summary;
    	summary << std::fixed << setprecision(precision1);
    	summary << "       --- major axis:     " << major.getValue();
    	if (includeUncertainties) {
    		summary << " +/- " << majorErr.getValue();
    	}
    	summary << " " << prefUnits << endl;
    	summary << "       --- minor axis:     " << minor.getValue();
    	if (includeUncertainties) {
    	    summary << " +/- " << minorErr.getValue();
    	}
    	summary << " "<< prefUnits << endl;
    	summary << setprecision(precision2);
    	summary << "       --- position angle: " << pa;
    	if (includeUncertainties) {
    		summary << " +/- " << dpa;
    	}
    	summary << " deg" << endl;
    	return summary.str();
    }

    String ImageFitter::_fluxToString(uInt compNumber) const {
    	Vector<String> unitPrefix(8);
		unitPrefix[0] = "T";
		unitPrefix[1] = "G";
		unitPrefix[2] = "M";
		unitPrefix[3] = "k";
		unitPrefix[4] = "";
		unitPrefix[5] = "m";
		unitPrefix[6] = "u";
		unitPrefix[7] = "n";

    	ostringstream fluxes;
    	Vector<Quantity> fluxQuant;
    	Quantity fluxDensity;
    	Quantity fluxDensityError;

    	results.getFlux(fluxQuant, compNumber);
    	// TODO there is probably a better way to get the flux component we want...
        Vector<String> polarization = results.getStokes(compNumber);
        for(uInt i = 0; i < polarization.size(); i++) {
        	if (polarization[i] == stokesString) {
        		fluxDensity = fluxQuant[i];
        		complex<double> error = results.component(compNumber).flux().errors()[i];
        		fluxDensityError.setValue(sqrt(error.real()*error.real() + error.imag()*error.imag()));
        		fluxDensityError.setUnit(fluxDensity.getUnit());
        		break;
        	}
        }
        Quantity peakIntensity;
        Quantity resolutionElementArea;
        ImageMetaData md(*image);
        Quantity intensityToFluxConversion(1.0, "beam");
        Unit brightnessUnit = image->units();
        const ComponentShape* compShape = results.getShape(compNumber);

        // does the image have a restoring beam?
        if(! md.getBeamArea(resolutionElementArea)) {
            // if no restoring beam, let's hope the the brightness units are
            // in [prefix]Jy/pixel and let's find the pixel size.
            if(md.getDirectionPixelArea(resolutionElementArea)) {
                intensityToFluxConversion.setUnit("pixel");
            }
            else {
                // can't find  pixel size, which is extremely bad!
                *itsLog << "Unable to determine the resolution element area of image "
                	<< image->name()<< LogIO::EXCEPTION;
            }
        }
        Quantity compArea;
        if (compShape->type() == ComponentType::GAUSSIAN) {
        	compArea = (static_cast<const GaussianShape *>(compShape))->getArea();
        	peakIntensity = fluxDensity/intensityToFluxConversion*resolutionElementArea/compArea;
        	peakIntensity.convert("Jy/" + intensityToFluxConversion.getUnit());
        }
        fluxes << "Flux ---" << endl;
        String unit;
        for (uInt i=0; i<unitPrefix.size(); i++) {
        	unit = unitPrefix[i] + "Jy";
        	if (fluxDensity.getValue(unit) > 1) {
        		fluxDensity.convert(unit);
        		fluxDensityError.convert(unit);
        		break;
        	}
        }
        Vector<Double> fd(2);
        fd[0] = fluxDensity.getValue();
        fd[1] = fluxDensityError.getValue();
        uInt precision = _precision(fd, Vector<Double>());
        fluxes << std::fixed << setprecision(precision);
        fluxes << "       ---   Integrated: " << fluxDensity.getValue()
			<< " +/- " << fluxDensityError.getValue() << " "
			<< fluxDensity.getUnit() << endl;

        Quantity peakIntensityError = peakIntensity*fluxDensityError/fluxDensity;
        for (uInt i=0; i<unitPrefix.size(); i++) {
         	unit = unitPrefix[i] + "Jy/" + intensityToFluxConversion.getUnit();
         	if (peakIntensity.getValue(unit) > 1) {
         		peakIntensity.convert(unit);
         		peakIntensityError.convert(unit);
         		break;
         	}
         }
         Vector<Double> pi(2);
         pi[0] = peakIntensity.getValue();
         pi[1] = peakIntensityError.getValue();
         precision = _precision(pi, Vector<Double>());
         fluxes << std::fixed << setprecision(precision);
         fluxes << "       ---         Peak: " << peakIntensity.getValue()
 			<< " +/- " << peakIntensityError.getValue() << " "
 			<< peakIntensity.getUnit() << endl;
         fluxes << "       --- Polarization: " << stokesString << endl;
         return fluxes.str();
    }

    String ImageFitter::_spectrumToString(uInt compNumber) const {
    	Vector<String> unitPrefix(8);
		unitPrefix[0] = "T";
		unitPrefix[1] = "G";
		unitPrefix[2] = "M";
		unitPrefix[3] = "k";
		unitPrefix[4] = "";
		unitPrefix[5] = "m";
		unitPrefix[6] = "u";
		unitPrefix[7] = "n";
    	ostringstream spec;
    	const SpectralModel& spectrum = results.component(compNumber).spectrum();
    	Quantity frequency = spectrum.refFrequency().get("MHz");
    	Quantity c(C::c, "m/s");
    	Quantity wavelength = c/frequency;
    	String prefUnit;
    	for (uInt i=0; i<unitPrefix.size(); i++) {
    		prefUnit = unitPrefix[i] + "Hz";
    		if (frequency.getValue(prefUnit) > 1) {
    			frequency.convert(prefUnit);
    			break;
    		}
    	}
    	for (uInt i=0; i<unitPrefix.size(); i++) {
    		prefUnit = unitPrefix[i] + "m";
    		if (wavelength.getValue(prefUnit) > 1) {
    			wavelength.convert(prefUnit);
    			break;
    		}
    	}

    	spec << "Spectrum ---" << endl;
    	spec << "      --- frequency:        " << frequency << " (" << wavelength << ")" << endl;
    	return spec.str();
    }

}

