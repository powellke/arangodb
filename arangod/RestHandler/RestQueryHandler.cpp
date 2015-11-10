////////////////////////////////////////////////////////////////////////////////
/// @brief query request handler
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestQueryHandler.h"

#include "Aql/Query.h"
#include "Aql/QueryList.h"
#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Basics/json.h"
#include "Basics/string-buffer.h"
#include "Basics/json-utilities.h"
#include "Rest/HttpRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterMethods.h"

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

RestQueryHandler::RestQueryHandler (HttpRequest* request, ApplicationV8* applicationV8)
  : RestVocbaseBaseHandler(request),
    _applicationV8(applicationV8) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestQueryHandler::isDirect () const {
  return _request->requestType() != HttpRequest::HTTP_REQUEST_POST;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestQueryHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: deleteQuery(); break;
    case HttpRequest::HTTP_REQUEST_GET:    readQuery(); break;
    case HttpRequest::HTTP_REQUEST_PUT:    replaceProperties(); break;
    case HttpRequest::HTTP_REQUEST_POST:   parseQuery(); break;

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
/// @startDocuBlock GetApiQueryProperties
/// @brief returns the configuration for the AQL query tracking
///
/// @RESTHEADER{GET /_api/query/properties, Returns the properties for the AQL query tracking}
///
/// @RESTDESCRIPTION
/// Returns the current query tracking configuration. The configuration is a
/// JSON object with the following properties:
/// 
/// - *enabled*: if set to *true*, then queries will be tracked. If set to 
///   *false*, neither queries nor slow queries will be tracked.
///
/// - *trackSlowQueries*: if set to *true*, then slow queries will be tracked
///   in the list of slow queries if their runtime exceeds the value set in 
///   *slowQueryThreshold*. In order for slow queries to be tracked, the *enabled*
///   property must also be set to *true*.
/// 
/// - *maxSlowQueries*: the maximum number of slow queries to keep in the list
///   of slow queries. If the list of slow queries is full, the oldest entry in
///   it will be discarded when additional slow queries occur.
///
/// - *slowQueryThreshold*: the threshold value for treating a query as slow. A
///   query with a runtime greater or equal to this threshold value will be
///   put into the list of slow queries when slow query tracking is enabled.
///   The value for *slowQueryThreshold* is specified in seconds.
///
/// - *maxQueryStringLength*: the maximum query string length to keep in the
///   list of queries. Query strings can have arbitrary lengths, and this property
///   can be used to save memory in case very long query strings are used. The
///   value is specified in bytes.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if properties were retrieved successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestQueryHandler::readQueryProperties () {
  try {
    auto queryList = static_cast<QueryList*>(_vocbase->_queries);

    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("error", VPackValue(false));
    result.add("code", VPackValue(HttpResponse::OK));
    result.add("enabled", VPackValue(queryList->enabled()));
    result.add("trackSlowQueries", VPackValue(queryList->trackSlowQueries()));
    result.add("maxSlowQueries", VPackValue(static_cast<uint64_t>(queryList->maxSlowQueries())));
    result.add("slowQueryThreshold", VPackValue(queryList->slowQueryThreshold()));
    result.add("maxQueryStringLength", VPackValue(static_cast<uint64_t>(queryList->maxQueryStringLength())));
    result.close();
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
/// @startDocuBlock GetApiQueryCurrent
/// @brief returns a list of currently running AQL queries
///
/// @RESTHEADER{GET /_api/query/current, Returns the currently running AQL queries}
///
/// @RESTDESCRIPTION
/// Returns an array containing the AQL queries currently running in the selected
/// database. Each query is a JSON object with the following attributes:
///
/// - *id*: the query's id
///
/// - *query*: the query string (potentially truncated)
///
/// - *started*: the date and time when the query was started
///
/// - *runTime*: the query's run time up to the point the list of queries was
///   queried
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned when the list of queries can be retrieved successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock GetApiQuerySlow
/// @brief returns a list of slow running AQL queries
///
/// @RESTHEADER{GET /_api/query/slow, Returns the list of slow AQL queries}
///
/// @RESTDESCRIPTION
/// Returns an array containing the last AQL queries that exceeded the slow 
/// query threshold in the selected database. 
/// The maximum amount of queries in the list can be controlled by setting
/// the query tracking property `maxSlowQueries`. The threshold for treating
/// a query as *slow* can be adjusted by setting the query tracking property
/// `slowQueryThreshold`.
///
/// Each query is a JSON object with the following attributes:
///
/// - *id*: the query's id
///
/// - *query*: the query string (potentially truncated)
///
/// - *started*: the date and time when the query was started
///
/// - *runTime*: the query's run time up to the point the list of queries was
///   queried
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned when the list of queries can be retrieved successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestQueryHandler::readQuery (bool slow) {
  try {
    auto queryList = static_cast<QueryList*>(_vocbase->_queries);
    auto const&& queries = slow ? queryList->listSlow() : queryList->listCurrent(); 
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Array));

    for (auto it : queries) {
      const auto&& timeString = TRI_StringTimeStamp(it.started);
      const auto& queryString = it.queryString;

      result.add(VPackValue(VPackValueType::Object));
      result.add("id", VPackValue(StringUtils::itoa(it.id)));
      result.add("query", VPackValue(queryString));
      result.add("started", VPackValue(timeString));
      result.add("runTime", VPackValue(it.runTime));
      result.close();
    }
    result.close();
    VPackSlice s = result.slice();

    generateResult(s);
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
/// @brief returns AQL query tracking
////////////////////////////////////////////////////////////////////////////////

bool RestQueryHandler::readQuery () {
  const auto& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting GET /_api/query/<type>");
    return true;
  }

  const auto& name = suffix[0];

  if (name == "slow") {
    return readQuery(true);
  }
  else if (name == "current") {
    return readQuery(false);
  }
  else if (name == "properties") {
    return readQueryProperties();
  }

  generateError(HttpResponse::NOT_FOUND,
                TRI_ERROR_HTTP_NOT_FOUND,
                "unknown type '" + name + "', expecting 'slow', 'current', or 'properties'");
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock DeleteApiQuerySlow
/// @brief clears the list of slow AQL queries
///
/// @RESTHEADER{DELETE /_api/query/slow, Clears the list of slow AQL queries}
///
/// @RESTDESCRIPTION
/// Clears the list of slow AQL queries
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// The server will respond with *HTTP 200* when the list of queries was
/// cleared successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestQueryHandler::deleteQuerySlow () {
  auto queryList = static_cast<triagens::aql::QueryList*>(_vocbase->_queries);
  queryList->clearSlow();

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
/// @startDocuBlock DeleteApiQueryKill
/// @brief kills an AQL query
///
/// @RESTHEADER{DELETE /_api/query/{query-id}, Kills a running AQL query}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{query-id,string,required}
/// The id of the query.
///
/// @RESTDESCRIPTION
/// Kills a running query. The query will be terminated at the next cancelation
/// point.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// The server will respond with *HTTP 200* when the query was still running when
/// the kill request was executed and the query's kill flag was set.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request.
///
/// @RESTRETURNCODE{404}
/// The server will respond with *HTTP 404* when no query with the specified
/// id was found.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestQueryHandler::deleteQuery (const string& name) {
  auto id = StringUtils::uint64(name);
  auto queryList = static_cast<triagens::aql::QueryList*>(_vocbase->_queries);
  TRI_ASSERT(queryList != nullptr);

  auto res = queryList->kill(id);

  if (res == TRI_ERROR_NO_ERROR) {
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
  }
  else {
    generateError(HttpResponse::BAD, res, "cannot kill query '" + name + "'");
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief interrupts a query
////////////////////////////////////////////////////////////////////////////////

bool RestQueryHandler::deleteQuery () {
  const auto& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/query/<id> or /_api/query/slow");
    return true;
  }

  const auto& name = suffix[0];

  if (name == "slow") {
    return deleteQuerySlow();
  }
  else {
    return deleteQuery(name);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock PutApiQueryProperties
/// @brief changes the configuration for the AQL query tracking
///
/// @RESTHEADER{PUT /_api/query/properties, Changes the properties for the AQL query tracking}
///
/// @RESTBODYPARAM{enabled,boolean,required,}
/// If set to *true*, then queries will be tracked. If set to 
/// *false*, neither queries nor slow queries will be tracked.
///
/// @RESTBODYPARAM{trackSlowQueries,boolean,required,}
/// If set to *true*, then slow queries will be tracked
/// in the list of slow queries if their runtime exceeds the value set in 
/// *slowQueryThreshold*. In order for slow queries to be tracked, the *enabled*
/// property must also be set to *true*.
/// 
/// @RESTBODYPARAM{maxSlowQueries,integer,required,int64}
/// The maximum number of slow queries to keep in the list
/// of slow queries. If the list of slow queries is full, the oldest entry in
/// it will be discarded when additional slow queries occur.
///
/// @RESTBODYPARAM{slowQueryThreshold,integer,required,int64}
/// The threshold value for treating a query as slow. A
/// query with a runtime greater or equal to this threshold value will be
/// put into the list of slow queries when slow query tracking is enabled.
/// The value for *slowQueryThreshold* is specified in seconds.
///
/// @RESTBODYPARAM{maxQueryStringLength,integer,required,int64}
/// The maximum query string length to keep in the list of queries.
/// Query strings can have arbitrary lengths, and this property
/// can be used to save memory in case very long query strings are used. The
/// value is specified in bytes.
///
/// @RESTDESCRIPTION
/// The properties need to be passed in the attribute *properties* in the body
/// of the HTTP request. *properties* needs to be a JSON object.
///
/// After the properties have been changed, the current set of properties will
/// be returned in the HTTP response.
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

bool RestQueryHandler::replaceProperties () {
  const auto& suffix = _request->suffix();

  if (suffix.size() != 1 || suffix[0] != "properties") {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/query/properties");
    return true;
  }

  unique_ptr<TRI_json_t> body(parseJsonBody());

  if (body == nullptr) {
    // error message generated in parseJsonBody
    return true;
  }

  auto queryList = static_cast<triagens::aql::QueryList*>(_vocbase->_queries);

  try {
    bool enabled = queryList->enabled();
    bool trackSlowQueries = queryList->trackSlowQueries();
    size_t maxSlowQueries = queryList->maxSlowQueries();
    double slowQueryThreshold = queryList->slowQueryThreshold();
    size_t maxQueryStringLength = queryList->maxQueryStringLength();

    // TODO(fc) add a "hasSomething" to JsonHelper?

    if (JsonHelper::getObjectElement(body.get(), "enabled") != nullptr) {
      enabled = JsonHelper::checkAndGetBooleanValue(body.get(), "enabled");
    }

    if (JsonHelper::getObjectElement(body.get(), "trackSlowQueries") != nullptr) {
      trackSlowQueries = JsonHelper::checkAndGetBooleanValue(body.get(), "trackSlowQueries");
    }

    if (JsonHelper::getObjectElement(body.get(), "maxSlowQueries") != nullptr) {
      maxSlowQueries = JsonHelper::checkAndGetNumericValue<size_t>(body.get(), "maxSlowQueries");
    }

    if (JsonHelper::getObjectElement(body.get(), "slowQueryThreshold") != nullptr) {
      slowQueryThreshold = JsonHelper::checkAndGetNumericValue<double>(body.get(), "slowQueryThreshold");
    }

    if (JsonHelper::getObjectElement(body.get(), "maxQueryStringLength") != nullptr) {
      maxQueryStringLength = JsonHelper::checkAndGetNumericValue<size_t>(body.get(), "maxQueryStringLength");
    }

    queryList->enabled(enabled);
    queryList->trackSlowQueries(trackSlowQueries);
    queryList->maxSlowQueries(maxSlowQueries);
    queryList->slowQueryThreshold(slowQueryThreshold);
    queryList->maxQueryStringLength(maxQueryStringLength);

    return readQueryProperties();
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
/// @startDocuBlock PostApiQueryProperties
/// @brief parse an AQL query and return information about it
///
/// @RESTHEADER{POST /_api/query, Parse an AQL query}
///
/// @RESTBODYPARAM{query,string,required,string}
/// To validate a query string without executing it, the query string can be
/// passed to the server via an HTTP POST request.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the query is valid, the server will respond with *HTTP 200* and
/// return the names of the bind parameters it found in the query (if any) in
/// the *bindVars* attribute of the response. It will also return an array
/// of the collections used in the query in the *collections* attribute. 
/// If a query can be parsed successfully, the *ast* attribute of the returned
/// JSON will contain the abstract syntax tree representation of the query.
/// The format of the *ast* is subject to change in future versions of 
/// ArangoDB, but it can be used to inspect how ArangoDB interprets a given
/// query. Note that the abstract syntax tree will be returned without any
/// optimizations applied to it.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
/// or if the query contains a parse error. The body of the response will
/// contain the error details embedded in a JSON object.
///
/// @EXAMPLES
///
/// a Valid query
///
///     @EXAMPLE_ARANGOSH_RUN{RestQueryValid}
///     var url = "/_api/query";
///     var body = '{ "query" : "FOR p IN products FILTER p.name == @name LIMIT 2 RETURN p.n" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     @END_EXAMPLE_ARANGOSH_RUN
///
/// an Invalid query
///
///     @EXAMPLE_ARANGOSH_RUN{RestQueryInvalid}
///     var url = "/_api/query";
///     var body = '{ "query" : "FOR p IN products FILTER p.name = @name LIMIT 2 RETURN p.n" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
///     @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestQueryHandler::parseQuery () {
  const auto& suffix = _request->suffix();

  if (! suffix.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/query");
    return true;
  }

  unique_ptr<TRI_json_t> body(parseJsonBody());

  if (body.get() == nullptr) {
    // error message generated in parseJsonBody
    return true;
  }

  try {
    const string&& queryString = JsonHelper::checkAndGetStringValue(body.get(), "query");

    Query query(_applicationV8, true, _vocbase, queryString.c_str(), queryString.size(), nullptr, nullptr, PART_MAIN);
    
    auto parseResult = query.parse();

    if (parseResult.code != TRI_ERROR_NO_ERROR) {
      generateError(HttpResponse::BAD,
                    parseResult.code,
                    parseResult.details);
      return true;
    }

    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("error", VPackValue(false));
    result.add("code", VPackValue(HttpResponse::OK));
    result.add("parsed", VPackValue(true));

    result.add("collections", VPackValue(VPackValueType::Array));
    for (const auto& it : parseResult.collectionNames) {
      result.add(VPackValue(it));
    }
    result.close(); // Collections

    result.add("bindVars", VPackValue(VPackValueType::Array));
    for (const auto& it : parseResult.bindParameters) {
      result.add(VPackValue(it));
    }
    result.close(); // bindVars
    // TODO The AST
    // result.add("ast", Json(TRI_UNKNOWN_MEM_ZONE, parseResult.json, Json::NOFREE).copy());
    
    result.add("warnings", VPackValue(VPackValueType::Array));
    // TODO The Warnings
    // if (parseResult.warnings != nullptr) {
    //   result.set("warnings", Json(TRI_UNKNOWN_MEM_ZONE, parseResult.warnings, Json::NOFREE).copy());
    // }
    result.close(); //warnings

    result.close(); // base
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
