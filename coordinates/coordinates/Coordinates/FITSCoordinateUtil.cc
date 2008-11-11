//# FITSCoordinateUtil.cc: inter-convert CoordinateSystem and FITS headers
//# Copyright (C) 1997,1998,1999,2000,2001,2002,2003,2004
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
//#
//# $Id: 


#include <coordinates/Coordinates/FITSCoordinateUtil.h>

#include <coordinates/Coordinates/CoordinateSystem.h>
#include <coordinates/Coordinates/LinearCoordinate.h>
#include <coordinates/Coordinates/DirectionCoordinate.h>
#include <coordinates/Coordinates/SpectralCoordinate.h>
#include <coordinates/Coordinates/TabularCoordinate.h>
#include <coordinates/Coordinates/StokesCoordinate.h>
#include <coordinates/Coordinates/ObsInfo.h>

#include <coordinates/Coordinates/CoordinateUtil.h>

#include <casa/Arrays/Vector.h>
#include <casa/Arrays/IPosition.h>
#include <casa/Containers/Record.h>
#include <casa/Logging/LogIO.h>
#include <casa/BasicSL/String.h>
#include <casa/Quanta/MVTime.h>
#include <casa/Quanta/MVDirection.h>
#include <casa/Quanta/Quantum.h>
#include <casa/Quanta/Unit.h>
#include <casa/Quanta/UnitMap.h>
#include <casa/Utilities/Assert.h>
#include <casa/Utilities/Regex.h>
#include <fits/FITS/FITSDateUtil.h>
#include <measures/Measures/MDoppler.h>
#include <measures/Measures/MEpoch.h>

#include <casa/iostream.h>


#include <wcslib/wcs.h>
#include <wcslib/wcshdr.h>
#include <wcslib/wcsfix.h>
#include <wcslib/wcsmath.h>
#include <wcslib/fitshdr.h>

namespace casa { //# NAMESPACE CASA - BEGIN

    Bool FITSCoordinateUtil::toFITSHeader(RecordInterface &header, 
					  IPosition &shape,
					  const CoordinateSystem& cSys,
					  Bool oneRelative,
					  Char prefix, Bool writeWCS,
					  Bool preferVelocity, 
					  Bool opticalVelocity) const
    {
	LogIO os(LogOrigin("FITSCoordinateUtil", "toFITSHeader", WHERE));

// If we have any tabular axes that aren't pure linear report that the
// table will be lost.

	Int tabCoord = -1;
	while ((tabCoord = cSys.findCoordinate(Coordinate::TABULAR, tabCoord)) > 0) {
	    if (cSys.tabularCoordinate(tabCoord).pixelValues().nelements() > 0) {
		os << LogIO::WARN <<
		    "Note: Your coordinate system has one or more TABULAR axes.\n"
		    "The lookup table will be lost in the conversion to FITS, and\n"
		    "will be replaced by averaged (i.e. linearized) axes." <<
		    LogIO::POST;
		break;
	    }
	}

// Validation

	const Int n = cSys.nWorldAxes();
	String sprefix(prefix);
	if (header.isDefined(sprefix + "rval") ||
	    header.isDefined(sprefix + "rpix") ||
	    header.isDefined(sprefix + "delt") ||
	    header.isDefined(sprefix + "type") ||
	    header.isDefined(sprefix + "unit")) {
	    os << LogIO::SEVERE << "Already contains one or more of *rval, *rpix, "
		"*delt, *type, *unit";
	    return False;
	}

	Double offset = 0.0;
	if (oneRelative) {
	    offset = 1.0;
	}

// Canonicalize units and find sky axes

	CoordinateSystem coordsys = cSys;

// Find the sky coordinate, if any

	Int skyCoord = coordsys.findCoordinate(Coordinate::DIRECTION);
	Int longAxis = -1, latAxis = -1;

// Find the spectral axis, if any

	Int specCoord = coordsys.findCoordinate(Coordinate::SPECTRAL);
	Int specAxis = -1;
    
// Find the Stokes axis if any. 

	Int stokesCoord = coordsys.findCoordinate(Coordinate::STOKES);
	Int stokesAxis = -1;

// If any axes have been removed from a coordinate, you find it out here.  

	Int i;
	for (i=0; i<n ; i++) {
	    Int c, a;
	    coordsys.findWorldAxis(c, a, i);
	    if (c == skyCoord) {
		if (a == 0) {
		    longAxis = i;
		} else if (a == 1) {
		    latAxis = i;
		}
	    } else if (c == specCoord) {
		specAxis = i;
	    } else if (c == stokesCoord) {
		stokesAxis = i;
	    }
	}

// If both axes for the DC are removed, set coordinate to missing.
// For other coordinates, they are handled via axes, not
// Coordinate number, so we don't need to do this. I think.

	if (longAxis==-1 && latAxis==-1) skyCoord = -1;

// change the units to degrees for the sky axes

	Vector<String> units(coordsys.worldAxisUnits().copy());
	if (longAxis >= 0) units(longAxis) = "deg";
	if (latAxis >= 0) units(latAxis) = "deg";
	if (specAxis >= 0) units(specAxis) = "Hz";
	if (stokesAxis >= 0) units(stokesAxis) = "";
	coordsys.setWorldAxisUnits(units);

// Generate keywords.  If we find we have a DC with one of the
// axes removed, it will be linearized here.

	Double longPole, latPole;
	Vector<Double> crval, crpix, cdelt, pvi_ma, crota;
	Vector<String> ctype, cunit;
	Matrix<Double> pc;
	Bool isNCP = False;
	if (!generateFITSKeywords (os, isNCP, longPole, latPole, crval, crpix, 
				   cdelt, crota,  
				   pvi_ma, ctype, cunit, pc, 
				   coordsys, skyCoord, longAxis, latAxis, 
				   specAxis, stokesAxis, writeWCS,
				   offset, sprefix)) {
	    return False;
	}

// Special stokes handling

	if (stokesAxis >= 0) {
	    if (!toFITSHeaderStokes (crval, crpix, cdelt, os, coordsys,
				     stokesAxis, stokesCoord)) return False;
	}

// If there are more world than pixel axes, we will need to add
// degenerate pixel axes and modify the shape.

	if (Int(coordsys.nPixelAxes()) < n) {
	    IPosition shapetmp = shape; 
	    shape.resize(n);
	    Vector<Double> crpixtmp = crpix.copy();
	    crpix.resize(n);
	    Int count = 0;
	    for (Int worldAxis=0; worldAxis<n; worldAxis++) {
		Int coordinate, axisInCoordinate;
		coordsys.findWorldAxis(coordinate, axisInCoordinate, worldAxis);
		Int pixelAxis = coordsys.pixelAxes(coordinate)(axisInCoordinate);
		if (pixelAxis >= 0) {
		    // We have a pixel axis
		    shape(worldAxis) = shapetmp(count);
		    crpix(worldAxis) = crpixtmp(count);
		    count++;
		} else {
		    // No corresponding pixel axis.
		    shape(worldAxis) = 1;
		    crpix(worldAxis) = 1.0;
		}
	    }
	}

// Try to work out the epoch/equinox
// Also LONGPOLE and LATPOLE here.

	if (skyCoord >= 0) {
	    const DirectionCoordinate& dCoord = coordsys.directionCoordinate(skyCoord);
	    MDirection::Types radecsys = dCoord.directionType();
	    Double equinox = -1.0;
	    switch(radecsys) {
	    case MDirection::J2000:
		equinox = 2000.0;
		break;
	    case MDirection::B1950:
		equinox = 1950.0;
		break;
	    case MDirection::B1950_VLA:
		equinox = 1979.9;
		break;
	    default:
		; // Nothing
	    }
	    if (equinox > 0) {
		if (writeWCS) {
		    header.define("equinox", equinox);
		} else {
		    header.define("epoch", equinox);
		}
	    }
//
	    header.define("lonpole", longPole);
	    //const Projection& proj = dCoord.projection();
	    //if (!Projection::isZenithal(proj.type())) {    
	    //	header.define("latpole", latPole);       // Not relevant for zenithals
	    //}
	    header.define("latpole", latPole);     
	}

// Actually write the header

	if (writeWCS && Int(coordsys.nPixelAxes()) == n) {
	    header.define("pc", pc);
	} else if (writeWCS) {
	    os << LogIO::SEVERE << "writeWCS && nPixelAxes() != n. Requires "
		"development!!!"  << LogIO::POST;
	}

	header.define(sprefix + "type", ctype);
	header.define(sprefix + "rval", crval);
	header.define(sprefix + "delt", cdelt);
	header.define(sprefix + "rota", crota);
	header.define(sprefix + "rpix", crpix);
	header.define(sprefix + "unit", cunit);

	if (skyCoord >=0 && pvi_ma.nelements() > 0) {
	    if (!writeWCS) {
		for (uInt k=0; k<pvi_ma.nelements(); k++) {
		    if (!casa::nearAbs(pvi_ma(k), 0.0)) {
			os << LogIO::WARN << 
			    "Projection parameters not all zero.Information lost in FITS"
			    " conversion. Try WCS?." <<
			    LogIO::POST;
			break;
		    }
		}
	    }
	    else {
		// determine which axis is the "latitude" axis, i.e. DEC or xLAT
		int theLatAxisNum = -1;
		for (int k=0; k<ctype.nelements(); k++){
		    string theType(ctype[k]);
		    if (theType.substr(0,3) == "DEC" || theType.substr(1,3) == "LAT"){
			theLatAxisNum = k;
			break;
		    }
		}
		if (theLatAxisNum == -1){
		    header.define("pv2_", pvi_ma);
		    os << LogIO::WARN << 
			"There is no axis with type DEC or LAT. Cannot identify latitude axis for WCS."
			" Will assume axis 2 as default." <<
			LogIO::POST;
		    header.define("pv2_", pvi_ma);		}
		else {
		    ostringstream oss;
		    oss << "pv" << theLatAxisNum+1 << "_"; // numbers are start at 1 in WCS
		    String s(oss);

		    header.define(s, pvi_ma);

		    os << LogIO::DEBUG1 << 
			"Identified axis number " << theLatAxisNum+1 << " as latitude axis for WCS. " << 
			s << " is the keyword name." <<
			LogIO::POST;
		}
	    }
	}
	if (specAxis >= 0) {
	    const SpectralCoordinate &spec = coordsys.spectralCoordinate(specCoord);
	    spec.toFITS(header, specAxis, os, oneRelative, preferVelocity, 
			opticalVelocity);
	}

// Write out the obsinfo

	String error;
	Bool ok = coordsys.obsInfo().toFITS(error, header);
	if (!ok) {
	    os << LogIO::SEVERE << "Error converting ObsInfo: " << error << 
		LogIO::POST;
	}

	return ok;
    }


