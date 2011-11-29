/*
 * Copyright (C) 2005-2008 by Dr. Marc Boris Duerner
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <cxxtools/serializationinfo.h>

namespace cxxtools {

SerializationInfo::SerializationInfo()
: _parent(0)
, _category(Value)
, _t(t_none)
{ }


SerializationInfo::SerializationInfo(const SerializationInfo& si)
: _parent(si._parent)
, _category(si._category)
, _name(si._name)
, _type(si._type)
, _id(si._id)
, _u(si._u)
, _nodes(si._nodes)
, _t(si._t)
{
    switch (_t)
    {
        case t_string:  new (_StringPtr()) String(*si._StringPtr());
                        break;

        case t_string8: new (_String8Ptr()) std::string(*si._String8Ptr());
                        break;

    }
}


SerializationInfo& SerializationInfo::operator=(const SerializationInfo& si)
{
    _parent = si._parent;
    _category = si._category;
    _name = si._name;
    _id = si._id;
    _type = si._type;
    _nodes = si._nodes;

    if (si._t == t_string)
        _setString( *si._StringPtr() );
    else if (si._t == t_string8)
        _setString8( *si._String8Ptr() );
    else
    {
        _releaseValue();
        _u = si._u;
        _t = si._t;
    }

    return *this;
}


void SerializationInfo::reserve(size_t n)
{
    _nodes.reserve(n);
}


SerializationInfo& SerializationInfo::addMember(const std::string& name)
{
    SerializationInfo info;
    _nodes.push_back( info );
    _nodes.back().setParent(*this);
    _nodes.back().setName(name);

    // category Array overrides Object (is this a hack?)
    // This is needed for xmldeserialization. In the xml file the root node of a array
    // has a category attribute. When the serializationinfo of the array is created
    // it is known, that it is of category Array. When the members of the array are read,
    // they should not make an object out of the array.
    if (_category != Array)
        _category = Object;

    return _nodes.back();
}


SerializationInfo::Iterator SerializationInfo::begin()
{
    if(_nodes.size() == 0)
        return 0;

    return &( _nodes[0] );
}


SerializationInfo::Iterator SerializationInfo::end()
{
    if(_nodes.size() == 0)
        return 0;

    return &_nodes[0] + _nodes.size();
}


SerializationInfo::ConstIterator SerializationInfo::begin() const
{
    if(_nodes.size() == 0)
        return 0;

    return &( _nodes[0] );
}


SerializationInfo::ConstIterator SerializationInfo::end() const
{
    if(_nodes.size() == 0)
        return 0;

    return &_nodes[0] + _nodes.size();
}


const SerializationInfo& SerializationInfo::getMember(const std::string& name) const
{
    Nodes::const_iterator it = _nodes.begin();
    for(; it != _nodes.end(); ++it)
    {
        if( it->name() == name )
            return *it;
    }

    throw SerializationError("Missing info for '" + name + "'", CXXTOOLS_SOURCEINFO);
}


const SerializationInfo* SerializationInfo::findMember(const std::string& name) const
{
    Nodes::const_iterator it = _nodes.begin();
    for(; it != _nodes.end(); ++it)
    {
        if( it->name() == name )
            return &(*it);
    }

    return 0;
}


SerializationInfo* SerializationInfo::findMember(const std::string& name)
{
    Nodes::iterator it = _nodes.begin();
    for(; it != _nodes.end(); ++it)
    {
        if( it->name() == name )
            return &(*it);
    }

    return 0;
}

void SerializationInfo::clear()
{
    _category = Void;
    _name.clear();
    _type.clear();
    _id.clear();
    _nodes.clear();
    switch (_t)
    {
        case t_string: _StringPtr()->clear(); break;
        case t_string8: _String8Ptr()->clear(); break;
        default: _t = t_none; break;
    }
}

void SerializationInfo::swap(SerializationInfo& si)
{
    if (this == &si)
        return;

    std::swap(_parent, si._parent);
    std::swap(_category, si._category);
    std::swap(_name, si._name);
    std::swap(_type, si._type);
    std::swap(_id, si._id);

    if (_t == t_string)
    {
        if (si._t == t_string)
        {
            // this: String, other: String
            _StringPtr()->swap(*si._StringPtr());
        }
        else if (si._t == t_string8)
        {
            // this: String, other: std::string
            std::string s;
            si._String8Ptr()->swap(s);
            si.setValue(String());
            si._StringPtr()->swap(*_StringPtr());
            setValue(s);
        }
        else
        {
            // this: String, other: something
            U u = si._u;
            T t = si._t;
            si.setValue(String());
            _StringPtr()->swap(*si._StringPtr());
            _releaseValue();
            _u = u;
            _t = t;
        }
    }
    else if (_t == t_string8)
    {
        if (si._t == t_string)
        {
            // this: std::string, other: String
            String s;
            si._StringPtr()->swap(s);
            si.setValue(std::string());
            _String8Ptr()->swap(*si._String8Ptr());
            setValue(s);
        }
        else if (si._t == t_string8)
        {
            // this: std::string, other: std::string
            _String8Ptr()->swap(*si._String8Ptr());
        }
        else
        {
            // this: std::string, other: something
            U u = si._u;
            T t = si._t;
            si.setValue(*_String8Ptr());
            _releaseValue();
            _u = u;
            _t = t;
        }
    }
    else
    {
        if (si._t == t_string)
        {
            // this: something, other: String
            U u = _u;
            T t = _t;
            setValue(*si._StringPtr());
            si._releaseValue();
            si._u = u;
            si._t = t;
        }
        else if (si._t == t_string8)
        {
            // this: something, other: std::string
            U u = _u;
            T t = _t;
            setValue(*si._String8Ptr());
            si._releaseValue();
            si._u = u;
            si._t = t;
        }
        else
        {
            // this: something, other: something
            std::swap(_u, si._u);
            std::swap(_t, si._t);
        }
    }

    _nodes.swap(si._nodes);
}

void SerializationInfo::_releaseValue()
{
    switch (_t)
    {
        case t_string: _StringPtr()->~String(); break;
        case t_string8: _String8Ptr()->std::string::~string(); break;
    }
    _t = t_none;
}

void SerializationInfo::_setString(const String& value)
{
    if (_t != t_string)
    {
        _releaseValue();
        new (_StringPtr()) String(value);
        _t = t_string;
    }
    else
    {
        _StringPtr()->assign(value);
    }

    _category = Value;
}

void SerializationInfo::_setString8(const std::string& value)
{
    if (_t != t_string8)
    {
        _releaseValue();
        new (_String8Ptr()) std::string(value);
        _t = t_string8;
    }
    else
    {
        _String8Ptr()->assign(value);
    }

    _category = Value;
}

void SerializationInfo::_setString8(const char* value)
{
    if (_t != t_string8)
    {
        _releaseValue();
        new (_String8Ptr()) std::string(value);
        _t = t_string8;
    }
    else
    {
        _String8Ptr()->assign(value);
    }

    _category = Value;
}

void SerializationInfo::_setInt(LongInt value)
{
    if (_t != t_int)
    {
        _releaseValue();
        _t = t_int;
    }

    _u._i = value;

    _category = Value;
}

void SerializationInfo::_setUInt(ULongInt value)
{
    if (_t != t_uint)
    {
        _releaseValue();
        _t = t_uint;
    }

    _u._u= value;

    _category = Value;
}

void SerializationInfo::_setFloat(long double value)
{
    if (_t != t_float)
    {
        _releaseValue();
        _t = t_float;
    }

    _u._f = value;

    _category = Value;
}


void SerializationInfo::getValue(String& value) const
{
    switch (_t)
    {
        case t_none:    value.clear(); break;
        case t_string:  value.assign(*_StringPtr()); break;
        case t_string8: value.assign(*_String8Ptr()); break;
        case t_int:     convert(value, _u._i); break;
        case t_uint:    convert(value, _u._u); break;
        case t_float:   convert(value, _u._f); break;
    }
}

void SerializationInfo::getValue(std::string& value) const
{
    switch (_t)
    {
        case t_none:    value.clear(); break;
        case t_string:  value = _StringPtr()->narrow(); break;
        case t_string8: value.assign(*_String8Ptr()); break;
        case t_int:     convert(value, _u._i); break;
        case t_uint:    convert(value, _u._u); break;
        case t_float:   convert(value, _u._f); break;
    }
}

namespace
{
    inline bool isTrue(const char c)
    {
        return c == '1'
            || c == 't'
            || c == 'T'
            || c == 'y'
            || c == 'Y';
    }
}

void SerializationInfo::getValue(bool& value) const
{
    switch (_t)
    {
        case t_none:    value = false; break;
        case t_string:  value = !_StringPtr()->empty() && isTrue((*_StringPtr())[0].narrow()); break;
        case t_string8: value = !_StringPtr()->empty() && isTrue((*_String8Ptr())[0]); break;
        case t_int:     value = _u._i; break;
        case t_uint:    value = _u._u; break;
        case t_float:   value = _u._f; break;
    }
}

void SerializationInfo::getValue(char& value) const
{
    switch (_t)
    {
        case t_none:    value = '\0'; break;
        case t_string:  value = _StringPtr()->empty() ? '\0' : (*_StringPtr())[0].narrow(); break;
        case t_string8: value = _StringPtr()->empty() ? '\0' : (*_String8Ptr())[0]; break;
        case t_int:     value = _u._i; break;
        case t_uint:    value = _u._u; break;
        case t_float:   value = _u._f; break;
    }
}

void SerializationInfo::getValue(signed char& value) const
{
    switch (_t)
    {
        case t_none:    value = '\0'; break;
        case t_string:  value = _StringPtr()->empty() ? '\0' : (*_StringPtr())[0].narrow(); break;
        case t_string8: value = _StringPtr()->empty() ? '\0' : (*_String8Ptr())[0]; break;
        case t_int:     value = _u._i; break;
        case t_uint:    value = _u._u; break;
        case t_float:   value = _u._f; break;
    }
}

void SerializationInfo::getValue(unsigned char& value) const
{
    switch (_t)
    {
        case t_none:    value = '\0'; break;
        case t_string:  value = _StringPtr()->empty() ? '\0' : (*_StringPtr())[0].narrow(); break;
        case t_string8: value = _StringPtr()->empty() ? '\0' : (*_String8Ptr())[0]; break;
        case t_int:     value = _u._i; break;
        case t_uint:    value = _u._u; break;
        case t_float:   value = _u._f; break;
    }
}

SerializationInfo::LongInt SerializationInfo::_getInt() const
{
    switch (_t)
    {
        case t_none:    return 0; break;
        case t_string:  return convert<LongInt>(*_StringPtr()); break;
        case t_string8: return convert<LongInt>(*_String8Ptr()); break;
        case t_int:     return _u._i; break;
        case t_uint:    return _u._u; break;
        case t_float:   return _u._f; break;
    }
}

SerializationInfo::ULongInt SerializationInfo::_getUInt() const
{
    switch (_t)
    {
        case t_none:    return 0; break;
        case t_string:  return convert<ULongInt>(*_StringPtr()); break;
        case t_string8: return convert<ULongInt>(*_String8Ptr()); break;
        case t_int:     return _u._i; break;
        case t_uint:    return _u._u; break;
        case t_float:   return _u._f; break;
    }
}

long double SerializationInfo::_getFloat() const
{
    switch (_t)
    {
        case t_none:    return 0; break;
        case t_string:  return convert<long double>(*_StringPtr()); break;
        case t_string8: return convert<long double>(*_String8Ptr()); break;
        case t_int:     return _u._i; break;
        case t_uint:    return _u._u; break;
        case t_float:   return _u._f; break;
    }
}


} // namespace cxxtools
