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

#ifndef VELOCYPACK_BUFFER_H
#define VELOCYPACK_BUFFER_H 1

#include <cstring>

#include "velocypack/velocypack-common.h"

namespace arangodb {
  namespace velocypack {

    template<typename T>
    class Buffer {

      public:

        Buffer () 
          : _buffer(_local), 
            _alloc(sizeof(_local)), 
            _pos(0) {
#ifdef VELOCYPACK_DEBUG
          // poison memory
          memset(_buffer, 0xa5, _alloc);
#endif
        } 

        explicit Buffer (ValueLength expectedLength) 
          : Buffer() {
          reserve(expectedLength);
        }

        Buffer (Buffer const& that)
          : Buffer() {
          
          if (that._pos > 0) {
            if (that._pos > sizeof(_local)) {
              _buffer = new T[that._pos];
            }
            memcpy(_buffer, that._buffer, that._pos);
            _alloc = that._pos;
            _pos = that._pos;
          }
        }
        
        Buffer& operator= (Buffer const& that) {
          if (this != &that) {
            reset();

            if (that._pos > 0) {
              if (that._pos > sizeof(_local)) {
                _buffer = new T[that._pos];
              }
              memcpy(_buffer, that._buffer, that._pos);
              _alloc = that._pos;
              _pos = that._pos;
            }
          }
          return *this;
        }

        Buffer (Buffer&& that)
          : Buffer() {
          
          if (that._buffer == that._local) {
            memcpy(_buffer, that._buffer, that._pos);
            _pos = that._pos;
            that._pos = 0;
          }
          else {
            _buffer = that._buffer;
            _alloc = that._alloc;
            _pos = that._pos;
            that._buffer = that._local;
            that._alloc = sizeof(that._local);
            that._pos = 0;
          }
        }

        ~Buffer () {
          reset();
        }

        T* data () {
          return _buffer;
        }

        T const* data () const {
          return _buffer;
        }

        ValueLength size () const {
          return _pos;
        }

        void clear () {
          reset();
        }

        void reset () {
          if (_buffer != _local) {
            delete[] _buffer;
            _buffer = _local;
            _alloc = sizeof(_local);
#ifdef VELOCYPACK_DEBUG
            // poison memory
            memset(_buffer, 0xa5, _alloc);
#endif
          }
          _pos = 0;
        }

        void push_back (char c) {
          reserve(1); 
          _buffer[_pos++] = c;
        }

        void append (char c) {
          reserve(1); 
          _buffer[_pos++] = c;
        }

        void append (char const* p, ValueLength len) {
          reserve(len);
          memcpy(_buffer + _pos, p, len);
          _pos += len;
        }

        void append (uint8_t const* p, ValueLength len) {
          reserve(len);
          memcpy(_buffer + _pos, p, len);
          _pos += len;
        }

        void reserve (ValueLength len) {
          if (_pos + len < _alloc) {
            return;
          }

          VELOCYPACK_ASSERT(_pos + len >= sizeof(_local));

          static ValueLength const MinLength = sizeof(_local);

          // need reallocation
          ValueLength newLen = _pos + len;
          if (newLen < MinLength) {
            // ensure we don't alloc too small blocks
            newLen = MinLength;
          }
          static double const GrowthFactor = 1.25;
          if (_pos > 0 && newLen < GrowthFactor * _pos) {
            // ensure the buffer grows sensibly and not by 1 byte only
            newLen = static_cast<ValueLength>(GrowthFactor * _pos);
          }
          VELOCYPACK_ASSERT(newLen > _pos);

          T* p = new T[newLen];
#ifdef VELOCYPACK_DEBUG
          // poison memory
          memset(p, 0xa5, newLen);
#endif
          // copy old data
          memcpy(p, _buffer, _pos);
          if (_buffer != _local) {
            delete[] _buffer;
          }
          _buffer = p;
          _alloc = newLen;
        }

        // reserve and zero fill
        void prealloc (ValueLength len) {
          reserve(len);
          // memset(_buffer + _pos, 0, len);
          _pos += len;
        }
       
      private:

        ValueLength capacity () const {
          return _alloc;
        }

 
        T*          _buffer;
        ValueLength _alloc;
        ValueLength _pos;

        // an already initialized space for small values
        T           _local[192];

    };

    typedef Buffer<char> CharBuffer;

  }  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
