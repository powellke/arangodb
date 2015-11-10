////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_SLICE_H
#define VELOCYPACK_SLICE_H 1

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <ostream>
#include <algorithm>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"

namespace arangodb {
  namespace velocypack {

    // forward for fasthash64 function declared elsewhere
    uint64_t fasthash64 (void const*, size_t, uint64_t);

    class Slice {

      // This class provides read only access to a VPack value, it is
      // intentionally light-weight (only one pointer value), such that
      // it can easily be used to traverse larger VPack values.

        friend class Builder;

        uint8_t const* _start;

      public:
        
        CustomTypeHandler* customTypeHandler;
 
        // constructor for an empty Value of type None 
        Slice () 
          : Slice("\x00") {
        }

        explicit Slice (uint8_t const* start, CustomTypeHandler* handler = nullptr) 
          : _start(start), customTypeHandler(handler) {
        }

        explicit Slice (char const* start, CustomTypeHandler* handler = nullptr) 
          : _start(reinterpret_cast<uint8_t const*>(start)), customTypeHandler(handler) {
        }

        Slice (Slice const& other) 
          : _start(other._start), customTypeHandler(other.customTypeHandler) {
        }

        Slice& operator= (Slice const& other) {
          _start = other._start;
          customTypeHandler = other.customTypeHandler;
          return *this;
        }
        
        uint8_t const* begin () {
          return _start;
        }
        
        uint8_t const* begin () const {
          return _start;
        }
        
        uint8_t const* end () {
          return _start + byteSize();
        }

        uint8_t const* end () const {
          return _start + byteSize();
        }

        // No destructor, does not take part in memory management,

        // get the type for the slice
        inline ValueType type () const {
          return TypeMap[head()];
        }

        char const* typeName () const {
          return valueTypeName(type());
        }

        // pointer to the head byte
        uint8_t const* start () const {
          return _start;
        }

        // value of the head byte
        inline uint8_t head () const {
          return *_start;
        }

        inline uint64_t hash () const {
          return fasthash64(start(), byteSize(), 0xdeadbeef);
        }

        // check if slice is of the specified type
        inline bool isType (ValueType t) const {
          return type() == t;
        }

        // check if slice is a None object
        bool isNone () const {
          return isType(ValueType::None);
        }

        // check if slice is a Null object
        bool isNull () const {
          return isType(ValueType::Null);
        }

        // check if slice is a Bool object
        bool isBool () const {
          return isType(ValueType::Bool);
        }

        // check if slice is a Bool object - this is an alias for isBool()
        bool isBoolean () const {
          return isBool();
        }

        // check if slice is an Array object
        bool isArray () const {
          return isType(ValueType::Array);
        }

        // check if slice is an Object object
        bool isObject () const {
          return isType(ValueType::Object);
        }
        
        // check if slice is a Double object
        bool isDouble () const {
          return isType(ValueType::Double);
        }
        
        // check if slice is a UTCDate object
        bool isUTCDate () const {
          return isType(ValueType::UTCDate);
        }

        // check if slice is an External object
        bool isExternal () const {
          return isType(ValueType::External);
        }

        // check if slice is a MinKey object
        bool isMinKey () const {
          return isType(ValueType::MinKey);
        }
        
        // check if slice is a MaxKey object
        bool isMaxKey () const {
          return isType(ValueType::MaxKey);
        }

        // check if slice is an Int object
        bool isInt () const {
          return isType(ValueType::Int);
        }
        
        // check if slice is a UInt object
        bool isUInt () const {
          return isType(ValueType::UInt);
        }

        // check if slice is a SmallInt object
        bool isSmallInt () const {
          return isType(ValueType::SmallInt);
        }

        // check if slice is a String object
        bool isString () const {
          return isType(ValueType::String);
        }

        // check if slice is a Binary object
        bool isBinary () const {
          return isType(ValueType::Binary);
        }

        // check if slice is a BCD
        bool isBCD () const {
          return isType(ValueType::BCD);
        }
        
