//# ExprMathNode.cc: Nodes representing scalar mathematical operators in table select expression tree
//# Copyright (C) 1994,1995,1996,1997,1998,1999,2000
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
//# $Id: ExprMathNode.cc 19796 2006-12-19 05:12:59Z gvandiep $

#include <tables/Tables/ExprMathNode.h>
#include <tables/Tables/ExprUnitNode.h>
#include <tables/Tables/TableError.h>
#include <casa/Quanta/MVTime.h>


namespace casa { //# NAMESPACE CASA - BEGIN

// Implement the arithmetic operators for each data type.

TableExprNodePlus::TableExprNodePlus (NodeDataType dt,
				      const TableExprNodeRep& node)
: TableExprNodeBinary (dt, node, OtPlus)
{}
TableExprNodePlus::~TableExprNodePlus()
{}

TableExprNodePlusDouble::TableExprNodePlusDouble (const TableExprNodeRep& node)
: TableExprNodePlus (NTDouble, node)
{}
TableExprNodePlusDouble::~TableExprNodePlusDouble()
{}
Double TableExprNodePlusDouble::getDouble (const TableExprId& id)
    { return lnode_p->getDouble(id) + rnode_p->getDouble(id); }
DComplex TableExprNodePlusDouble::getDComplex (const TableExprId& id)
    { return lnode_p->getDouble(id) + rnode_p->getDouble(id); }

TableExprNodePlusDComplex::TableExprNodePlusDComplex (const TableExprNodeRep& node)
: TableExprNodePlus (NTComplex, node)
{}
TableExprNodePlusDComplex::~TableExprNodePlusDComplex()
{}
DComplex TableExprNodePlusDComplex::getDComplex (const TableExprId& id)
    { return lnode_p->getDComplex(id) + rnode_p->getDComplex(id); }

TableExprNodePlusString::TableExprNodePlusString (const TableExprNodeRep& node)
: TableExprNodePlus (NTString, node)
{}
TableExprNodePlusString::~TableExprNodePlusString()
{}
String TableExprNodePlusString::getString (const TableExprId& id)
    { return lnode_p->getString(id) + rnode_p->getString(id); }

TableExprNodePlusDate::TableExprNodePlusDate (const TableExprNodeRep& node)
: TableExprNodePlus (NTDate, node)
{}
TableExprNodePlusDate::~TableExprNodePlusDate()
{}
void TableExprNodePlusDate::handleUnits()
{
    if (lnode_p->dataType() == NTDouble) {
        TableExprNodeUnit::adaptUnit (lnode_p, "d");
    } else if (rnode_p->dataType() == NTDouble) {
        TableExprNodeUnit::adaptUnit (rnode_p, "d");
    }
}
MVTime TableExprNodePlusDate::getDate(const TableExprId& id)
{ return lnode_p->getDouble(id) + rnode_p->getDouble(id); }
Double TableExprNodePlusDate::getDouble(const TableExprId& id)
{ return lnode_p->getDouble(id) + rnode_p->getDouble(id); }


TableExprNodeMinus::TableExprNodeMinus (NodeDataType dt,
					const TableExprNodeRep& node)
: TableExprNodeBinary (dt, node, OtMinus)
{}
TableExprNodeMinus::~TableExprNodeMinus()
{}

TableExprNodeMinusDouble::TableExprNodeMinusDouble (const TableExprNodeRep& node)
: TableExprNodeMinus (NTDouble, node)
{}
TableExprNodeMinusDouble::~TableExprNodeMinusDouble()
{}
void TableExprNodeMinusDouble::handleUnits()
{
    if (lnode_p->dataType() == NTDate  &&  rnode_p->dataType() == NTDate) {
        setUnit("d");                     //# date-date results in days
    } else {
        TableExprNodeBinary::handleUnits();
    }
}
Double TableExprNodeMinusDouble::getDouble (const TableExprId& id)
    { return lnode_p->getDouble(id) - rnode_p->getDouble(id); }
DComplex TableExprNodeMinusDouble::getDComplex (const TableExprId& id)
    { return lnode_p->getDouble(id) - rnode_p->getDouble(id); }

TableExprNodeMinusDComplex::TableExprNodeMinusDComplex (const TableExprNodeRep& node)
: TableExprNodeMinus (NTComplex, node)
{}
TableExprNodeMinusDComplex::~TableExprNodeMinusDComplex()
{}
DComplex TableExprNodeMinusDComplex::getDComplex (const TableExprId& id)
    { return lnode_p->getDComplex(id) - rnode_p->getDComplex(id); }

TableExprNodeMinusDate::TableExprNodeMinusDate (const TableExprNodeRep& node)
: TableExprNodeMinus (NTDate, node)
{}
TableExprNodeMinusDate::~TableExprNodeMinusDate()
{}
void TableExprNodeMinusDate::handleUnits()
{
    // Right hand side must be in days.
    TableExprNodeUnit::adaptUnit (rnode_p, "d");
}
MVTime TableExprNodeMinusDate::getDate(const TableExprId& id)
    { return lnode_p->getDouble(id) - rnode_p->getDouble(id); }
Double TableExprNodeMinusDate::getDouble(const TableExprId& id)
    { return lnode_p->getDouble(id) - rnode_p->getDouble(id); }


TableExprNodeTimes::TableExprNodeTimes (NodeDataType dt,
					const TableExprNodeRep& node)
: TableExprNodeBinary (dt, node, OtTimes)
{}
TableExprNodeTimes::~TableExprNodeTimes()
{}
void TableExprNodeTimes::handleUnits()
{
    if (lnode_p->unit().empty()) {
        setUnit (rnode_p->unit());
    } else if (rnode_p->unit().empty()) {
        setUnit (lnode_p->unit());
    } else {
        Quantity q1 (1, lnode_p->unit());
	Quantity q2 (1, rnode_p->unit());
	setUnit ((q1*q2).getFullUnit());
    }
}

TableExprNodeTimesDouble::TableExprNodeTimesDouble (const TableExprNodeRep& node)
: TableExprNodeTimes (NTDouble, node)
{}
TableExprNodeTimesDouble::~TableExprNodeTimesDouble()
{}
Double TableExprNodeTimesDouble::getDouble (const TableExprId& id)
    { return lnode_p->getDouble(id) * rnode_p->getDouble(id); }
DComplex TableExprNodeTimesDouble::getDComplex (const TableExprId& id)
    { return lnode_p->getDouble(id) * rnode_p->getDouble(id); }

TableExprNodeTimesDComplex::TableExprNodeTimesDComplex (const TableExprNodeRep& node)
: TableExprNodeTimes (NTComplex, node)
{}
TableExprNodeTimesDComplex::~TableExprNodeTimesDComplex()
{}
DComplex TableExprNodeTimesDComplex::getDComplex (const TableExprId& id)
    { return lnode_p->getDComplex(id) * rnode_p->getDComplex(id); }


TableExprNodeDivide::TableExprNodeDivide (NodeDataType dt,
					  const TableExprNodeRep& node)
: TableExprNodeBinary (dt, node, OtDivide)
{}
TableExprNodeDivide::~TableExprNodeDivide()
{}
void TableExprNodeDivide::handleUnits()
{
    if (lnode_p->unit().empty()) {
        setUnit (rnode_p->unit());
    } else if (rnode_p->unit().empty()) {
        // For backward compatibility dividing seconds by 86400 is a
        // conversion to days.
        if (rnode_p->isConstant()  &&  rnode_p->dataType() == NTDouble
        &&  rnode_p->getDouble(0) == 86400.
        &&  lnode_p->unit().getName() == "s") {
	    setUnit ("d");
	} else {
	    setUnit (lnode_p->unit());
	}
    } else {
        Quantity q1 (1, lnode_p->unit());
	Quantity q2 (1, rnode_p->unit());
	setUnit ((q1/q2).getFullUnit());
    }
}

TableExprNodeDivideDouble::TableExprNodeDivideDouble (const TableExprNodeRep& node)
: TableExprNodeDivide (NTDouble, node)
{}
TableExprNodeDivideDouble::~TableExprNodeDivideDouble()
{}
Double TableExprNodeDivideDouble::getDouble (const TableExprId& id)
    { return lnode_p->getDouble(id) / rnode_p->getDouble(id); }
DComplex TableExprNodeDivideDouble::getDComplex (const TableExprId& id)
    { return lnode_p->getDouble(id) / rnode_p->getDouble(id); }

TableExprNodeDivideDComplex::TableExprNodeDivideDComplex (const TableExprNodeRep& node)
: TableExprNodeDivide (NTComplex, node)
{}
TableExprNodeDivideDComplex::~TableExprNodeDivideDComplex()
{}
DComplex TableExprNodeDivideDComplex::getDComplex (const TableExprId& id)
    { return lnode_p->getDComplex(id) / rnode_p->getDComplex(id); }


TableExprNodeModulo::TableExprNodeModulo (NodeDataType dt,
					  const TableExprNodeRep& node)
: TableExprNodeBinary (dt, node, OtModulo)
{}
TableExprNodeModulo::~TableExprNodeModulo()
{}
void TableExprNodeModulo::handleUnits()
{
    setUnit (lnode_p->unit());
}

TableExprNodeModuloDouble::TableExprNodeModuloDouble (const TableExprNodeRep& node)
: TableExprNodeModulo (NTDouble, node)
{}
TableExprNodeModuloDouble::~TableExprNodeModuloDouble()
{}
Double TableExprNodeModuloDouble::getDouble (const TableExprId& id)
    { return std::fmod (lnode_p->getDouble(id), rnode_p->getDouble(id)); }
DComplex TableExprNodeModuloDouble::getDComplex (const TableExprId& id)
    { return std::fmod (lnode_p->getDouble(id), rnode_p->getDouble(id)); }


TableExprNodeMIN::TableExprNodeMIN (const TableExprNodeRep& node)
: TableExprNodeBinary (node.dataType(), node, OtMIN)
{}
TableExprNodeMIN::~TableExprNodeMIN()
{}
Double TableExprNodeMIN::getDouble (const TableExprId& id)
    { return -(lnode_p->getDouble(id)); }
DComplex TableExprNodeMIN::getDComplex (const TableExprId& id)
    { return -(lnode_p->getDComplex(id)); }


} //# NAMESPACE CASA - END
