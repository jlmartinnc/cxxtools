/*
 * Copyright (C) 2010 Tommi Maekitalo
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

#ifndef CXXTOOLS_CSVSERIALIZER_H
#define CXXTOOLS_CSVSERIALIZER_H

#include <cxxtools/textstream.h>
#include <cxxtools/utf8codec.h>
#include <cxxtools/utf8.h>
#include <cxxtools/string.h>
#include <cxxtools/char.h>
#include <cxxtools/serializationinfo.h>
#include <vector>
#include <memory>

namespace cxxtools
{
class CsvSerializer
{
    CsvSerializer(const CsvSerializer&);
    CsvSerializer& operator= (const CsvSerializer&);

public:
    explicit CsvSerializer(std::ostream& os, TextCodec<Char, char>* codec = new Utf8Codec());

    explicit CsvSerializer(TextOStream& os);

    void selectColumn(const std::string& title)
        { _titles.emplace_back(title); }
    void selectColumn(const std::string& memberName, const String& title)
        { _titles.emplace_back(memberName, title); }

    void delimiter(String delimiter)    { _delimiter = delimiter; }
    void delimiter(Char delimiter)      { _delimiter = delimiter; }
    void quote(Char quote)              { _quote = quote; }
    void lineEnding(const String& le)   { _lineEnding = le; }

    const CsvSerializer& serialize(const SerializationInfo& si) const;

    template <typename T>
    const CsvSerializer& serialize(const T& v) const
    {
        SerializationInfo si;
        si <<= v;
        serialize(si);
        return *this;
    }

    // titles and member names
    class Title
    {
    public:
        Title(const std::string& memberName, const String& title)
            : _memberName(memberName),
              _title(title)
            { }
        explicit Title(const std::string& memberName)
            : _memberName(memberName),
              _title(Utf8(memberName))
            { }

        const std::string& memberName() const   { return _memberName; }
        const String& title() const             { return _title; }

        bool operator== (const std::string& memberName) const
            { return _memberName == memberName; }

    private:
        std::string _memberName;
        String _title;

    };

private:
    void csvOut(const String& value) const;

    void dataOut();
    std::string memberName() const;

    String _delimiter;
    Char _quote;
    String _lineEnding;

    std::vector<Title> _titles;

    std::vector<String> _data;
    std::vector<std::string> _memberNames;
    std::unique_ptr<TextOStream> _ts;
    TextOStream& _os;
};
}

#endif // CXXTOOLS_CSVSERIALIZER_H