        // check if slice is a custom type
        bool isCustom () const {
          return isType(ValueType::Custom);
        }
        
        // check if a slice is any number type
        bool isInteger () const {
          return isType(ValueType::Int) || isType(ValueType::UInt) || isType(ValueType::SmallInt);
        }

        // check if slice is any Number-type object
        bool isNumber () const {
          return isInteger() || isDouble();
        }
        
        bool isSorted () const {
          auto const h = head();
          return (h >= 0x0b && h <= 0x0e);
        }

        // return the value for a Bool object
        bool getBool () const {
          assertType(ValueType::Bool);
          return (head() == 0x1a); // 0x19 == false, 0x1a == true
        }

        // return the value for a Bool object - this is an alias for getBool()
        bool getBoolean () const {
          return getBool();
        }

        // return the value for a Double object
        double getDouble () const {
          assertType(ValueType::Double);
          union {
            uint64_t dv;
            double d;
          } v;
          v.dv = readInteger<uint64_t>(_start + 1, 8);
          return v.d; 
        }

        // extract the array value at the specified index
        // - 0x02      : array without index table (all subitems have the same
        //               byte length), bytelen 1 byte, no number of subvalues
        // - 0x03      : array without index table (all subitems have the same
        //               byte length), bytelen 2 bytes, no number of subvalues
        // - 0x04      : array without index table (all subitems have the same
        //               byte length), bytelen 4 bytes, no number of subvalues
        // - 0x05      : array without index table (all subitems have the same
        //               byte length), bytelen 8 bytes, no number of subvalues
        // - 0x06      : array with 1-byte index table entries
        // - 0x07      : array with 2-byte index table entries
        // - 0x08      : array with 4-byte index table entries
        // - 0x09      : array with 8-byte index table entries
        Slice at (ValueLength index) const {
          if (! isType(ValueType::Array)) {
            throw Exception(Exception::InvalidValueType, "Expecting Array");
          }

          return getNth(index);
        }

        Slice operator[] (ValueLength index) const {
          return at(index);
        }

        // return the number of members for an Array or Object object
        ValueLength length () const {
          if (type() != ValueType::Array && type() != ValueType::Object) {
            throw Exception(Exception::InvalidValueType, "Expecting Array or Object");
          }

          auto const h = head();
          if (h == 0x01 || h == 0x0a) {
            // special case: empty!
            return 0;
          }

          ValueLength const offsetSize = indexEntrySize(h);
          ValueLength end = readInteger<ValueLength>(_start + 1, offsetSize);

          // find number of items
          if (h <= 0x05) {    // No offset table or length, need to compute:
            ValueLength firstSubOffset = findDataOffset(h);
            Slice first(_start + firstSubOffset, customTypeHandler);
            return (end - firstSubOffset) / first.byteSize();
          } 
          else if (offsetSize < 8) {
            return readInteger<ValueLength>(_start + offsetSize + 1, offsetSize);
          }

          return readInteger<ValueLength>(_start + end - offsetSize, offsetSize);
        }

        // extract a key from an Object at the specified index
        // - 0x0a      : empty object
        // - 0x0b      : object with 1-byte index table entries, sorted by attribute name
        // - 0x0c      : object with 2-byte index table entries, sorted by attribute name
        // - 0x0d      : object with 4-byte index table entries, sorted by attribute name
        // - 0x0e      : object with 8-byte index table entries, sorted by attribute name
        // - 0x0f      : object with 1-byte index table entries, not sorted by attribute name
        // - 0x10      : object with 2-byte index table entries, not sorted by attribute name
        // - 0x11      : object with 4-byte index table entries, not sorted by attribute name
        // - 0x12      : object with 8-byte index table entries, not sorted by attribute name
        Slice keyAt (ValueLength index) const {
          if (! isType(ValueType::Object)) {
            throw Exception(Exception::InvalidValueType, "Expecting Object");
          }
          
          return getNth(index);
        }

