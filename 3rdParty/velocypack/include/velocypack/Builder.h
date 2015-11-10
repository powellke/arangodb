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

#ifndef VELOCYPACK_BUILDER_H
#define VELOCYPACK_BUILDER_H

#include <ostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>

#include "velocypack/velocypack-common.h"
#include "velocypack/Buffer.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"
#include "velocypack/Slice.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"

namespace arangodb {
  namespace velocypack {

    class Builder {

        friend class Parser;   // The parser needs access to internals.

      public:
        // A struct for sorting index tables for objects:
        struct SortEntry {
          uint8_t const* nameStart;
          uint64_t nameSize;
          uint64_t offset;
        };

        void reserve (ValueLength len) {
          reserveSpace(len);
        }

      private:

        Buffer<uint8_t> _buffer;  // Here we collect the result
        uint8_t*        _start;   // Always points to the start of _buffer
        ValueLength     _size;    // Always contains the size of _buffer
        ValueLength     _pos;     // the append position, always <= _size
        std::vector<ValueLength>              _stack;  // Start positions of
                                                       // open objects/arrays
        std::vector<std::vector<ValueLength>> _index;  // Indices for starts
                                                       // of subindex

        // Here are the mechanics of how this building process works:
        // The whole VPack being built starts at where _start points to
        // and uses at most _size bytes. The variable _pos keeps the
        // current write position. The method "set" simply writes a new
        // VPack subobject at the current write position and advances
        // it. Whenever one makes an array or object, a ValueLength for
        // the beginning of the value is pushed onto the _stack, which
        // remembers that we are in the process of building an array or
        // object. The _index vectors are used to collect information
        // for the index tables of arrays and objects, which are written
        // behind the subvalues. The add methods are used to keep track
        // of the new subvalue in _index followed by a set, and are
        // what the user from the outside calls. The close method seals
        // the innermost array or object that is currently being built
        // and pops a ValueLength off the _stack. The vectors in _index
        // stay until the next clearTemporary() is called to minimize
        // allocations. In the beginning, the _stack is empty, which
        // allows to build a sequence of unrelated VPack objects in the
        // buffer. Whenever the stack is empty, one can use the start,
        // size and slice methods to get out the ready built VPack
        // object(s).

        void reserveSpace (ValueLength len) {
          // Reserves len bytes at pos of the current state (top of stack)
          // or throws an exception
          if (_pos + len <= _size) {
            return;  // All OK, we can just increase tos->pos by len
          }
          CheckValueLength(_pos + len);

          _buffer.prealloc(len);
          _start = _buffer.data();
          _size = _buffer.size();
        }

        // Sort the indices by attribute name:
        static void doActualSort (std::vector<SortEntry>& entries);

        // Find the actual bytes of the attribute name of the VPack value
        // at position base, also determine the length len of the attribute.
        // This takes into account the different possibilities for the format
        // of attribute names:
        static uint8_t const* findAttrName (uint8_t const* base, uint64_t& len);

        static void sortObjectIndexShort (uint8_t* objBase,
                                          std::vector<ValueLength>& offsets);

        static void sortObjectIndexLong (uint8_t* objBase,
                                         std::vector<ValueLength>& offsets);

        static void sortObjectIndex (uint8_t* objBase,
                                     std::vector<ValueLength>& offsets);

      public:
        
        Options options;

        // Constructor and destructor:

        Builder (Options const& options = Options::Defaults)
          : _buffer({ 0 }),
            _pos(0),
            options(options) {
          _start = _buffer.data();
          _size = _buffer.size();
        }

        // The rule of five:

        ~Builder () {
        }

        Builder (Builder const& that) 
          : _buffer(that._buffer), _start(_buffer.data()), _size(_buffer.size()), _pos(that._pos),
            _stack(that._stack), _index(that._index), options(that.options) {
        }

        Builder& operator= (Builder const& that) {
          _buffer = that._buffer;
          _start = _buffer.data();
          _size = _buffer.size();
          _pos = that._pos;
          _stack = that._stack;
          _index = that._index;
          options = that.options;
          return *this;
        }

