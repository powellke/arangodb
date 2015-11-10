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

#ifndef VELOCYPACK_COMMON_H
#define VELOCYPACK_COMMON_H 1

#include <cstdint>

// debug mode
#ifdef VELOCYPACK_DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#include <cassert>
#define VELOCYPACK_ASSERT(x) assert(x)

#else

#ifndef NDEBUG
#define NDEBUG
#endif
#define VELOCYPACK_ASSERT(x) 
#endif

// check for environment type (32 or 64 bit)
// if the environment type cannot be determined reliably, then this will
// abort compilation. this will abort on systems that neither have 32 bit
// nor 64 bit pointers!
#if INTPTR_MAX == INT32_MAX
#define VELOCYPACK_32BIT

#elif INTPTR_MAX == INT64_MAX
#define VELOCYPACK_64BIT

#else
#error "Could not determine environment type (32 or 64 bits)"
#endif

// attribute used to tag potentially unused functions (used mostly in tests/)
#ifdef __GNUC__
#define VELOCYPACK_UNUSED __attribute__ ((unused))
#else
#define VELOCYPACK_UNUSED /* unused */
#endif

namespace arangodb {
  namespace velocypack {

    // unified size type for VPack, can be used on 32 and 64 bit
    // though no VPack values can exceed the bounds of 32 bit on a 32 bit OS
    typedef uint64_t ValueLength;

#ifndef VELOCYPACK_64BIT
    // check if the length is beyond the size of a SIZE_MAX on this platform
    static void CheckValueLength (ValueLength);
#else
    static inline void CheckValueLength (ValueLength) { 
      // do nothing on a 64 bit platform 
    }
#endif

    // returns current value for UTCDate
    int64_t CurrentUTCDateValue ();

    static inline uint64_t ToUInt64 (int64_t v) {
      // If v is negative, we need to add 2^63 to make it positive,
      // before we can cast it to an uint64_t:
      uint64_t shift2 = 1ULL << 63;
      int64_t shift = static_cast<int64_t>(shift2 - 1);
      return v >= 0 ? static_cast<uint64_t>(v)
                    : static_cast<uint64_t>((v + shift) + 1) + shift2;
      // Note that g++ and clang++ with -O3 compile this away to
      // nothing. Further note that a plain cast from int64_t to
      // uint64_t is not guaranteed to work for negative values!
    }

    static inline int64_t ToInt64 (uint64_t v) {
      uint64_t shift2 = 1ULL << 63;
      int64_t shift = static_cast<int64_t>(shift2 - 1);
      return v >= shift2 ? (static_cast<int64_t>(v - shift2) - shift) - 1
                         : static_cast<int64_t>(v);
    }
       
  }  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
