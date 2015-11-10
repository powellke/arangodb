////////////////////////////////////////////////////////////////////////////////
/// @brief general server job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpServerJob.h"

#include "Basics/logging.h"
#include "Basics/WorkMonitor.h"
#include "Dispatcher/DispatcherQueue.h"
#include "HttpServer/AsyncJobManager.h"
#include "HttpServer/HttpCommTask.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServer.h"

using namespace arangodb;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                               class HttpServerJob
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new server job
////////////////////////////////////////////////////////////////////////////////

HttpServerJob::HttpServerJob (HttpServer* server,
                              HttpHandler* handler,
                              HttpCommTask* task) 
  : Job("HttpServerJob"),
    _server(server),
    _handler(handler),
    _task(task),
    _refCount(task == nullptr ? 1 : 2),
    _isInCleanup(false),
    _isDetached(task == nullptr) {

  TRI_ASSERT(_server != nullptr);
  TRI_ASSERT(_handler != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a server job
////////////////////////////////////////////////////////////////////////////////

HttpServerJob::~HttpServerJob () {
  if (_handler != nullptr) {
    WorkMonitor::releaseHandler(_handler);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the job is detached
////////////////////////////////////////////////////////////////////////////////

bool HttpServerJob::isDetached () const {
  return _isDetached;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

size_t HttpServerJob::queue () const {
  return _handler->queue();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpServerJob::setDispatcherThread (DispatcherThread* thread) {
  _handler->setDispatcherThread(thread);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::status_t HttpServerJob::work () {
  TRI_ASSERT(_handler != nullptr); 

  LOG_TRACE("beginning job %p", (void*) this);
 
  HandlerWorkStack work(_handler, false);
  this->RequestStatisticsAgent::transfer(_handler); // FMH TODO: MOVE

  // check if task is already gone
  if (! isDetached() && _task == nullptr) {
    return status_t(Job::JOB_DONE);
  }

  RequestStatisticsAgentSetRequestStart(_handler);
  _handler->prepareExecute();

  HttpHandler::status_t status;

  try {
    status = _handler->execute();
  }
  catch (...) {
    _handler->finalizeExecute();
    throw;
  }

  _handler->finalizeExecute();
  RequestStatisticsAgentSetRequestEnd(_handler);

  LOG_TRACE("finished job %p with status %d", (void*) this, (int) status.status);

  return status.jobStatus();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool HttpServerJob::cancel () {
  return _handler->cancel();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpServerJob::cleanup (DispatcherQueue* queue) {
  if (isDetached()) {
    _server->jobManager()->finishAsyncJob(this);
  }
  else {
    _isInCleanup.store(true);
    
    if (_task != nullptr) {
      _task->setHandler(_handler);
      _handler = nullptr;
      _task->signal();
    }
    
    _isInCleanup.store(false, std::memory_order_relaxed);
  }

  queue->removeJob(this);

  if (--_refCount == 0) {
    delete this;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpServerJob::beginShutdown () {

  // must wait until cleanup procedure is finished
  while (_isInCleanup.load()) {
    usleep(1000);
  }

  _task = nullptr;
  LOG_TRACE("shutdown job %p", (void*) this);

  if (--_refCount == 0) {
    delete this;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpServerJob::handleError (triagens::basics::Exception const& ex) {
  _handler->handleError(ex);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
