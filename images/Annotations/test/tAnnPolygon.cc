//# tWCLELMask.cc:  mechanical test of the WCLELMask class
//# Copyright (C) 2000,2001,2003
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
//# $Id: tWCLELMask.cc 20567 2009-04-09 23:12:39Z gervandiepen $

#include <casa/aips.h>
#include <images/Annotations/AnnPolygon.h>

#include <coordinates/Coordinates/CoordinateUtil.h>
#include <coordinates/Coordinates/DirectionCoordinate.h>
#include <coordinates/Coordinates/SpectralCoordinate.h>

#include <casa/namespace.h>


int main () {
	try {
		CoordinateSystem csys = CoordinateUtil::defaultCoords4D();
		AnnRegion::unitInit();
		LogIO log;

		{
			log << LogIO::NORMAL
				<< "Test mixed world and pixel coordinates throws exception"
				<< LogIO::POST;
			Bool thrown = True;
			Vector<Quantity> x(3);
			Vector<Quantity> y(3);

			x[0] = Quantity(0.01, "deg");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0, "deg");
			y[1] = Quantity(0.01, "pix");
			x[2] = Quantity(0, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq, endFreq;
			String dirTypeString = MDirection::showType(
				csys.directionCoordinate().directionType(False)
			);
			String freqRefFrameString = MFrequency::showType(
				csys.spectralCoordinate().frequencySystem()
			);
			String dopplerString = MDoppler::showType(
				csys.spectralCoordinate().velocityDoppler()
			);
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			try {
				AnnPolygon poly(
					x, y, dirTypeString,
					csys, beginFreq, endFreq, freqRefFrameString,
					dopplerString, restfreq, stokes, False
				);
				thrown = False;
			} catch (AipsError x) {
				log << LogIO::NORMAL
					<< "Exception thrown as expected: "
					<< x.getMesg() << LogIO::POST;
			}
			AlwaysAssert(thrown, AipsError);
		}
		{
			log << LogIO::NORMAL
				<< "Test bad quantity for world direction coordinate throws exception"
				<< LogIO::POST;
			Bool thrown = True;
			Vector<Quantity> x(3);
			Vector<Quantity> y(3);
			x[0] = Quantity(0.01, "km/s");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0, "deg");
			y[1] = Quantity(0.01, "deg");
			x[2] = Quantity(0, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq, endFreq;
			String dirTypeString = MDirection::showType(
				csys.directionCoordinate().directionType(False)
			);
			String freqRefFrameString = MFrequency::showType(
				csys.spectralCoordinate().frequencySystem()
			);
			String dopplerString = MDoppler::showType(
				csys.spectralCoordinate().velocityDoppler()
			);
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			try {
				AnnPolygon poly(
					x, y, dirTypeString,
					csys, beginFreq, endFreq, freqRefFrameString,
					dopplerString, restfreq, stokes, False
				);
				thrown = False;
			} catch (AipsError x) {
				log << LogIO::NORMAL
					<< "Exception thrown as expected: "
					<< x.getMesg() << LogIO::POST;
			}
			AlwaysAssert(thrown, AipsError);

		}
		{
			log << LogIO::NORMAL << "Verify corners"
				<< LogIO::POST;
			Vector<Quantity> x(3);
			Vector<Quantity> y(3);
			x[0] = Quantity(0.05, "deg");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0.015, "deg");
			y[1] = Quantity(0.01, "deg");
			x[2] = Quantity(0.015, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq, endFreq;
			String dirTypeString = MDirection::showType(
				csys.directionCoordinate().directionType(False)
			);
			String freqRefFrameString = MFrequency::showType(
				csys.spectralCoordinate().frequencySystem()
			);
			String dopplerString = MDoppler::showType(
				csys.spectralCoordinate().velocityDoppler()
			);
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			AnnPolygon poly(
				x, y, dirTypeString,
				csys, beginFreq, endFreq, freqRefFrameString,
				dopplerString, restfreq, stokes, False
			);

			Vector<MDirection> corners = poly.getCorners();
			AlwaysAssert(corners.size() == 3, AipsError);

			AlwaysAssert(
				near(
					corners[0].getAngle("deg").getValue("deg")[0],
					x[0].getValue("deg")
				), AipsError
			);
			AlwaysAssert(
				near(
					corners[0].getAngle("deg").getValue("deg")[1],
					y[0].getValue("deg")
				), AipsError
			);
			AlwaysAssert(
				near(
					corners[1].getAngle("deg").getValue("deg")[0],
					x[1].getValue("deg")
				), AipsError
			);
			AlwaysAssert(
				near(
					corners[1].getAngle("deg").getValue("deg")[1],
					y[1].getValue("deg")
				), AipsError
			);
			cout << poly.getCorners() << endl;
		}
		{
			log << LogIO::NORMAL
				<< "Test precessing from B1950 to J2000"
				<< LogIO::POST;

			Vector<Quantity> x(3);
			Vector<Quantity> y(3);
			x[0] = Quantity(0.05, "deg");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0.015, "deg");
			y[1] = Quantity(0.01, "deg");
			x[2] = Quantity(0.015, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq, endFreq;
			String dirTypeString = "B1950";
			String freqRefFrameString = MFrequency::showType(
				csys.spectralCoordinate().frequencySystem()
			);
			String dopplerString = MDoppler::showType(
				csys.spectralCoordinate().velocityDoppler()
			);
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			AnnPolygon poly(
				x, y, dirTypeString,
				csys, beginFreq, endFreq, freqRefFrameString,
				dopplerString, restfreq, stokes, False
			);
			Vector<MDirection> corners = poly.getCorners();
			AlwaysAssert(corners.size() == 3, AipsError);
			AlwaysAssert(
				near(
					corners[0].getAngle("rad").getValue("rad")[0],
					0.012055422536187882
				), AipsError
			);
			AlwaysAssert(
				near(
					corners[0].getAngle("rad").getValue("rad")[1],
					0.00485808148440817
				), AipsError
			);
			AlwaysAssert(
				near(
					corners[1].getAngle("rad").getValue("rad")[0],
					0.011444556041464694
				), AipsError
			);
			AlwaysAssert(
				near(
					corners[1].getAngle("rad").getValue("rad")[1],
					0.0050326323941514792
				), AipsError
			);
			cout << poly.getCorners() << endl;
		}

		{
			log << LogIO::NORMAL
				<< "Test unmodified frequencies"
				<< LogIO::POST;

			Vector<Quantity> x(3);
			Vector<Quantity> y(3);
			x[0] = Quantity(0.05, "deg");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0.015, "deg");
			y[1] = Quantity(0.01, "deg");
			x[2] = Quantity(0.015, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq(1415, "MHz");
			Quantity endFreq(1450e6, "Hz");

			String dirTypeString = MDirection::showType(
				csys.directionCoordinate().directionType(False)
			);
			String freqRefFrameString = MFrequency::showType(
				csys.spectralCoordinate().frequencySystem()
			);
			String dopplerString = MDoppler::showType(
				csys.spectralCoordinate().velocityDoppler()
			);
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			AnnPolygon poly(
				x, y, dirTypeString,
				csys, beginFreq, endFreq, freqRefFrameString,
				dopplerString, restfreq, stokes, False
			);

			Vector<MFrequency> freqs = poly.getFrequencyLimits();
			AlwaysAssert(
				near(freqs[0].get("Hz").getValue(), beginFreq.getValue("Hz")),
				AipsError
			);
			AlwaysAssert(
				near(freqs[1].get("Hz").getValue(), endFreq.getValue("Hz")),
				AipsError
			);
		}

		{
			log << LogIO::NORMAL
				<< "Test frequencies GALACTO -> LSRK"
				<< LogIO::POST;

			Vector<Quantity> x(3);
			Vector<Quantity> y(3);
			x[0] = Quantity(0.05, "deg");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0.015, "deg");
			y[1] = Quantity(0.01, "deg");
			x[2] = Quantity(0.015, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq(1415, "MHz");
			Quantity endFreq(1450e6, "Hz");

			String dirTypeString = MDirection::showType(
				csys.directionCoordinate().directionType(False)
			);
			String freqRefFrameString = "GALACTO";
			String dopplerString = MDoppler::showType(
				csys.spectralCoordinate().velocityDoppler()
			);
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			AnnPolygon poly(
				x, y, dirTypeString,
				csys, beginFreq, endFreq, freqRefFrameString,
				dopplerString, restfreq, stokes, False
			);

			Vector<MFrequency> freqs = poly.getFrequencyLimits();
			AlwaysAssert(
				near(freqs[0].get("Hz").getValue(), 1415508785.4853702),
				AipsError
			);
			AlwaysAssert(
				near(freqs[1].get("Hz").getValue(), 1450521370.2853618),
				AipsError
			);
		}

		{
			log << LogIO::NORMAL
				<< "Test unmodified frequencies when specifying relativistic velocities"
				<< LogIO::POST;

			Vector<Quantity> x(3);
			Vector<Quantity> y(3);
			x[0] = Quantity(0.05, "deg");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0.015, "deg");
			y[1] = Quantity(0.01, "deg");
			x[2] = Quantity(0.015, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq(-250000, "km/s");
			Quantity endFreq(250000000, "m/s");

			String dirTypeString = MDirection::showType(
				csys.directionCoordinate().directionType(False)
			);
			String freqRefFrameString = MFrequency::showType(
				csys.spectralCoordinate().frequencySystem()
			);
			String dopplerString = MDoppler::showType(
				csys.spectralCoordinate().velocityDoppler()
			);
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			AnnPolygon poly(
				x, y, dirTypeString,
				csys, beginFreq, endFreq, freqRefFrameString,
				dopplerString, restfreq, stokes, False
			);

			Vector<MFrequency> freqs = poly.getFrequencyLimits();
			AlwaysAssert(
				near(freqs[0].get("Hz").getValue(), 2604896650.3078709),
				AipsError
			);
			AlwaysAssert(
				near(freqs[1].get("Hz").getValue(), 235914853.26413003),
				AipsError
			);
		}

		{
			log << LogIO::NORMAL
				<< "Test unmodified frequencies when specifying velocities"
				<< LogIO::POST;

			Vector<Quantity> x(3);
			Vector<Quantity> y(3);
			x[0] = Quantity(0.05, "deg");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0.015, "deg");
			y[1] = Quantity(0.01, "deg");
			x[2] = Quantity(0.015, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq(-20, "km/s");
			Quantity endFreq(20000, "m/s");

			String dirTypeString = MDirection::showType(
				csys.directionCoordinate().directionType(False)
			);
			String freqRefFrameString = MFrequency::showType(
				csys.spectralCoordinate().frequencySystem()
			);
			String dopplerString = MDoppler::showType(
				csys.spectralCoordinate().velocityDoppler()
			);
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			AnnPolygon poly(
				x, y, dirTypeString,
				csys, beginFreq, endFreq, freqRefFrameString,
				dopplerString, restfreq, stokes, False
			);

			Vector<MFrequency> freqs = poly.getFrequencyLimits();
			AlwaysAssert(
				near(freqs[0].get("Hz").getValue(), 1420500511.0578821),
				AipsError
			);
			AlwaysAssert(
				near(freqs[1].get("Hz").getValue(), 1420310992.5141187),
				AipsError
			);
		}

		{
			log << LogIO::NORMAL
				<< "Test modified doppler definitions"
				<< LogIO::POST;
			Vector<Quantity> x(3);
			Vector<Quantity> y(3);
			x[0] = Quantity(0.05, "deg");
			y[0] = Quantity(0, "deg");
			x[1] = Quantity(0.015, "deg");
			y[1] = Quantity(0.01, "deg");
			x[2] = Quantity(0.015, "deg");
			y[2] = Quantity(0, "deg");

			Quantity beginFreq(2013432.1736247784, "m/s");
			Quantity endFreq(-1986.7458583077, "km/s");

			String dirTypeString = MDirection::showType(
				csys.directionCoordinate().directionType(False)
			);
			String freqRefFrameString = MFrequency::showType(
				csys.spectralCoordinate().frequencySystem()
			);
			String dopplerString = "OPTICAL";
			Quantity restfreq(
				csys.spectralCoordinate().restFrequency(), "Hz"
			);
			Vector<Stokes::StokesTypes> stokes(0);
			AnnPolygon poly(
				x, y, dirTypeString,
				csys, beginFreq, endFreq, freqRefFrameString,
				dopplerString, restfreq, stokes, False
			);
			cout << poly << endl;

			Vector<MFrequency> freqs = poly.getFrequencyLimits();
			cout << freqs[0].get("Hz").getValue() << endl;
			AlwaysAssert(
				near(freqs[0].get("Hz").getValue(), 1410929824.5978253),
				AipsError
			);
			AlwaysAssert(
				near(freqs[1].get("Hz").getValue(), 1429881678.974175),
				AipsError
			);
		}

	} catch (AipsError x) {
		cerr << "Caught exception: " << x.getMesg() << endl;
		return 1;
	}

	cout << "OK" << endl;
	return 0;
}