        Slice valueAt (ValueLength index) const {
          Slice key = keyAt(index);
          return Slice(key.start() + key.byteSize(), customTypeHandler);
        }

        // look for the specified attribute path inside an Object
        // returns a Slice(ValueType::None) if not found
        Slice get (std::vector<std::string> const& attributes) const { 
          size_t const n = attributes.size();
          if (n == 0) {
            throw Exception(Exception::InvalidAttributePath);
          }

          // use ourselves as the starting point
          Slice last = Slice(start(), customTypeHandler);
          for (size_t i = 0; i < attributes.size(); ++i) {
            // fetch subattribute
            last = last.get(attributes[i]);

            // abort as early as possible
            if (last.isNone() || (i + 1 < n && ! last.isObject())) {
              return Slice();
            }
          }

          return last;
        }

        // look for the specified attribute inside an Object
        // returns a Slice(ValueType::None) if not found
        Slice get (std::string const& attribute) const {
          if (! isType(ValueType::Object)) {
            throw Exception(Exception::InvalidValueType, "Expecting Object");
          }

          auto const h = head();
          if (h == 0xa) {
            // special case, empty object
            return Slice();
          }
          
          ValueLength const offsetSize = indexEntrySize(h);
          ValueLength end = readInteger<ValueLength>(_start + 1, offsetSize);
          ValueLength dataOffset = 0;

          // read number of items
          ValueLength n;
          if (h <= 0x05) {    // No offset table or length, need to compute:
            dataOffset = findDataOffset(h);
            Slice first(_start + dataOffset, customTypeHandler);
            n = (end - dataOffset) / first.byteSize();
          } 
          else if (offsetSize < 8) {
            n = readInteger<ValueLength>(_start + 1 + offsetSize, offsetSize);
          }
          else {
            n = readInteger<ValueLength>(_start + end - offsetSize, offsetSize);
          }
          
          if (n == 1) {
            // Just one attribute, there is no index table!
            if (dataOffset == 0) {
              dataOffset = findDataOffset(h);
            }
            Slice attrName = Slice(_start + dataOffset, customTypeHandler);
            if (! attrName.isString()) {
              return Slice();
            }
            ValueLength attrLength;
            char const* k = attrName.getString(attrLength); 
            if (attrLength != static_cast<ValueLength>(attribute.size())) {
              // key must have the exact same length as the attribute we search for
              return Slice();
            }
            if (memcmp(k, attribute.c_str(), attribute.size()) != 0) {
              return Slice();
            }

            return Slice(attrName.start() + attrName.byteSize(), customTypeHandler);
          }

          ValueLength const ieBase = end - n * offsetSize 
                                     - (offsetSize == 8 ? offsetSize : 0);

          // only use binary search for attributes if we have at least this many entries
          // otherwise we'll always use the linear search
          static ValueLength const SortedSearchEntriesThreshold = 4;

          if (isSorted() && n >= SortedSearchEntriesThreshold) {
            // This means, we have to handle the special case n == 1 only
            // in the linear search!
            return searchObjectKeyBinary(attribute, ieBase, offsetSize, n);
          }

          return searchObjectKeyLinear(attribute, ieBase, offsetSize, n);
        }

        Slice operator[] (std::string const& attribute) const {
          return get(attribute);
        }
 
        // whether or not an Object has a specific key
        bool hasKey (std::string const& attribute) const {
          return ! get(attribute).isNone();
        }

        // whether or not an Object has a specific sub-key
        bool hasKey (std::vector<std::string> const& attributes) const {
          return ! get(attributes).isNone();
        }

        // return the pointer to the data for an External object
        char const* getExternal () const {
          return extractValue<char const*>();
        }

