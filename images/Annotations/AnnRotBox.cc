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


#include <images/Annotations/AnnRotBox.h>

#include <images/Regions/WCPolygon.h>

namespace casa {

AnnRotBox::AnnRotBox(
	const Quantity& xcenter,
	const Quantity& ycenter,
	const Quantity& xwidth,
	const Quantity& ywidth, const Quantity& positionAngle,
	const String& dirRefFrameString,
	const CoordinateSystem& csys,
	const Quantity& beginFreq,
	const Quantity& endFreq,
	const String& freqRefFrameString,
	const String& dopplerString,
	const Quantity& restfreq,
	const Vector<Stokes::StokesTypes> stokes,
	const Bool annotationOnly
) : AnnRegion(
		ROTATED_BOX, dirRefFrameString, csys, beginFreq,
		endFreq, freqRefFrameString, dopplerString, restfreq,
		stokes, annotationOnly
	), _center(Vector<Quantity>(2)), _widths(Vector<Quantity>(2)),
	_positionAngle(positionAngle), _corners(Vector<MDirection>(4)) {
	String preamble(String(__FUNCTION__) + ": ");

	if (! xwidth.isConform("rad") && ! xwidth.isConform("pix")) {
		throw AipsError(
			preamble + "x width unit " + xwidth.getUnit() + " is not an angular unit."
		);
	}
	if (! ywidth.isConform("rad") && ! ywidth.isConform("pix")) {
		throw AipsError(
			preamble + "y width unit " + ywidth.getUnit() + " is not an angular unit."
		);
	}
	_widths[0] = _lengthToAngle(xwidth, _directionAxes[0]);
	_widths[1] = _lengthToAngle(ywidth, _directionAxes[1]);

	_center[0] = xcenter;
	_center[1] = ycenter;
	_checkAndConvertDirections(String(__FUNCTION__), _center);

	_doCorners();
	Vector<Double> xv(4), yv(4);
	for (uInt i=0; i<4; i++) {
		Vector<Double> coords = _corners[i].getAngle("rad").getValue();
		xv[i] = coords[0];
		yv[i] = coords[1];
	}
	Quantum<Vector<Double> > x(xv, "rad");
	Quantum<Vector<Double> > y(yv, "rad");
	WCPolygon box(x, y, _directionAxes, _csys, RegionType::Abs);
	_extend(box);

	ostringstream os;
	os << "rotbox [[" << xcenter << ", " << ycenter << "], ["
		<< xwidth << ", " << ywidth << "], " << positionAngle << "]";
	_stringRep += os.str();
}

void AnnRotBox::_doCorners() {
	Quantity realAngle = Quantity(90, "deg") + _positionAngle;

	Vector<Double> inc = _csys.increment();

	Double xFactor = inc(_directionAxes[0]) > 0 ? 1.0 : -1.0;
	Double yFactor = inc(_directionAxes[1]) > 0 ? 1.0 : -1.0;

	// assumes first direction axis is the longitudinal axis
    Vector<Quantity> xShift(2);
    Vector<Quantity> yShift(2);
    for (uInt i=0; i<2; i++) {
	    Double angleRad = (realAngle + Quantity(i*90, "deg")).getValue("rad");
        xShift[i] = Quantity(0.5*xFactor*cos(angleRad))*_widths[0];
	    yShift[i] = Quantity(0.5*yFactor*sin(angleRad))*_widths[1];
        _corners[i] = _convertedDirections[0];
        _corners[i].shift(xShift[i], yShift[i]);
        _corners[i+2] = _convertedDirections[0];
        _corners[i+2].shift(Quantity(-1)*xShift[i], Quantity(-1)*yShift[i]);
    }
}

Vector<MDirection> AnnRotBox::getCorners() const {
	return _corners;
}

}