        Builder (Builder&& that) {
          _buffer.reset();
          _buffer = that._buffer;
          that._buffer.reset();
          _start = _buffer.data();
          _size = _buffer.size();
          _pos = that._pos;
          _stack.clear();
          _stack.swap(that._stack);
          _index.clear();
          _index.swap(that._index);
          options = that.options;
          that._start = nullptr;
          that._size = 0;
          that._pos = 0;
        }

        Builder& operator= (Builder&& that) {
          _buffer.reset();
          _buffer = that._buffer;
          that._buffer.reset();
          _start = _buffer.data();
          _size = _buffer.size();
          _pos = that._pos;
          _stack.clear();
          _stack.swap(that._stack);
          _index.clear();
          _index.swap(that._index);
          options = that.options;
          that._start = nullptr;
          that._size = 0;
          that._pos = 0;
          return *this;
        }

        static Builder clone (Slice const& slice, Options const& options = Options::Defaults) {
          Builder b;
          b.options = options;
          b.add(slice);
          return b; 
        }

        // Clear and start from scratch:
        void clear () {
          _pos = 0;
          _stack.clear();
        }

        // Return a pointer to the start of the result:
        uint8_t* start () const {
          if (! isClosed()) {
            throw Exception(Exception::BuilderNotSealed);
          }
          return _start;
        }

        // Return a Slice of the result:
        Slice slice () const {
          return Slice(start(), options.customTypeHandler);
        }

        // Compute the actual size here, but only when sealed
        ValueLength size () const {
          if (! isClosed()) {
            throw Exception(Exception::BuilderNotSealed);
          }
          return _pos;
        }

        bool isClosed () const throw() {
          return _stack.empty();
        }

        // Add a subvalue into an object from a Value:
        uint8_t* add (std::string const& attrName, Value const& sub);
        
        // Add a subvalue into an object from a Slice:
        uint8_t* add (std::string const& attrName, Slice const& sub);

        // Add a subvalue into an object from a ValuePair:
        uint8_t* add (std::string const& attrName, ValuePair const& sub);

        // Add a subvalue into an array from a Value:
        uint8_t* add (Value const& sub);
        
        // Add a slice to an array
        uint8_t* add (Slice const& sub);

        // Add a subvalue into an array from a ValuePair:
        uint8_t* add (ValuePair const& sub);
        
        // Seal the innermost array or object:
        void close ();

        // Remove last subvalue written to an (unclosed) object or array:
        // Throws if an error occurs.
        void removeLast ();

        // whether or not a specific key is present in an Object value
        bool hasKey (std::string const& key) const;

        // Syntactic sugar for add:
        Builder& operator() (std::string const& attrName, Value sub) {
          add(attrName, sub);
          return *this;
        }

        // Syntactic sugar for add:
        Builder& operator() (std::string const& attrName, ValuePair sub) {
          add(attrName, sub);
          return *this;
        }

        // Syntactic sugar for add:
        Builder& operator() (Value sub) {
          add(sub);
          return *this;
        }

        // Syntactic sugar for add:
        Builder& operator() (ValuePair sub) {
          add(sub);
          return *this;
        }

        // Syntactic sugar for close:
        Builder& operator() () {
          close();
          return *this;
        }

        void addNull () {
          reserveSpace(1);
          _start[_pos++] = 0x18;
        }

        void addFalse () {
          reserveSpace(1);
          _start[_pos++] = 0x19;
        }

        void addTrue () {
          reserveSpace(1);
          _start[_pos++] = 0x1a;
        }

        void addDouble (double v) {
          uint64_t dv;
          memcpy(&dv, &v, sizeof(double));
          ValueLength vSize = sizeof(double);
          reserveSpace(1 + vSize);
          _start[_pos++] = 0x1b;
          for (uint64_t x = dv; vSize > 0; vSize--) {
            _start[_pos++] = x & 0xff;
            x >>= 8;
          }
        }

        void addInt (int64_t v) {
          if (v >= 0 && v <= 9) {
            reserveSpace(1);
            _start[_pos++] = static_cast<uint8_t>(0x30 + v);
          }
          else if (v < 0 && v >= -6) {
            reserveSpace(1);
            _start[_pos++] = static_cast<uint8_t>(0x40 + v);
          }
          else {
            appendInt(v, 0x1f);
          }
        }