        // return the value for an Int object
        int64_t getInt () const {
          uint8_t const h = head();
          if (h >= 0x20 && h <= 0x27) {
            // Int  T
            uint64_t v = readInteger<uint64_t>(_start + 1, h - 0x1f);
            if (h == 0x27) {
              return ToInt64(v);
            }
            else {
              int64_t vv = static_cast<int64_t>(v);
              int64_t shift = 1LL << ((h - 0x1f) * 8 - 1);
              return vv < shift ? vv : vv - (shift << 1);
            }
          }

          if (h >= 0x28 && h <= 0x2f) { 
            // UInt
            uint64_t v = getUInt();
            if (v > static_cast<uint64_t>(INT64_MAX)) {
              throw Exception(Exception::NumberOutOfRange);
            }
            return static_cast<int64_t>(v);
          }
          
          if (h >= 0x30 && h <= 0x3f) {
            // SmallInt
            return getSmallInt();
          }

          throw Exception(Exception::InvalidValueType, "Expecting type Int");
        }

        // return the value for a UInt object
        uint64_t getUInt () const {
          uint8_t const h = head();
          if (h >= 0x28 && h <= 0x2f) {
            // UInt
            return readInteger<uint64_t>(_start + 1, h - 0x27);
          }
          
          if (h >= 0x20 && h <= 0x27) {
            // Int 
            int64_t v = getInt();
            if (v < 0) {
              throw Exception(Exception::NumberOutOfRange);
            }
            return static_cast<int64_t>(v);
          }

          if (h >= 0x30 && h <= 0x39) {
            // Smallint >= 0
            return static_cast<uint64_t>(h - 0x30);
          }

          if (h >= 0x3a && h <= 0x3f) {
            // Smallint < 0
            throw Exception(Exception::NumberOutOfRange);
          }
          
          throw Exception(Exception::InvalidValueType, "Expecting type UInt");
        }

        // return the value for a SmallInt object
        int64_t getSmallInt () const {
          uint8_t const h = head();

          if (h >= 0x30 && h <= 0x39) {
            // Smallint >= 0
            return static_cast<int64_t>(h - 0x30);
          }

          if (h >= 0x3a && h <= 0x3f) {
            // Smallint < 0
            return static_cast<int64_t>(h - 0x3a) - 6;
          }

          if ((h >= 0x20 && h <= 0x27) ||
              (h >= 0x28 && h <= 0x2f)) {
            // Int and UInt
            // we'll leave it to the compiler to detect the two ranges above are adjacent
            return getInt();
          }

          throw Exception(Exception::InvalidValueType, "Expecting type Smallint");
        }

        template<typename T>
        T getNumericValue () const {
          if (std::is_integral<T>()) {
            if (std::is_signed<T>()) {
              // signed integral type
              if (isDouble()) {
                auto v = getDouble();
                if (v < static_cast<double>(std::numeric_limits<T>::min()) || 
                    v > static_cast<double>(std::numeric_limits<T>::max())) {
                  throw Exception(Exception::NumberOutOfRange);
                }
                return static_cast<T>(v);
              }

              int64_t v = getInt();
              if (v < static_cast<int64_t>(std::numeric_limits<T>::min()) || 
                  v > static_cast<int64_t>(std::numeric_limits<T>::max())) {
                throw Exception(Exception::NumberOutOfRange);
              }
              return static_cast<T>(v);
            }
            else {
              // unsigned integral type
              if (isDouble()) {
                auto v = getDouble();
                if (v < 0.0 ||
                    v > static_cast<double>(UINT64_MAX) ||
                    v > static_cast<double>(std::numeric_limits<T>::max())) {
                  throw Exception(Exception::NumberOutOfRange);
                }
                return static_cast<T>(v);
              }

              uint64_t v = getUInt();
              if (v > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
                throw Exception(Exception::NumberOutOfRange);
              }
              return static_cast<T>(v);
            }
          }

          // floating point type

          if (isDouble()) {
            return static_cast<T>(getDouble());
          }
          if (isInt() || isSmallInt()) {
            return static_cast<T>(getInt());
          }
          if (isUInt()) {
            return static_cast<T>(getUInt());
          }

          throw Exception(Exception::InvalidValueType, "Expecting numeric type");
        }
        