    Bool FITSCoordinateUtil::toFITSHeaderStokes(Vector<Double>& crval,
						Vector<Double>& crpix,
						Vector<Double>& cdelt,
						LogIO& os,
						const CoordinateSystem& coordsys,
						Int stokesAxis, Int stokesCoord)  const
    {
	Vector<Int> stokes(coordsys.stokesCoordinate(stokesCoord).stokes());
	Int inc = 1;
	Bool inorder = True;
	if (stokes.nelements() > 1) {
	    inc = Stokes::FITSValue(Stokes::StokesTypes(stokes(1))) - 
		Stokes::FITSValue(Stokes::StokesTypes(stokes(0)));
	    for (uInt k=2; k<stokes.nelements(); k++) {
		if ((Stokes::FITSValue(Stokes::StokesTypes(stokes(k))) - 
		     Stokes::FITSValue(Stokes::StokesTypes(stokes(k-1)))) !=
		    inc) {
		    inorder = False;
		}
	    }
	}
	if (inorder) {
	    crval(stokesAxis) = Stokes::FITSValue(Stokes::StokesTypes(stokes(0)));
	    crpix(stokesAxis) = 1;
	    cdelt(stokesAxis) = inc;
	} else {

// The idea here is to write non-standard records, to indicate something
// funny, and then write the rest of the Stokes axis as non-standard
// keywords.  Since fromFITSHeader can't decode this anyway, for now
// return False.

/*
  crval(stokesAxis) = Stokes::FITSValue(Stokes::StokesTypes(stokes(0))) + 200;
  crpix(stokesAxis) = 1;
  cdelt(stokesAxis) = 1;
*/

	    os << LogIO::SEVERE 
	       <<  "The Stokes coordinate in this CoordinateSystem is too" << endl;
	    os << LogIO::SEVERE 
	       << "complex to convert to the FITS convention" << LogIO::POST;
	    return False;
	}
//
	return True;
    }


