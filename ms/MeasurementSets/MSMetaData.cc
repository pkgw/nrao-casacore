//# MSMetaData.cc
//# Copyright (C) 1998,1999,2000,2001
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

#include <ms/MeasurementSets/MSMetaData.h>

#include <casa/Arrays/ArrayLogical.h>
#include <casa/Arrays/MaskArrMath.h>
#include <casa/Utilities/GenSort.h>
#include <measures/Measures/MCPosition.h>
#include <measures/Measures/MeasConvert.h>
#include <measures/Measures/MeasTable.h>
#include <ms/MeasurementSets/MeasurementSet.h>
#include <tables/Tables/ScalarColumn.h>
#include <tables/Tables/TableParse.h>
#include <tables/Tables/TableProxy.h>
#include <tables/Tables/TableRecord.h>

#include <iomanip>

// DEBUG ONLY
/*
#include <casa/Arrays/ArrayIO.h>
#include <iomanip>
#include <casa/OS/PrecTimer.h>
*/

#define _ORIGIN "MSMetaData::" + String(__FUNCTION__) + ": "

namespace casa {

MSMetaData::~MSMetaData() {}

// leave intact in this class
std::set<Double> MSMetaData::getTimesForScan(const uInt scan) {
	std::set<uInt> scans;
	scans.insert(scan);
	// scan validity check is done in getTimesForScans()
	return getTimesForScans(scans);
}

std::set<uInt> MSMetaData::getScansForField(const String& field) {
	std::set<uInt> fieldIDs = getFieldIDsForField(field);
	std::set<uInt> scans;
	for (
		std::set<uInt>::const_iterator fieldID=fieldIDs.begin();
		fieldID!=fieldIDs.end(); fieldID++
	) {
		std::set<uInt> myscans = getScansForFieldID(*fieldID);
		scans.insert(myscans.begin(), myscans.end());
	}
	return scans;
}

vector<String> MSMetaData::_getFieldNames(const MeasurementSet& ms) {
	String fieldNameColName = MSField::columnName(MSFieldEnums::NAME);
	ROScalarColumn<String> nameCol(ms.field(), fieldNameColName);
	return nameCol.getColumn().tovector();
}

vector<MPosition> MSMetaData::_getAntennaPositions(
	vector<String>& antennaNames, const MeasurementSet& ms
) {
	String antNameColName = MSAntenna::columnName(MSAntennaEnums::NAME);
	ROScalarColumn<String> nameCol(ms.antenna(), antNameColName);
	antennaNames = nameCol.getColumn().tovector();
	String antPosColName = MSAntenna::columnName(MSAntennaEnums::POSITION);
	ArrayColumn<Double> posCol(ms.antenna(), antPosColName);
	Array<Double> xyz = posCol.getColumn();
	Vector<String> posUnits = posCol.keywordSet().asArrayString("QuantumUnits");
	String sFrame = posCol.keywordSet().asRecord("MEASINFO").asString("Ref");
	MPosition::Types posType = MPosition::getType(sFrame);
	Array<Double>::const_iterator end = xyz.end();
	Quantity x(0, posUnits[0]);
	Quantity y(0, posUnits[1]);
	Quantity z(0, posUnits[2]);
    vector<MPosition> antennaPositions;
	for (Array<Double>::const_iterator iter=xyz.begin(); iter!=end; iter++) {
		x.setValue(*iter);
		Double xm = x.getValue("m");
		iter++;
		y.setValue(*iter);
		Double ym = y.getValue("m");
		iter++;
		z.setValue(*iter);
		Double zm = z.getValue("m");
		MPosition antPos(MVPosition(xm, ym, zm), posType);
		antennaPositions.push_back(antPos);
	}
	return antennaPositions;
}

vector<Quantum<Vector<Double> > > MSMetaData::_getAntennaOffsets(
	const vector<MPosition>& antennaPositions,
	const MPosition& observatoryPosition
) {
	MPosition obsPos = observatoryPosition;
	if (obsPos.type() != MPosition::ITRF) {
		MeasConvert<MPosition> toItrf(obsPos, MPosition::ITRF);
		obsPos = toItrf(obsPos);
	}
	Vector<Double> obsXYZ = obsPos.get("m").getValue();
	Double xo = obsXYZ[0];
	Double yo = obsXYZ[1];
	Double zo = obsXYZ[2];
	Double rObs = sqrt(xo*xo + yo*yo + zo*zo);
	Vector<Double> obsLongLat = obsPos.getAngle("rad").getValue();
	Double longObs = obsLongLat[0];
	Double latObs = obsLongLat[1];
	vector<MPosition>::const_iterator end = antennaPositions.end();
	vector<Quantum<Vector<Double> > > antennaOffsets;
	for (
		vector<MPosition>::const_iterator iter=antennaPositions.begin();
		iter!=end; iter++
	) {
		Vector<Double> xyz = iter->get("m").getValue();
		Double x = xyz[0];
		Double y = xyz[1];
		Double z = xyz[2];
		Double rAnt = sqrt(x*x + y*y + z*z);
		Vector<Double> antLongLat = iter->getAngle("rad").getValue();
		Double longAnt = antLongLat[0];
		Double latAnt = antLongLat[1];
		Vector<Double> offset(3);
		offset[0] = (longAnt - longObs)*rObs*cos(latObs);
		offset[1] = (latAnt - latObs)*rObs;
		offset[2] = rAnt - rObs;
		Quantum<Vector<Double> > qoffset(offset, "m");
		antennaOffsets.push_back(qoffset);
	}
	return antennaOffsets;
}

vector<String> MSMetaData::_getAntennaNames(
	map<String, uInt>& namesToIDs, const MeasurementSet& ms
) {
	String antNameColName = MSAntenna::columnName(MSAntennaEnums::NAME);
	ROScalarColumn<String> nameCol(ms.antenna(), antNameColName);
	Vector<String> names = nameCol.getColumn();
	Vector<String>::const_iterator end = names.end();
	uInt i = 0;
	for (
		Vector<String>::const_iterator name=names.begin();
		name!=end; name++, i++
	) {
		namesToIDs[*name] = i;
	}
	return names.tovector();
}

std::map<uInt, std::set<uInt> > MSMetaData::_getScanToStatesMap(
	const Vector<Int>& scans, const Vector<Int>& states
) {
	Vector<Int>::const_iterator curScan = scans.begin();
	Vector<Int>::const_iterator lastScan = scans.end();
	Vector<Int>::const_iterator curStateID = states.begin();
	std::map<uInt, std::set<uInt> > scanToStatesMap;
	while (curScan != lastScan) {
		scanToStatesMap[*curScan].insert(*curStateID);
		curScan++;
		curStateID++;
	}
	return scanToStatesMap;
}

Vector<Int> MSMetaData::_getStates(const MeasurementSet& ms) {
	String stateColName = MeasurementSet::columnName(MSMainEnums::STATE_ID);
	return ROScalarColumn<Int>(ms, stateColName).getColumn();
}

Vector<Int> MSMetaData::_getObservationIDs(const MeasurementSet& ms) {
	String obsColName = MeasurementSet::columnName(MSMainEnums::OBSERVATION_ID);
	return ROScalarColumn<Int>(ms, obsColName).getColumn();
}

Vector<Int> MSMetaData::_getArrayIDs(const MeasurementSet& ms) {
	String arrColName = MeasurementSet::columnName(MSMainEnums::ARRAY_ID);
	return ROScalarColumn<Int>(ms, arrColName).getColumn();
}

void MSMetaData::_getStateToIntentsMap(
	vector<std::set<String> >& stateToIntentsMap,
	std::set<String>& uniqueIntents, const MeasurementSet& ms
) {
	String intentsColName = MSState::columnName(MSStateEnums::OBS_MODE);
	ROScalarColumn<String> intentsCol(ms.state(), intentsColName);
	Vector<String> intentSets = intentsCol.getColumn();
	stateToIntentsMap.resize(_getNStates(ms));
	Vector<String>::const_iterator end = intentSets.end();
	vector<std::set<String> >::iterator sIter = stateToIntentsMap.begin();
	for(
		Vector<String>::const_iterator curIntentSet=intentSets.begin();
		curIntentSet!=end; curIntentSet++, sIter++
	) {
		Vector<String> intents = casa::stringToVector(*curIntentSet, ',');
		*sIter = std::set <String>(intents.begin(), intents.end());
		uniqueIntents.insert(intents.begin(), intents.end());
	}
}

uInt MSMetaData::_getNStates(const MeasurementSet& ms) {
	return ms.state().nrow();
}

Vector<Int> MSMetaData::_getDataDescIDs(const MeasurementSet& ms) {
	String ddColName = MeasurementSet::columnName(MSMainEnums::DATA_DESC_ID);
	ROScalarColumn<Int> ddCol(ms, ddColName);
	return ddCol.getColumn();
}

vector<uInt> MSMetaData::_getDataDescIDToSpwMap(const MeasurementSet& ms) {
	String spwColName = MSDataDescription::columnName(MSDataDescriptionEnums::SPECTRAL_WINDOW_ID);
	ROScalarColumn<Int> spwCol(ms.dataDescription(), spwColName);
	return _toUIntVector(spwCol.getColumn());
}

Vector<Int> MSMetaData::_getFieldIDs(const MeasurementSet& ms) {
	String fieldIdColName = MeasurementSet::columnName(MSMainEnums::FIELD_ID);
	return ROScalarColumn<Int>(ms, fieldIdColName).getColumn();
}

std::map<uInt, std::set<Double> > MSMetaData::_getScanToTimesMap(
	const Vector<Int>& scans, const Vector<Double>& times
) {
	Vector<Int>::const_iterator curScan = scans.begin();
	Vector<Int>::const_iterator lastScan = scans.end();
	Vector<Double>::const_iterator curTime = times.begin();
	std::map<uInt, std::set<Double> > scanToTimesMap;
	while (curScan != lastScan) {
		scanToTimesMap[*curScan].insert(*curTime);
		curScan++;
		curTime++;
	}
	return scanToTimesMap;
}

Vector<Double> MSMetaData::_getTimes(const MeasurementSet& ms) {
	String timeColName = MeasurementSet::columnName(MSMainEnums::TIME);
	return ScalarColumn<Double>(ms, timeColName).getColumn();
}

Vector<Double> MSMetaData::_getTimeCentroids(const MeasurementSet& ms) {
	String timeCentroidColName = MeasurementSet::columnName(MSMainEnums::TIME_CENTROID);
	return ScalarColumn<Double>(ms, timeCentroidColName).getColumn();
}

Vector<Double> MSMetaData::_getIntervals(const MeasurementSet& ms) {
	String intervalColName = MeasurementSet::columnName(MSMainEnums::INTERVAL);
	return ScalarColumn<Double>(ms, intervalColName).getColumn();
}

Vector<Bool> MSMetaData::_getFlagRows(const MeasurementSet& ms) {
	String flagRowColName = MeasurementSet::columnName(MSMainEnums::FLAG_ROW);
	return ScalarColumn<Bool>(ms, flagRowColName).getColumn();
}


ArrayColumn<Bool>* MSMetaData::_getFlags(const MeasurementSet& ms) {
	String flagColName = MeasurementSet::columnName(MSMainEnums::FLAG);
	return new ArrayColumn<Bool>(ms, flagColName);
}

vector<MSMetaData::SpwProperties>  MSMetaData::_getSpwInfo(
	std::set<uInt>& avgSpw, std::set<uInt>& tdmSpw, std::set<uInt>& fdmSpw,
	std::set<uInt>& wvrSpw, const MeasurementSet& ms
) {
	const MSSpectralWindow spw = ms.spectralWindow();
	String bandwidthColName = MSSpectralWindow::columnName(MSSpectralWindowEnums::TOTAL_BANDWIDTH);
	ScalarColumn<Double> bwCol(spw, bandwidthColName);
	Vector<Double> bws = bwCol.getColumn();
	String cfColName = MSSpectralWindow::columnName(MSSpectralWindowEnums::CHAN_FREQ);
	ArrayColumn<Double> cfCol(spw, cfColName);
	String cwColName = MSSpectralWindow::columnName(MSSpectralWindowEnums::CHAN_WIDTH);
	ArrayColumn<Double> cwCol(spw, cwColName);
	String nsColName = MSSpectralWindow::columnName(MSSpectralWindowEnums::NET_SIDEBAND);
	ScalarColumn<Int> nsCol(spw, nsColName);
	Vector<Int> nss = nsCol.getColumn();
	SpwProperties props;
	vector<Double> freqLimits(2);
	Vector<Double> tmp;
	vector<SpwProperties> spwInfo;
	for (uInt i=0; i<bws.size(); i++) {
		props.bandwidth = bws[i];
		props.chanfreqs.resize(0);
		tmp.resize(0);
		cfCol.get(i, tmp);
		tmp.tovector(props.chanfreqs);
		freqLimits[0] = min(tmp);
		freqLimits[1] = max(tmp);
		props.edgechans = freqLimits;
		tmp.resize(0);
		cwCol.get(i, tmp);
		props.chanwidths = tmp.tovector();
		// coded this way in ValueMapping
		props.netsideband = nss[i] == 2 ? 1 : -1;
		props.meanfreq = mean(tmp);
		tmp.tovector(props.chanfreqs);
		props.nchans = props.chanfreqs.size();
		spwInfo.push_back(props);
		if (props.nchans==64 || props.nchans==128 || props.nchans==256) {
			tdmSpw.insert(i);
		}
		else if (props.nchans==1) {
			avgSpw.insert(i);
		}
		else if (props.nchans==4) {
			wvrSpw.insert(i);
		}
		else {
			fdmSpw.insert(i);
		}
	}
	return spwInfo;
}

Vector<Int> MSMetaData::_getScans(const MeasurementSet& ms) {
	String scanColName = MeasurementSet::columnName(MSMainEnums::SCAN_NUMBER);
	return ROScalarColumn<Int>(ms, scanColName).getColumn();
}

vector<MPosition> MSMetaData::_getObservatoryPositions(
	vector<String>& names, const MeasurementSet& ms
) {
	String tnameColName = MSObservation::columnName(MSObservationEnums::TELESCOPE_NAME);
	ROScalarColumn<String> telescopeNameCol(ms.observation(), tnameColName);
	names = telescopeNameCol.getColumn().tovector();
	vector<MPosition> observatoryPositions(names.size());
	for (uInt i=0; i<observatoryPositions.size(); i++) {
		if (names[i].length() == 0) {
			throw AipsError(
				_ORIGIN
				+ "The name of the telescope is not stored in the measurement set."
			);
		}
		if (! MeasTable::Observatory(observatoryPositions[i], names[i])) {
			throw AipsError(
				_ORIGIN
				+ "The name of the telescope is not stored in the measurement set."
			);
		}
	}
	return observatoryPositions;
}

std::map<Double, Double> MSMetaData::_getTimeToAggregateExposureMap(
	const vector<Double>& times, const vector<Double>& exposures
) {
	vector<Double>::const_iterator tIter = times.begin();
	vector<Double>::const_iterator end = times.end();
	vector<Double>::const_iterator eIter = exposures.begin();
	std::map<Double, vector<Double> > timeToExposuresMap;

	std::map<Double, Double> timeToAggregateExposureMap;
	while (tIter != end) {
		std::map<Double, vector<Double> >::iterator pair = timeToExposuresMap.find(*tIter);
		if (pair == timeToExposuresMap.end()) {
			timeToExposuresMap[*tIter] = vector<Double>(0);
		}
		timeToExposuresMap[*tIter].push_back(*eIter);
		tIter++;
		eIter++;
	}
	std::map<Double, vector<Double> >::const_iterator end1 = timeToExposuresMap.end();
	for (
		std::map<Double, vector<Double> >::const_iterator iter=timeToExposuresMap.begin();
		iter!=end1; iter++
	) {
		timeToAggregateExposureMap[iter->first] = mean(Vector<Double>(iter->second));
	}
	return timeToAggregateExposureMap;
}

Matrix<Bool> MSMetaData::_getUniqueBaselines(
	const Vector<Int>& antenna1, const Vector<Int>& antenna2
) {
	Vector<Int>::const_iterator a1Iter = antenna1.begin();
	Vector<Int>::const_iterator a2Iter = antenna2.begin();
	Vector<Int>::const_iterator end = antenna1.end();
	uInt nAnts = nAntennas();
	Matrix<Bool> baselines(nAnts, nAnts, False);
	while (a1Iter != end) {
		baselines(*a1Iter, *a2Iter) = True;
		baselines(*a2Iter, *a1Iter) = True;
		a1Iter++;
		a2Iter++;
	}
	return baselines;
}

uInt MSMetaData::nBaselines() {
	Matrix<Bool> baselines = getUniqueBaselines();
	for (uInt i=0; i<baselines.nrow(); i++) {
		// discard autocorrelation "baselines" for calculation
		baselines(i, i) = False;
	}
	return ntrue(baselines)/2;
}

Quantity MSMetaData::_getTotalExposureTime(
	const MeasurementSet& ms, const std::map<Double, Double>& timeToBWMap,
	const vector<SpwProperties>& spwProperties,
	const vector<uInt>& dataDescToSpwIdMap
) {
	uInt nAnts = nAntennas();
	uInt maxNBaselines = nAnts*(nAnts-1)/2;
	Double totalExposure = 0;
	String taql = "select FLAG, DATA_DESC_ID, EXPOSURE, TIME from "
		+ ms.tableName() + " where ANTENNA1 != ANTENNA2 and FLAG_ROW==True";
	Table result(tableCommand(taql));
	Vector<Int> ddIDs = ScalarColumn<Int>(result, "DATA_DESC_ID").getColumn();
	Vector<Double> exposures = ScalarColumn<Double>(result, "EXPOSURE").getColumn();
	Vector<Double> times = ScalarColumn<Double>(result, "TIME").getColumn();
	// each row represents a unique baseline, data description ID, and time combination
	uInt nrows = result.nrow();
	for (uInt i=0; i<nrows; i++) {
		Vector<Double> channelWidths(
			Vector<Double>(
				spwProperties[dataDescToSpwIdMap[ddIDs[i]]].chanwidths
			)
		);
		Matrix<Bool> flagsMatrix(ArrayColumn<Bool>(result, "FLAG").get(i));
		uInt nCorrelations = flagsMatrix.nrow();
		Double denom = (timeToBWMap.find(times[i])->second)*maxNBaselines*nCorrelations;
		for (uInt corr=0; corr<nCorrelations; corr++) {
			MaskedArray<Double> flaggedChannelWidths(
				channelWidths, flagsMatrix.row(corr), True
			);
			Double effectiveBW = sum(flaggedChannelWidths);
			totalExposure += exposures[i]*effectiveBW/denom;
		}
	}
	String unit = ScalarColumn<Double>(ms, "EXPOSURE").keywordSet().asArrayString("QuantumUnits").tovector()[0];
	return Quantity(totalExposure, unit);
}

std::map<Double, Double> MSMetaData::_getTimeToTotalBWMap(
	const Vector<Double>& times, const Vector<Int>& ddIDs,
	const vector<uInt>& dataDescIDToSpwMap,
	const vector<MSMetaData::SpwProperties>& spwInfo
) {
	std::map<Double, Double> timeToBWMap;
	std::map<Double,std::set<uInt> > timeToDDIDMap;
	Vector<Double>::const_iterator end = times.end();
	Vector<Double>::const_iterator tIter = times.begin();
	Vector<Int>::const_iterator dIter = ddIDs.begin();
	while (tIter!=end) {
		timeToDDIDMap[*tIter].insert(*dIter);
		tIter++;
		dIter++;
	}
	std::map<Double, std::set<uInt> >::const_iterator end1 = timeToDDIDMap.end();
	for (
		std::map<Double,std::set<uInt> >::const_iterator iter=timeToDDIDMap.begin();
		iter!=end1; iter++
	) {
		std::set<uInt> ddIDs = iter->second;
		timeToBWMap[iter->first] = 0;
		std::set<uInt>::const_iterator end2 = ddIDs.end();
		for (
			std::set<uInt>::const_iterator dIter=ddIDs.begin();
			dIter!=end2; dIter++
		) {
			timeToBWMap[iter->first] += spwInfo[dataDescIDToSpwMap[*dIter]].bandwidth;
		}
	}
	return timeToBWMap;
}

std::map<Int, vector<Double> > MSMetaData::_getScanToTimeRangeMap(
	const Vector<Int>& scans, const Vector<Double>& timeCentroids,
	const Vector<Double>& intervals
) {
	std::map<Int, vector<Double> > out;
	Vector<Int>::const_iterator sIter = scans.begin();
	Vector<Int>::const_iterator sEnd = scans.end();
	Vector<Double>::const_iterator  tIter = timeCentroids.begin();
	Vector<Double>::const_iterator  iIter = intervals.begin();
	while (sIter != sEnd) {
		Double half = *iIter/2;
		if (out.find(*sIter) == out.end()) {
			out[*sIter] = vector<Double>(2, *tIter-half);
			out[*sIter][1] = *tIter+half;
		}
		else {
			out[*sIter][0] = min(out[*sIter][0], *tIter-half);
			out[*sIter][1] = max(out[*sIter][1], *tIter+half);
		}
		sIter++;
		tIter++;
		iIter++;
	}

	return out;
}


void MSMetaData::_getAntennas(
	Vector<Int>& ant1, Vector<Int>& ant2, const MeasurementSet& ms
) {
	String ant1ColName = MeasurementSet::columnName(MSMainEnums::ANTENNA1);
	ROScalarColumn<Int> ant1Col(ms, ant1ColName);
	ant1 = ant1Col.getColumn();
	String ant2ColName = MeasurementSet::columnName(MSMainEnums::ANTENNA2);
	ROScalarColumn<Int> ant2Col(ms, ant2ColName);
	ant2 = ant2Col.getColumn();
}

void MSMetaData::_getRowStats(
	uInt& nACRows, uInt& nXCRows,
	AOSFMapI& scanToNACRowsMap,
	AOSFMapI& scanToNXCRowsMap,
	vector<uInt>& fieldToNACRowsMap,
	vector<uInt>& fieldToNXCRowsMap,
	const Vector<Int>& ant1, const Vector<Int>& ant2,
	const Vector<Int>& scans, const Vector<Int> fieldIDs,
	const Vector<Int>& obsIDs, const Vector<Int>& arrIDs
) {
	nACRows = 0;
	nXCRows = 0;
	std::set<uInt> scanNumbers = getScanNumbers();
	std::set<uInt>::const_iterator lastScan = scanNumbers.end();
	scanToNACRowsMap.clear();
	scanToNXCRowsMap.clear();
	uInt nObs = nObservations();
	uInt nArr = nArrays();
	uInt nfields = nFields();
	for (uInt arrID=0; arrID<nArr; arrID++) {
		for (uInt obsID=0; obsID<nObs; obsID++) {
			for (
				std::set<uInt>::const_iterator scanNum=scanNumbers.begin();
					scanNum!=lastScan; scanNum++
			) {
				scanToNACRowsMap[arrID][obsID][*scanNum] = vector<uInt>(nfields, 0);
				scanToNXCRowsMap[arrID][obsID][*scanNum] = vector<uInt>(nfields, 0);
			}
		}
	}
	fieldToNACRowsMap = vector<uInt>(nfields, 0);
	fieldToNXCRowsMap = vector<uInt>(nfields, 0);
	Vector<Int>::const_iterator aEnd = ant1.end();
	Vector<Int>::const_iterator a1Iter = ant1.begin();
	Vector<Int>::const_iterator a2Iter = ant2.begin();
	Vector<Int>::const_iterator sIter = scans.begin();
	Vector<Int>::const_iterator fIter = fieldIDs.begin();
	Vector<Int>::const_iterator oIter = obsIDs.begin();
	Vector<Int>::const_iterator arIter = arrIDs.begin();

	while (a1Iter!=aEnd) {
		if (*a1Iter == *a2Iter) {
			nACRows++;
			scanToNACRowsMap[*arIter][*oIter][*sIter][*fIter]++;
			fieldToNACRowsMap[*fIter]++;
		}
		else {
			nXCRows++;
			scanToNXCRowsMap[*arIter][*oIter][*sIter][*fIter]++;
			fieldToNXCRowsMap[*fIter]++;
		}
		a1Iter++;
		a2Iter++;
		sIter++;
		fIter++;
		arIter++;
		oIter++;
	}
}

void MSMetaData::_getUnflaggedRowStats(
	Double& nACRows, Double& nXCRows,
	vector<Double>& fieldNACRows, vector<Double>& fieldNXCRows,
	AOSFMapD& scanNACRows,
	AOSFMapD& scanNXCRows,
	const Vector<Int>& ant1, const Vector<Int>& ant2,
	const Vector<Bool>& flagRow, const Vector<Int>& dataDescIDs,
	const vector<uInt>& dataDescIDToSpwMap,
	const vector<SpwProperties>& spwInfo,
	const ArrayColumn<Bool>& flags,
	const Vector<Int>& fieldIDs, const Vector<Int>& scans,
	const Vector<Int>& obsIDs, const Vector<Int>& arIDs
) {
	nACRows = 0;
	nXCRows = 0;
	uInt nfields = nFields();
	fieldNACRows = vector<Double>(nfields, 0);
	fieldNXCRows = vector<Double>(nfields, 0);
	scanNACRows.clear();
	scanNXCRows.clear();
	std::set<uInt> scanNumbers = getScanNumbers();
	std::set<uInt>::const_iterator lastScan = scanNumbers.end();
	uInt nObs = nObservations();
	uInt nArr = nArrays();
	for (uInt arrID=0; arrID<nArr; arrID++) {
		for (uInt obsID=0; obsID<nObs; obsID++) {
			for (
				std::set<uInt>::const_iterator scanNum=scanNumbers.begin();
				scanNum!=lastScan; scanNum++
			) {
				scanNACRows[arrID][obsID][*scanNum] = vector<Double>(nfields, 0);
				scanNXCRows[arrID][obsID][*scanNum] = vector<Double>(nfields, 0);
			}
		}
	}

	Vector<Int>::const_iterator aEnd = ant1.end();
	Vector<Int>::const_iterator a1Iter = ant1.begin();
	Vector<Int>::const_iterator a2Iter = ant2.begin();
	Vector<Int>::const_iterator sIter = scans.begin();
	Vector<Int>::const_iterator fIter = fieldIDs.begin();
	Vector<Int>::const_iterator oIter = obsIDs.begin();
	Vector<Int>::const_iterator arIter = arIDs.begin();
	Vector<Int>::const_iterator dIter = dataDescIDs.begin();
	Vector<Bool>::const_iterator flagIter = flagRow.begin();
	uInt i = 0;
    uInt64 count = 0;
	while (a1Iter!=aEnd) {
		if (*flagIter) {
			SpwProperties spwProp = spwInfo[dataDescIDToSpwMap[*dIter]];
			Vector<Double> channelWidths(
				Vector<Double>(spwProp.chanwidths)
			);
			const Matrix<Bool>& flagsMatrix(flags.get(i));
            count += flagsMatrix.size();
            Double x = 0;
            if (allTrue(flagsMatrix)) {
                // all channels are unflagged
                x = 1;
            }
            else if (! anyTrue(flagsMatrix)) {
                // do nothing. All channels are flagged for this row
                continue;
            }
            else {
                // some channels are flagged, some aren't
			    uInt nCorrelations = flagsMatrix.nrow();
			    Double denom = spwProp.bandwidth*nCorrelations;
			    Double bwSum = 0;

			    for (uInt corr=0; corr<nCorrelations; corr++) {
                    Vector<Bool> corrRow = flagsMatrix.row(corr);
                    if (allTrue(corrRow)) {
                        // all channels for this correlation are unflagged
                        bwSum += spwProp.bandwidth;
                    }
                    else if (! anyTrue(corrRow)) {
                        // do nothing, all channels for this correlation
                        // have been flagged
                        continue;
                    }
                    else {
                        // some channels are flagged for this correlation, some aren't
                        MaskedArray<Double> unFlaggedChannelWidths(
	    				    channelWidths, corrRow, True
		    		    );
                        bwSum += sum(unFlaggedChannelWidths);
                    }
			    }
			    x = bwSum/denom;
            }
			if (*a1Iter == *a2Iter) {
				fieldNACRows[*fIter] += x;
				scanNACRows[*arIter][*oIter][*sIter][*fIter] += x;
			}
			else {
				fieldNXCRows[*fIter]+= x;
				scanNXCRows[*arIter][*oIter][*sIter][*fIter] += x;
			}
		}
		a1Iter++;
		a2Iter++;
		sIter++;
		fIter++;
		arIter++;
		oIter++;
		flagIter++;
		dIter++;
		i++;
	}
	nACRows = sum(Vector<Double>(fieldNACRows));
	nXCRows = sum(Vector<Double>(fieldNXCRows));

}

void MSMetaData::_getUnflaggedRowStats(
	Double& nACRows, Double& nXCRows,
	vector<Double>& fieldNACRows, vector<Double>& fieldNXCRows,
	AOSFMapD& scanNACRows,
	AOSFMapD& scanNXCRows,
	const vector<uInt>& dataDescIDToSpwMap,
	const vector<SpwProperties>& spwInfo,
	const MeasurementSet& ms
) {
	String taql = "select FLAG, ARRAY_ID, OBSERVATION_ID, DATA_DESC_ID, ANTENNA1, ANTENNA2, SCAN_NUMBER, FIELD_ID, FLAG_ROW from "
		+ ms.tableName() + " where FLAG_ROW==True";
	Table result(tableCommand(taql));
	std::auto_ptr<ArrayColumn<Bool> > flags(
		new ArrayColumn<Bool>(result, "FLAG")
	);
	_getUnflaggedRowStats(
		nACRows, nXCRows, fieldNACRows, fieldNXCRows,
		scanNACRows, scanNXCRows,
		ScalarColumn<Int>(result, "ANTENNA1").getColumn(),
		ScalarColumn<Int>(result, "ANTENNA2").getColumn(),
		ScalarColumn<Bool>(result, "FLAG_ROW").getColumn(),
		ScalarColumn<Int>(result, "DATA_DESC_ID").getColumn(),
		dataDescIDToSpwMap, spwInfo, *flags,
		ScalarColumn<Int>(result, "FIELD_ID").getColumn(),
		ScalarColumn<Int>(result, "SCAN_NUMBER").getColumn(),
		ScalarColumn<Int>(result, "OBSERVATION_ID").getColumn(),
		ScalarColumn<Int>(result, "ARRAY_ID").getColumn()
	);
}

vector<uInt> MSMetaData::_toUIntVector(const Vector<Int>& v) {
	if (anyLT(v, 0)) {
		throw AipsError("Column that should contain nonnegative ints has a negative int");
	}
	vector<uInt> u;
	for (Vector<Int>::const_iterator iter=v.begin(); iter!=v.end(); iter++) {
		u.push_back(*iter);
	}
	return u;
}

void MSMetaData::_checkTolerance(const Double tol) {
	if (tol < 0) {
		throw AipsError(
			"MSMetaData::" + String(__FUNCTION__)
			+ " : Tolerance cannot be less than zero"
		);
	}
}

}