        // return the value for a UTCDate object
        int64_t getUTCDate () const {
          assertType(ValueType::UTCDate);
          uint64_t v = readInteger<uint64_t>(_start + 1, sizeof(uint64_t));
          return ToInt64(v);
        }

        // return the value for a String object
        char const* getString (ValueLength& length) const {
          uint8_t const h = head();
          if (h >= 0x40 && h <= 0xbe) {
            // short UTF-8 String
            length = h - 0x40;
            return reinterpret_cast<char const*>(_start + 1);
          }

          if (h == 0xbf) {
            // long UTF-8 String
            length = readInteger<ValueLength>(_start + 1, 8);
            CheckValueLength(length);
            return reinterpret_cast<char const*>(_start + 1 + 8);
          }

          throw Exception(Exception::InvalidValueType, "Expecting type String");
        }

        // return a copy of the value for a String object
        std::string copyString () const {
          uint8_t h = head();
          if (h >= 0x40 && h <= 0xbe) {
            // short UTF-8 String
            ValueLength length = h - 0x40;
            return std::string(reinterpret_cast<char const*>(_start + 1), static_cast<size_t>(length));
          }

          if (h == 0xbf) {
            ValueLength length = readInteger<ValueLength>(_start + 1, 8);
            CheckValueLength(length);
            return std::string(reinterpret_cast<char const*>(_start + 1 + 8), length);
          }

          throw Exception(Exception::InvalidValueType, "Expecting type String");
        }

        // return the value for a Binary object
        uint8_t const* getBinary (ValueLength& length) const {
          assertType(ValueType::Binary);
          uint8_t const h = head();

          if (h >= 0xc0 && h <= 0xc7) {
            length = readInteger<ValueLength>(_start + 1, h - 0xbf); 
            CheckValueLength(length);
            return _start + 1 + h - 0xbf;
          }

          throw Exception(Exception::InvalidValueType, "Expecting type Binary");
        }

        // return a copy of the value for a Binary object
        std::vector<uint8_t> copyBinary () const {
          assertType(ValueType::Binary);
          uint8_t const h = head();

          if (h >= 0xc0 && h <= 0xc7) {
            std::vector<uint8_t> out;
            ValueLength length = readInteger<ValueLength>(_start + 1, h - 0xbf); 
            CheckValueLength(length);
            out.reserve(static_cast<size_t>(length));
            out.insert(out.end(), _start + 1 + h - 0xbf, _start + 1 + h - 0xbf + length);
            return out; 
          }

          throw Exception(Exception::InvalidValueType, "Expecting type Binary");
        }

        // get the total byte size for the slice, including the head byte
        ValueLength byteSize () const {
          switch (type()) {
            case ValueType::None:
            case ValueType::Null:
            case ValueType::Bool: 
            case ValueType::MinKey: 
            case ValueType::MaxKey: 
            case ValueType::SmallInt: {
              return 1; 
            }

            case ValueType::Double: {
              return 1 + sizeof(double);
            }

            case ValueType::Array:
            case ValueType::Object: {
              auto const h = head();
              if (h == 0x01 || h == 0x0a) {
                // empty array or object
                return 1;
              }

              return readInteger<ValueLength>(_start + 1, WidthMap[h]);
            }

            case ValueType::External: {
              return 1 + sizeof(char*);
            }

            case ValueType::UTCDate: {
              return 1 + sizeof(int64_t);
            }

            case ValueType::Int: {
              return static_cast<ValueLength>(1 + (head() - 0x1f));
            }

            case ValueType::UInt: {
              return static_cast<ValueLength>(1 + (head() - 0x27));
            }

            case ValueType::String: {
              auto const h = head();
              if (h == 0xbf) {
                // long UTF-8 String
                return static_cast<ValueLength>(1 + 8 + readInteger<ValueLength>(_start + 1, 8));
              }

              // short UTF-8 String
              return static_cast<ValueLength>(1 + h - 0x40);
            }

            case ValueType::Binary: {
              auto const h = head();
              return static_cast<ValueLength>(1 + h - 0xbf + readInteger<ValueLength>(_start + 1, h - 0xbf));
            }

            case ValueType::BCD: {
              auto const h = head();
              if (h <= 0xcf) {
                // positive BCD
                return static_cast<ValueLength>(1 + h - 0xc7 + readInteger<ValueLength>(_start + 1, h - 0xc7));
              } 

              // negative BCD
              return static_cast<ValueLength>(1 + h - 0xcf + readInteger<ValueLength>(_start + 1, h - 0xcf));
            }

            case ValueType::Custom: {
              if (customTypeHandler == nullptr) {
                throw Exception(Exception::NeedCustomTypeHandler);
              }

              return customTypeHandler->byteSize(*this);
            }
          }

          VELOCYPACK_ASSERT(false);
          return 0;
        }

