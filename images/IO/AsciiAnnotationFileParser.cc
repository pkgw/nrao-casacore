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

#include <images/IO/AsciiAnnotationFileParser.h>

#include <casa/IO/RegularFileIO.h>
#include <coordinates/Coordinates/DirectionCoordinate.h>
#include <coordinates/Coordinates/SpectralCoordinate.h>
#include <images/Annotations/AnnAnnulus.h>
#include <images/Annotations/AnnCenterBox.h>
#include <images/Annotations/AnnCircle.h>
#include <images/Annotations/AnnEllipse.h>
#include <images/Annotations/AnnLine.h>
#include <images/Annotations/AnnPolygon.h>
#include <images/Annotations/AnnRectBox.h>
#include <images/Annotations/AnnRotBox.h>
#include <images/Annotations/AnnSymbol.h>
#include <images/Annotations/AnnText.h>
#include <images/Annotations/AnnVector.h>

#include <measures/Measures/MCDirection.h>
#include <measures/Measures/MDirection.h>
#include <measures/Measures/VelocityMachine.h>

namespace casa {

const String AsciiAnnotationFileParser::sOnePair = "[[:space:]]*\\[[^\\[,]+,[^\\[,]+\\][[:space:]]*";
const String AsciiAnnotationFileParser::bTwoPair = "\\[" + sOnePair
		+ "," + sOnePair;
// explicit onePair at the end because that one should not be followed by a comma
const String AsciiAnnotationFileParser::sNPair = "\\[(" + sOnePair
		+ ",)+" + sOnePair + "\\]";
const Regex AsciiAnnotationFileParser::startOnePair("^" + sOnePair);
const Regex AsciiAnnotationFileParser::startNPair("^" + sNPair);

AsciiAnnotationFileParser::AsciiAnnotationFileParser(
	const String& filename, const CoordinateSystem& csys
) : _file(RegularFile(filename)), _csys(csys),
	_log(new LogIO()),
	_currentGlobals(ParamSet()),
	_lines(Vector<AsciiAnnotationFileLine>(0)),
	_globalKeysToApply(Vector<AnnotationBase::Keyword>(0))
	{
	String preamble = String(__FUNCTION__) + ": ";
	if (! _file.exists()) {
		throw AipsError(
			preamble + "File "
			+ filename + " does not exist."
		);
	}
	if (! _file.isReadable()) {
		throw AipsError(
			preamble + "File "
			+ filename + " is not readable."
		);
	}
	if (! _csys.hasDirectionCoordinate()) {
		throw AipsError(
			preamble
			+ "Coordinate system has not direction coordintate"
		);
	}
	_setInitialGlobals();
	_parse();
}

Vector<AsciiAnnotationFileLine> AsciiAnnotationFileParser::getLines() const {
	return _lines;
}


AsciiAnnotationFileParser::~AsciiAnnotationFileParser() {
	delete _log;
	_log = 0;
}

void AsciiAnnotationFileParser::_parse() {
	_log->origin(LogOrigin("AsciiRegionFileParser", __FUNCTION__));
	const Regex startAnn("^ann[[:space:]]+");
	const Regex startDiff("^-[[:space:]]+");
	const Regex startGlobal("^global[[:space:]]+");

	RegularFileIO fileIO(_file);
	Int bufSize = 4096;
	char *buffer = new char[bufSize];
	int nRead;
	String contents;

	while ((nRead = fileIO.read(bufSize, buffer, False)) == bufSize) {
		*_log << LogIO::NORMAL << "read: " << nRead << LogIO::POST;
		String chunk(buffer, bufSize);
		contents += chunk;
	}
	// get the last chunk
	String chunk(buffer, nRead);
	contents += chunk;

	Vector<String> lines = stringToVector(contents, '\n');
	uInt lineCount = 0;
	Vector<Quantity> qFreqs(2, Quantity(0));

	for(Vector<String>::iterator iter=lines.begin(); iter!=lines.end(); iter++) {
		lineCount++;
		Bool annOnly = False;
		ostringstream preambleoss;
		preambleoss << _file.path().baseName() + " line# " << lineCount << ": ";
		String preamble = preambleoss.str();
		Bool difference = False;
		iter->trim();
		if (
			iter->empty() || iter->startsWith("#")
		) {
			// ignore comments and blank lines
			_addLine(AsciiAnnotationFileLine(*iter));
			*_log << LogIO::NORMAL << preamble << "comment found" << LogIO::POST;
			continue;
		}
		String consumeMe = *iter;
		consumeMe.downcase();
		Bool spectralParmsUpdated;
		ParamSet newParams;
		if (consumeMe.contains(startDiff)) {
			difference = True;
			// consume the difference character to allow further processing of string
			consumeMe.del(0, 1);
			consumeMe.trim();
			*_log << LogIO::NORMAL << preamble << "difference found" << LogIO::POST;
		}
		else if(consumeMe.contains(startAnn)) {
			annOnly = True;
			// consume the annotation chars
			consumeMe.del(0, 3);
			consumeMe.trim();
			*_log << LogIO::NORMAL << preamble << "annotation only found" << LogIO::POST;
		}
		else if(consumeMe.contains(startGlobal)) {
			consumeMe.del(0, 6);
			_currentGlobals = _getCurrentParamSet(
				spectralParmsUpdated, newParams,
				consumeMe, preamble
			);
			//_setCurrentGlobalKeys();
			map<AnnotationBase::Keyword, String> gParms;
			for (
				ParamSet::const_iterator iter=newParams.begin();
				iter != newParams.end(); iter++
			) {
				gParms[iter->first] = iter->second.stringVal;
			}
			_addLine(AsciiAnnotationFileLine(gParms));
			if (_csys.hasSpectralAxis() && spectralParmsUpdated) {
				qFreqs = _quantitiesFromFrequencyString(
					newParams[AnnotationBase::RANGE].stringVal, preamble
				);
			}
			*_log << LogIO::NORMAL << preamble << "global found" << LogIO::POST;
			continue;
		}
		// now look for shapes and annotations
		Vector<Quantity> qDirs;
		Vector<Quantity> quantities;
		String textString;
		AnnotationBase::Type annType = _getAnnotationType(
			qDirs, quantities, textString, consumeMe, preamble
		);
		ParamSet currentParamSet = _getCurrentParamSet(
			spectralParmsUpdated, newParams, consumeMe, preamble
		);
		if (_csys.hasSpectralAxis() && spectralParmsUpdated) {
			qFreqs = _quantitiesFromFrequencyString(
				currentParamSet[AnnotationBase::RANGE].stringVal, preamble
			);
		}
		ParamSet globalsLessLocal = _currentGlobals;
		for (
			ParamSet::const_iterator iter=newParams.begin();
			iter != newParams.end(); iter++
		) {
			AnnotationBase::Keyword key = iter->first;
			if (globalsLessLocal.find(key) != globalsLessLocal.end()) {
				globalsLessLocal.erase(key);
			}
		}
		_globalKeysToApply.resize(globalsLessLocal.size(), False);
		uInt i = 0;
		for (
			ParamSet::const_iterator iter=globalsLessLocal.begin();
			iter != globalsLessLocal.end(); iter++
		) {
			_globalKeysToApply[i] = iter->first;
			i++;
		}
		_createAnnotation(
			annType, qDirs, qFreqs, quantities, textString,
			currentParamSet, annOnly, difference, preamble
		);
	}
	*_log << LogIO::NORMAL << "end" << LogIO::POST;
}

void AsciiAnnotationFileParser::_addLine(const AsciiAnnotationFileLine& line) {
	_lines.resize(_lines.size()+1, True);
	_lines[_lines.size()-1] = line;
}

/*
void AsciiAnnotationFileParser::_setCurrentGlobalKeys() {
	_currentGlobalKeys.resize(_currentGlobals.size(), False);
	uInt i=0;
	for (
		ParamSet::const_iterator iter=_currentGlobals.begin();
		iter != _currentGlobals.end(); iter++
	) {
		_currentGlobalKeys[i] = iter->first;
		i++;
	}
}
*/

AnnotationBase::Type AsciiAnnotationFileParser::_getAnnotationType(
	//Vector<MDirection>& dirs,
	Vector<Quantity>& qDirs,
	Vector<Quantity>& quantities,
	String& textString,
	String& consumeMe, const String& preamble
) const {
	const String sOnePairOneSingle =
		"\\[" + sOnePair + ",[^\\[,]+\\]";
	const String sOnePairAndText =
		"\\[" + sOnePair + ",[[:space:]]*[\"\'].*[\"\'][[:space:]]*\\]";
	const String sTwoPair = bTwoPair + "\\]";
	const Regex startTwoPair("^" + sTwoPair);
	const Regex startOnePairAndText("^" + sOnePairAndText);
	const String sTwoPairOneSingle = bTwoPair
			+ ",[[:space:]]*[^\\[,]+[[:space:]]*\\]";
	const Regex startTwoPairOneSingle("^" + sTwoPairOneSingle);
	const Regex startOnePairOneSingle("^" + sOnePairOneSingle);
	consumeMe.trim();
	String tmp = consumeMe.through(Regex("[[:alpha:]]+"));
	consumeMe.del(0, (Int)tmp.length() + 1);
	consumeMe.trim();
	AnnotationBase::Type annotationType = AnnotationBase::typeFromString(tmp);
	switch(annotationType) {
	case AnnotationBase::RECT_BOX:
		if (! consumeMe.contains(startTwoPair)) {
			*_log << preamble << "Illegal box specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs = _extractNQuantityPairs(consumeMe, preamble);
		if (qDirs.size() != 4) {
			throw AipsError(preamble
				+ "rectangle box spec must contain exactly 2 direction pairs but it has "
				+ String::toString(qDirs.size())
			);
		}
		cout << preamble << "rect box found " << endl;
		break;

	case AnnotationBase::CENTER_BOX:
		if (! consumeMe.contains(startTwoPair)) {
			*_log << preamble << "Illegal center box specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(2);
		quantities.resize(2);
		{
			Vector<Quantity> qs = _extractNQuantityPairs(consumeMe, preamble);
			qDirs[0] = qs[0];
			qDirs[1] = qs[1];
			quantities[0] = qs[2];
			quantities[1] = qs[3];
		}
		cout << preamble << "center box found " << endl;
		break;
	case AnnotationBase::ROTATED_BOX:
		if (! consumeMe.contains(startTwoPairOneSingle)) {
			*_log << preamble << "Illegal rotated box specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(2);
		quantities.resize(3);
		{
			Vector<Quantity> qs = _extractTwoQuantityPairsAndSingleQuantity(consumeMe, preamble);
			qDirs[0] = qs[0];
			qDirs[1] = qs[1];
			quantities[0] = qs[2];
			quantities[1] = qs[3];
			quantities[2] = qs[4];
		}
		cout << preamble << "rotated box found " << endl;
		break;

	case AnnotationBase::POLYGON:
		if (! consumeMe.contains(startNPair)) {
			*_log << preamble << "Illegal polygon specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		{
			Vector<Quantity> qs = _extractNQuantityPairs(
				consumeMe, preamble
			);
			qDirs.resize(qs.size());
			qDirs = qs;
		}
		cout << preamble << "polygon found " << endl;
		break;
	case AnnotationBase::CIRCLE:
		if (! consumeMe.contains(startOnePairOneSingle)) {
			*_log << preamble << "Illegal circle specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(2);
		quantities.resize(1);
		{
			Vector<Quantity> qs = _extractQuantityPairAndSingleQuantity(
				consumeMe, preamble
			);
			cout << "qs " << qs << endl;
			qDirs[0] = qs[0];
			qDirs[1] = qs[1];
			quantities[0] = qs[2];
		}
		cout << preamble << "circle found " << endl;
		break;
	case AnnotationBase::ANNULUS:
		if (! consumeMe.contains(startTwoPair)) {
			*_log << preamble << "Illegal annulus specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(2);
		quantities.resize(2);
		{
			Vector<Quantity> qs = _extractNQuantityPairs(
				consumeMe, preamble
			);
			qDirs[0] = qs[0];
			qDirs[1] = qs[1];
			quantities[0] = qs[2];
			quantities[1] = qs[3];
		}
		cout << preamble << "annulus found " << endl;
		break;
	case AnnotationBase::ELLIPSE:
		if (! consumeMe.contains(startTwoPairOneSingle)) {
			*_log << preamble << "Illegal ellipse specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(2);
		quantities.resize(3);
		{
			Vector<Quantity> qs = _extractTwoQuantityPairsAndSingleQuantity(
				consumeMe, preamble
			);
			qDirs[0] = qs[0];
			qDirs[1] = qs[1];
			quantities[0] = qs[2];
			quantities[1] = qs[3];
			quantities[2] = qs[4];
		}
		cout << preamble << "ellipse found " << endl;
		break;
	case AnnotationBase::LINE:
		if (! consumeMe.contains(startTwoPair)) {
			*_log << preamble << "Illegal line specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(4);
		qDirs = _extractNQuantityPairs(consumeMe, preamble);
		if (qDirs.size() != 4) {
			throw AipsError(preamble
				+ "line spec must contain exactly 2 direction pairs but it has "
				+ String::toString(qDirs.size())
			);
		}
		cout << preamble << "line found " << endl;
		break;
	case AnnotationBase::VECTOR:
		if (! consumeMe.contains(startTwoPair)) {
			*_log << preamble << "Illegal vector specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(4);
		qDirs = _extractNQuantityPairs(consumeMe, preamble);
		if (qDirs.size() != 4) {
			throw AipsError(preamble
				+ "line spec must contain exactly 2 direction pairs but it has "
				+ String::toString(qDirs.size())
			);
		}
		cout << preamble << "vector found " << endl;
		break;
	case AnnotationBase::TEXT:
		if (! consumeMe.contains(startOnePairAndText)) {
			*_log << preamble << "Illegal text specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(2);
		_extractQuantityPairAndString(
			qDirs, textString, consumeMe, preamble, True
		);
		cout << preamble << "text found " << endl;
		cout << "text string " << textString << endl;
		break;
	case AnnotationBase::SYMBOL:
		if (! consumeMe.contains(startOnePairOneSingle)) {
			*_log << preamble << "Illegal symbol specification "
				<< consumeMe << LogIO::EXCEPTION;
		}
		qDirs.resize(2);
		_extractQuantityPairAndString(
			qDirs, textString, consumeMe, preamble, False
		);
		cout << preamble << "symbol found " << endl;
		textString.trim();
		if (textString.length() > 1) {
			throw AipsError(
				preamble
					+ ": A symbol is defined by a single character. The provided string ("
					+ textString
					+ ") has more than one"
			);
		}
		cout << "symbol string " << textString << endl;
		break;
	default:
		throw AipsError(
			preamble + "Unable to determine annotation type"
		);
	}
	return annotationType;
}

AsciiAnnotationFileParser::ParamSet
AsciiAnnotationFileParser::_getCurrentParamSet(
	Bool& spectralParmsUpdated, ParamSet& newParams,
	String& consumeMe, const String& preamble
) const {
	ParamSet currentParams = _currentGlobals;
	spectralParmsUpdated = False;
	// get key-value pairs on the line
	while (consumeMe.size() > 0) {
		ParamValue paramValue;
		AnnotationBase::Keyword key = AnnotationBase::UNKNOWN;
		consumeMe.trim();
		consumeMe.ltrim(',');
		consumeMe.trim();

		if (! consumeMe.contains('=')) {
			*_log << preamble << "Illegal extra characters on line ("
				<< consumeMe << "). Did you forget a '='?"
				<< LogIO::EXCEPTION;
		}
		uInt equalPos = consumeMe.find('=');
		String keyword = consumeMe.substr(0, equalPos);
		keyword.trim();
		keyword.downcase();
		consumeMe.del(0, (Int)equalPos + 1);
		consumeMe.trim();
		if (keyword == "label") {
			key = AnnotationBase::LABEL;
			paramValue.stringVal = _doLabel(consumeMe, preamble);
		}
		else {
			paramValue.stringVal = _getKeyValue(consumeMe, preamble);
			if (keyword == "coord") {
				key = AnnotationBase::COORD;
			}
			else if (keyword == "corr") {
				if (_csys.hasPolarizationAxis()) {
					key = AnnotationBase::CORR;
					paramValue.stokes = _stokesFromString(
							paramValue.stringVal, preamble
					);
				}
				else {
					*_log << LogIO::WARN << preamble
						<< "Keyword " << keyword << " specified but will be ignored "
						<< "because the coordinate system has no polarization axis."
						<< LogIO::POST;
				}
			}
			else if (
				keyword == "frame" || keyword == "range"
				|| keyword == "veltype" || keyword == "restfreq"
			) {
				spectralParmsUpdated = True;
				if (! _csys.hasSpectralAxis()) {
					spectralParmsUpdated = False;
					*_log << LogIO::WARN << preamble
						<< "Keyword " << keyword << " specified but will be ignored "
						<< "because the coordinate system has no spectral axis."
						<< LogIO::POST;
				}
				else if (keyword == "frame") {
					key = AnnotationBase::FRAME;
				}
				else if (keyword == "range") {
					key = AnnotationBase::RANGE;
				}
				else if (keyword == "veltype") {
					key = AnnotationBase::VELTYPE;
				}
				else if (keyword == "restfreq") {
					key = AnnotationBase::RESTFREQ;
					Quantity qRestfreq;
					if (! readQuantity(qRestfreq, paramValue.stringVal)) {
						*_log << preamble << "Could not convert rest frequency "
							<< paramValue.stringVal << " to quantity"
							<< LogIO::EXCEPTION;
					}
					paramValue.doubleVal = qRestfreq.getValue("Hz");
				}
			}
			else if (keyword == "linewidth") {
				key = AnnotationBase::LINEWIDTH;
				if (! paramValue.stringVal.matches(Regex("^[1-9]+$"))) {
					*_log << preamble << "linewidth (" << paramValue.stringVal
						<< ") must be a positive integer but is not." << LogIO::EXCEPTION;
				}
				paramValue.intVal = String::toInt(paramValue.stringVal);
			}
			else if (keyword == "linestyle") {
				key = AnnotationBase::LINESTYLE;
			}
			else if (keyword == "symsize") {
				key = AnnotationBase::SYMSIZE;
				if (! paramValue.stringVal.matches(Regex("^[1-9]+$"))) {
					*_log << preamble << "symsize (" << paramValue.stringVal
						<< ") must be a positive integer but is not." << LogIO::EXCEPTION;
				}
				paramValue.intVal = String::toInt(paramValue.stringVal);
			}
			else if (keyword == "symthick") {
				key = AnnotationBase::SYMTHICK;
				if (! paramValue.stringVal.matches(Regex("^[1-9]+$"))) {
					*_log << preamble << "symthick (" << paramValue.stringVal
						<< ") must be a positive integer but is not." << LogIO::EXCEPTION;
				}
				paramValue.intVal = String::toInt(paramValue.stringVal);
			}
			else if (keyword == "color") {
				key = AnnotationBase::COLOR;
			}
			else if (keyword == "font") {
				key = AnnotationBase::FONT;
			}
			else if (keyword == "fontsize") {
				key = AnnotationBase::FONTSIZE;
			}
			else if (keyword == "fontstyle") {
				key = AnnotationBase::FONTSTYLE;
			}
			else if (keyword == "usetex") {
				String v = paramValue.stringVal;
				v.downcase();
				key = AnnotationBase::USETEX;
				if (
					v != "true"  && v != "t"
					&& v != "false" && v != "f"
				) {
					*_log << preamble << "Cannot determine boolean value of usetex"
						<< paramValue.stringVal << LogIO::EXCEPTION;
				}
				paramValue.boolVal = (v == "true" || v == "t");
			}
			else {
				*_log << preamble << "Unrecognized key " << keyword
					<< LogIO::EXCEPTION;
			}
		}
		cout << "*** key " << key << " value " << paramValue.stringVal << endl;

		consumeMe.trim();
		if (key != AnnotationBase::UNKNOWN) {
			currentParams[key] = paramValue;
			newParams[key] = paramValue;

		}
	}
	if (
		currentParams.find(AnnotationBase::RANGE) == currentParams.end()
		&& currentParams.find(AnnotationBase::FRAME) != currentParams.end()
	) {
		*_log << preamble << "Frame specified but frequency range not specified"
			<< LogIO::EXCEPTION;
	}
	if (
		currentParams.find(AnnotationBase::RANGE) == currentParams.end()
		&& currentParams.find(AnnotationBase::RESTFREQ) != currentParams.end()
	) {
		*_log << preamble << "Rest frequency specified but velocity range not specified"
			<< LogIO::EXCEPTION;
	}
	return currentParams;
}

Vector<Quantity> AsciiAnnotationFileParser::_quantitiesFromFrequencyString(
	const String& freqString, const String& preamble
) const {
	if (! freqString.matches(sOnePair)) {
		*_log << preamble << "Incorrect spectral range specification ("
			<< freqString << ")" << LogIO::EXCEPTION;
	}
	return _extractSingleQuantityPair(
		freqString, preamble
	);
}

void AsciiAnnotationFileParser::_createAnnotation(
	const AnnotationBase::Type annType,
	const Vector<Quantity>& qDirs,
	const Vector<Quantity>& qFreqs,
	const Vector<Quantity>& quantities,
	const String& textString,
	const ParamSet& currentParamSet,
	const Bool annOnly, const Bool isDifference,
	const String& preamble
) {
	AnnotationBase *annotation = 0;
	Vector<Stokes::StokesTypes> stokes(0);
	if (
		currentParamSet.find(AnnotationBase::CORR) != currentParamSet.end()
		&& _csys.hasPolarizationAxis()
	) {
		stokes.resize(currentParamSet.at(AnnotationBase::CORR).stokes.size());
		stokes = currentParamSet.at(AnnotationBase::CORR).stokes;
	}
	String dirRefFrame = currentParamSet.at(AnnotationBase::COORD).stringVal;
	String freqRefFrame = currentParamSet.at(AnnotationBase::FRAME).stringVal;
	String doppler = currentParamSet.at(AnnotationBase::VELTYPE).stringVal;
	Quantity restfreq;
	if (
		! readQuantity(
			restfreq, currentParamSet.at(AnnotationBase::RESTFREQ).stringVal
		)
	) {
		*_log << preamble << "restfreq value "
			<< currentParamSet.at(AnnotationBase::RESTFREQ).stringVal << " is not "
			<< "a valid quantity." << LogIO::EXCEPTION;
	}

	try {
	switch (annType) {
		case AnnotationBase::RECT_BOX:
			annotation = new AnnRectBox(
				qDirs[0], qDirs[1], qDirs[2], qDirs[3],
				dirRefFrame, _csys, qFreqs[0], qFreqs[1],
				freqRefFrame, doppler, restfreq, stokes,
				annOnly
			);
			break;
		case AnnotationBase::CENTER_BOX:
			annotation = new AnnCenterBox(
				qDirs[0], qDirs[1], quantities[0], quantities[1],
				dirRefFrame, _csys, qFreqs[0], qFreqs[1],
				freqRefFrame, doppler, restfreq, stokes,
				annOnly
			);
			// cout << dynamic_cast<AnnRegion *>(annotation)->asRecord() << endl;
			break;
		case AnnotationBase::ROTATED_BOX:
			annotation = new AnnRotBox(
				qDirs[0], qDirs[1], quantities[0], quantities[1],
				quantities[2], dirRefFrame, _csys, qFreqs[0], qFreqs[1],
				freqRefFrame, doppler, restfreq,  stokes, annOnly
			);
			// cout << dynamic_cast<AnnRegion *>(annotation)->asRecord() << endl;
			break;
		case AnnotationBase::POLYGON:
			{
				Vector<Quantity> x(qDirs.size()/2);
				Vector<Quantity> y(qDirs.size()/2);
				for (uInt i=0; i<x.size(); i++) {
					x[i] = qDirs[2*i];
					y[i] = qDirs[2*i + 1];
				}
				annotation = new AnnPolygon(
					x, y, dirRefFrame,  _csys, qFreqs[0], qFreqs[1],
					freqRefFrame, doppler, restfreq,  stokes, annOnly
				);
			}
			//cout << dynamic_cast<AnnRegion *>(annotation)->asRecord() << endl;
			break;
		case AnnotationBase::CIRCLE:
			annotation = new AnnCircle(
				qDirs[0], qDirs[1], quantities[0],
				dirRefFrame,  _csys, qFreqs[0], qFreqs[1],
				freqRefFrame, doppler, restfreq,  stokes, annOnly
			);
			// cout << dynamic_cast<AnnRegion *>(annotation)->asRecord() << endl;
			break;
		case AnnotationBase::ANNULUS:
			annotation = new AnnAnnulus(
				qDirs[0], qDirs[1], quantities[0], quantities[1],
				dirRefFrame,  _csys, qFreqs[0], qFreqs[1],
				freqRefFrame, doppler, restfreq,  stokes, annOnly
			);
			// cout << dynamic_cast<AnnRegion *>(annotation)->asRecord() << endl;
			break;
		case AnnotationBase::ELLIPSE:
			annotation = new AnnEllipse(
				qDirs[0], qDirs[1], quantities[0], quantities[1], quantities[2],
				dirRefFrame,  _csys, qFreqs[0], qFreqs[1],
				freqRefFrame, doppler, restfreq,  stokes, annOnly
			);
			// cout << dynamic_cast<AnnRegion *>(annotation)->asRecord() << endl;
			break;
		case AnnotationBase::LINE:
			annotation = new AnnLine(
				qDirs[0], qDirs[1], qDirs[2],
				qDirs[3], dirRefFrame,  _csys
			);
			break;
		case AnnotationBase::VECTOR:
			annotation = new AnnVector(
				qDirs[0], qDirs[1], qDirs[2],
				qDirs[3], dirRefFrame,  _csys
			);
			break;
		case AnnotationBase::TEXT:
			annotation = new AnnText(
				qDirs[0], qDirs[1], dirRefFrame,
				_csys, textString
			);
			break;
		case AnnotationBase::SYMBOL:
			annotation = new AnnSymbol(
				qDirs[0], qDirs[1], dirRefFrame,
				_csys, textString.firstchar()
			);
			break;
		default:
			throw AipsError(
				"Logic error. Unhandled type "
					+  String::toString(annType) + " in switch statement"
			);
	}
	}
	catch (AipsError x) {
		*_log << preamble << x.getMesg() << LogIO::EXCEPTION;
	}
	if (annotation->isRegion()) {
		dynamic_cast<AnnRegion *>(annotation)->setDifference(isDifference);
	}
	annotation->setLineWidth(currentParamSet.at(AnnotationBase::LINEWIDTH).intVal);
	annotation->setLineStyle(currentParamSet.at(AnnotationBase::LINESTYLE).stringVal);
	annotation->setSymbolSize(currentParamSet.at(AnnotationBase::SYMSIZE).intVal);
	annotation->setSymbolThickness(currentParamSet.at(AnnotationBase::SYMTHICK).intVal);
	annotation->setColor(currentParamSet.at(AnnotationBase::COLOR).stringVal);
	annotation->setFont(currentParamSet.at(AnnotationBase::FONT).stringVal);
	annotation->setFontSize(currentParamSet.at(AnnotationBase::FONTSIZE).stringVal);
	annotation->setFontStyle(currentParamSet.at(AnnotationBase::FONTSTYLE).stringVal);
	annotation->setUseTex(currentParamSet.at(AnnotationBase::USETEX).boolVal);
	annotation->setGlobals(_globalKeysToApply);
	AsciiAnnotationFileLine line(annotation);
	_addLine(line);
}

Array<String> AsciiAnnotationFileParser::_extractTwoPairs(uInt& end, const String& string) const {
	end = 0;
	Int firstBegin = string.find('[', 1);
	Int firstEnd = string.find(']', firstBegin);
	String firstPair = string.substr(firstBegin, firstEnd - firstBegin + 1);
	Int secondBegin = string.find('[', firstEnd);
	Int secondEnd = string.find(']', secondBegin);
	String secondPair = string.substr(secondBegin, secondEnd - secondBegin + 1);
	Vector<String> first = _extractSinglePair(firstPair);
	Vector<String> second = _extractSinglePair(secondPair);

	end = secondEnd;
	Array<String> ret(IPosition(2, 2, 2));
	ret(IPosition(2, 0, 0)) = first[0];
	ret(IPosition(2, 0, 1)) = first[1];
	ret(IPosition(2, 1, 0)) = second[0];
	ret(IPosition(2, 1, 1)) = second[1];
	return ret;
}

Vector<String> AsciiAnnotationFileParser::_extractSinglePair(const String& string) const {
	cout << "string " << string << endl;
	Char quotes[2];
	quotes[0] = '\'';
	quotes[1] = '"';
	Int firstBegin = string.find('[', 0) + 1;
	Int firstEnd = string.find(',', firstBegin);
	String first = string.substr(firstBegin, firstEnd - firstBegin);
	cout << "first " << first << endl;
	first.trim();
	first.trim(quotes, 2);
	Int secondBegin = firstEnd + 1;
	Int secondEnd = string.find(']', secondBegin);
	String second = string.substr(secondBegin + 1, secondEnd - secondBegin - 1);
	second.trim();
	second.trim(quotes, 2);
	Vector<String> ret(2);
	ret[0] = first;
	ret[1] = second;
	return ret;
}

String AsciiAnnotationFileParser::_doLabel(
	String& consumeMe, const String& preamble
) const {
	Char firstChar = consumeMe.firstchar();
	if (firstChar != '\'' && firstChar != '"') {
		*_log << preamble
			<< "keyword 'label' found but first non-whitespace character after the '=' is not a quote. It must be."
			<< LogIO::EXCEPTION;
	}
	String::size_type posCloseQuote = consumeMe.find(firstChar, 1);
	if (posCloseQuote == String::npos) {
		*_log << preamble << "Could not find closing quote ("
			<< String(firstChar) << ") for label" << LogIO::EXCEPTION;
	}
	String label = consumeMe.substr(1, posCloseQuote - 2);
	consumeMe.del(0, (Int)posCloseQuote + 1);
	return label;
}

String AsciiAnnotationFileParser::_getKeyValue(
	String& consumeMe, const String& preamble
) const {
	String value;
	if (consumeMe.startsWith("[")) {
		if (! consumeMe.contains("]")) {
			*_log << preamble << "Unmatched open bracket: "
				<< consumeMe << LogIO::EXCEPTION;
		}
		Int closeBracketPos = consumeMe.find("]");
		// don't save the open and close brackets
		value = consumeMe.substr(1, closeBracketPos - 1);
		consumeMe.del(0, closeBracketPos + 1);
	}
	if (consumeMe.contains(",")) {
		Int commaPos = consumeMe.find(",");
		if (value.empty()) {
			value = consumeMe.substr(0, commaPos - 1);
		}
		consumeMe.del(0, commaPos);
	}
	else if (value.empty()) {
		// last key-value pair on the line
		value = consumeMe;
		consumeMe = "";
	}
	consumeMe.trim();
	Char quotes[2];
	quotes[0] = '\'';
	quotes[1] = '"';
	value.trim();
	value.trim(quotes, 2);
	value.trim();
	return value;
}

Vector<Quantity> AsciiAnnotationFileParser::_extractTwoQuantityPairsAndSingleQuantity(
	String& consumeMe, const String& preamble
) const {
	Vector<Quantity> quantities = _extractTwoQuantityPairs(
		consumeMe, preamble
	);
	consumeMe.trim();
	consumeMe.ltrim(',');
	consumeMe.trim();
	Char quotes[2];
	quotes[0] = '\'';
	quotes[1] = '"';

	Int end = consumeMe.find(']', 0);
	String qString = consumeMe.substr(0, end - 1);
	qString.trim();

	qString.trim(quotes, 2);
	quantities.resize(5, True);
	if (! readQuantity(quantities[4], qString)) {
		*_log << preamble + "Could not convert "
			<< qString << " to quantity." << LogIO::EXCEPTION;
	}
	consumeMe.del(0, end + 1);
	return quantities;
}

void AsciiAnnotationFileParser::_extractQuantityPairAndString(
	Vector<Quantity>& quantities, String& string,
	String& consumeMe, const String& preamble,
	const Bool requireQuotesAroundString
) const {
	consumeMe.ltrim('[');
	SubString pairString = consumeMe.through(startOnePair);
	quantities = _extractSingleQuantityPair(pairString, preamble);
	//mdir = _directionFromPair(dirString, preamble);
	consumeMe.del(0, (Int)pairString.length() + 1);
	consumeMe.trim();
	consumeMe.ltrim(',');
	consumeMe.trim();
	Int end = 0;
	String::size_type startSearchPos = 0;
	if (requireQuotesAroundString) {
		Char quoteChar = consumeMe.firstchar();
		if (quoteChar != '\'' && quoteChar != '"') {
			*_log << preamble
				<< "Quotes around string required but no quotes were found";
		}
		startSearchPos = consumeMe.find(quoteChar, 1);
		if (startSearchPos == String::npos) {
			*_log << preamble
				<< "Quotes required around string but no matching close quote found"
				<< LogIO::EXCEPTION;
		}
	}
	end = consumeMe.find(']', startSearchPos);
	string = consumeMe.substr(0, end);
	consumeMe.del(0, end + 1);
	string.trim();
	Char quotes[2];
	quotes[0] = '\'';
	quotes[1] = '"';
	string.trim(quotes, 2);
	string.trim();
}

Vector<Quantity> AsciiAnnotationFileParser::_extractQuantityPairAndSingleQuantity(
	String& consumeMe, const String& preamble
) const {
	String qString;
	Vector<Quantity> quantities(2);

	_extractQuantityPairAndString(
		quantities, qString, consumeMe, preamble, False
	);
	quantities.resize(3, True);
	if (! readQuantity(quantities[2], qString)) {
		*_log << preamble << "Could not convert "
			<< qString << " to quantity" << LogIO::EXCEPTION;
	}
	return quantities;
}

Vector<Quantity> AsciiAnnotationFileParser::_extractTwoQuantityPairs(
	String& consumeMe, const String& preamble
) const {
	const Regex startbTwoPair("^" + bTwoPair);
	String mySubstring = String(consumeMe).through(startbTwoPair);
	cout << "mySubstring " << mySubstring << endl;
	uInt end = 0;
	Array<String> pairs = _extractTwoPairs(end, mySubstring);
	cout << "pairs " << pairs << endl;
	Vector<Quantity> quantities(4);

	for (uInt i=0; i<4; i++) {
		String desc("string " + String::toString(i));
		String value = pairs(IPosition(2, i/2, i%2));
		if (! readQuantity(quantities[i], value)) {
			*_log << preamble << "Could not convert " << desc
				<< " (" << value << ") to quantity." << LogIO::EXCEPTION;
		}
	}
	consumeMe.del(0, (Int)end + 1);
	return quantities;
}

Vector<Quantity> AsciiAnnotationFileParser::_extractNQuantityPairs (
		String& consumeMe, const String& preamble
) const {
	String pairs = consumeMe.through(startNPair);
	consumeMe.del(0, (Int)pairs.length() + 1);
	pairs.trim();
	pairs.ltrim('[');
	pairs.trim();
	Vector<Quantity> qs(0);
	while (pairs.length() > 1) {
		Vector<Quantity> myqs = _extractSingleQuantityPair(pairs, preamble);
		qs.resize(qs.size() + 2, True);
		qs[qs.size() - 2] = myqs[0];
		qs[qs.size() - 1] = myqs[1];
		pairs.del(0, (Int)pairs.find(']', 0) + 1);
		pairs.trim();
		pairs.ltrim(',');
		pairs.trim();
	}
	return qs;
}

Vector<Quantity> AsciiAnnotationFileParser::_extractSingleQuantityPair(
	const String& pairString, const String& preamble
) const {
	String mySubstring = String(pairString).through(sOnePair, 0);
	Vector<String> pair = _extractSinglePair(mySubstring);
	cout << "pair " << pair << endl;
	Vector<Quantity> quantities(2);

	for (uInt i=0; i<2; i++) {
		String value = pair[i];
		if (! readQuantity(quantities[i], value)) {
			*_log << preamble << "Could not convert "
				<< " (" << value << ") to quantity.";
		}
	}
	return quantities;
}

Vector<Stokes::StokesTypes>
AsciiAnnotationFileParser::_stokesFromString(
	const String& stokes, const String& preamble
) const {
	Int maxn = Stokes::NumberOfTypes;
	string res[maxn];
	cout << "stokes " << "\"" << stokes << "\"" << endl;
	Int nStokes = split(stokes, res, maxn, ",");
	cout << "nStokes " << nStokes << endl;
	cout << "maxn " << maxn << endl;
	Vector<Stokes::StokesTypes> myTypes(nStokes);
	for (Int i=0; i<nStokes; i++) {
		String x(res[i]);
		x.trim();
		myTypes[i] = Stokes::type(x);
		if (myTypes[i] == Stokes::Undefined) {
			throw AipsError(preamble + "Unknown correlation type " + x);
		}
	}
	return myTypes;
}

void AsciiAnnotationFileParser::_setInitialGlobals() {
	ParamValue coord;
	coord.intVal = _csys.directionCoordinate(
		_csys.findCoordinate(Coordinate::DIRECTION)
	).directionType(False);
	coord.stringVal = MDirection::showType(coord.intVal);
	_currentGlobals[AnnotationBase::COORD] = coord;

	ParamValue range;
	range.freqRange = Vector<MFrequency>(0);
	_currentGlobals[AnnotationBase::RANGE] = range;

	ParamValue corr;
	corr.stokes = Vector<Stokes::StokesTypes>(0);
	_currentGlobals[AnnotationBase::CORR] = corr;

	if (_csys.hasSpectralAxis()) {
		SpectralCoordinate spectral = _csys.spectralCoordinate(
			_csys.findCoordinate(Coordinate::SPECTRAL)
		);

		ParamValue frame;
		frame.intVal = spectral.frequencySystem(False);
		_currentGlobals[AnnotationBase::FRAME] = frame;

		ParamValue veltype;
		veltype.intVal = spectral.velocityDoppler();
		_currentGlobals[AnnotationBase::VELTYPE] = veltype;
		cout << "veltype " << MDoppler::showType(veltype.intVal) << endl;

		ParamValue restfreq;
		restfreq.doubleVal = spectral.restFrequency();
		_currentGlobals[AnnotationBase::RESTFREQ] = restfreq;
		cout << "restfreq " << restfreq.doubleVal << endl;
	}
	ParamValue linewidth;
	linewidth.intVal = AnnotationBase::DEFAULT_LINEWIDTH;
	_currentGlobals[AnnotationBase::LINEWIDTH] = linewidth;

	ParamValue linestyle;
	linestyle.stringVal = AnnotationBase::DEFAULT_LINESTYLE;
	_currentGlobals[AnnotationBase::LINESTYLE] = linestyle;

	ParamValue symsize;
	symsize.intVal = AnnotationBase::DEFAULT_SYMBOLSIZE;
	_currentGlobals[AnnotationBase::SYMSIZE] = symsize;

	ParamValue symthick;
	symthick.intVal = AnnotationBase::DEFAULT_SYMBOLTHICKNESS;
	_currentGlobals[AnnotationBase::SYMTHICK] = symthick;

	ParamValue color;
	color.stringVal = AnnotationBase::DEFAULT_COLOR;
	_currentGlobals[AnnotationBase::COLOR] = color;

	ParamValue font;
	font.stringVal = AnnotationBase::DEFAULT_FONT;
	_currentGlobals[AnnotationBase::FONT] = font;

	ParamValue fontsize;
	fontsize.stringVal = AnnotationBase::DEFAULT_FONTSIZE;
	_currentGlobals[AnnotationBase::FONTSIZE] = fontsize;

	ParamValue fontstyle;
	fontstyle.stringVal = AnnotationBase::DEFAULT_FONTSTYLE;
	_currentGlobals[AnnotationBase::FONTSTYLE] = fontstyle;

	ParamValue usetex;
	usetex.boolVal = AnnotationBase::DEFAULT_USETEX;
	_currentGlobals[AnnotationBase::USETEX] = usetex;
}


}

