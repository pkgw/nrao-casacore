//# ImageInterface.cc: defines the Image base class non pure virtual stuff
//# Copyright (C) 1996,1997,1998,1999,2000,2001,2003
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
//# $Id: ImageInterface.tcc 20356 2008-06-23 11:37:34Z gervandiepen $


#include <casa/aips.h>

#include <images/Images/ImageProperties.h>

namespace casa { //# NAMESPACE CASA - BEGIN


//    template <class T> ImageProperties::ImageProperties(const ImageInterface<T>& image) :
//            itsCoordinates (image.coordinates()), itsShape(image.shape()) {}

    Int ImageProperties::spectralCoordinateNumber() const {
        // don't do a hasSpectralAxis() check or you will go down an infinite recursion path
        return itsCoordinates.findCoordinate(Coordinate::SPECTRAL);
    }

    Bool ImageProperties::hasSpectralAxis() const {
        uInt spectralCoordNum = spectralCoordinateNumber();
        return (spectralCoordNum >= 0 && spectralCoordNum < itsCoordinates.nCoordinates());
    } 

    Int ImageProperties::spectralAxisNumber() const {
        if (! hasSpectralAxis()) {
            return -1;
        }
        Int specIndex = itsCoordinates.findCoordinate(Coordinate::SPECTRAL);
        return itsCoordinates.pixelAxes(specIndex)[0];
    }    

    uInt ImageProperties::nChannels() const {
        if (! hasSpectralAxis()) {
            return 0;
        }
        return itsShape[spectralAxisNumber()];
    }

    Bool ImageProperties::isChannelNumberValid(const uInt chan) const {
        if (! hasSpectralAxis()) {
            return False;
        }
        return (chan < nChannels());
    }

    Int ImageProperties::polarizationCoordinateNumber() const {
        // don't do hasPolarizationAxis check or you will go down an infinite recursion path :)
        return itsCoordinates.findCoordinate(Coordinate::STOKES);
    }

    Bool ImageProperties::hasPolarizationAxis() const {
        uInt polarizationCoordNum = polarizationCoordinateNumber();
        return (
            polarizationCoordNum >= 0 
            && polarizationCoordNum < itsCoordinates.nCoordinates()
        );
    } 

    Int ImageProperties::polarizationAxisNumber() const {
        if (! hasPolarizationAxis()) {
            return -1;
        }
        return itsCoordinates.pixelAxes(polarizationCoordinateNumber())[0];
    }       

    uInt ImageProperties::nStokes() const {
        if (! hasPolarizationAxis()) {
            return 0;
        }
        return itsShape[polarizationAxisNumber()];
    }

    Int ImageProperties::stokesPixelNumber(const String& stokesString) const {
        if (! hasPolarizationAxis()) {
            return -1;
        }
        Int polCoordNum = polarizationCoordinateNumber();
        StokesCoordinate stokesCoord = itsCoordinates.stokesCoordinate(polCoordNum);
        Int stokesPix;
        stokesCoord.toPixel(stokesPix, Stokes::type(stokesString));
        if (stokesPix < 0 || stokesPix >= nStokes()) {
            return -1;
        }
        return stokesPix;
    }

    Bool ImageProperties::isStokesValid(const String& stokesString) const {
        if (! hasPolarizationAxis()) {
            return False;
        }
        Int stokesPixNum = stokesPixelNumber(stokesString);
        return stokesPixNum >= 0 && stokesPixNum < nStokes(); 
    }

    Int ImageProperties::directionCoordinateNumber() const {
        // don't do a hasDirectionCoordinate() check or you will go down an infinite recursion path
        return itsCoordinates.findCoordinate(Coordinate::DIRECTION);
    }

    Bool ImageProperties::hasDirectionCoordinate() const {
        uInt directionCoordNum = directionCoordinateNumber();
        return (
            directionCoordNum >= 0 
            && directionCoordNum < itsCoordinates.nCoordinates()
        );
    } 

    Vector<Int> ImageProperties::directionAxesNumbers() const {
        if (! hasDirectionCoordinate()) {
            return Vector<Int>(-1, -1);
        }
        return itsCoordinates.pixelAxes(directionCoordinateNumber());
    }    

    Vector<Int> ImageProperties::directionShape() const {
        Vector<Int> dirAxesNums = directionAxesNumbers();
        if (! dirAxesNums[0] < 0) {
            return Vector<Int>(0, 0);
        }
        Vector<Int> dirShape(2);
        dirShape[0] = itsShape[dirAxesNums[0]];
        dirShape[1] = itsShape[dirAxesNums[1]];
        return dirShape;
    }

    Bool ImageProperties::areChannelAndStokesValid(
        String& message, const uInt chan, const String& stokesString
    ) const {
        ostringstream os;
        Bool areValid = True;
        if (! isChannelNumberValid(chan)) {
            os << "Zero-based channel number " << chan << " is too large. There are only "
                << nChannels() << " spectral channels in this image.";
            areValid = False;
        }    
        if (! isStokesValid(stokesString)) {
            if (! areValid) {
                os << " and ";
            }
            os << "Stokes parameter " << stokesString << " is not in image";
            areValid = False;
        }
        if (! areValid) {
            message = os.str();
        }    
        return areValid;
    }
} //# NAMESPACE CASA - END
