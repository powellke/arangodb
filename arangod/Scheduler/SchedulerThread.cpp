////////////////////////////////////////////////////////////////////////////////
/// @brief scheduler thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SchedulerThread.h"

#include "Basics/logging.h"
#include "Basics/MutexLocker.h"
#include "Basics/SpinLocker.h"
#include "velocypack/velocypack-aliases.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Scheduler/Scheduler.h"
#include "Scheduler/Task.h"

using namespace triagens::basics;
using namespace triagens::rest;

#ifdef TRI_USE_SPIN_LOCK_SCHEDULER_THREAD
#define SCHEDULER_LOCKER(a) SPIN_LOCKER(a)
#else
#define SCHEDULER_LOCKER(a) MUTEX_LOCKER(a)
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

SchedulerThread::SchedulerThread (Scheduler* scheduler, EventLoop loop, bool defaultLoop)
  : Thread("scheduler"),
    _scheduler(scheduler),
    _defaultLoop(defaultLoop),
    _loop(loop),
    _stopping(false),
    _stopped(false),
    _open(false),
    _hasWork(false) {

  // allow cancelation
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

SchedulerThread::~SchedulerThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the scheduler thread is up and running
////////////////////////////////////////////////////////////////////////////////

bool SchedulerThread::isStarted () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the scheduler thread for business
////////////////////////////////////////////////////////////////////////////////

bool SchedulerThread::open () {
  _open = true;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::beginShutdown () {
  LOG_TRACE("beginning shutdown sequence of scheduler thread (%llu)", (unsigned long long) threadId());

  _stopping = true;
  _scheduler->wakeupLoop(_loop);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

bool SchedulerThread::registerTask (Scheduler* scheduler, Task* task) {

  // thread has already been stopped
  if (_stopped.load()) {
    // do nothing
    return false;
  }

  TRI_ASSERT(scheduler != nullptr);
  TRI_ASSERT(task != nullptr);

  // same thread, in this case it does not matter if we are inside the loop
  if (threadId() == currentThreadId()) {
    bool ok = setupTask(task, scheduler, _loop);

    if (ok) {
      ++_numberTasks;
    }
    else {
      LOG_WARNING("In SchedulerThread::registerTask setupTask has failed");
      cleanupTask(task);
      deleteTask(task);
    }

    return ok;
  }

  Work w(SETUP, scheduler, task);

  // different thread, be careful - we have to stop the event loop
  // put the register request onto the queue
  SCHEDULER_LOCKER(_queueLock);

  _queue.push_back(w);
  _hasWork = true;

  scheduler->wakeupLoop(_loop);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::unregisterTask (Task* task) {

  // thread has already been stopped
  if (_stopped.load()) {
    // do nothing
  }

  // same thread, in this case it does not matter if we are inside the loop
  else if (threadId() == currentThreadId()) {
    cleanupTask(task);
    --_numberTasks;
  }

  // different thread, be careful - we have to stop the event loop
  else {
    Work w(CLEANUP, nullptr, task);

    // put the unregister request into the queue
    SCHEDULER_LOCKER(_queueLock);

    _queue.push_back(w);
    _hasWork = true;

    _scheduler->wakeupLoop(_loop);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::destroyTask (Task* task) {

  // thread has already been stopped
  if (_stopped.load()) {
    deleteTask(task);
  }

  // same thread, in this case it does not matter if we are inside the loop
  else if (threadId() == currentThreadId()) {
    cleanupTask(task);
    deleteTask(task);
    --_numberTasks;
  }

  // different thread, be careful - we have to stop the event loop
  else {
    // put the unregister request into the queue
    Work w(DESTROY, nullptr, task);

    SCHEDULER_LOCKER(_queueLock);

    _queue.push_back(w);
    _hasWork = true;

    _scheduler->wakeupLoop(_loop);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::run () {
  LOG_TRACE("scheduler thread started (%llu)", (unsigned long long) threadId());

  if (_defaultLoop) {
#ifdef TRI_HAVE_POSIX_THREADS
    sigset_t all;
    sigemptyset(&all);
    pthread_sigmask(SIG_SETMASK, &all, 0);
#endif
  }

  while (! _stopping.load() && ! _open.load()) {
    usleep(1000);
  }

  while (! _stopping.load()) {
    try {
      _scheduler->eventLoop(_loop);
    }
    catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
      if (_stopping.load()) {
        LOG_WARNING("caught cancelation exception during work");
        throw;
      }
#endif

      LOG_WARNING("caught exception from ev_loop");
    }

#if defined(DEBUG_SCHEDULER_THREAD)
    LOG_TRACE("left scheduler loop %d", (int) threadId());
#endif

    while (true) {
      Work w;

      {
        SCHEDULER_LOCKER(_queueLock);

        if (! _hasWork.load() || _queue.empty()) {
          break;
        }

        w = _queue.front();
        _queue.pop_front();
      }

      // will only get here if there is something to do
      switch (w.work) {
        case CLEANUP: {
          cleanupTask(w.task);
          --_numberTasks;
          break;
        }

        case SETUP: {
          bool ok = setupTask(w.task, w.scheduler, _loop);

          if (ok) {
            ++_numberTasks;
          }
          else {
            cleanupTask(w.task);
            deleteTask(w.task);
          }

          break;
        }

        case DESTROY: {
          cleanupTask(w.task);
          deleteTask(w.task);
          --_numberTasks;
          break;
        }

        case INVALID: {
          LOG_ERROR("logic error. got invalid Work item");
          break;
        }
      }
    }
  }

  LOG_TRACE("scheduler thread stopped (%llu)", (unsigned long long) threadId());

  _stopped = true;

  // pop all elements from the queue and delete them
  while (true) {
    Work w;

    {
      SCHEDULER_LOCKER(_queueLock);

      if (_queue.empty()) {
        break;
      }
    
      w = _queue.front();
      _queue.pop_front();
    }

    // will only get here if there is something to do
    switch (w.work) {
      case CLEANUP:
        break;

      case SETUP:
        break;

      case DESTROY:
        deleteTask(w.task);
        break;

      case INVALID: 
        LOG_ERROR("logic error. got invalid Work item");
        break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::addStatus(VPackBuilder* b) {
  Thread::addStatus(b);
  b->add("stopping", VPackValue(_stopping.load()));
  b->add("open", VPackValue(_open.load()));
  b->add("stopped", VPackValue(_stopped.load()));
  b->add("numberTasks", VPackValue(_numberTasks.load()));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
