//# tFluxStandard3.cc: Test programs for the FluxStandard class 
//#
//# Essentiallly same as tFluxStandard.cc but tests on Perley-Butler 2013
//# since it has different set of the primary flux calibrators than
//# the other standards.
//#
//# Copyright (C) 2010
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

#include <casa/aips.h>
//#include <components/ComponentModels/ComponentType.h>
#include <components/ComponentModels/FluxStandard.h>
//#include <components/ComponentModels/FluxCalcQS.h>
//#include <components/ComponentModels/TwoSidedShape.h>
#include <casa/Arrays/Vector.h>
#include <casa/Exceptions/Error.h>
#include <casa/BasicSL/Constants.h>
//#include <casa/BasicMath/Math.h>
#include <measures/Measures/MFrequency.h>
#include <casa/Quanta/Quantum.h>
#include <casa/Utilities/Assert.h>
#include <casa/BasicSL/String.h>
#include <casa/iostream.h>

#include <casa/namespace.h>
int main() {
  try {
    String fluxScaleName;
    Bool matchedScale=False;

    //leave possibility to expand
    Vector<String> qsScNames(1);          Vector<FluxStandard::FluxScale> qsScEnums(1);
    qsScNames[0] = "Perley-Butler 2013";  qsScEnums[0] = FluxStandard::PERLEY_BUTLER_2013;

    Vector<String> srcNames(2);
    srcNames[0] = "3C196"; // model with 5th order
    srcNames[1] = "3C286";    

    Vector<MFrequency> freqs(2);
    freqs[0] = MFrequency(Quantity(2.0, "GHz"));
    freqs[1] = MFrequency(Quantity(20.0, "GHz"));
    
    // Expected flux densities for qsScNames with srcNames at freqs.
    Vector<Vector<Vector<Float> > > expfds(1);
    for(Int scNum = qsScNames.nelements(); scNum--;){
      expfds[scNum].resize(2);
      for(Int srcInd = srcNames.nelements(); srcInd--;)
        expfds[scNum][srcInd].resize(2);
    }
    //expfds[0][0][0] = 10.5213241;       // Perley-Butler 2013, 3C196, 2.0 GHz
    expfds[0][0][0] = 10.4697005;       // Perley-Butler 2013, 3C196, 2.0 GHz(Oct1st,2012)
    //expfds[0][0][1] = 0.85778257;       // Perley-Butler 2013, 3C196, 20.0 GHz
    expfds[0][0][1] = 0.85275018;       // Perley-Butler 2013, 3C196, 20.0 GHz(Oct1st,2012)
    //expfds[0][1][0] = 12.6024607;       // Perley-Butler 2013, 3C286, 2.0 GHz
    expfds[0][1][0] = 12.5386591;       // Perley-Butler 2013, 3C286, 2.0 GHz(Oct1st,2012)
    //expfds[0][1][1] = 2.74100521;       // Perley-Butler 2013, 3C286, 20.0 GHz
    expfds[0][1][1] = 2.72945558;       // Perley-Butler 2013, 3C286, 20.0 GHz(Oct1st,2012)

    Vector<Double> fluxUsed(4);

    for(Int scNum = qsScNames.nelements(); scNum--;){
      FluxStandard::FluxScale fluxScaleEnum;
      matchedScale = FluxStandard::matchStandard(qsScNames[scNum], fluxScaleEnum,
                                                 fluxScaleName);
      // cout << "matchStandard(" << qsScNames[scNum] << ") = " << fluxScaleEnum << endl;
      // cout << "qsScEnums[scNum] = " << qsScEnums[scNum] << endl;
      AlwaysAssert(fluxScaleEnum == qsScEnums[scNum], AipsError);
      cout << "Passed the matchStandard("
           << qsScNames[scNum] << ") test" << endl;

      FluxStandard fluxStd(fluxScaleEnum);
      Flux<Double> returnFlux, returnFluxErr;
      
      for(Int srcInd = srcNames.nelements(); srcInd--;){
        for(Int freqInd = freqs.nelements(); freqInd--;){
          Bool foundStd = fluxStd.compute(srcNames[srcInd], freqs[freqInd],
                                          returnFlux, returnFluxErr);
          AlwaysAssert(foundStd, AipsError);
          cout << "Passed foundStd for " << qsScNames[scNum]
               << ", " << srcNames[srcInd]
               << ", " << (freqInd ? 20.0 : 2.0) << " GHz." << endl;

          returnFlux.value(fluxUsed); // Read this as fluxUsed = returnFlux.value();
          AlwaysAssert(fabs(fluxUsed[0] - expfds[scNum][srcInd][freqInd]) < 0.001,
                       AipsError);          
          cout << "Passed flux density test for " << qsScNames[scNum]
               << ", " << srcNames[srcInd]
               << ", " << (freqInd ? 20.0 : 2.0) << " GHz." << endl;
        }
      }
    }
  }
  catch (AipsError x) {
    cerr << x.getMesg() << endl;
    cout << "FAIL" << endl;
    return 1;
  }
  catch (...) {
    cerr << "Exception not derived from AipsError" << endl;
    cout << "FAIL" << endl;
    return 2;
  }
  cout << "OK" << endl;
}
