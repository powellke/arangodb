////////////////////////////////////////////////////////////////////////////////
/// @brief simple document batch request handler
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestSimpleHandler.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "V8Server/ApplicationV8.h"

using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestSimpleHandler::RestSimpleHandler (HttpRequest* request,
                                      std::pair<triagens::arango::ApplicationV8*, triagens::aql::QueryRegistry*>* pair) 
  : RestVocbaseBaseHandler(request),
    _applicationV8(pair->first),
    _queryRegistry(pair->second),
    _queryLock(),
    _query(nullptr),
    _queryKilled(false) {

}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestSimpleHandler::execute () {
  // extract the request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_PUT) {
    bool parsingSuccess = true;
    VPackBuilder parsedBody = parseVelocyPackBody(parsingSuccess);

    if (! parsingSuccess) {
      return status_t(HANDLER_DONE);
    }
    VPackSlice body = parsedBody.slice();

    if (! body.isObject()) {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting JSON object body");
      return status_t(HANDLER_DONE);
    }
    
    char const* prefix = _request->requestPath();

    if (strcmp(prefix, RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH.c_str()) == 0) {
      removeByKeys(body);
    }
    else if (strcmp(prefix, RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH.c_str()) == 0) {
      lookupByKeys(body);
    }
    else {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "unsupported value for <operation>");
    }

    return status_t(HANDLER_DONE);
  }

  generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED); 
  return status_t(HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestSimpleHandler::cancel () {
  return cancelQuery();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief register the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::registerQuery (triagens::aql::Query* query) {
  MUTEX_LOCKER(_queryLock);

  TRI_ASSERT(_query == nullptr);
  _query = query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::unregisterQuery () {
  MUTEX_LOCKER(_queryLock);

  _query = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the currently running query
////////////////////////////////////////////////////////////////////////////////

bool RestSimpleHandler::cancelQuery () {
  MUTEX_LOCKER(_queryLock);

  if (_query != nullptr) {
    _query->killed(true);
    _queryKilled = true;
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was canceled
////////////////////////////////////////////////////////////////////////////////

bool RestSimpleHandler::wasCanceled () {
  MUTEX_LOCKER(_queryLock);
  return _queryKilled;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock RestRemoveByKeys
/// @brief removes multiple documents by their keys
///
/// @RESTHEADER{PUT /_api/simple/remove-by-keys, Remove documents by their keys}
///
/// @RESTBODYPARAM{collection,string,required,string}
/// The name of the collection to look in for the documents to remove
///
/// @RESTBODYPARAM{keys,array,required,string}
/// array with the _keys of documents to remove.
///
/// @RESTBODYPARAM{options,object,optional,put_api_simple_remove_by_keys_opts}
/// a json object which can contains following attributes:
///
/// @RESTSTRUCT{waitForSync,put_api_simple_remove_by_keys_opts,string,optional,string}
/// if set to true, then all removal operations will
/// instantly be synchronized to disk. If this is not specified, then the
/// collection's default sync behavior will be applied.
///
/// @RESTDESCRIPTION
/// Looks up the documents in the specified collection using the array of keys
/// provided, and removes all documents from the collection whose keys are
/// contained in the *keys* array. Keys for which no document can be found in
/// the underlying collection are ignored, and no exception will be thrown for 
/// them.
///
/// The body of the response contains a JSON object with information how many 
/// documents were removed (and how many were not). The *removed* attribute will
/// contain the number of actually removed documents. The *ignored* attribute 
/// will contain the number of keys in the request for which no matching document
/// could be found.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the operation was carried out successfully. The number of removed
/// documents may still be 0 in this case if none of the specified document keys
/// were found in the collection.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{405}
/// is returned if the operation was called with a different HTTP METHOD than PUT.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleRemove}
///     var cn = "test";
///   ~ db._drop(cn);
///     db._create(cn);
///     keys = [ ];
///     for (var i = 0; i < 10; ++i) {
///       db.test.insert({ _key: "test" + i });
///       keys.push("test" + i);
///     }
///
///     var url = "/_api/simple/remove-by-keys";
///     var data = { keys: keys, collection: cn };
///     var response = logCurlRequest('PUT', url, data);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleRemoveNotFound}
///     var cn = "test";
///   ~ db._drop(cn);
///     db._create(cn);
///     keys = [ ];
///     for (var i = 0; i < 10; ++i) {
///       db.test.insert({ _key: "test" + i });
///     }
///
///     var url = "/_api/simple/remove-by-keys";
///     var data = { keys: [ "foo", "bar", "baz" ], collection: cn };
///     var response = logCurlRequest('PUT', url, data);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::removeByKeys (VPackSlice const& slice) {
  TRI_ASSERT(slice.isObject());
  try { 
    std::string collectionName;
    {
      VPackSlice const value = slice.get("collection");

      if (! value.isString()) {
        generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting string for <collection>");
        return;
      }
    
      collectionName = value.copyString();

      if (! collectionName.empty()) {
        auto const* col = TRI_LookupCollectionByNameVocBase(_vocbase, collectionName.c_str());

        if (col != nullptr && collectionName.compare(col->_name) != 0) {
          // user has probably passed in a numeric collection id.
          // translate it into a "real" collection name
          collectionName = std::string(col->_name);
        }
      }
    }

    VPackSlice const keys = slice.get("keys");

    if (! keys.isArray()) { 
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting array for <keys>");
      return;
    }
    
    bool waitForSync = false;
    {
      VPackSlice const value = slice.get("options");
      if (value.isObject()) {
        VPackSlice wfs = value.get("waitForSync");
        if (wfs.isBool()) {
          waitForSync = wfs.getBool();
        }
      }
    }

    VPackBuilder bindVars;
    bindVars.addObject();
    bindVars.add("@collection", VPackValue(collectionName));
    bindVars.add("keys", keys);
    bindVars.close();
    VPackSlice varsSlice = bindVars.slice();

    std::string aql("FOR key IN @keys REMOVE key IN @@collection OPTIONS { ignoreErrors: true, waitForSync: ");
    aql.append(waitForSync ? "true" : "false");
    aql.append(" }");
   
    triagens::aql::Query query(_applicationV8, 
                               false, 
                               _vocbase, 
                               aql.c_str(),
                               aql.size(),
                               triagens::basics::VelocyPackHelper::velocyPackToJson(varsSlice),
                               nullptr,
                               triagens::aql::PART_MAIN);
 
    registerQuery(&query); 
    auto queryResult = query.execute(_queryRegistry);
    unregisterQuery(); 

    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
          (queryResult.code == TRI_ERROR_QUERY_KILLED && wasCanceled())) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
      }

      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }

    { 
      _response = createResponse(HttpResponse::OK);
      _response->setContentType("application/json; charset=utf-8");

      size_t ignored = 0;
      size_t removed = 0;

      if (queryResult.stats != nullptr) {
        auto found = TRI_LookupObjectJson(queryResult.stats, "writesIgnored");
        if (TRI_IsNumberJson(found)) {
          ignored = static_cast<size_t>(found->_value._number);
        }
        
        found = TRI_LookupObjectJson(queryResult.stats, "writesExecuted");
        if (TRI_IsNumberJson(found)) {
          removed = static_cast<size_t>(found->_value._number);
        }
      }

      VPackBuilder result;
      result.add(VPackValue(VPackValueType::Object));
      result.add("removed", VPackValue(removed));
      result.add("ignored", VPackValue(ignored));
      result.add("error", VPackValue(false));
      result.add("code", VPackValue(_response->responseCode()));
      result.close();
      VPackSlice s = result.slice();

      triagens::basics::StringBufferAdapter buffer(_response->body().stringBuffer());
      VPackDumper dumper(&buffer); 
      dumper.dump(s);
    }
  }  
  catch (triagens::basics::Exception const& ex) {
    unregisterQuery(); 
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  }
  catch (...) {
    unregisterQuery(); 
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock RestLookupByKeys
/// @brief fetches multiple documents by their keys
///
/// @RESTHEADER{PUT /_api/simple/lookup-by-keys, Find documents by their keys}
///
/// @RESTBODYPARAM{collection,string,required,string}
/// The name of the collection to look in for the documents
///
/// @RESTBODYPARAM{keys,array,required,string}
/// array with the _keys of documents to remove.
///
/// @RESTDESCRIPTION
/// Looks up the documents in the specified collection using the array of keys
/// provided. All documents for which a matching key was specified in the *keys*
/// array and that exist in the collection will be returned. 
/// Keys for which no document can be found in the underlying collection are ignored, 
/// and no exception will be thrown for them.
///
/// The body of the response contains a JSON object with a *documents* attribute. The
/// *documents* attribute is an array containing the matching documents. The order in
/// which matching documents are present in the result array is unspecified.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the operation was carried out successfully. 
///
/// @RESTRETURNCODE{404}
/// is returned if the collection was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{405}
/// is returned if the operation was called with a different HTTP METHOD than PUT.
///
/// @EXAMPLES
///
/// Looking up existing documents
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleLookup}
///     var cn = "test";
///   ~ db._drop(cn);
///     db._create(cn);
///     keys = [ ];
///     for (i = 0; i < 10; ++i) {
///       db.test.insert({ _key: "test" + i, value: i });
///       keys.push("test" + i);
///     }
///
///     var url = "/_api/simple/lookup-by-keys";
///     var data = { keys: keys, collection: cn };
///     var response = logCurlRequest('PUT', url, data);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Looking up non-existing documents
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleLookupNotFound}
///     var cn = "test";
///   ~ db._drop(cn);
///     db._create(cn);
///     keys = [ ];
///     for (i = 0; i < 10; ++i) {
///       db.test.insert({ _key: "test" + i, value: i });
///     }
///
///     var url = "/_api/simple/lookup-by-keys";
///     var data = { keys: [ "foo", "bar", "baz" ], collection: cn };
///     var response = logCurlRequest('PUT', url, data);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::lookupByKeys (VPackSlice const& slice) {
  try {
    std::string collectionName;
    { 
      VPackSlice const value = slice.get("collection");
      if (! value.isString()) {
        generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting string for <collection>");
        return;
      }
      collectionName = slice.copyString();

      if (! collectionName.empty()) {
        auto const* col = TRI_LookupCollectionByNameVocBase(_vocbase, collectionName.c_str());

        if (col != nullptr && collectionName.compare(col->_name) != 0) {
          // user has probably passed in a numeric collection id.
          // translate it into a "real" collection name
          collectionName = std::string(col->_name);
        }
      }
    }

    VPackSlice const keys = slice.get("keys");

    if (! keys.isArray()) { 
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting array for <keys>");
      return;
    }

    VPackBuilder bindVars;
    bindVars.add(VPackValue(VPackValueType::Object));
    bindVars.add("@collection", VPackValue(collectionName));

    VPackSlice stripped = triagens::aql::BindParameters::StripCollectionNames(keys, collectionName.c_str());

    bindVars.add("keys", stripped);
    bindVars.close();
    VPackSlice varsSlice = bindVars.slice();

    std::string const aql("FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc");
    
    triagens::aql::Query query(_applicationV8, 
                               false, 
                               _vocbase, 
                               aql.c_str(),
                               aql.size(),
                               triagens::basics::VelocyPackHelper::velocyPackToJson(varsSlice),
                               nullptr,
                               triagens::aql::PART_MAIN);
 
    registerQuery(&query); 
    auto queryResult = query.execute(_queryRegistry);
    unregisterQuery(); 

    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
          (queryResult.code == TRI_ERROR_QUERY_KILLED && wasCanceled())) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
      }

      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }

    { 
      _response = createResponse(HttpResponse::OK);
      _response->setContentType("application/json; charset=utf-8");

      size_t n = 10; 
      if (TRI_IsArrayJson(queryResult.json)) {
        n = TRI_LengthArrayJson(queryResult.json);
      }

      triagens::basics::Json result(triagens::basics::Json::Object, 3);
      result.set("documents", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, queryResult.json, triagens::basics::Json::AUTOFREE));
      queryResult.json = nullptr;
      
      result.set("error", triagens::basics::Json(false));
      result.set("code", triagens::basics::Json(static_cast<double>(_response->responseCode())));

      // reserve 48 bytes per result document by default
      int res = _response->body().reserve(48 * n);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
       
      result.dump(_response->body());
    }
  }  
  catch (triagens::basics::Exception const& ex) {
    unregisterQuery(); 
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  }
  catch (...) {
    unregisterQuery(); 
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