    Bool FITSCoordinateUtil::generateFITSKeywords (LogIO& os, Bool& isNCP, 
						   Double& longPole,  Double& latPole,
						   Vector<Double>& crval,
						   Vector<Double>& crpix,
						   Vector<Double>& cdelt,
						   Vector<Double>& crota,
						   Vector<Double>& pvi_ma,
						   Vector<String>& ctype,
						   Vector<String>& cunit,
						   Matrix<Double>& pc,
						   const CoordinateSystem& cSys,
						   Int skyCoord, Int longAxis, 
						   Int latAxis, Int specAxis, 
						   Int stokesAxis, Bool writeWCS, 
						   Double offset, const String& sprefix) const
    {
	const Int n = cSys.nWorldAxes();
	crval = cSys.referenceValue();
	crpix = cSys.referencePixel() + offset;
	cdelt = cSys.increment();

// Generate FITS ctypes from DirectionCoordinate

	Vector<String> cctype(2);
	if (skyCoord >= 0) {
	    const DirectionCoordinate dCoord = cSys.directionCoordinate(skyCoord);


	    pvi_ma = dCoord.projection().parameters();

	    longPole = dCoord.longLatPoles()(2);
	    latPole =  dCoord.longLatPoles()(3);
//
	    const DirectionCoordinate &dc = cSys.directionCoordinate(skyCoord);
	    cctype = cTypeFromDirection (isNCP, dc.projection(), 
					 DirectionCoordinate::axisNames(dc.directionType(),
									True), C::pi/180.0*crval(latAxis), True);
	}
//
	ctype = cSys.worldAxisNames();
	for (Int i=0; i < n; i++) {
	    if (i == longAxis || i == latAxis) { 
		if (i==longAxis) {
		    ctype[i] = cctype[0];
		} else {
		    ctype[i] = cctype[1];
		}
	    } else if (i == specAxis) {

// Nothing - will be handled by SpectralCoordinate

	    } else if (i == stokesAxis) {
		ctype[i] = "STOKES";
	    } else {

// Linear and Tabular

		ctype[i].upcase();
		if (ctype[i].length() > 8) {
		    ctype[i] = ctype[i].at(0,8);
		}
		while (ctype[i].length() < 8) ctype[i] += " ";
	    }
	}

// CUNIT is case sensitive. 

	cunit = cSys.worldAxisUnits();
	for (Int i=0; i<n; i++) {
	    if (cunit(i).length() > 8) {
		cunit(i) = cunit(i).at(0,8);
	    }
	    while (cunit(i).length() < 8) cunit(i) += " ";
	}
//
	pc = cSys.linearTransform();

// crota: See Greisen and Calabretta "Converting Previous Formats"

	crota.resize(n);
	crota = 0;
	if (longAxis >= 0 && latAxis >= 0) {
	    Double rholong = atan2(pc(latAxis, longAxis)*C::pi/180.0,
				   pc(longAxis, longAxis)*C::pi/180.0)*180.0/C::pi;
	    Double rholat = atan2(-pc(longAxis, latAxis)*C::pi/180.0,
				  pc(latAxis, latAxis)*C::pi/180.0)*180.0/C::pi;
	    crota[latAxis] = (rholong + rholat)/2.0;
	    if (!casa::near(rholong, rholat)) {
		os << LogIO::WARN << sprefix + "rota is not very accurate." 
		   <<  " PC matrix is not a pure rotation.";
		if (!writeWCS) {
		    os << endl << "Consider writing the DRAFT WCS convention to avoid losing information.";
		}
		os << LogIO::POST;
	    }
	}
	return True;
    }


//     Bool FITSCoordinateUtil::fromFITSHeaderOld(Int& stokesFITSValue,
// 					       CoordinateSystem &coordsys,
// 					       const RecordInterface &header,
// 					       const IPosition& shape,
// 					       Bool oneRelative,
// 					       Char prefix)
//     {
// 	LogIO os(LogOrigin("FITCoordinateUtil", "fromFITSHeaderOld", WHERE));
// 	os << "Using old aips++-based FITS parser" << LogIO::POST;
// 	if (coordsys.nCoordinates() != 0) {
// 	    CoordinateSystem empty;
// 	    coordsys = empty;
// 	}

// 	String sprefix(prefix);
// 	Double offset = 0.0;
// 	if (oneRelative) {
// 	    offset = 1.0;
// 	}

// // FITS angular units are degrees

// 	Bool warnStokes = stokesFITSValue > 0;
// 	stokesFITSValue = -1;
// 	Vector<Double> cdelt, crval, crpix;
// 	Vector<Int> naxes;
// 	Vector<String> ctype, cunit;
// 	Matrix<Double> pc;
// 	Bool haveUnit = False;
// 	Int rotationAxis = -1;
// 	Bool hasCD = False;
// 	Int n = 0;
// 	try {
// 	    if (header.isDefined("naxis")) {
// 		Vector<Int> naxis;
// 		header.get("naxis", naxis);
// 		n = naxis.nelements();
// 	    }
// //
// 	    if (header.isDefined(sprefix + "rval")) {
// 		header.get(sprefix + "rval", crval);
// 	    }
// 	    if (header.isDefined(sprefix + "rpix")) {
// 		header.get(sprefix + "rpix", crpix);
// 		crpix -= offset;
// 	    }
// 	    if (header.isDefined(sprefix + "type")) {
// 		header.get(sprefix + "type", ctype);
// 		if (n==0) n = ctype.nelements();
// 	    }
// 	    if (header.isDefined(sprefix + "unit")) {
// 		header.get(sprefix + "unit", cunit);
// 		UnitMap::addFITS();
// 		haveUnit = True;
// 	    }

// 	    // PC, CD (PC*CDELT) and/or CROTA is optional. We prefer CD if it is defined.
// 	    // We will use variable "pc" to house the true PC matrix, or the CD matrix

// 	    hasCD = getCDFromHeader(pc, n, header);
// 	    if (hasCD) {
// 		if (header.isDefined(sprefix + "delt")) {
// 		    os << LogIO::NORMAL << "Ignoring meaningless " << sprefix << "delt because CD cards present" << LogIO::POST;
// 		}
// 		cdelt.resize(pc.nrow());
// 		cdelt = 1.0;               // degrees
// 	    } else {

// // Get cdelt  and PC from header

// 		if (header.isDefined(sprefix + "delt")) {
// 		    header.get(sprefix + "delt", cdelt);
// 		}
// 		getPCFromHeader(os, rotationAxis, pc, n, header, sprefix);
// 	    }
// 	} catch (AipsError x) {
// 	    os << LogIO::WARN << "Failed to retrieve *rval, *rpix, *delt, *type "
// 		"from header with error : " << x.getMesg() << endl;
// 	    return False;
// 	}

// // Make some consistency checks.  If naxis is in the header,
// // it is used preferentially.  Pad or discard.

// 	if (Int(ctype.nelements()) != n) {
// 	    Int n2 = ctype.nelements();
// 	    ctype.resize(n,True);
// 	    if (n > n2) {
// 		for (Int i=n2; i<n; i++) {
// 		    ostringstream oss;
// 		    oss << String("Dummy") << (i+1);
// 		    ctype(i) = String(oss);
// 		}
// 		os << LogIO::WARN << "Padding missing ctype values with 'Dummy'" << LogIO::POST;
// 	    } else {
// 		os << LogIO::WARN << "Discarding excess ctype values" << LogIO::POST;
// 	    }
// 	}
// 	if (Int(crval.nelements()) != n) {
// 	    Int n2 = crval.nelements();
// 	    crval.resize(n,True);
// 	    if (n > n2) {
// 		for (Int i=n2; i<n; i++) crval(i) = 0.0;
// 		os << LogIO::WARN << "Padding missing crval values with 0.0" << LogIO::POST;
// 	    } else {
// 		os << LogIO::WARN << "Discarding excess crval values" << LogIO::POST;
// 	    }
// 	}
// 	if (!hasCD && Int(cdelt.nelements()) != n) {
// 	    Int n2 = cdelt.nelements();
// 	    cdelt.resize(n,True);
// 	    if (n > n2) {
// 		for (Int i=n2; i<n; i++) cdelt(i) = 1.0;       // Degrees
// 		os << LogIO::WARN << "Padding missing cdelt values with 1.0deg" << LogIO::POST;
// 	    } else {
// 		os << LogIO::WARN << "Discarding excess cdelt values" << LogIO::POST;
// 	    }
// 	}
// 	if (Int(crpix.nelements()) != n) {
// 	    Int n2 = crpix.nelements();
// 	    crpix.resize(n,True);
// 	    if (n > n2) {
// 		for (Int i=n2; i<n; i++) crpix(i) = 0.0;
// 		os << LogIO::WARN << "Padding missing crpix values with 0.0" << LogIO::POST;
// 	    } else {
// 		os << LogIO::WARN << "Discarding excess crpix values" << LogIO::POST;
// 	    }
// 	}
// //
// 	if (Int(pc.nrow()) != n ||  Int(pc.ncolumn()) != n ||
// 	    (cunit.nelements() > 0 && Int(cunit.nelements()) != n)) {
// 	    os << LogIO::SEVERE << "Inconsistent number of axes in header" << LogIO::POST;
// 	    return False;
// 	}


// // OK, find out what standard axes we have.

// 	Int longAxis=-1, latAxis=-1, stokesAxis=-1, specAxis=-1;
// 	Int i;
// 	for (i=0; i<n; i++) {
// 	    String subRA(ctype(i).at(0,2));
// 	    String subDEC(ctype(i).at(0,3));
// 	    if (subRA==String("RA") || ctype(i).contains("LON") || subRA==String("LL")) {
// 		if (longAxis >= 0) {
// 		    os << LogIO::SEVERE << "More than one longitude axis is "
// 			"present in header!";
// 		    return False;
// 		}
// 		longAxis = i;
// 	    } else if (subDEC==String("DEC") || ctype(i).contains("LAT") || subDEC.contains("MM")) {
// 		if (latAxis >= 0) {
// 		    os << LogIO::SEVERE << "More than one latitude axis is "
// 			"present in header!";
// 		    return False; // we already have a latitude axis!
// 		}
// 		latAxis = i;
// 	    } else if (ctype(i).contains("STOKES")) {
// 		stokesAxis = i;
// 	    } else if (ctype(i).contains("FREQ") ||
// 		       ctype(i).contains("FELO") ||
// 		       ctype(i).contains("VELO")) {
// 		specAxis = i;
// 	    }
// 	}

// // We must have longitude AND latitude

// 	if (longAxis >= 0 && latAxis < 0) {
// 	    os << LogIO::WARN << "We have a longitude axis but no latitude axis - making a Linear axis" << LogIO::POST;
// 	    longAxis = -1;
// 	}
// 	if (latAxis >= 0 && longAxis < 0) {
// 	    os << LogIO::WARN << "We have a latitude axis but no longitude axis - making a Linear axis" << LogIO::POST;
// 	    latAxis = -1;
// 	}


// // Sanity check that PC is only non-diagonal for the longitude and
// // latitude axes.

// 	for (Int j=0; j<n; j++) {
// 	    for (i=0; i<n; i++) {
// 		if (i == j) {
// 		    continue;
// 		} else {
// 		    if (!casa::near(pc(i,j), 0.0)) {
// 			if (rotationAxis < 0 || (i == longAxis && j == latAxis) ||
// 			    (i == latAxis  && j == longAxis)) {
// 			    continue;
// 			} else {
// 			    os << LogIO::WARN << sprefix + "rota may only" <<
// 				" be set for longitude/latitude axes" <<
// 				LogIO::POST;
// 			}
// 		    }
// 		}
// 	    }
// 	}


// // DIRECTION

// 	String proj1, proj2;
// 	Bool isGalactic = False;
// 	Bool failedDirection = False;
// 	if (longAxis >= 0) {
// 	    proj1 = ctype(longAxis);
// 	    proj2 = ctype(latAxis);
// //
// 	    if (proj1.contains("GLON")) isGalactic = True;

// // Get rid of the first 4 characters, e.g., RA--

// 	    const Int l1 = proj1.length();
// 	    const Int l2 = proj2.length();
// 	    proj1 = String(proj1.at(4, l1-4));
// 	    proj2 = String(proj2.at(4, l2-4));

// // Get rid of leading -'s

// 	    proj1.gsub(Regex("^-*"), String(""));
// 	    proj2.gsub(Regex("^-*"), String(""));

// // Get rid of spaces

// 	    proj1.gsub(Regex(" *"), String(""));
// 	    proj2.gsub(String(" "), String(""));
// //
// 	    if (proj1=="" && proj2=="") {

// // We must abandon making a celestial coordinate if there is no
// // projection.  Defaulting to cartesian is the wrong thing to do
// // We must make a Linear Coordinate from it.

// 		os << WHERE << LogIO::WARN <<
// 		    "No projection has been defined so cannot make a Celestial Coordinate\n"
// 		    "from this FITS header.  Will make a LinearCoordinate instead" << LogIO::POST;
// 		longAxis = -1;
// 		latAxis = -1;
// 		failedDirection = True;
// 	    }
// 	}
// //
// 	if (longAxis >= 0) {
// 	    if (proj1 != proj2) {

// // Maybe instead I should switch to CAR, or use the first?

// 		os << LogIO::SEVERE << "Longitude and latitude axes have different"
// 		    " projections (" << proj1 << "!=" << proj2 << ")" << LogIO::POST;
// 		return False;
// 	    }

// // OK, let's make our Direction coordinate and add it to the
// // coordinate system. We'll worry about transposing later. FITS
// // should always be degrees, but if the units are set we'll honor
// // them.

// // First, work out what the projection actually is.
// // Special case NCP - now SIN with  parameters

// //	    Vector<Double> projp;
// 	    Vector<Double> pvi_ma;
// 	    Projection::Type ptype;

// 	    if (proj1 == "NCP") {
// 		os << LogIO::NORMAL << "NCP projection is now SIN projection in"
// 		    " WCS.\nOld FITS readers will not handle this correctly." <<
// 		    LogIO::POST;
// 		ptype = Projection::SIN;
// 		//		projp.resize(2);
// 		pvi_ma.resize(2);

// // According to Greisen and Calabretta

// //		projp(0) = 0.0;
// //		projp(1) = 1.0/tan(crval(latAxis)*C::pi/180.0);
// 		pvi_ma(0) = 0.0;
// 		pvi_ma(1) = 1.0/tan(crval(latAxis)*C::pi/180.0);
// 	    } else {
// 		ptype = Projection::type(proj1);
// 		if (ptype == Projection::N_PROJ) {
// 		    os << LogIO::SEVERE << "Unknown projection: (" << proj1 << ")";
// 		    return False;
// 		}
// 		//if (header.isDefined("projp")) {
// 		//    header.get("projp", projp);
// 		//}
// 		if (header.isDefined("pvi_ma")) {
// 		    header.get("pvi_ma", pvi_ma);
// 		}
// 	    }

// // OK, now try making the projection

// 	    Projection projn;
// 	    try {
// 		// projn = Projection(ptype, projp);
// 		projn = Projection(ptype, pvi_ma);
// 	    } catch (AipsError x) {
// 		os << LogIO::SEVERE << "Error forming projection, maybe the "
// 		    "wrong number of parameters\n(" << x.getMesg() << ")" <<
// 		    LogIO::POST;
// 		return False;
// 	    }


// // Assume the units are degrees unless we are told otherwise

// 	    Double toRadX = C::pi/180.0;
// 	    Double toRadY = toRadX;
// 	    if (cunit.nelements() > 0) {
// 		Unit longu = cunit(longAxis);
// 		Unit latu = cunit(latAxis);
// 		Unit rad = "rad";
// 		if (longu.getValue() != rad.getValue() ||
// 		    latu.getValue() != rad.getValue()) {
// 		    os << LogIO::SEVERE << "Longitude or latitude units are unknwon "
// 			"or incompatible with angle (" << cunit(longAxis) <<
// 			"," << cunit(latAxis) << ")" << LogIO::POST;
// 		}
// 		toRadX = longu.getValue().getFac()/rad.getValue().getFac();
// 		toRadY = latu.getValue().getFac()/rad.getValue().getFac();
// 	    }

// // Fish out LONG/LATPOLE

// 	    Double longPole = 999.0;
// 	    Double latPole = 999.0;
// 	    if (header.isDefined("lonpole")) {
// 		header.get("lonpole", longPole);
// 		longPole *= C::pi / 180.0;
// 	    }
// 	    if (header.isDefined("latpole")) {
// 		header.get("latpole", latPole);
// 		latPole *= C::pi / 180.0;
// 	    }

// // DEFAULT

// 	    MDirection::Types radecsys = MDirection::J2000;
// 	    if (isGalactic) {
// 		radecsys = MDirection::GALACTIC;
// 	    } else {
// 		if (header.isDefined("epoch") &&
// 		    (header.dataType("epoch") == TpDouble ||
// 		     header.dataType("epoch") == TpFloat ||
// 		     header.dataType("epoch") == TpInt)) {
// 		    Double epoch = header.asdouble("epoch");
// 		    if (casa::near(epoch, 1950.0)) {
// 			radecsys = MDirection::B1950;
// 		    } else if (casa::near(epoch, 1979.9)) {
// 			radecsys = MDirection::B1950_VLA;
// 		    } else if (casa::near(epoch, 2000.0)) {
// 			radecsys = MDirection::J2000;
// 		    }
// 		} else if (header.isDefined("equinox") &&
// 			   (header.dataType("equinox") == TpDouble ||
// 			    header.dataType("equinox") == TpDouble ||
// 			    header.dataType("equinox") == TpInt)) {
// 		    Double epoch = header.asdouble("equinox");
// 		    if (casa::near(epoch, 1950.0)) {
// 			radecsys = MDirection::B1950;
// 		    } else if (casa::near(epoch, 1979.9)) {
// 			radecsys = MDirection::B1950_VLA;
// 		    } else if (casa::near(epoch, 2000.0)) {
// 			radecsys = MDirection::J2000;
// 		    }
// 		} else {
// 		    os << LogIO::NORMAL << "Could not find or figure out the "
// 			"equinox from the FITS header, using J2000" << LogIO::POST;
// 		}
// 	    }

// 	    Matrix<Double> dirpc(2,2);
// 	    dirpc(0,0) = pc(longAxis, longAxis);
// 	    dirpc(0,1) = pc(longAxis, latAxis);
// 	    dirpc(1,0) = pc(latAxis, longAxis);
// 	    dirpc(1,1) = pc(latAxis, latAxis);

// // watch for cdelt=0 - its okay if that axis is degenerate
// // and (crpix+offset)=1 and rotationAxis < 0 = i.e. the only
// // pixel on that axis is the reference pixel and there is
// // no rotation specified - then cdelt=1 on that axis.  If that
// // isn't done, that coord. can't be constructed because the
// // PC matrix will be reported as singular since its first
// // multiplied by cdelt before its used and in this case, that
// // doesn't matter since other pixels on that axis are never used.

// 	    if (!hasCD) {
// 		if (casa::near(cdelt(latAxis), 0.0) &&
// 		    casa::near(crpix(latAxis)+offset, 1.0) && rotationAxis < 0) {
// 		    cdelt(latAxis) = 1.0;            // degrees
// 		}
// //
// 		if (casa::near(cdelt(longAxis), 0.0) &&
// 		    casa::near(crpix(longAxis)+offset, 1.0) && rotationAxis < 0) {
// 		    cdelt(longAxis) = 1.0;          // degrees
// 		}
// 	    }
// 	    DirectionCoordinate dir(radecsys,
// 				    projn,
// 				    crval(longAxis)*toRadX, crval(latAxis)*toRadY,
// 				    cdelt(longAxis)*toRadX, cdelt(latAxis)*toRadY,
// 				    dirpc,
// 				    crpix(longAxis), crpix(latAxis),
// 				    longPole, latPole);
// 	    coordsys.addCoordinate(dir);
// 	}

// // STOKES.   shape is used only here as the StokesCoordinate
// // is a bit peculiar, and not really separable from the shap

// 	if (stokesAxis >= 0) {
// 	    if (shape(stokesAxis)>4) {
// 		os << "Stokes axis longer than 4 pixels.  This is not acceptable"
// 		   << LogIO::EXCEPTION;
// 		return False;
// 	    }
// 	    Vector<Int> stokes(shape(stokesAxis));
// //
// 	    for (Int k=0; k<shape(stokesAxis); k++) {

// // crpix is 0-relative

// 		Double tmp = crval(stokesAxis) +
// 		    (k - crpix(stokesAxis))*cdelt(stokesAxis);
// 		if (tmp >= 0) {
// 		    stokes(k) = Int(tmp + 0.01);
// 		} else {
// 		    stokes(k) = Int(tmp - 0.01);
// 		}
// //
// 		if (stokes(k)==0) {
// 		    if (warnStokes) {
// 			os << LogIO::WARN
// 			   << "Detected Stokes coordinate = 0; this is an unoffical" << endl;
// 			os << "Convention for an image containing a beam.  Putting Stokes=Undefined" << endl;
// 			os << "Better would be to write your FITS image with the correct Stokes" << LogIO::POST;
// 		    }
// 		    stokes(k) = Stokes::Undefined;
// 		    stokesFITSValue = 0;
// 		} else if (stokes(k)==5) {
// 		    os << LogIO::SEVERE << "The FITS image Stokes axis has the unofficial percentage polarization value." << endl;
// 		    os << "This is not supported.  Will use fractional polarization instead " << endl;
// 		    os << "You must scale the image by 0.01" << LogIO::POST;
// 		    stokes(k) = Stokes::PFlinear;
// 		} else if (stokes(k)==8) {
// 		    if (warnStokes) {
// 			os << LogIO::SEVERE << "The FITS image Stokes axis has the unofficial spectral index value." << endl;
// 			os << "This is not supported. Putting Stokes=Undefined" << LogIO::POST;
// 		    }
// 		    stokes(k) = Stokes::Undefined;
// 		    stokesFITSValue = 8;
// 		} else if (stokes(k)==9) {
// 		    if (warnStokes) {
// 			os << LogIO::SEVERE << "The Stokes axis has the unofficial optical depth" << endl;
// 			os << "value.  This is not supported. Putting Stokes=Undefined" << LogIO::POST;
// 		    }
// 		    stokes(k) = Stokes::Undefined;
// 		    stokesFITSValue = 9;
// 		} else {
// 		    Stokes::StokesTypes type = Stokes::fromFITSValue(stokes(k));
// 		    if (type == Stokes::Undefined) {
// 			os << LogIO::SEVERE << "A Stokes coordinate of " << stokes(k)
// 			   << " was detected; this is not valid. Putting Stokes=Undefined" << endl;
// 		    }
// 		    stokes(k) = type;
// 		}
// 	    }
// 	    try {
// 		StokesCoordinate sc(stokes);
// 		coordsys.addCoordinate(sc);
// 	    } catch (AipsError x) {
// 		os << LogIO::SEVERE << "Error forming stokes axis : " << x.getMesg() << LogIO::POST;
// 		return False;
// 	    }
// 	}


// // SPECTRAL

// 	if (specAxis >= 0) {

// // Will be overwritten or ignored.

// 	    SpectralCoordinate tmp;
// 	    String error;
// 	    if (SpectralCoordinate::fromFITSOld(tmp, error, header, specAxis,
// 						os)) {
// 		coordsys.addCoordinate(tmp);
// 	    } else {
// 		os << LogIO::WARN << "Cannot convert apparent spectral axis " <<
// 		    ctype(specAxis) << " into a true spectral coordinate (error="
// 		   << error << "). Turning it into a linear axis." << LogIO::POST;
// 		specAxis = -1;
// 	    }
// 	}


// // Remaining axes are LINEAR

// 	uInt nlin = n;
// 	if (longAxis >= 0) {nlin--;}
// 	if (latAxis >= 0) {nlin--;}
// 	if (specAxis >= 0) {nlin--;}
// 	if (stokesAxis >= 0) {nlin--;}
// 	if (nlin > 0) {
// 	    if (nlin > 1) {
// 		os << LogIO::NORMAL <<
// 		    "Assuming no rotation/skew/... in linear axes."
// 		   << LogIO::POST;
// 	    }
// 	    Matrix<Double> linpc(nlin, nlin); linpc = 0; linpc.diagonal() = 1.0;
// 	    Vector<Double> lincrpix(nlin), lincdelt(nlin), lincrval(nlin);
// 	    Vector<String> linctype(nlin), lincunit(nlin);
// 	    Int where_i = 0;
// 	    for (i=0; i<n; i++) {
// 		if (i != longAxis && i != latAxis && i != stokesAxis &&
// 		    i != specAxis) {
// 		    lincrpix(where_i) = crpix(i);
// 		    lincrval(where_i) = crval(i);
// 		    lincdelt(where_i) = cdelt(i);
// 		    linctype(where_i) = ctype(i);
// 		    if (cunit.nelements() > 0) {
// 			lincunit(where_i) = cunit(i);
// 		    } else if (longAxis < 0 &&
// 			       (ctype(i).contains("RA") ||
// 				ctype(i).contains("LON") ||
// 				ctype(i).contains("LL"))) {
// 			lincunit(where_i) = "deg";
// 		    } else if (latAxis < 0 &&
// 			       (ctype(i).contains("DEC") ||
// 				ctype(i).contains("LAT") ||
// 				ctype(i).contains("MM"))) {
// 			lincunit(where_i) = "deg";
// 		    } else if (specAxis < 0 && (ctype(i).contains("FELO") ||
// 						ctype(i).contains("VELO"))) {
// 			lincunit(where_i) = "m/s";
// 		    } else if (specAxis < 0 && ctype(i).contains("FREQ")) {
// 			lincunit(where_i) = "Hz";
// 		    }
// 		    where_i++;
// 		}
// 	    }
// 	    Int where_j = 0;
// 	    for (Int j=0; j<n; j++) {
// 		where_i = 0;
// 		if (j != longAxis && j != latAxis && j != stokesAxis &&
// 		    j != specAxis) {
// 		    for (i=0; i<n; i++) {
// 			if (i != longAxis && i != latAxis && i != stokesAxis &&
// 			    i != specAxis) {
// 			    linpc(where_i, where_j) = pc(i,j);
// 			    where_i++;
// 			}
// 		    }
// 		    where_j++;
// 		}
// 	    }
// //
// 	    for (uInt j=0; j<nlin; j++) {
// 		if (casa::near(lincdelt(j),0.0)) {
// 		    lincdelt(j) = 1.0;
// 		    os << "For the LinearCoordinate, axis " << j+1 << " the increment is zero." << endl;
// 		    os << "I am setting this increment arbitrarily to unity" << LogIO::WARN;
// 		}
// 	    }
// 	    LinearCoordinate lc(linctype, lincunit, lincrval, lincdelt,
// 				linpc, lincrpix);
// 	    coordsys.addCoordinate(lc);
// 	}


// // Now we need to work out the transpose order

// 	Vector<Int> order(n);
// 	Int nspecial = 0;
// 	if (longAxis >= 0) nspecial++;
// 	if (latAxis >= 0) nspecial++;
// 	if (stokesAxis >= 0) nspecial++;
// 	if (specAxis >= 0) nspecial++;

// 	Int linused = 0;
// 	for (i=0; i<n; i++) {
// 	    if (i == longAxis) {
// 		order(i) = 0; // long is always first if it exist
// 	    } else if (i == latAxis) {
// 		order(i) = 1; // lat is always second if it exists
// 	    } else if (i == stokesAxis) {
// 		if (longAxis >= 0) { // stokes is axis 0 if no dir, otherwise 2
// 		    order(i) = 2;
// 		} else {
// 		    order(i) = 0;
// 		}
// 	    } else if (i == specAxis) {
// 		if (longAxis >= 0 && stokesAxis >= 0) {
// 		    order(i) = 3; // stokes and dir
// 		} else if (longAxis >= 0) {
// 		    order(i) = 2; // dir only
// 		} else if (stokesAxis >= 0) {
// 		    order(i) = 1;  // stokes but no dir
// 		} else {
// 		    order(i) = 0; // neither stokes or dir
// 		}
// 	    } else {
// 		order(i) = nspecial + linused;
// 		linused++;
// 	    }
// 	}
// //
// 	coordsys.transpose(order, order);

// // Set the ObsInfo.  Errors are not regarded as fatal to the construction of the
// // CoordinateSystem as the default ObsInfo is viable

// 	ObsInfo oi;
// 	Vector<String> error;
// 	Bool ok = oi.fromFITSOld(error, header);
// 	coordsys.setObsInfo(oi);
// //
// 	if (!ok) {
// 	    os << LogIO::WARN << "The following errors occurred decoding the ObsInfo from FITS" << LogIO::POST;
// 	    for (uInt i=0; i<error.nelements(); i++) {
// 		if (error(i).length() > 0) {
// 		    os << LogIO::WARN << "  " << error(i) << LogIO::POST;
// 		}
// 	    }
// 	}
// //
// 	return True;
//     }





