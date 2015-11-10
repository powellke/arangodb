////////////////////////////////////////////////////////////////////////////////
/// @brief query cache request handler
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
/// @author Jan Steemann
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestQueryCacheHandler.h"

#include "Aql/QueryCache.h"
#include "Rest/HttpRequest.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestQueryCacheHandler::RestQueryCacheHandler (HttpRequest* request)
  : RestVocbaseBaseHandler(request) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestQueryCacheHandler::isDirect () const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestQueryCacheHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: clearCache(); break;
    case HttpRequest::HTTP_REQUEST_GET:    readProperties(); break;
    case HttpRequest::HTTP_REQUEST_PUT:    replaceProperties(); break;

    case HttpRequest::HTTP_REQUEST_POST:   
    case HttpRequest::HTTP_REQUEST_HEAD:
    case HttpRequest::HTTP_REQUEST_PATCH:
    case HttpRequest::HTTP_REQUEST_ILLEGAL:
    default: {
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      break;
    }
  }

  // this handler is done
  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock DeleteApiQueryCache
/// @brief clears the AQL query cache
///
/// @RESTHEADER{DELETE /_api/query-cache, Clears any results in the AQL query cache}
///
/// @RESTDESCRIPTION
/// clears the query cache
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// The server will respond with *HTTP 200* when the cache was cleared
/// successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestQueryCacheHandler::clearCache () {
  auto queryCache = triagens::aql::QueryCache::instance();
  queryCache->invalidate();
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("error", VPackValue(false));
    result.add("code", VPackValue(HttpResponse::OK));
    result.close();
    VPackSlice slice = result.slice();
    generateResult(slice);
  }
  catch (...) {
    // Ignore the error
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock GetApiQueryCacheProperties
/// @brief returns the global configuration for the AQL query cache
///
/// @RESTHEADER{GET /_api/query-cache/properties, Returns the global properties for the AQL query cache}
///
/// @RESTDESCRIPTION
/// Returns the global AQL query cache configuration. The configuration is a
/// JSON object with the following properties:
/// 
/// - *mode*: the mode the AQL query cache operates in. The mode is one of the following
///   values: *off*, *on* or *demand*.
///
/// - *maxResults*: the maximum number of query results that will be stored per database-specific
///   cache.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the properties can be retrieved successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestQueryCacheHandler::readProperties () {
  try {
    auto queryCache = triagens::aql::QueryCache::instance();

    VPackBuilder result = queryCache->properties();
    VPackSlice slice = result.slice();
    generateResult(slice);
  }
  catch (Exception const& err) {
    handleError(err);
  }
  catch (std::exception const& ex) {
    triagens::basics::Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  }
  catch (...) {
    triagens::basics::Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock PutApiQueryCacheProperties
/// @brief changes the configuration for the AQL query cache
///
/// @RESTHEADER{PUT /_api/query-cache/properties, Globally adjusts the AQL query result cache properties}
///
/// @RESTDESCRIPTION
/// After the properties have been changed, the current set of properties will
/// be returned in the HTTP response.
///
/// Note: changing the properties may invalidate all results in the cache.
/// The global properties for AQL query cache.
/// The properties need to be passed in the attribute *properties* in the body
/// of the HTTP request. *properties* needs to be a JSON object with the following
/// properties:
/// 
/// @RESTBODYPARAM{mode,string,required,string}
///  the mode the AQL query cache should operate in. Possible values are *off*, *on* or *demand*.
///
/// @RESTBODYPARAM{maxResults,integer,required,int64}
/// the maximum number of query results that will be stored per database-specific cache.
///
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the properties were changed successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestQueryCacheHandler::replaceProperties () {
  auto const& suffix = _request->suffix();

  if (suffix.size() != 1 || suffix[0] != "properties") {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/query-cache/properties");
    return true;
  }
  bool validBody = true;
  VPackBuilder parsedBody = parseVelocyPackBody(validBody);

  if (! validBody) {
    // error message generated in parseJsonBody
    return true;
  }
  VPackSlice body = parsedBody.slice();

  if (! body.isObject()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON-Object body");
    return true;
  }

  auto queryCache = triagens::aql::QueryCache::instance();

  try {
    std::pair<std::string, size_t> cacheProperties;
    queryCache->properties(cacheProperties);

    VPackSlice attribute = body.get("mode");
    if (attribute.isString()) {
      VPackValueLength len;
      char const* st = attribute.getString(len);
      cacheProperties.first = std::string(st, len);
    }

    attribute = body.get("maxResults");

    if (attribute.isNumber()) {
      cacheProperties.second = static_cast<size_t>(attribute.getUInt());
    }

    queryCache->setProperties(cacheProperties);

    return readProperties();
  }
  catch (Exception const& err) {
    handleError(err);
  }
  catch (std::exception const& ex) {
    triagens::basics::Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  }
  catch (...) {
    triagens::basics::Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
