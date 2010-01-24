//# tImageFitter.cc:  test the PagedImage class
//# Copyright (C) 1994,1995,1998,1999,2000,2001,2002
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
//# $Id: $


#include <casa/BasicMath/Math.h>
#include <components/ComponentModels/ComponentList.h>
#include <images/Images/ImageFitter.h>
#include <measures/Measures/MDirection.h>
#include <components/ComponentModels/ComponentShape.h>
#include <images/Images/ImageAnalysis.h>
#include <images/Images/FITSImage.h>
#include <images/Images/ImageMetaData.h>
#include <images/Regions/WCBox.h>
#include <casa/BasicSL/Constants.h>
#include <casa/OS/Directory.h>
#include <casa/namespace.h>

#include <sys/types.h>
#include <unistd.h>


void writeTestString(const String& test) {
    cout << "\n" << "*** " << test << " ***" << endl;
}

void checkImage(
		const String& gotImage, const String& expectedImage,
		const String& differenceImage
	) {
    ImageAnalysis ia;
    ia.open(gotImage);
    String expr = "\"" + gotImage + "\" - \"" + expectedImage + "\"";
    ia.imagecalc(differenceImage, expr, True);
    ia.open(differenceImage);
    Record stats;
    Vector<Int> axes(2);
    axes[0] = 0;
    axes[1] = 1;
    Record region;
    Vector<String> plotstats(0);
    ia.statistics(stats, axes, region, "", plotstats, Vector<Float>(0), Vector<Float>(0));

    Array<Double> minArray = stats.asArrayDouble("min");
    Array<Double> maxArray = stats.asArrayDouble("max");

    vector<Double> min, max;
    minArray.tovector(min);
    maxArray.tovector(max);

    AlwaysAssert(min[0] == 0 && max[0] == 0, AipsError);
}

