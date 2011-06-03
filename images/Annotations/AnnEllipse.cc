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


#include <images/Annotations/AnnEllipse.h>

#include <images/Regions/WCEllipsoid.h>

namespace casa {

AnnEllipse::AnnEllipse(
	const Quantity& xcenter, const Quantity& ycenter,
	const Quantity& majorAxis,
	const Quantity& minorAxis, const Quantity& positionAngle,
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
		ELLIPSE, dirRefFrameString, csys, beginFreq,
		endFreq, freqRefFrameString, dopplerString,
		restfreq, stokes, annotationOnly
), _inputCenter(Vector<Quantity>(2)), _inputMajorAxis(majorAxis),
_inputMinorAxis(minorAxis),
	_inputPositionAngle(positionAngle) {
	//Vector<Double> myCenter = _center.getAngle("rad").getValue();

	_convertedMajorAxis = _lengthToAngle(majorAxis, _directionAxes[0]);
	_convertedMinorAxis = _lengthToAngle(minorAxis, _directionAxes[0]);

	String preamble = String(__FUNCTION__) + ": ";
	if (
		_convertedMinorAxis.getValue("rad")
		> _convertedMajorAxis.getValue("rad")
	) {
		throw AipsError(
			preamble
			+ "Major axis must be greater than or "
			+ "equal to minor axis"
		);
	}
	if (! _inputPositionAngle.isConform("rad")) {
		throw AipsError(
			preamble
			+ "Position angle must have angular units"
		);
	}
	_inputCenter[0] = xcenter;
	_inputCenter[1] = ycenter;

	_checkAndConvertDirections(String(__FUNCTION__), _inputCenter);

	Vector<Double> coords = _convertedDirections[0].getAngle("rad").getValue();

	Vector<Quantity> qCenter(2);
	qCenter[0] = Quantity(coords[0], "rad");
	qCenter[1] = Quantity(coords[1], "rad");


	WCEllipsoid ellipse(
		qCenter[0], qCenter[1],
		_convertedMajorAxis, _convertedMinorAxis, positionAngle, _directionAxes[0],
		_directionAxes[1], _csys
	);
	_extend(ellipse);

	ostringstream os;
	os << "ellipse [[" << xcenter << ", " << ycenter << "], ["
		<< majorAxis << ", " << minorAxis << "]]";
	_stringRep += os.str();
}

MDirection AnnEllipse::getCenter() const {
	return _convertedDirections[0];
}

Quantity AnnEllipse::getMajorAxis() const {
	return _convertedMajorAxis;
}

Quantity AnnEllipse::getMinorAxis() const {
	return _convertedMinorAxis;
}

Quantity AnnEllipse::getPositionAngle() const {
	return _inputPositionAngle;
}

}
