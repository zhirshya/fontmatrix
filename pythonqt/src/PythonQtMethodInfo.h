#ifndef _PYTHONQTMETHODINFO_H
#define _PYTHONQTMETHODINFO_H

/*
 *
 *  Copyright (C) 2006 MeVis Research GmbH All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  Further, this software is distributed without any warranty that it is
 *  free of the rightful claim of any third person regarding infringement
 *  or the like.  Any license provided herein, whether implied or
 *  otherwise, applies only to this software file.  Patent licenses, if
 *  any, provided herein do not apply to combinations of this program with
 *  other software, or any other product whatsoever.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Contact information: MeVis Research GmbH, Universitaetsallee 29,
 *  28359 Bremen, Germany or:
 *
 *  http://www.mevis.de
 *
 */

//----------------------------------------------------------------------------------
/*!
// \file    PythonQtMethodInfo.h
// \author  Florian Link
// \author  Last changed by $Author: florian $
// \date    2006-05
*/
//----------------------------------------------------------------------------------

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMetaMethod>

//! stores information about a specific signal/slot/method
class PythonQtMethodInfo
{
public:
  //! this is a funny type union of QMetaType and QVariant, only god knows
  //! why QMetaType do not support identical types in Qt4,
  //! especial the support of Long, Short, Char etc. is missing in QVariant
  //! and QMetaType does not know anything about most variant types and e.g. LongLong
  enum ParameterType {
    Unknown = -1,
    Variant = -2
  };

  //! stores the QVariant id (if available) and the name of the type
  struct ParameterInfo {
    int typeId; // a mixture from QMetaType and ParameterType
    QByteArray name;
    bool isPointer;
    bool isConst;
  };

  PythonQtMethodInfo() {};
  ~PythonQtMethodInfo() {};
  PythonQtMethodInfo(const QMetaMethod& meta);
  PythonQtMethodInfo(const PythonQtMethodInfo& other) {
    _parameters = other._parameters;
  }

  //! returns the method info of the signature, uses a cache internally to speed up
  //! multiple requests for the same method
  static const PythonQtMethodInfo* getCachedMethodInfo(const QMetaMethod& method);

  //! cleanup the cache
  static void cleanupCachedMethodInfos();

  //! returns the number of parameters (without the return value)
  int  parameterCount() const { return _parameters.size(); };

  //! returns the id for the given type (using an internal dictionary)
  static int nameToType(const char* name);

  //! get the parameter infos
  const QList<ParameterInfo>& parameters() const { return _parameters; }

  //! add an alias for a typename, e.g. QObjectList and QList<QObject*>.
  static void addParameterTypeAlias(const QByteArray& alias, const QByteArray& name);

  protected:
  static void fillParameterInfo(ParameterInfo& type, const QByteArray& name);

  static QHash<QByteArray, int> _parameterTypeDict;
  static QHash<QByteArray, QByteArray> _parameterNameAliases;

  //! stores the cached signatures of methods to speedup mapping from Qt to Python types
  static QHash<QByteArray, PythonQtMethodInfo*> _cachedSignatures;

  QList<ParameterInfo> _parameters;
};

//! stores information about a slot, including a next pointer to overloaded slots
class PythonQtSlotInfo : public PythonQtMethodInfo
{
public:
  enum Type {
    MemberSlot, InstanceDecorator, ClassDecorator
  };

  PythonQtSlotInfo(const PythonQtSlotInfo& info):PythonQtMethodInfo() {
    _meta = info._meta;
    _parameters = info._parameters;
    _slotIndex = info._slotIndex;
    _next = NULL;
    _decorator = info._decorator;
    _type = info._type;
  }

  PythonQtSlotInfo(const QMetaMethod& meta, int slotIndex, QObject* decorator = NULL, Type type = MemberSlot ):PythonQtMethodInfo()
  { 
    const PythonQtMethodInfo* info = getCachedMethodInfo(meta);
    _meta = meta;
    _parameters = info->parameters();
    _slotIndex = slotIndex;
    _next = NULL;
    _decorator = decorator;
    _type = type;
  }


public:
  const QMetaMethod* metaMethod() const { return &_meta; }

  //! get the index of the slot (needed for qt_metacall)
  int slotIndex() const { return _slotIndex; }

  //! get next overloaded slot (which has the same name)
  PythonQtSlotInfo* nextInfo()  const { return _next; }

  //! set the next overloaded slot
  void setNextInfo(PythonQtSlotInfo* next) { _next = next; }

  //! returns if the slot is a decorator slot
  bool isInstanceDecorator() { return _decorator!=NULL && _type == InstanceDecorator; }

  //! returns if the slot is a constructor slot
  bool isClassDecorator() { return _decorator!=NULL && _type == ClassDecorator; }

  QObject* decorator() { return _decorator; }

  //! get the full signature including return type
  QString fullSignature(bool skipFirstArg);

  //! get the short slot name
  QByteArray slotName();

private:
  int  _slotIndex;
  PythonQtSlotInfo* _next;
  QObject* _decorator;
  Type _type;
  QMetaMethod       _meta;
};


#endif