    Bool FITSCoordinateUtil::fromFITSHeader (Int& stokesFITSValue, 
					     CoordinateSystem& cSys,
					     RecordInterface& recHeader,
					     const Vector<String>& header,
					     const IPosition& shape, 
					     uInt which) const

    {
	LogIO os(LogOrigin("FITSCoordinateUtil", "fromFITSHeader", WHERE));
	CoordinateSystem cSysTmp;
	//	os << "Using new WCS-based FITS parser.\n" << LogIO::DEBUGGING;
//
	if (header.nelements()==0) {
	    os << "Header is empty - cannot create CoordinateSystem" << LogIO::WARN;
	    return False;
	}

// Convert header to char* for wcs parser

	int nkeys = header.nelements();
	String all;
	for (int i=0; i<nkeys; i++) {
	    int hsize=header[i].size();
	    if (hsize >= 19 &&       // kludge changes 'RA--SIN ' to 'RA---SIN', etc.
		header[i][0]=='C' && header[i][1]=='T' && header[i][2]=='Y' &&
		header[i][3]=='P' && header[i][4]=='E' &&
		(header[i][5]=='1'|| header[i][5]=='2') &&
		header[i][14]=='-' && header[i][18]==' ') {
		char tmp[hsize];
		strncpy(tmp,header[i].c_str(),hsize+1);
		tmp[18]=tmp[17];tmp[17]=tmp[16];tmp[16]=tmp[15];tmp[15]=tmp[14];
		all = all.append(tmp);
		os << LogIO::WARN
		   << "Header\n"<< header[i] << "\nrewrote as\n" << tmp << LogIO::POST;
	    } else if (hsize >= 19 &&	  // change GLON-FLT to GLON-CAR, etc.
		       header[i][0]=='C' && header[i][1]=='T' && header[i][2]=='Y' &&
		       header[i][3]=='P' && header[i][4]=='E' &&
		       (header[i][5]=='1'|| header[i][5]=='2') &&
		       header[i][15]=='-' && header[i][16]=='F' &&
		       header[i][17]=='L' && header[i][18]=='T') {
		char tmp[hsize];
		strncpy(tmp,header[i].c_str(),hsize+1);
		tmp[16]='C'; tmp[17]='A'; tmp[18]='R';
		all = all.append(tmp);
		os << LogIO::WARN
		   << "Header\n"<< header[i] << "\nrewrote as\n" << tmp << LogIO::POST;
	    } else if (hsize >= 19 &&	  // change 'GLON    ' to 'GLON-CAR', etc.
		       header[i][0]=='C' && header[i][1]=='T' && header[i][2]=='Y' &&
		       header[i][3]=='P' && header[i][4]=='E' &&
		       (header[i][5]=='1'|| header[i][5]=='2') &&
		       header[i][15]==' ' && header[i][16]==' ' &&
		       header[i][17]==' ' && header[i][18]==' ') {
		char tmp[hsize];
		strncpy(tmp,header[i].c_str(),hsize+1);
		tmp[15]='-'; tmp[16]='C'; tmp[17]='A'; tmp[18]='R';
		all = all.append(tmp);
		os << LogIO::WARN
		   << "Header\n"<< header[i] << "\nrewrote as\n" << tmp << LogIO::POST;
	    } else {
		all = all.append(header(i));
	    }
	}
	char* pChar2 = const_cast<char *>(all.chars());
    
// Print cards for debugging
  
	Bool print(False);
	if (print) {
	    cerr << "Header Cards " << endl;
	    for (Int i=0; i<nkeys; i++) {
		uInt pt = i*80;
		char* pChar3 = &pChar2[pt];
		String s(pChar3,80);
		cerr << s << endl;
	    }
	    cerr << endl;
	}
  
// Parse FITS header cards with wcs and remove wcs cards from char header

	::wcsprm* wcsPtr = 0;
	int relax = WCSHDR_all;
	int nrej = 0;
	int nwcs = 0;
	int ctrl = -2;
	int status = wcspih(pChar2, nkeys, relax, ctrl, &nrej, &nwcs, &wcsPtr);
	if (status!=0) {
//      os << LogIO::WARN << "wcs FITS parse error with error " << wcspih_errmsg[status] << LogIO::POST;
	    os << LogIO::SEVERE << "wcs FITS parse error with error code " << status << LogIO::POST;
	    return False;
	}
	if (which >= uInt(nwcs)) {
	    os << LogIO::SEVERE << "Specified Coordinate Representation is out of range - number available is " << nwcs << LogIO::POST;
	    return False;
	}

// Put the rest of the header into a Record for subsequent use

	cardsToRecord (os, recHeader, pChar2);

// Add FITS units to system

	UnitMap::addFITS();

// Set the ObsInfo.  Some of what we need is in the WCS struct (date) and some in
// the FITS Records now.  Remove cards from recHeader as used.

	ObsInfo obsInfo = getObsInfo (os, recHeader, wcsPtr[which]);
	cSysTmp.setObsInfo(obsInfo);
//
// Now fix up wcs internal values for various inconsistencies,errors
// and non-standard FITS formats.  This may invoke  :
//  celfix:   translate AIPS-convention celestial projection types, -NCP and -GLS, set in CTYPEia.
//  spcfix:   translate AIPS-convention spectral types, FREQ-LSR, FELO-HEL, etc., set in CTYPEia.
//  datfix:   recast the older DATE-OBS date format to year-2000 standard
//            form, and derive MJD-OBS from it if not already set.
//  cylfix:   fixes WCS FITS header cards for malformed cylindrical projections 
//            that suffer from the problem described in  Sect. 7.3.4 of Paper I.
//  unitifx:  fixes non-standard units

//
	Vector<String> wcsNames(NWCSFIX);
	wcsNames(DATFIX) = String("datfix");
	wcsNames(UNITFIX) =  String("unitfix");
	wcsNames(CELFIX) = String("celfix");
	wcsNames(SPCFIX) = String("spcfix");
	wcsNames(CYLFIX) =  String("cylfix");
//
	int stat[NWCSFIX];
	ctrl = 7;                                             // Do all unsafe unit corrections
	if (wcsfix(ctrl, shape.storage(), &wcsPtr[which], stat) > 0) {
	    for (int i=0; i<NWCSFIX; i++) {
		int err = stat[i];
		if (err>0) {
		    if (i==DATFIX) {
			os << LogIO::WARN << wcsNames(i) << " incurred the error " << wcsfix_errmsg[err] <<  LogIO::POST;
			os << LogIO::WARN << "this probably isn't fatal so continuing" << LogIO::POST;
		    } else {
			os << LogIO::SEVERE << "The wcs function '"
			   << wcsNames(i) << "' failed with error: "
			   << wcsfix_errmsg[err] <<  LogIO::POST;
//
			status = wcsvfree(&nwcs, &wcsPtr);
			if (status!=0) {
			    String errmsg = "wcs memory deallocation error: ";
			    os << errmsg << LogIO::EXCEPTION;
			}
//
			return False;
		    }
		}
	    }	   
	}	  

// Now fish out the various coordinates from the wcs structure and build the CoordinateSystem

	Vector<Int> dirAxes;
	Vector<Int> linAxes;
	Int longAxis = -1;
	Int latAxis = -1;
	Int specAxis = -1;
	Int stokesAxis = -1;
	const uInt nAxes = wcsPtr[which].naxis;
//
	Bool ok=True;
	ok = addDirectionCoordinate (cSysTmp, dirAxes, wcsPtr[which], os);
	if (!ok) {
	    wcsvfree(&nwcs, &wcsPtr);
	    return False;
	}
	if (dirAxes.nelements()==2) {
	    longAxis = dirAxes[0];
	    latAxis = dirAxes[1];
	}
//
	ok = addStokesCoordinate (cSysTmp, stokesAxis, stokesFITSValue, wcsPtr[which], shape, os);
	if (!ok) {
	    wcsvfree(&nwcs, &wcsPtr);
	    return False;
	}
//
	ok = addSpectralCoordinate (cSysTmp, specAxis, wcsPtr[which], os);
	if (!ok) {
	    wcsvfree(&nwcs, &wcsPtr);
	    return False;
	}
//
	ok = addLinearCoordinate (cSysTmp, linAxes, wcsPtr[which], os);
	if (!ok) {
	    wcsvfree(&nwcs, &wcsPtr);
	    return False;
	}

// Free up wcs memory

	status = wcsvfree(&nwcs, &wcsPtr);
	if (status!=0) {
	    String errmsg = "wcs memory deallocation error: ";
	    os << errmsg << LogIO::EXCEPTION;
	}

// Now we need to work out the transpose order of the CS

	Vector<Int> order(nAxes);
	Int nspecial = 0;                    // Anything other than linear
//
	if (longAxis >=0) nspecial++;
	if (latAxis >=0) nspecial++;
	if (stokesAxis >= 0) nspecial++;
	if (specAxis >= 0) nspecial++;
//
	Int linused = 0;
	for (Int i=0; i<Int(nAxes); i++) {
	    if (i == longAxis) {
		order(i) = 0; // long is always first if it exist
	    } else if (i == latAxis) {
		order(i) = 1; // lat is always second if it exists
	    } else if (i == stokesAxis) {
		if (longAxis >= 0) { // stokes is axis 0 if no dir, otherwise 2
		    order(i) = 2;
		} else {
		    order(i) = 0;
		}
	    } else if (i == specAxis) {
		if (longAxis >= 0 && stokesAxis >= 0) {
		    order(i) = 3; // stokes and dir
		} else if (longAxis >= 0) {
		    order(i) = 2; // dir only
		} else if (stokesAxis >= 0) {
		    order(i) = 1;  // stokes but no dir
		} else {
		    order(i) = 0; // neither stokes or dir
		}
	    } else {
		order(i) = nspecial + linused;
		linused++;
	    }
	}
//
	cSysTmp.transpose(order,order);
//
	cSys = cSysTmp;
	return True;
    }