        std::string toString () const;
        std::string hexType () const;

      private:
        
        ValueLength findDataOffset (uint8_t const head) const {
          // Must be called for a nonempty array or object at start():
          unsigned int fsm = FirstSubMap[head];
          if (fsm <= 2 && _start[2] != 0) {
            return 2;
          }
          if (fsm <= 3 && _start[3] != 0) {
            return 3;
          }
          if (fsm <= 5 && _start[5] != 0) {
            return 5;
          }
          return 9;
        }
          
        // extract the nth member from an Array or Object type
        Slice getNth (ValueLength index) const {
          VELOCYPACK_ASSERT(type() == ValueType::Array || type() == ValueType::Object);

          auto const h = head();
          if (h == 0x01 || h == 0x0a) {
            // special case. empty array or object
            throw Exception(Exception::IndexOutOfBounds);
          }

          ValueLength const offsetSize = indexEntrySize(h);
          ValueLength end = readInteger<ValueLength>(_start + 1, offsetSize);

          ValueLength dataOffset = findDataOffset(h);
          
          // find the number of items
          ValueLength n;
          if (h <= 0x05) {    // No offset table or length, need to compute:
            Slice first(_start + dataOffset, customTypeHandler);
            n = (end - dataOffset) / first.byteSize();
          }
          else if (offsetSize < 8) {
            n = readInteger<ValueLength>(_start + 1 + offsetSize, offsetSize);
          }
          else {
            n = readInteger<ValueLength>(_start + end - offsetSize, offsetSize);
          }

          if (index >= n) {
            throw Exception(Exception::IndexOutOfBounds);
          }

          // empty array case was already covered
          VELOCYPACK_ASSERT(n > 0);

          if (h <= 0x05 || n == 1) {
            // no index table, but all array items have the same length
            // now fetch first item and determine its length
            if (dataOffset == 0) {
              dataOffset = findDataOffset(h);
            }
            Slice firstItem(_start + dataOffset, customTypeHandler);
            return Slice(_start + dataOffset + index * firstItem.byteSize(), customTypeHandler);
          }
          
          ValueLength const ieBase = end - n * offsetSize + index * offsetSize
                                     - (offsetSize == 8 ? 8 : 0);
          return Slice(_start + readInteger<ValueLength>(_start + ieBase, offsetSize), customTypeHandler);
        }

        ValueLength indexEntrySize (uint8_t head) const {
          return static_cast<ValueLength>(WidthMap[head]);
        }

        // perform a linear search for the specified attribute inside an Object
        Slice searchObjectKeyLinear (std::string const& attribute, 
                                          ValueLength ieBase, 
                                          ValueLength offsetSize, 
                                          ValueLength n) const {
          for (ValueLength index = 0; index < n; ++index) {
            ValueLength offset = ieBase + index * offsetSize;
            Slice key(_start + readInteger<ValueLength>(_start + offset, offsetSize), customTypeHandler);
            if (! key.isString()) {
              // invalid object
              return Slice();
            }

            ValueLength keyLength;
            char const* k = key.getString(keyLength); 
            if (keyLength != static_cast<ValueLength>(attribute.size())) {
              // key must have the exact same length as the attribute we search for
              continue;
            }

            if (memcmp(k, attribute.c_str(), attribute.size()) != 0) {
              continue;
            }
            // key is identical. now return value
            return Slice(key.start() + key.byteSize(), customTypeHandler);
          }

          // nothing found
          return Slice();
        }

