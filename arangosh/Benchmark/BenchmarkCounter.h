////////////////////////////////////////////////////////////////////////////////
/// @brief shared counter that can be accessed atomically
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BENCHMARK_BENCHMARK_COUNTER_H
#define TRIAGENS_BENCHMARK_BENCHMARK_COUNTER_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"

using namespace triagens::basics;

namespace triagens {
  namespace arangob {

// -----------------------------------------------------------------------------
// --SECTION--                                            class BenchmarkCounter
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Client
/// @{
////////////////////////////////////////////////////////////////////////////////

    template<class T>
    class BenchmarkCounter {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the counter
////////////////////////////////////////////////////////////////////////////////

        BenchmarkCounter (T initialValue, const T maxValue) : 
          _mutex(),
          _value(initialValue),
          _maxValue(maxValue),
          _failures(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the counter
////////////////////////////////////////////////////////////////////////////////
        
        ~BenchmarkCounter () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Client
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the counter value
////////////////////////////////////////////////////////////////////////////////

        T getValue() {
          MUTEX_LOCKER(this->_mutex);
          return _value;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the failures value
////////////////////////////////////////////////////////////////////////////////

        const size_t failures () {
          MUTEX_LOCKER(this->_mutex);
          return _failures;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the next x until the max is reached
////////////////////////////////////////////////////////////////////////////////

        T next (const T value) {
          T realValue = value;

          if (value == 0) {
            realValue = 1;
          }

          MUTEX_LOCKER(this->_mutex);

          T oldValue = _value;
          if (oldValue + realValue > _maxValue) {
            _value = _maxValue;
            return _maxValue - oldValue;
          }

          _value += realValue;
          return realValue;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief register a failure
////////////////////////////////////////////////////////////////////////////////

        void incFailures (const size_t value) {
          MUTEX_LOCKER(this->_mutex);
          _failures += value;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex protecting the counter
////////////////////////////////////////////////////////////////////////////////

        Mutex _mutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief the current value
////////////////////////////////////////////////////////////////////////////////

        T _value;

////////////////////////////////////////////////////////////////////////////////
/// @brief the maximum value
////////////////////////////////////////////////////////////////////////////////

        const T _maxValue;

////////////////////////////////////////////////////////////////////////////////
/// @brief the number of errors
////////////////////////////////////////////////////////////////////////////////

        size_t _failures;

    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