        void addUInt (uint64_t v) {
          if (v <= 9) {
            reserveSpace(1);
            _start[_pos++] = static_cast<uint8_t>(0x30 + v);
          }
          else {
            appendUInt(v, 0x27);
          }
        }

        void addUTCDate (int64_t v) {
          uint8_t vSize = sizeof(int64_t);   // is always 8
          uint64_t x = ToUInt64(v);
          reserveSpace(1 + vSize);
          _start[_pos++] = 0x1c;
          appendLength(x, 8);
        }

        uint8_t* addString (uint64_t strLen) {
          uint8_t* target;
          if (strLen > 126) {
            // long string
            _start[_pos++] = 0xbf;
            // write string length
            appendLength(strLen, 8);
          }
          else {
            // short string
            _start[_pos++] = static_cast<uint8_t>(0x40 + strLen);
          }
          target = _start + _pos;
          _pos += strLen;
          return target;
        }

        void addArray () {
          addCompoundValue(0x06);
        }

        void addObject () {
          addCompoundValue(0x0b);
        }

      private:

        template<typename T>
        uint8_t* addInternal (T const& sub) {
          if (! _stack.empty()) {
            ValueLength& tos = _stack.back();
            if (_start[tos] != 0x06) {
              throw Exception(Exception::BuilderNeedOpenArray);
            }
            reportAdd(tos);
          }
          return set(sub);
        }
  
        template<typename T>
        uint8_t* addInternal (std::string const& attrName, T const& sub) {
          if (! _stack.empty()) {
            ValueLength& tos = _stack.back();
            if (_start[tos] != 0x0b) {
              throw Exception(Exception::BuilderNeedOpenObject);
            }
            reportAdd(tos);
          }
          set(Value(attrName, ValueType::String));
          return set(sub);
        }

        void addCompoundValue (uint8_t type) {
          reserveSpace(9);
          // an array is started:
          _stack.push_back(_pos);
          while (_stack.size() > _index.size()) {
            _index.emplace_back();
          }
          _index[_stack.size() - 1].clear();
          _start[_pos++] = type;
          memset(_start + _pos, 0, 8);
          _pos += 8;    // Will be filled later with bytelength and nr subs
        }

        uint8_t* set (Value const& item);

        uint8_t* set (ValuePair const& pair);
        
        uint8_t* set (Slice const& item);
        
        void reportAdd (ValueLength base) {
          size_t depth = _stack.size() - 1;
          _index[depth].push_back(_pos - base);
        }

        void appendLength (ValueLength v, uint64_t n) {
          reserveSpace(n);
          for (uint64_t i = 0; i < n; ++i) {
            _start[_pos++] = v & 0xff;
            v >>= 8;
          }
        }

        void appendUInt (uint64_t v, uint8_t base) {
          reserveSpace(9);
          ValueLength save = _pos++;
          uint8_t vSize = 0;
          do {
            vSize++;
            _start[_pos++] = static_cast<uint8_t>(v & 0xff);
            v >>= 8;
          } while (v != 0);
          _start[save] = base + vSize;
        }

        // returns number of bytes required to store the value in 2s-complement
        static inline uint8_t intLength (int64_t value) {
          if (value >= -0x80 && value <= 0x7f) {
            // shortcut for the common case
            return 1;
          }
          uint64_t x = value >= 0 ? static_cast<uint64_t>(value)
                                  : static_cast<uint64_t>(-(value + 1));
          uint8_t xSize = 0;
          do {
            xSize++;
            x >>= 8;
          } 
          while (x >= 0x80);
          return xSize + 1;
        }

        void appendInt (int64_t v, uint8_t base) {
          uint8_t vSize = intLength(v);
          uint64_t x;
          if (vSize == 8) {
            x = ToUInt64(v);
          }
          else {
            int64_t shift = 1LL << (vSize * 8 - 1);  // will never overflow!
            x = v >= 0 ? static_cast<uint64_t>(v)
                       : static_cast<uint64_t>(v + shift) + shift;
          }
          reserveSpace(1 + vSize);
          _start[_pos++] = base + vSize;
          while (vSize-- > 0) {
            _start[_pos++] = x & 0xff;
            x >>= 8;
          }
        }
 
        void checkAttributeUniqueness (Slice const obj) const;
    };

  }  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
