////////////////////////////////////////////////////////////////////////////////
/// @brief work monitor class
///
/// @file
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
/// @author Dr. Frank Celler
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "WorkMonitor.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"

#include <boost/lockfree/queue.hpp>

#include <iostream>

using namespace triagens::basics;
using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief singleton
////////////////////////////////////////////////////////////////////////////////

static WorkMonitor WORK_MONITOR;

////////////////////////////////////////////////////////////////////////////////
/// @brief current work item
////////////////////////////////////////////////////////////////////////////////

static thread_local Thread* CURRENT_THREAD = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief all known threads
////////////////////////////////////////////////////////////////////////////////

static std::set<Thread*> THREADS;

////////////////////////////////////////////////////////////////////////////////
/// @brief guard for THREADS
////////////////////////////////////////////////////////////////////////////////

static Mutex THREADS_LOCK;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of free descriptions
////////////////////////////////////////////////////////////////////////////////

static boost::lockfree::queue<WorkDescription*> EMPTY_WORK_DESCRIPTION(128);

////////////////////////////////////////////////////////////////////////////////
/// @brief list of freeable descriptions
////////////////////////////////////////////////////////////////////////////////

static boost::lockfree::queue<WorkDescription*> FREEABLE_WORK_DESCRIPTION(128);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a description and its resources
////////////////////////////////////////////////////////////////////////////////

static void deleteWorkDescription (WorkDescription* desc) {
  if (desc->_destroy) {
    switch (desc->_type) {
      case WorkType::THREAD:
        WorkMonitor::DELETE_THREAD(desc);
        break;

      case WorkType::HANDLER:
        WorkMonitor::DELETE_HANDLER(desc);
        break;
    }
  }

  EMPTY_WORK_DESCRIPTION.push(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief string representation of a work description
////////////////////////////////////////////////////////////////////////////////

static std::string stringWorkDescription (WorkDescription* desc) {
  if (desc == nullptr) {
    return "idle";
  }

  switch (desc->_type) {
    case WorkType::THREAD:
      return WorkMonitor::STRING_THREAD(desc);

    case WorkType::HANDLER:
      return WorkMonitor::STRING_HANDLER(desc);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                            struct WorkDescription
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

WorkDescription::WorkDescription (WorkType type, WorkDescription* prev)
  : _type(type), _destroy(true), _prev(prev) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class WorkMonitor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors a new monitor
////////////////////////////////////////////////////////////////////////////////

WorkMonitor::WorkMonitor ()
  : Thread("Work Monitor"), _stopping(false) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty WorkDescription
////////////////////////////////////////////////////////////////////////////////

WorkDescription* WorkMonitor::createWorkDescription (WorkType type) {
  WorkDescription* desc;

  if (EMPTY_WORK_DESCRIPTION.pop(desc)) {
    desc->_type = type;
    desc->_prev = CURRENT_THREAD->workDescription();
    desc->_destroy = true;
  }
  else {
    desc = new WorkDescription(type, CURRENT_THREAD->workDescription());
  }

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief activates a WorkDescription
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::activateWorkDescription (WorkDescription* desc) {
  CURRENT_THREAD->setWorkDescription(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivates a WorkDescription
////////////////////////////////////////////////////////////////////////////////

WorkDescription* WorkMonitor::deactivateWorkDescription () {
  return CURRENT_THREAD->setPrevWorkDescription();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees an WorkDescription
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::freeWorkDescription (WorkDescription* desc) {
  FREEABLE_WORK_DESCRIPTION.push(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pushes a thread
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::pushThread (Thread* thread) {
  CURRENT_THREAD = thread;

  WorkDescription* desc = createWorkDescription(WorkType::THREAD);
  new (&(desc->_data).name) std::string(thread->name());

  activateWorkDescription(desc);

  {
    MutexLocker guard(&THREADS_LOCK);
    THREADS.insert(thread);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a thread
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::destroyThread (Thread* thread) {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc->_type == WorkType::THREAD);
  TRI_ASSERT(desc->_data.name == thread->name());

  freeWorkDescription(desc);

  {
    MutexLocker guard(&THREADS_LOCK);
    THREADS.erase(thread);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thread deleter
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::DELETE_THREAD (WorkDescription* desc) {
  (desc->_data.name).~basic_string<char>();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thread description string
////////////////////////////////////////////////////////////////////////////////

std::string WorkMonitor::STRING_THREAD (WorkDescription* desc) {
  return "idle";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::run () {
  const uint32_t maxSleep = 100 * 1000;
  const uint32_t minSleep = 100;
  uint32_t s = minSleep;

  double x = TRI_microtime();

  while (! _stopping) {
    bool found = false;
    WorkDescription* desc;

    while (FREEABLE_WORK_DESCRIPTION.pop(desc)) {
      found = true;
      deleteWorkDescription(desc);
    }

    if (found) {
      s = minSleep;
    }
    else if (s < maxSleep) {
      s *= 2;
    }

    double y = TRI_microtime();

    if (x + 10 < y) {
      x = y;

      MutexLocker guard(&THREADS_LOCK);

      for (auto& thread : THREADS) {
        WorkDescription* desc = thread->workDescription();

        std::cout << thread->name() << ": " << stringWorkDescription(desc) << "\n";
      }

      std::cout << "----------------------------------------------------------------------\n";
    }

    usleep(s);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    module methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief starts garbage collector
////////////////////////////////////////////////////////////////////////////////

void arangodb::InitializeWorkMonitor () {
  WORK_MONITOR.start();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops garbage collector
////////////////////////////////////////////////////////////////////////////////

void arangodb::ShutdownWorkMonitor () {
  WORK_MONITOR.shutdown();
  WORK_MONITOR.join();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
