////////////////////////////////////////////////////////////////////////////////
/// @brief debug helper handler
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestDebugHelperHandler.h"

#include "Basics/conversions.h"
#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Dispatcher/DispatcherThread.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestDebugHelperHandler::RestDebugHelperHandler (HttpRequest* request)
  : RestBaseHandler(request) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestDebugHelperHandler::isDirect () const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server version number
///
/// Parameters:
/// - sleep: sleep for X seconds
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestDebugHelperHandler::execute () {
  RequestStatisticsAgentSetIgnore(this);

  bool found;
  char const* sleepStr = _request->value("sleep", found);
  auto s = static_cast<unsigned long>(StringUtils::doubleDecimal(sleepStr) * 1000.0 * 1000.0);

  char const* blockStr = _request->value("block", found);
  bool block = (found && StringUtils::boolean(blockStr));

  if (block && _dispatcherThread != nullptr) {
    _dispatcherThread->block();
  }

  if (0 < s) {
    usleep(s);
  }

  if (block && _dispatcherThread != nullptr) {
    _dispatcherThread->unblock();
  }

  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("server", VPackValue("arango"));
    result.add("version", VPackValue(TRI_VERSION));
    result.add("sleep", VPackValue(s / 1000000.0));
    result.add("block", VPackValue(block));
    result.close();
    VPackSlice slice(result.start());
    generateResult(slice);
  }
  catch (...) {
    // Ignore the error
  }
  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