    Bool FITSCoordinateUtil::addDirectionCoordinate (CoordinateSystem& cSys, 
						     Vector<Int>& dirAxes, 
						     const ::wcsprm& wcs,
						     LogIO& os) const
    {

// Extract wcs structure pertaining to Direction Coordinate

	int alloc = 1;                    // Allocate memory for output structures
	int nsub = 2;
	Block<int> axes(nsub);
	axes[0] = WCSSUB_LONGITUDE;
	axes[1] = WCSSUB_LATITUDE;
//
	::wcsprm wcsDest;
	wcsDest.flag = -1;
	int ierr = wcssub (alloc, &wcs, &nsub, axes.storage(), &wcsDest);
//
	Bool ok = True;
	String errMsg;
	if (ierr!=0) {
	    errMsg = String("wcslib wcssub error: ") + wcssub_errmsg[ierr];
	    os << LogIO::WARN << errMsg << LogIO::POST;
	    ok = False;
	}

// See if we found the Sky

	if (ok && nsub==2) {

// Call wcssset on new struct

	    setWCS (wcsDest);
//
	    dirAxes.resize(2);
	    dirAxes[0] = axes[0] - 1;          // 1 -> 0 rel
	    dirAxes[1] = axes[1] - 1;

// Extract Direction system

	    MDirection::Types dirSystem;
	    if (!directionSystemFromWCS (os, dirSystem, errMsg, wcsDest)) {
		os << LogIO::WARN << errMsg << LogIO::POST;
		ok = False;
	    }

// Try to make DirectionCoordinate and fix up zero increments etc and add to CoordinateSystem

	    if (ok) {
		try {
		    Bool oneRel = True;           // wcs structure from FITS has 1-rel pixel coordinates
		    DirectionCoordinate c(dirSystem, wcsDest, oneRel);
//
		    fixCoordinate (c, os);
		    cSys.addCoordinate(c);
		} catch (AipsError x) {
		    os << LogIO::WARN << x.getMesg() << LogIO::POST;
		    ok = False;
		}
	    }
	}

// Clean up

	wcsfree (&wcsDest);
	return ok;
    }