        // perform a binary search for the specified attribute inside an Object
        Slice searchObjectKeyBinary (std::string const& attribute, 
                                          ValueLength ieBase,
                                          ValueLength offsetSize, 
                                          ValueLength n) const {
          VELOCYPACK_ASSERT(n > 0);
            
          ValueLength const attributeLength = static_cast<ValueLength>(attribute.size());

          ValueLength l = 0;
          ValueLength r = n - 1;

          while (true) {
            // midpoint
            ValueLength index = l + ((r - l) / 2);

            ValueLength offset = ieBase + index * offsetSize;
            Slice key(_start + readInteger<ValueLength>(_start + offset, offsetSize), customTypeHandler);
            if (! key.isString()) {
              // invalid object
              return Slice();
            }

            ValueLength keyLength;
            char const* k = key.getString(keyLength); 
            size_t const compareLength = static_cast<size_t>((std::min)(keyLength, attributeLength));
            int res = memcmp(k, attribute.c_str(), compareLength);

            if (res == 0 && keyLength == attributeLength) {
              // key is identical. now return value
              return Slice(key.start() + key.byteSize(), customTypeHandler);
            }

            if (res > 0 || (res == 0 && keyLength > attributeLength)) {
              if (index == 0) {
                return Slice();
              }
              r = index - 1;
            }
            else {
              l = index + 1;
            }
            if (r < l) {
              return Slice();
            }
          }
        }
         
        // assert that the slice is of a specific type
        // can be used for debugging and removed in production
#ifdef VELOCYPACK_ASSERT
        inline void assertType (ValueType) const {
        }
#else
        inline void assertType (ValueType type) const {
          VELOCYPACK_ASSERT(this->type() == type);
        }
#endif
          
        // read an unsigned little endian integer value of the
        // specified length, starting at the specified byte offset
        template <typename T>
        T readInteger (uint8_t const* start, ValueLength numBytes) const {
          T value = 0;
          uint8_t const* p = start;
          uint8_t const* e = p + numBytes;
          T digit = 0;

          while (p < e) {
            value += static_cast<T>(*p) << (digit * 8);
            ++digit;
            ++p;
          }

          return value;
        }

        // extracts a value from the slice and converts it into a 
        // built-in type
        template<typename T> T extractValue () const {
          union {
            T value;
            char binary[sizeof(T)];
          }; 
          memcpy(&binary[0], _start + 1, sizeof(T));
          return value; 
        }

      private:

        static ValueType const TypeMap[256];
        static unsigned int const WidthMap[256];
        static unsigned int const FirstSubMap[256];
    };

  }  // namespace arangodb::velocypack
}  // namespace arangodb

namespace std {
  template<> struct hash<arangodb::velocypack::Slice> {
    size_t operator () (arangodb::velocypack::Slice const& slice) const {
      return slice.hash();
    }
  };

  template<> struct equal_to<arangodb::velocypack::Slice> {
    bool operator () (arangodb::velocypack::Slice const& a,
                      arangodb::velocypack::Slice const& b) const {
      if (*a.start() != *b.start()) {
        return false;
      }

      auto const aSize = a.byteSize();
      auto const bSize = b.byteSize();

      if (aSize != bSize) {
        return false;
      }

      return (memcmp(a.start(), b.start(), aSize) == 0);
    }
  };
}
        
std::ostream& operator<< (std::ostream&, arangodb::velocypack::Slice const*);

std::ostream& operator<< (std::ostream&, arangodb::velocypack::Slice const&);

#endif
