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
#include "velocypack/velocypack-aliases.h"

#include <boost/lockfree/queue.hpp>

#include <iostream>

using namespace arangodb;
using namespace triagens::basics;

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
        break;

      case WorkType::HANDLER:
        WorkMonitor::DELETE_HANDLER(desc);
        break;
    }
  }

  EMPTY_WORK_DESCRIPTION.push(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief vpack representation of a work description
////////////////////////////////////////////////////////////////////////////////

static void vpackWorkDescription (VPackBuilder* b, WorkDescription* desc) {
  switch (desc->_type) {
    case WorkType::THREAD:
      WorkMonitor::VPACK_THREAD(b, desc);
      break;

    case WorkType::HANDLER:
      WorkMonitor::VPACK_HANDLER(b, desc);
      break;
  }

  if (desc->_prev != nullptr) {
    b->add("parent", VPackValue(VPackValueType::Object));
    vpackWorkDescription(b, desc->_prev);
    b->close();
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
  desc->_data.thread = thread;
  // new (&(desc->_data).name) std::string(thread->name());

  activateWorkDescription(desc);

  {
    MutexLocker guard(&THREADS_LOCK);
    THREADS.insert(thread);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a thread
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::popThread (Thread* thread) {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc->_type == WorkType::THREAD);
  TRI_ASSERT(desc->_data.thread == thread);

  freeWorkDescription(desc);

  {
    MutexLocker guard(&THREADS_LOCK);
    THREADS.erase(thread);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thread description string
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::VPACK_THREAD (VPackBuilder* b, WorkDescription* desc) {
  b->add("type", VPackValue("thread"));
  b->add("name", VPackValue(desc->_data.thread->name()));
  b->add("status", VPackValue(VPackValueType::Object));
  desc->_data.thread->addStatus(b);
  b->close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief text deleter
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::DELETE_TEXT (WorkDescription* desc) {
  (desc->_data.text).~basic_string<char>();
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
      VPackBuilder b;
      b.add(VPackValue(VPackValueType::Array));

      for (auto& thread : THREADS) {
        WorkDescription* desc = thread->workDescription();

        if (desc != nullptr) {
          b.add(VPackValue(VPackValueType::Object));
          vpackWorkDescription(&b, desc);
          b.close();
        }
      }

      b.close();

      VPackSlice s(b.start());

      VPackOptions options;
      options.prettyPrint = true;

      VPackStringSink sink;

      VPackDumper dumper(&sink, options);
      dumper.dump(s);

      std::cout << sink.buffer << "\n";

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