    Bool FITSCoordinateUtil::addLinearCoordinate (CoordinateSystem& cSys, 
						  Vector<Int>& linAxes, 
						  const ::wcsprm& wcs,
						  LogIO& os) const
    {

// Extract wcs structure pertaining to Linear Coordinate

	int alloc = 1;                    // Allocate memory for output structures
	int nsub = 1;
	Block<int> axes(wcs.naxis);
	axes[0] = -(WCSSUB_LONGITUDE | WCSSUB_LATITUDE | WCSSUB_SPECTRAL | WCSSUB_STOKES);
//
	::wcsprm wcsDest;
	wcsDest.flag = -1;
	int ierr = wcssub (alloc, &wcs, &nsub, axes.storage(), &wcsDest);
//
	Bool ok = True;
	String errMsg;
	if (ierr!=0) {
	    errMsg = String("wcslib wcssub error: ") + wcssub_errmsg[ierr];
	    os << LogIO::WARN << errMsg << LogIO::POST;
	    ok = False;
	}

// See if we found the coordinate

	if (ok && nsub>0) {

// Call wcssset on new struct

	    setWCS (wcsDest);

	    linAxes.resize(nsub);
	    for (int i=0; i<nsub; i++) {
		linAxes[i] = axes[i] - 1;           // 1 -> 0 rel
	    }

// Try to make LinearCoordinate from wcs structure and
// fix up zero increments etc and add to CoordinateSystem

	    if (ok) {
		try {
		    Bool oneRel = True;    // wcs structure from FITS has 1-rel pixel coordinates
		    LinearCoordinate c(wcsDest, oneRel);
//
		    fixCoordinate (c, os);
		    cSys.addCoordinate(c);
		} catch (AipsError x) {
		    os << LogIO::WARN << x.getMesg() << LogIO::POST;
		    ok = False;
		}
	    }
	}

// Clean up

	wcsfree (&wcsDest);
	return ok;
    }


    Bool FITSCoordinateUtil::addStokesCoordinate (CoordinateSystem& cSys, 
						  Int& stokesAxis,  Int& stokesFITSValue,
						  const ::wcsprm& wcs, const IPosition& shape,
						  LogIO& os) const
    { 

// Extract wcs structure pertaining to Stokes Coordinate

	int nsub = 1;
	Block<int> axes(nsub);
	axes[0] = WCSSUB_STOKES;
//
	::wcsprm wcsDest;
	wcsDest.flag = -1;
	int alloc = 1;
	int ierr = wcssub (alloc, &wcs, &nsub, axes.storage(), &wcsDest);
//
	Bool ok = True;
	String errMsg;
	if (ierr!=0) {
	    errMsg = String("wcslib wcssub error: ") + wcssub_errmsg[ierr];
	    os << LogIO::WARN << errMsg << LogIO::POST;
	    ok = False;
	}

// See if we found the axis

	if (ok && nsub==1) {

// Call wcssset on new struct

	    setWCS (wcsDest);

// Try to create StokesCoordinate

	    stokesAxis = axes[0] - 1;              // 1 -> 0 rel
	    Bool warnStokes = stokesFITSValue > 0;
	    stokesFITSValue = -1;
	    Vector<Int> stokes(1); stokes = 1;
	    StokesCoordinate c(stokes);                  // No default constructor
	    String errMsg;
	    if (stokesCoordinateFromWCS (os, c, stokesFITSValue, errMsg, wcsDest, 
					 shape(stokesAxis), warnStokes)) {
		cSys.addCoordinate(c);
	    } else {
		os << LogIO::WARN << errMsg << LogIO::POST;
		ok = False;
	    }
	}

// Clean up

	wcsfree (&wcsDest);
	return ok;
    }

  


    Bool FITSCoordinateUtil::addSpectralCoordinate (CoordinateSystem& cSys, 
						    Int& specAxis,
						    const ::wcsprm& wcs,
						    LogIO& os) const
    {

// Extract wcs structure pertaining to Spectral Coordinate

	int nsub = 1;
	Block<int> axes(nsub);
	axes[0] = WCSSUB_SPECTRAL;
//
	::wcsprm wcsDest;
	wcsDest.flag = -1;
	int alloc = 1;
	int ierr = wcssub (alloc, &wcs, &nsub, axes.storage(), &wcsDest);
//
	Bool ok = True;
	String errMsg;
	if (ierr!=0) {
	    errMsg = String("wcslib wcssub error: ") + wcssub_errmsg[ierr];
	    os << LogIO::WARN << errMsg << LogIO::POST;
	    ok = False;
	}

// See if we found the axis

	if (ok && nsub==1) {

// Call wcssset on new struct

	    setWCS (wcsDest);

// Convert the struct to a frequency base

	    int index=0;
	    char ctype[9];  // Patched by rrusk 25-Oct-2007
	    String cType = wcs.ctype[axes[0]-1];
	    if (cType.contains("FREQ")) strcpy(ctype,"FREQ-???");
	    else if (cType.contains("VELO")) strcpy(ctype, "VELO-???");
	    else if (cType.contains("FELO")) strcpy(ctype, "FELO-???");
	    else {
		os << LogIO::WARN << "Unrecognized frequency type" << LogIO::POST;
		ok = False;
	    }
	    if (ok) {
		int iret = wcssptr (&wcsDest, &index, ctype);
		if (iret !=0) {
		    os << LogIO::WARN << "Failed to convert Spectral coordinate to Frequency, error status = "
		       << iret << LogIO::POST;
		    ok = False;	     
		}
	    }

// Find frequency system

	    MFrequency::Types freqSystem;
	    if (ok) {
		specAxis = axes[0] - 1;                  // 1 -> 0 rel
		if (!frequencySystemFromWCS (os, freqSystem, errMsg, wcsDest)) {
		    os << LogIO::WARN << errMsg << LogIO::POST;
		    ok = False;
		}
	    }

// Try to create SpectralCoordinate and fix up zero 
// increments etc and add to CoordinateSystem

	    if (ok) {
		try {
		    Bool oneRel = True;           // wcs structure from FITS has 1-rel pixel coordinate
		    SpectralCoordinate c(freqSystem, wcsDest, oneRel);
//
		    fixCoordinate (c, os);
		    cSys.addCoordinate(c);
		} catch (AipsError x) {
		    os << LogIO::WARN << x.getMesg() << LogIO::POST;
		    ok = False;
		}     
	    }
	}

// Clean up

	wcsfree (&wcsDest);
	return ok;
    }



    Bool FITSCoordinateUtil::directionSystemFromWCS (LogIO& os, MDirection::Types& type,String& errMsg,
						     const ::wcsprm& wcs) const
    {

// Extract Equinox keyword

//
	Bool eqIsDefined = !undefined(wcs.equinox);
	Double equinox(0.0);
	if (eqIsDefined) equinox = wcs.equinox;
	Bool eqIs1950(False);
	Bool eqIs1950VLA(False);
	Bool eqIs2000(False);
	if (eqIsDefined) {
	    eqIs1950 = casa::near(equinox, 1950.0);
	    eqIs1950VLA = casa::near(equinox, 1979.9);
	    eqIs2000 = casa::near(equinox, 2000.0);
	}

// Extract RADESYS keyword

	Bool sysIsDefined = wcs.radesys[0]!='\0';
	String raDecSys;
	if (sysIsDefined) {
	    String tt(wcs.radesys);
	    Int i1 = tt.index(RXwhite,0);
	    if (i1==-1) i1 = tt.length();
	    raDecSys = String(tt.before(i1));
	}

// Extract CTYPEs (must exist)

	String cTypeLon(wcs.ctype[0]);
	String cTypeLat(wcs.ctype[1]);
	cTypeLon.upcase();
	cTypeLat.upcase();

// See if we have xLON/xLAT pair

	String cLon(cTypeLon.at(0,4));
	String cLat(cTypeLat.at(0,4));
	ostringstream oss2;
	if (cLon=="GLON" && cLat=="GLAT") {

// galactic coordinates

	    type = MDirection::GALACTIC;   
	    return True;
	} else if (cLon=="ELON" && cLat=="ELAT") {

// ecliptic for J2000 equator and equinox
// Paper II suggests to use DATE-OBS or MJD-OBS rather than equinox ?

	    if (!eqIsDefined || (eqIsDefined && eqIs2000)) {
		type = MDirection::ECLIPTIC;   
		return True;
	    } else {
		oss2 << "Equinox " << equinox << " is invalid for Ecliptic Coordinates - must be 2000.0";
		errMsg = String(oss2);
		return False;
	    }
	} else if (cLon=="SLON" && cLat=="SLAT") {      

// supergalactic coordinates

	    type = MDirection::SUPERGAL;
	    return True;
	} else if (cLon=="HLON" && cLat=="HLAT") {
	    errMsg = String("Helioecliptic Coordinates are not supported");
	    return False;
	} else {
	    String cLon2(cTypeLon.at(1,3));
	    String cLat2(cTypeLat.at(1,3));
	    if ( (cLon2=="LON" || cLat2=="LAT") || (cLon2=="LAT" || cLat2=="LON") ) {
		oss2 << cLon << " and " << cLat << " are unsupported LON/LAT types";
		errMsg = String(oss2);
		return False;
	    }
	}

// OK we have dispensed with xLON/xLAT, let's move on to the rest
// Since we have successfully constructed a celestial wcsprm object
// we can assume the CTYPEs are correct

	if (raDecSys==String("ICRS")) {
	    if (!eqIsDefined || eqIs2000) {
		type = MDirection::ICRS;   
		return True;
	    } else {
		oss2 << "Direction system ICRS with equinox " << equinox << " is not supported";
		errMsg = String(oss2);
		return False;
	    }
	} else if (raDecSys==String("FK5")) {                
	    if (!eqIsDefined || eqIs2000) {                  // equinox always Julian for FK5
		type = MDirection::J2000;                     // Needs 
		return True;
	    } else {
		oss2 << "Direction system FK5 with equinox " << equinox << " is not supported";
		errMsg = String(oss2);
		return False;
	    }
	} else if (raDecSys==String("FK4")) {  
	    if (!eqIsDefined || eqIs1950) {                  // equinox always Besellian for FK4
		type = MDirection::B1950;   
		return True;
	    } else if (!eqIsDefined || eqIs1950VLA) {
		type = MDirection::B1950_VLA;
		return True;
	    } else {
		oss2 << "Direction system FK4 with equinox " << equinox << " is not supported";
		errMsg = String(oss2);
		return False;
	    }
	} else if (raDecSys==String("FK4-NO-E")) {
	    if (!eqIsDefined || eqIs1950) {    // equinox always Besellian
		type = MDirection::B1950;   
		return True;
	    } else if (!eqIsDefined || eqIs1950VLA) {
		type = MDirection::B1950_VLA;
		return True;
	    } else {
		oss2 << "Direction system FK4-NO-E with equinox " << equinox << " is not supported";
		errMsg = String(oss2);
		return False;        
	    }
	} else if (raDecSys==String("GAPPT")) {
	    type = MDirection::APP;   
	    errMsg = String("Direction system GAPPT is not supported");
	    return False;
	} else {
	    if (sysIsDefined) {
		oss2 << "Direction system '" << raDecSys << "' is not supported";
		errMsg = String(oss2);
		return False;
	    } else {
		if (eqIsDefined) {                            // No RaDecSys but Equinox available
		    if (equinox>=1984.0) {                     // Paper II
			type = MDirection::J2000;               // FK5
			return True;
		    } else if (casa::near(equinox,1979.9)) {
			type = MDirection::B1950_VLA;
			return True;
		    } else {
			type = MDirection::B1950;               // FK4
			return True;
		    }
		} else {                                      // No RaDecSys or equinox
		    os << LogIO::WARN << "No Direction system is defined - J2000 assumed" << LogIO::POST;
		    type = MDirection::J2000;                  // Defaults to ICRS
		    return True;
		}
	    }
	}
//
	errMsg = String("FITSCoordinateUtil::directionSystemFromWCS - logic error");
	return False;
    }