int main() {
    pid_t pid = getpid();
    ostringstream os;
    os << "tImageFitter_tmp_" << pid;
    String dirName = os.str();
	Directory workdir(dirName);
  	const Double DEGREES_PER_RADIAN = 180/C::pi;
    Double arcsecsPerRadian = DEGREES_PER_RADIAN*3600;
    String test;
    try {
       {
            writeTestString(
                "test fitter using all available image pixels with model with no noise"
            );
            ImageFitter fitter = ImageFitter("gaussian_model.fits", "", "");
            // test to ensure exception is thrown if convergence is checked for before fit is done
            try {
            	fitter.converged();
            	// should never get there
            	AlwaysAssert(false, AipsError);
            }
            catch (AipsError) {
            	// got here, just continue
            }
            ComponentList compList = fitter.fit();
            AlwaysAssert(fitter.converged(), AipsError);
            Vector<Quantity> flux;
            compList.getFlux(flux,0);
            // I stokes flux test
            AlwaysAssert(near(flux(0).getValue(), 60318.5801, 1e-4), AipsError);
            // Q stokes flux test
            AlwaysAssert(flux(1).getValue() == 0, AipsError);
            MDirection direction = compList.getRefDirection(0);
            AlwaysAssert(near(direction.getValue().getLong("rad").getValue(), 0.000213318, 1e-5), AipsError);
            AlwaysAssert(near(direction.getValue().getLat("rad").getValue(), 1.939254e-5, 1e-5), AipsError);

            Vector<Double> parameters = compList.getShape(0)->parameters();

            Double majorAxis = arcsecsPerRadian*parameters(0);
            AlwaysAssert(near(majorAxis, 23.548201, 1e-7), AipsError);

            Double minorAxis = arcsecsPerRadian*parameters(1);
            AlwaysAssert(near(minorAxis, 18.838560, 1e-7), AipsError);

            Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
            AlwaysAssert(near(positionAngle, 120.0, 1e-7), AipsError);
        }
        String noisyImage = "gaussian_model_with_noise.fits";
        {
            writeTestString(
                "test fitter using all available image pixels with model with noise added"
            );
            ImageFitter fitter = ImageFitter(noisyImage, 0, "");
            ComponentList compList = fitter.fit();
            AlwaysAssert(fitter.converged(), AipsError);
            Vector<Quantity> flux;
            compList.getFlux(flux,0);
            // I stokes flux test
            cout << flux(0).getValue() << endl;
            AlwaysAssert(near(flux(0).getValue(),  60291.80, 1e-5), AipsError);
            // Q stokes flux test
            AlwaysAssert(flux(1).getValue() == 0, AipsError);
            MDirection direction = compList.getRefDirection(0);
            AlwaysAssert(nearAbs(direction.getValue().getLong("rad").getValue(),  0.000213379, 1e-5), AipsError);
            AlwaysAssert(nearAbs(direction.getValue().getLat("rad").getValue(), 1.9358247e-5, 1e-5), AipsError);

            Vector<Double> parameters = compList.getShape(0)->parameters();

            Double majorAxis = arcsecsPerRadian*parameters(0);
            AlwaysAssert(near(majorAxis, 23.53002154, 1e-7), AipsError);

            Double minorAxis = arcsecsPerRadian*parameters(1);
            AlwaysAssert(near(minorAxis, 18.86212502, 1e-7), AipsError);

            Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
            AlwaysAssert(nearAbs(positionAngle, 119.881851057, 1e-7), AipsError);
        }
        {
            writeTestString(
                "test fitter using a box region with model with noise added"
            );
            ImageFitter fitter(noisyImage, 0, "130,89,170,129");
            ComponentList compList = fitter.fit();
            AlwaysAssert(fitter.converged(), AipsError);
            Vector<Quantity> flux;
            compList.getFlux(flux,0);
            // I stokes flux test
            AlwaysAssert(near(flux(0).getValue(), 60319.860, 1e-5), AipsError);
            // Q stokes flux test
            AlwaysAssert(flux(1).getValue() == 0, AipsError);
            MDirection direction = compList.getRefDirection(0);
            AlwaysAssert(nearAbs(direction.getValue().getLong("rad").getValue(), 0.000213372, 1e-5), AipsError);
            AlwaysAssert(nearAbs(direction.getValue().getLat("rad").getValue(), 1.9359058e-5, 1e-5), AipsError);

            Vector<Double> parameters = compList.getShape(0)->parameters();

            Double majorAxis = arcsecsPerRadian*parameters(0);
            AlwaysAssert(near(majorAxis, 23.545212, 1e-7), AipsError);

            Double minorAxis = arcsecsPerRadian*parameters(1);
            AlwaysAssert(near(minorAxis, 18.864505, 1e-7), AipsError);

            Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
            AlwaysAssert(nearAbs(positionAngle, 119.81297, 1e-5), AipsError);
        }
        {
        	writeTestString(
        			"test fitter using a region record with model with noise added"
        	);
        	FITSImage noisy(noisyImage);
        	IPosition imShape = noisy.shape();
        	Vector<Double> blc(imShape.nelements());
        	Vector<Double> trc(imShape.nelements());

        	for (uInt i=0; i<imShape.nelements(); i++) {
        		blc[i] = 0;
        		trc[i] = imShape[i] - 1;
        	}

        	Vector<Int> dirNums = ImageMetaData(noisy).directionAxesNumbers();
        	blc[dirNums[0]] = 130;
        	blc[dirNums[1]] = 89;
        	trc[dirNums[0]] = 170;
        	trc[dirNums[1]] = 129;

        	LCBox lcBox(blc, trc, imShape);
        	WCBox wcBox(lcBox, noisy.coordinates());
        	ImageRegion rg(wcBox);
        	Record regionRecord(rg.toRecord(""));
        	ImageFitter fitter(noisyImage, &regionRecord);
        	ComponentList compList = fitter.fit();
            AlwaysAssert(fitter.converged(), AipsError);

        	Vector<Quantity> flux;
        	compList.getFlux(flux,0);
        	// I stokes flux test
        	AlwaysAssert(near(flux(0).getValue(), 60319.8604, 1e-5), AipsError);
        	// Q stokes flux test
        	AlwaysAssert(flux(1).getValue() == 0, AipsError);
        	MDirection direction = compList.getRefDirection(0);
        	AlwaysAssert(nearAbs(direction.getValue().getLong("rad").getValue(), 0.000213372, 1e-5), AipsError);
        	AlwaysAssert(nearAbs(direction.getValue().getLat("rad").getValue(), 1.9359058e-5, 1e-5), AipsError);

        	Vector<Double> parameters = compList.getShape(0)->parameters();

        	Double majorAxis = arcsecsPerRadian*parameters(0);
        	AlwaysAssert(near(majorAxis, 23.545212, 1e-7), AipsError);

        	Double minorAxis = arcsecsPerRadian*parameters(1);
        	AlwaysAssert(near(minorAxis, 18.864505, 1e-7), AipsError);

        	Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
        	AlwaysAssert(near(positionAngle, 119.81297, 1e-5), AipsError);
        }
        {
            // test fitter using an includepix (i=0) and excludepix (i=1) range with model with noise
            for (uInt i=0; i<3; i++) {
                String mask;
                Vector<Float> includepix, excludepix;
                switch (i) {
                    case 0:
                        writeTestString("test using includepix range");
                        includepix.resize(2);
                        includepix(0) = 40;
                        includepix(1) = 121;
                        mask = "";
                        break;
                    case 1:
                        writeTestString("test using excludepix range");
                        includepix.resize(0);
                        excludepix.resize(2);
                        excludepix(0) = -10;
                        excludepix(1) = 40;
                        mask = "";
                        break;
                    case 2:
                        includepix.resize(0);
                        excludepix.resize(0);
                        mask = "\"" + noisyImage + "\">40";
                        writeTestString("test using LEL mask " + mask);
                        break;
                }
                ImageFitter fitter(
                	noisyImage, "", "", 0, "I", mask, includepix, excludepix
                );
                ComponentList compList = fitter.fit();
                AlwaysAssert(fitter.converged(), AipsError);
                Vector<Quantity> flux;
                compList.getFlux(flux,0);
                // I stokes flux test
                cout << "flux " << flux(0).getValue() << endl;
                AlwaysAssert(near(flux(0).getValue(), 60354.3232, 1e-5), AipsError);
                // Q stokes flux test
                AlwaysAssert(flux(1).getValue() == 0, AipsError);
                MDirection direction = compList.getRefDirection(0);
                AlwaysAssert(near(direction.getValue().getLong("rad").getValue(), 0.000213391, 1e-5), AipsError);
                AlwaysAssert(near(direction.getValue().getLat("rad").getValue(), 1.93449e-05, 1e-5), AipsError);

                Vector<Double> parameters = compList.getShape(0)->parameters();

                Double majorAxis = arcsecsPerRadian*parameters(0);
                AlwaysAssert(near(majorAxis, 23.541712, 1e-7), AipsError);

                Double minorAxis = arcsecsPerRadian*parameters(1);
                AlwaysAssert(near(minorAxis, 18.882029, 1e-7), AipsError);

                Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
                AlwaysAssert(near(positionAngle, 119.769648, 1e-7), AipsError);
            }
        }
        {
            writeTestString("test writing of residual and mdoel images");

            workdir.create();
            String residImage = dirName + "/residualImage";
            String modelImage = dirName + "/modelImage";
            String residDiff = dirName + "/residualImage.diff";
            String modelDiff = dirName + "/modelImage.diff";
            ImageFitter fitter(
            	noisyImage, "", "100,100,200,200", 0, "I", "",
            	Vector<Float>(0), Vector<Float>(0), residImage,
            	modelImage
            );
            fitter.fit();
            AlwaysAssert(fitter.converged(), AipsError);
            writeTestString("test residual image correctness");
            checkImage(
            	residImage, "gaussian_model_with_noise_resid.fits",
            	residDiff
            );
            writeTestString("test model image correctness");
            checkImage(
             	modelImage, "gaussian_model_with_noise_model.fits",
             	modelDiff
            );
            workdir.removeRecursive();
        }
        String convolvedModel = "gaussian_convolved.fits";
        {
        	writeTestString("test fitting model gaussian that has been convolved with a beam");
        	ImageFitter fitter(convolvedModel, "", "");
        	ComponentList compList = fitter.fit();
            AlwaysAssert(fitter.converged(), AipsError);
            Vector<Quantity> flux;

        	compList.getFlux(flux,0);
        	// I stokes flux test
        	AlwaysAssert(near(flux(0).getValue(), 60318.6, 1e-5), AipsError);
        	// Q stokes flux test
        	AlwaysAssert(flux(1).getValue() == 0, AipsError);
        	MDirection direction = compList.getRefDirection(0);
        	AlwaysAssert(near(direction.getValue().getLong("rad").getValue(), 0.000213318, 1e-5), AipsError);
        	AlwaysAssert(near(direction.getValue().getLat("rad").getValue(), 1.939254e-5, 1e-5), AipsError);

        	Vector<Double> parameters = compList.getShape(0)->parameters();

        	Double majorAxis = arcsecsPerRadian*parameters(0);
        	AlwaysAssert(near(majorAxis, 26.50461508, 1e-7), AipsError);

        	Double minorAxis = arcsecsPerRadian*parameters(1);
        	AlwaysAssert(near(minorAxis, 23.99821851, 1e-7), AipsError);

        	Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
        	AlwaysAssert(near(positionAngle, 126.3211060, 1e-7), AipsError);
        }
        {
        	writeTestString(
        		String("test fitting model gaussian that has been convolved with a beam and fix ")
        		+ String("the peak intensity to be artificially low")
        	);
            ImageFitter fitter(
            		convolvedModel, "", "", 0, "I", "",
             	Vector<Float>(0), Vector<Float>(0), "",
             	"", "estimates_convolved.txt"
            );
        	ComponentList compList = fitter.fit();
            AlwaysAssert(fitter.converged(), AipsError);
            Vector<Quantity> flux;

        	compList.getFlux(flux,0);
        	// I stokes flux test
        	AlwaysAssert(near(flux(0).getValue(), 60082.6, 1e-5), AipsError);
        	// Q stokes flux test
        	AlwaysAssert(flux(1).getValue() == 0, AipsError);
        	MDirection direction = compList.getRefDirection(0);
        	AlwaysAssert(nearAbs(direction.getValue().getLong("rad").getValue(), 0.000213318, 1e-5), AipsError);
        	AlwaysAssert(nearAbs(direction.getValue().getLat("rad").getValue(), 1.939254e-5, 1e-5), AipsError);

        	Vector<Double> parameters = compList.getShape(0)->parameters();

        	Double majorAxis = arcsecsPerRadian*parameters(0);
        	AlwaysAssert(near(majorAxis, 28.21859344, 1e-7), AipsError);

        	Double minorAxis = arcsecsPerRadian*parameters(1);

        	AlwaysAssert(near(minorAxis, 25.55011520, 1e-7), AipsError);

        	Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
        	AlwaysAssert(nearAbs(positionAngle, 126.3211050, 1e-7), AipsError);
        }
        {
         	writeTestString("Fit two gaussians");
            ImageFitter fitter(
             		"two_gaussian_model.fits", "", "", 0, "I", "",
              	Vector<Float>(0), Vector<Float>(0), "",
              	"", "estimates_2gauss.txt"
            );
         	ComponentList compList = fitter.fit();
            AlwaysAssert(fitter.converged(), AipsError);
            Vector<Quantity> flux;
            MDirection direction;
            Vector<Double> parameters;
            AlwaysAssert(compList.nelements() == 2, AipsError);
            Vector<Double> expectedFlux(2);
            expectedFlux[0] = 60318.5820312;
            expectedFlux[1] = 112174.6953125;
            Vector<Double> expectedLong(2);
            expectedLong[0] = 2.1331802e-04;
            expectedLong[1] = -2.2301344e-04;
            Vector<Double> expectedLat(2);
            expectedLat[0] = 1.9392547e-05;
            expectedLat[1] = 4.5572321e-04;
            Vector<Double> expectedMajorAxis(2);
            expectedMajorAxis[0] = 23.548201;
            expectedMajorAxis[1] = 46.582182;
            Vector<Double> expectedMinorAxis(2);
            expectedMinorAxis[0] = 18.838561;
            expectedMinorAxis[1] = 23.613296;
            Vector<Double> expectedPositionAngle(2);
            expectedPositionAngle[0] = 120.0;
            expectedPositionAngle[1] = 140.07385;

            for (uInt i = 0; i < compList.nelements(); i++) {
            	compList.getFlux(flux,i);
            	// I stokes flux test
            	AlwaysAssert(near(flux(0).getValue(), expectedFlux[i], 1e-7), AipsError);
            	// Q stokes flux test
            	AlwaysAssert(flux(1).getValue() == 0, AipsError);
            	direction = compList.getRefDirection(i);

            	AlwaysAssert(nearAbs(direction.getValue().getLong("rad").getValue(), expectedLong[i], 1e-7), AipsError);
            	AlwaysAssert(nearAbs(direction.getValue().getLat("rad").getValue(), expectedLat[i], 1e-7), AipsError);
             	Vector<Double> parameters = compList.getShape(i)->parameters();
            	Double majorAxis = arcsecsPerRadian*parameters(0);
             	AlwaysAssert(near(majorAxis, expectedMajorAxis[i], 1e-7), AipsError);

             	Double minorAxis = arcsecsPerRadian*parameters(1);
             	AlwaysAssert(near(minorAxis, expectedMinorAxis[i], 1e-7), AipsError);

             	Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
             	AlwaysAssert(nearAbs(positionAngle, expectedPositionAngle[i], 5e-6), AipsError);
            }
        }
        {
        	writeTestString("Test of nonconvergence");
            ImageFitter fitter(noisyImage, "", "0,0,20,20");
            fitter.fit();
            AlwaysAssert(! fitter.converged(), AipsError);
        }
        {
        	writeTestString("Test of fitting in a multi-polarization image");
        	Vector<String> stokes(4);
        	stokes[0] = "I";
        	stokes[1] = "Q";
        	stokes[2] = "U";
        	stokes[3] = "V";

        	Vector<Double> expectedFlux(stokes.size());
        	expectedFlux[0] = 133.60641;
        	expectedFlux[1] = 400.81921;
        	expectedFlux[2] = 375.76801;
        	expectedFlux[3] = -1157.92212;

        	Vector<Double> expectedRA(stokes.size());
        	expectedRA[0] = 1.2479113396;
        	expectedRA[1] = 1.2479113694;
        	expectedRA[2] = 1.2478908580;
        	expectedRA[3] = 1.2478908284;

        	Vector<Double> expectedDec(stokes.size());
        	expectedDec[0] = 0.782579122;
        	expectedDec[1] = 0.782593666;
        	expectedDec[2] = 0.782593687;
        	expectedDec[3] = 0.782579143;

            Vector<Double> expectedMajorAxis(stokes.size());
            expectedMajorAxis[0] = 7.992524398;
            expectedMajorAxis[1] = 11.988806751;
            expectedMajorAxis[2] = 8.991589959;
            expectedMajorAxis[3] = 12.987878913;

            Vector<Double> expectedMinorAxis(stokes.size());
            expectedMinorAxis[0] = 5.994405977;
            expectedMinorAxis[1] = 5.994395540;
            expectedMinorAxis[2] = 4.995338093;
            expectedMinorAxis[3] = 7.992524265;

            Vector<Double> expectedPositionAngle(stokes.size());
            expectedPositionAngle[0] = 40.083248;
            expectedPositionAngle[1] = 160.083213;
            expectedPositionAngle[2] = 50.082442;
            expectedPositionAngle[3] = 135.08243;

        	for (uInt i=0; i<stokes.size(); i++) {
        		ImageFitter fitter("imfit_stokes.fits", "", "", 0, stokes[i]);
        		ComponentList compList = fitter.fit();
        		AlwaysAssert(fitter.converged(), AipsError);
        		Vector<Quantity> flux;
        		MDirection direction = compList.getRefDirection(0);
        		compList.getFlux(flux,0);
        		AlwaysAssert(compList.nelements() == 1, AipsError);
        		AlwaysAssert(near(flux(i).getValue(), expectedFlux[i], 1e-5), AipsError);
        		AlwaysAssert(nearAbs(direction.getValue().getLong("rad").getValue(), expectedRA[i], 1e-8), AipsError);
        		AlwaysAssert(nearAbs(direction.getValue().getLat("rad").getValue(), expectedDec[i], 1e-8), AipsError);
                Vector<Double> parameters = compList.getShape(0)->parameters();
                Double majorAxis = arcsecsPerRadian*parameters(0);
                AlwaysAssert(near(majorAxis, expectedMajorAxis[i], 1e-7), AipsError);
                Double minorAxis = arcsecsPerRadian*parameters(1);
                AlwaysAssert(near(minorAxis, expectedMinorAxis[i], 1e-7), AipsError);
             	Double positionAngle = DEGREES_PER_RADIAN*parameters(2);
             	AlwaysAssert(nearAbs(positionAngle, expectedPositionAngle[i], 5e-6), AipsError);
        	}
        }
        cout << "ok" << endl;
    }
    catch (AipsError x) {
    	if(workdir.exists()) {
    		workdir.removeRecursive();
    	}
        cerr << "Exception caught: " << x.getMesg() << endl;
        return 1;
    } 
    return 0;
}