    Bool FITSCoordinateUtil::frequencySystemFromWCS (LogIO& os, MFrequency::Types& type,String& errMsg,
						     const ::wcsprm& wcs) const
    //
    // After running it through the wcsFixItUp function, I can assume that
    // wcs.specsys will always be filled in and that CTYPE will be adjusted
    // appropriately.
    //
    {
	if (wcs.specsys[0]=='\0') {
	    os << LogIO::WARN << "No frequency system is defined - TopoCentric assumed" << LogIO::POST;
	    type = MFrequency::TOPO;
	    return True;
	}
	String specSys(wcs.specsys);
	specSys.upcase();

// Extract system

	ostringstream oss;
	if (specSys=="TOPOCENT") {
	    type = MFrequency::TOPO;
	    return True;
	} else if (specSys=="GEOCENTR") {
	    type = MFrequency::GEO;
	    return True;
	} else if (specSys=="BARYCENT") {
	    type = MFrequency::BARY;
	    return True;
	} else if (specSys=="HELIOCEN") {
	    type = MFrequency::BARY;
	    os << LogIO::WARN << "The HELIOCENTRIC frequency system is deprecated in FITS - it is assumed BARYCENTIC was meant" << LogIO::POST;
	    return True;
	} else if (specSys=="LSRK") {
	    type = MFrequency::LSRK;
	    return True;
	} else if (specSys=="LSRD") {
	    type = MFrequency::LSRD;
	    return True;
	} else if (specSys=="GALACTOC") {
	    type = MFrequency::GALACTO;
	    return True;
	} else if (specSys=="LOCALGRP") {
	    type = MFrequency::LGROUP;
	    return True;
	} else if (specSys=="CMBDIPOL") {
	    type = MFrequency::CMB;
	    return True;
	} else if (specSys=="SOURCE") {
	    type = MFrequency::REST;
	    return True;
	} else {
	    oss << "Frequency system '" << specSys << "' is not supported";
	    errMsg = String(oss);
	    return False;
	}
//
	errMsg = String("FITSCoordinateUtil::frequencySystemFromWCS - logic error");
	return False;
    }



    Bool FITSCoordinateUtil::stokesCoordinateFromWCS (LogIO& os, StokesCoordinate& coord, 
						      Int& stokesFITSValue, String& errMsg, 
						      const ::wcsprm& wcs, 
						      uInt shape, Bool warnStokes) const
    {

// For the StokesCoordinate, the shape is not separable from the coordinate

	if (shape>4) {
	    os << "The Stokes axis is longer than 4 pixels.  This is not supported" 
	       << LogIO::EXCEPTION;       
	    return False;
	}
//
	if (wcs.naxis != 1) {
	    os << "The wcs structure holding the StokesAxis can only have one axis" << LogIO::EXCEPTION;
	}

// Fish out values

	Double crpix = wcs.crpix[0] - 1.0;            // Make 0-rel
	Double crval = wcs.crval[0];
	Double cdelt = wcs.cdelt[0];
//
	Vector<Int> stokes(shape); 
	for (uInt k=0; k<shape; k++) {
	    Double tmp = crval + (k - crpix)*cdelt;
	    if (tmp >= 0) {
		stokes(k) = Int(tmp + 0.01);
	    } else {
		stokes(k) = Int(tmp - 0.01);
	    }
//
	    if (stokes(k)==0) {
		if (warnStokes) {
		    os << LogIO::WARN 
		       << "Detected Stokes coordinate = 0; this is an unoffical" << endl;
		    os << "Convention for an image containing a beam.  Putting Stokes=Undefined" << endl;
		    os << "Better would be to write your FITS image with the correct Stokes" << LogIO::POST;
		}
//
		stokes(k) = Stokes::Undefined;
		stokesFITSValue = 0;
	    } else if (stokes(k)==5) {           
		os << LogIO::SEVERE << "The FITS image Stokes axis has the unofficial percentage polarization value." << endl;
		os << "This is not supported.  Will use fractional polarization instead " << endl;
		os << "You must scale the image by 0.01" << LogIO::POST;
		stokes(k) = Stokes::PFlinear;
	    } else if (stokes(k)==8) {
		if (warnStokes) {
		    os << LogIO::SEVERE << "The FITS image Stokes axis has the unofficial spectral index value." << endl;
		    os << "This is not supported. Putting Stokes=Undefined" << LogIO::POST;
		}
		stokes(k) = Stokes::Undefined;
		stokesFITSValue = 8;
	    } else if (stokes(k)==9) {
		if (warnStokes) {
		    os << LogIO::SEVERE << "The Stokes axis has the unofficial optical depth" << endl;
		    os << "value.  This is not supported. Putting Stokes=Undefined" << LogIO::POST;
		}
		stokes(k) = Stokes::Undefined;
		stokesFITSValue = 9;
	    } else {
		Stokes::StokesTypes type = Stokes::fromFITSValue(stokes(k));
		if (type == Stokes::Undefined) {
		    os << LogIO::SEVERE << "A Stokes coordinate of " << stokes(k) 
		       << " was detected; this is not valid. Putting Stokes=Undefined" << endl;
		}
		stokes(k) = type;
	    }
	}

// Now make StokesCoordinate

	try {
	    coord = StokesCoordinate(stokes);
	} catch (AipsError x) {
	    errMsg = x.getMesg();
	    return False;
	} 
//
	return True;
    }



    ObsInfo FITSCoordinateUtil::getObsInfo (LogIO& os, RecordInterface& header,
					    const ::wcsprm& wcs) const
    {
	ObsInfo oi;
   
// Observer and Telescope are in the FITS cards record.

	Vector<String> error;
	Bool ok = oi.fromFITS (error, header);

// Now overwrite the date info from the wcs struct

	String timeSysStr("UTC");
	if (header.isDefined("timesys")) {
	    Record subRec = header.asRecord("timesys");
	    timeSysStr = subRec.asString("value");
	}
//
	MEpoch::Types timeSystem;
	ok = MEpoch::getType (timeSystem, timeSysStr);

// The date information is in the WCS structure
// 'mjdobs' takes precedence over 'dateobs'

	Bool mjdIsDefined = !undefined(wcs.mjdobs);
	Bool dateObsDefined = wcs.dateobs[0]!='\0';
	if (mjdIsDefined) {
	    Double mjdObs = wcs.mjdobs;
//
	    MEpoch dateObs(Quantum<Double>(mjdObs,"d"), timeSystem);
	    oi.setObsDate (dateObs);
	} else if (dateObsDefined) {
	    //      String dateObsStr(wcs.dateobs[0]);
	    String dateObsStr(wcs.dateobs);
	    MVTime time; 
	    if (FITSDateUtil::fromFITS(time, timeSystem, dateObsStr, timeSysStr)) {
		oi.setObsDate(MEpoch(time.get(), timeSystem));
	    } else {
		os << LogIO::WARN << "Failed to decode DATE-OBS & TIMESYS keywords - no date set" << LogIO::POST;
	    }
	}

// Remove fields from record

	Vector<String> cards = ObsInfo::keywordNamesFITS();
	for (uInt i=0; i<cards.nelements(); i++) {
	    if (header.isDefined(cards(i))) header.removeField(cards[i]);
	}
//
	return oi;
    }



    Vector<String> FITSCoordinateUtil::cTypeFromDirection(Bool& isNCP, const Projection& proj,
							  const Vector<String>& axisNames,
							  Double refLat, Bool printError) 
    //
    // RefLat in radians
    //
    {  
	LogIO os(LogOrigin("FITSCoordinateUtil", "cTypeFromDirection", WHERE));
	Vector<String> ctype(2);

	isNCP = False;
	for (uInt i=0; i<2; i++) {
	    String name = axisNames(i);
	    while (name.length() < 4) {
		name += "-";
	    }
	    switch(proj.type()) {
		// Zenithal/Azimuthal perspective.
	    case Projection::AZP:
		// Slant zenithal perspective, new
	    case Projection::SZP:
		// Gnomonic.
	    case Projection::TAN: 
		// Stereographic.
	    case Projection::STG: 
		// zenith/azimuthal equidistant.
	    case Projection::ARC: 
		// zenithal/azimuthal polynomial.
	    case Projection::ZPN: 
		// zenithal/azimuthal equal area.
	    case Projection::ZEA: 
		// Airy.
	    case Projection::AIR: 
		// Cylindrical perspective.
	    case Projection::CYP: 
		// Plate carree
	    case Projection::CAR: 
		// Mercator.
	    case Projection::MER: 
		// Cylindrical equal area.
	    case Projection::CEA:
		// Conic perspective.
	    case Projection::COP: 
		// Conic equidistant.
	    case Projection::COD: 
		// Conic equal area.
	    case Projection::COE: 
		// Conic orthomorphic.
	    case Projection::COO: 
		// Bonne.
	    case Projection::BON: 
		// Polyconic.
	    case Projection::PCO: 
		// Sanson-Flamsteed (global sinusoidal).
		// The old GLS projection is now SFL. The 'GLS'
		// string will be converted to 'SFL'
	    case Projection::SFL: 
		// Parabolic.
	    case Projection::PAR: 
		// Hammer-Aitoff.
	    case Projection::AIT: 
		// Mollweide.
	    case Projection::MOL: 
		// COBE quadrilateralized spherical cube.
	    case Projection::CSC: 
		// Quadrilateralized spherical cube.
	    case Projection::QSC:
		// Tangential spherical cube.
	    case Projection::TSC:
		// HEALPix grid, new
	    case Projection::HPX: 
		// Orthographics/synthesis.
	    case Projection::SIN:
		name = name + "-" + proj.name();
		break;       
	    default:
		if (i == 0) {
	       
// Only print the message once for long/lat
         
		    if (printError) {  
			os << LogIO::WARN << proj.name()
			   << " is not known to standard FITS (it is known to WCS)."
			   << LogIO::POST;
		    }
		}
		name = name + "-" + proj.name();
		break;
	    }
	    ctype(i) = name;
	}
	return ctype;
    }



    void FITSCoordinateUtil::setWCS (::wcsprm& wcs) const
    {
	if (int iret = wcsset(&wcs)) {
	    String errmsg = "wcs wcsset_error: ";   
	    errmsg += wcsset_errmsg[iret];
	    throw(AipsError(errmsg));
	}
    }  


    Bool FITSCoordinateUtil::getCDFromHeader(Matrix<Double>& cd, uInt n, const RecordInterface& header) 
    //
    // We have to read the CDj_i cards and ultimately pack them into the 
    // WCS linprm structure in the right order.  
    // The expected order in WCS linprm is 
    //
    //  lin.pc = {CD1_1, CD1_2, CD2_1, CD2_2}  ...
    //
    // You can get this via
    //
    //  pc[2][2] = {{CD1_1, CD1_2},
    //              {CD2_1, CD2_2}}
    //
    // which is to say,
    //
    //  pc[0][0] = CD1_1,
    //  pc[0][1] = CD1_2,
    //  pc[1][0] = CD2_1,
    //  pc[1][1] = CD2_2,
    //
    // for which the storage order is
    //
    //  CD1_1, CD1_2, CD2_1, CD2_2
    //
    // so linprm will be happy if you set 
    //
    //  lin.pc = *pc;
    //
    // This packing and unpacking actually happens in
    // LinearXform::set_linprm and LinearXform::pc
    //
    // as we stuff the CD matrix inro the PC matrix 
    // and set cdelt = 1 deg
    //
    {
	cd.resize(n,n);
	cd = 0.0;
	cd.diagonal() = 1.0;
//
	for (uInt i=0; i<n; i++) {
	    for (uInt j=0; j<n; j++) {
		ostringstream oss;
		oss << "cd" << j+1 << "_" << i+1;
		String field(oss);
		if (header.isDefined(field)) {         
		    header.get(field, cd(i,j));
		} else {
		    cd.resize(0,0);
		    return False;
		}
	    }
	}
	return True;
    }


    void FITSCoordinateUtil::getPCFromHeader(LogIO& os, Int& rotationAxis, 
					     Matrix<Double>& pc, 
					     uInt n, 
					     const RecordInterface& header,
					     const String& sprefix)
    {
	if (header.isDefined("pc")) {

// Unlikely to encounter this, as the current WCS papers
// use the CD rather than PC matrix. The aips++ user binding
// (Image tool) does not allow the WCS definition to be written
// so probably we could remove this

	    if (header.isDefined(sprefix + "rota")) {
		os << "Ignoring redundant " << sprefix << "rota in favour of "
		    "pc matrix." << LogIO::NORMAL << LogIO::POST;
	    }
	    header.get("pc", pc);
	    if (pc.ncolumn() != pc.nrow()) {
		os << "The PC matrix must be square" << LogIO::EXCEPTION;
	    }
	} else if (header.isDefined(sprefix + "rota")) {
	    Vector<Double> crota;
	    header.get(sprefix + "rota", crota);

// Turn crota into PC matrix

	    pc.resize(crota.nelements(), crota.nelements());
	    pc = 0.0;
	    pc.diagonal() = 1.0;

// We can only handle one non-zero angle

	    for (uInt i=0; i<crota.nelements(); i++) {
		if (!casa::near(crota(i), 0.0)) {
		    if (rotationAxis >= 0) {
			os << LogIO::SEVERE << "Can only convert one non-zero"
			    " angle from " << sprefix << 
			    "rota to pc matrix. Using the first." << LogIO::POST;
		    } else {
			rotationAxis = i;
		    }
		}
	    }
//
	    if (rotationAxis >= 0 && pc.nrow() > 1) { // can't rotate 1D!
		if (rotationAxis > 0) {
		    pc(rotationAxis-1,rotationAxis-1) =
			pc(rotationAxis,rotationAxis) = cos(crota(rotationAxis)*C::pi/180.0);
		    pc(rotationAxis-1,rotationAxis)=
			-sin(crota(rotationAxis)*C::pi/180.0);
		    pc(rotationAxis,rotationAxis-1)=
			sin(crota(rotationAxis)*C::pi/180.0);
		} else {
		    os << LogIO::NORMAL << "Unusual to rotate about first"
			" axis." << LogIO::POST;
		    pc(rotationAxis+1,rotationAxis+1) =
			pc(rotationAxis,rotationAxis) = cos(crota(rotationAxis)*C::pi/180.0);

// Assume sign of rotation is correct although its not on the expected axis (AIPS convention)

		    pc(rotationAxis,rotationAxis+1)=-sin(crota(rotationAxis)*C::pi/180.0);
		    pc(rotationAxis+1,rotationAxis)= sin(crota(rotationAxis)*C::pi/180.0);
		}
	    }
	} else {

// Pure diagonal PC matrix

	    pc.resize(n, n);
	    pc = 0.0;
	    pc.diagonal() = 1.0;
	}
    }


    void FITSCoordinateUtil::cardsToRecord (LogIO& os, RecordInterface& rec, char* pHeader) const
    //
    // Convert the fitshdr struct to an aips++ Record for ease of later use
    //
    {

// Specific keywords to be located 

	const uInt nKeyIds = 0;
	::fitskeyid keyids[nKeyIds];

// Parse the header

	int nCards = strlen(pHeader) / 80;
	int nReject;
	::fitskey* keys;
	int status = fitshdr (pHeader, nCards, nKeyIds, keyids, &nReject, &keys);
	if (status != 0) {
	    throw(AipsError("Failed to extract non-coordinate cards from FITS header"));
	}
//
	for (Int i=0; i<nCards; i++) {
	    Record subRec;
//
	    String name(keys[i].keyword);
	    name.downcase();
//
	    int type = abs(keys[i].type);
	    switch (type) {
	    case 0:
	    {
		break;                              // No key value (e.g. HISTORY)
	    }
	    case 1:                                 // Logical
	    {
		Bool value(keys[i].keyvalue.i > 0);
		subRec.define("value", value);
		break;
	    }
	    case 2:                                 // 32-bit Int
	    {
		Int value(keys[i].keyvalue.i);       
		subRec.define("value", value);
		break;
	    }
	    case 3:                                 // 64-bit Int
	    {
		os << LogIO::WARN << "Cannot yet handle 64-bit Ints; dropping card " << name << LogIO::POST;
		break;
	    }
	    case 4:                                 // Very long integer
	    {
		os << LogIO::WARN << "Cannot yet handle very long Ints; dropping card " << name << LogIO::POST;
		break;
	    }
	    case 5:                                 // Floating point
	    {
		Float value(keys[i].keyvalue.f);
		subRec.define("value", value);
		break;
	    }
	    case 6:                                 // Integer and floating complex
	    case 7:
	    {
		Complex value(keys[i].keyvalue.c[0],keys[i].keyvalue.c[1]);
		subRec.define("value", value);
		break;
	    }
	    case 8:                                 // String
	    {
		String value(keys[i].keyvalue.s);
		subRec.define("value", value);
		break;
	    }
	    default:
	    {
		if (keys[i].type < 0) {
		    os << LogIO::WARN <<  "Failed to extract card " << keys[i].keyword << LogIO::POST;
		}
		break;
	    }
	    }

//      subRec.define("value", String("TEST"));

// If we managed to parse the keyword, then deal with Units and comments.
// Units are in inline comment in the form [m/s] (we strip the [])

	    if (subRec.isDefined("value")) {
		String comment(keys[i].comment);
		if (keys[i].ulen>0) {
		    String unit(comment, 1, keys[i].ulen-2);
		    subRec.define("unit", unit);
		} else {
		    subRec.define("comment", comment);
		}

// Define sub record 

		if (rec.isDefined(name)) {
		    os << LogIO::WARN << "Duplicate card '" <<  name << "'in header - only first will be used" << LogIO::POST;
		} else {
		    rec.defineRecord(name, subRec);
		}
	    }
	}
//
	free (keys);
    }


    void FITSCoordinateUtil::fixCoordinate (Coordinate& c, LogIO& os) const
    {
	return;

//
	Vector<Double> cdelt = c.increment();
	Vector<Double> crval = c.referenceValue();
//
	const uInt n = cdelt.nelements();
	Coordinate::Type type = c.type();
	String sType = c.showType();
//
	for (uInt i=0; i<n; i++) {
	    if (casa::near(cdelt(i),0.0)) {
		if (type==Coordinate::DIRECTION) {
		    cdelt[i] = C::pi/180.0;        // 1 deg
		    os << LogIO::WARN << "Zero increment in coordinate of type " << sType << " setting  to 1 deg" << LogIO::POST;
		} else {
		    cdelt[i] = crval[i] * 0.1;
		    os << LogIO::WARN << "Zero increment in coordinate of type " << sType << " setting  to refVal/10" << LogIO::POST;
		}
	    }  
	}
//
	c.setIncrement(cdelt);
    }

} //# NAMESPACE CASA - END

