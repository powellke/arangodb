/*jshint strict: false */
/*global require, describe, expect, beforeEach, afterEach, it */
////////////////////////////////////////////////////////////////////////////////
/// @brief test the server-side MVCC behaviour
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var db = internal.db;

// -----------------------------------------------------------------------------
// --SECTION--                                                  database methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test MVCC hash index non-sparse
////////////////////////////////////////////////////////////////////////////////

describe("MVCC hash index non-sparse", function () {
  'use strict';

  var cn = "UnitTestsMvccHashIndex";

  beforeEach(function () {
    db._useDatabase("_system");
    db._drop(cn);
    var c = db._create(cn);
    c.ensureIndex({type:"hash", fields: ["value"], sparse: false, unique: false});
  });

  afterEach(function () {
    db._drop(cn);
  });

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-sparse non-unique hash index and low level query function
////////////////////////////////////////////////////////////////////////////////
  
  it("should do a non-sparse non-unique hash index correctly", function () {
    var c = db[cn];
    var idx = c.getIndexes()[1];

    // Empty:
    var r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { });
    expect(r.documents).toEqual([]);

    // One document:
    c.mvccInsert({value : 1});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Two documents:
    c.mvccInsert({value : 1});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Three documents, different values:
    c.mvccInsert({value : 2});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A thousand documents:
    var i;
    for (i = 0; i < 1000; i++) {
      c.mvccInsert({value : 1});
    }
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1002);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A thousand more documents:
    for (i = 0; i < 1000; i++) {
      c.mvccInsert({value : i});
    }
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1003);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Some documents with value: null
    c.mvccInsert({value : null});
    c.mvccInsert({value : null});
    c.mvccInsert({value : null});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1003);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(3);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(3);

    // Some documents without a bound value
    c.mvccInsert({});
    c.mvccInsert({});
    c.mvccInsert({});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1003);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(6);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(6);
  });
      
});

////////////////////////////////////////////////////////////////////////////////
/// @brief test MVCC hash index sparse
////////////////////////////////////////////////////////////////////////////////

describe("MVCC hash index sparse", function () {
  'use strict';

  var cn = "UnitTestsMvccHashIndex";

  beforeEach(function () {
    db._useDatabase("_system");
    db._drop(cn);
    var c = db._create(cn);
    c.ensureIndex({type:"hash", fields: ["value"], sparse: true, unique: false});
  });

  afterEach(function () {
    db._drop(cn);
  });

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-sparse non-unique hash index and low level query function
////////////////////////////////////////////////////////////////////////////////
  
  it("should do a sparse non-unique hash index correctly", function () {
    var c = db[cn];
    var idx = c.getIndexes()[1];

    // Empty:
    var r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { });
    expect(r.documents).toEqual([]);

    // One document:
    c.mvccInsert({value : 1});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Two documents:
    c.mvccInsert({value : 1});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Three documents, different values:
    c.mvccInsert({value : 2});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A thousand documents:
    var i;
    for (i = 0; i < 1000; i++) {
      c.mvccInsert({value : 1});
    }
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1002);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A thousand more documents:
    for (i = 0; i < 1000; i++) {
      c.mvccInsert({value : i});
    }
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1003);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Some documents with value: null
    c.mvccInsert({value : null});
    c.mvccInsert({value : null});
    c.mvccInsert({value : null});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1003);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Some documents without a bound value
    c.mvccInsert({});
    c.mvccInsert({});
    c.mvccInsert({});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1003);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(2);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);
  });

});

////////////////////////////////////////////////////////////////////////////////
/// @brief test MVCC unique hash index non-sparse
////////////////////////////////////////////////////////////////////////////////

describe("MVCC unique hash index non-sparse", function () {
  'use strict';

  var cn = "UnitTestsMvccHashIndex";

  beforeEach(function () {
    db._useDatabase("_system");
    db._drop(cn);
    var c = db._create(cn);
    c.ensureIndex({type:"hash", fields: ["value"], sparse: false, unique: true});
  });

  afterEach(function () {
    db._drop(cn);
  });

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-sparse non-unique hash index and low level query function
////////////////////////////////////////////////////////////////////////////////
  
  it("should do a non-sparse unique hash index correctly", function () {
    var c = db[cn];
    var idx = c.getIndexes()[1];

    // Empty:
    var r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { });
    expect(r.documents).toEqual([]);

    // One document:
    c.mvccInsert({value : 1});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Two documents unique constraint violated:
    var error;
    try {
      c.mvccInsert({value : 1});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error.errorNum).toEqual(1210);
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Three documents, different values:
    c.mvccInsert({value : 2});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A thousand documents:
    var i;
    for (i = 3; i < 1001; i++) {
      c.mvccInsert({value : i});
    }
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 147 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A document with value: null
    c.mvccInsert({value : null});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(1);

    // Another document with value: null
    try {
      c.mvccInsert({value : null});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error.errorNum).toEqual(1210);

    // A document without a bound value
    try {
      c.mvccInsert({});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error.errorNum).toEqual(1210);

    // Remove the document again:
    r = c.mvccByExampleHash(idx, { value : null });
    c.mvccRemove(r.documents[0]._key);

    // A document with unbound value:
    c.mvccInsert({});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(1);

    // Another document with value: null
    try {
      c.mvccInsert({value : null});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error.errorNum).toEqual(1210);

    // A document without a bound value
    try {
      c.mvccInsert({});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error.errorNum).toEqual(1210);
  });
      
});

////////////////////////////////////////////////////////////////////////////////
/// @brief test MVCC unique hash index sparse
////////////////////////////////////////////////////////////////////////////////

describe("MVCC unique hash index sparse", function () {
  'use strict';

  var cn = "UnitTestsMvccHashIndex";

  beforeEach(function () {
    db._useDatabase("_system");
    db._drop(cn);
    var c = db._create(cn);
    c.ensureIndex({type:"hash", fields: ["value"], sparse: true, unique: true});
  });

  afterEach(function () {
    db._drop(cn);
  });

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse non-unique hash index and low level query function
////////////////////////////////////////////////////////////////////////////////
  
  it("should do a sparse unique hash index correctly", function () {
    var c = db[cn];
    var idx = c.getIndexes()[1];

    // Empty:
    var r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.documents).toEqual([]);
    r = c.mvccByExampleHash(idx, { });
    expect(r.documents).toEqual([]);

    // One document:
    c.mvccInsert({value : 1});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Two documents unique constraint violated:
    var error;
    try {
      c.mvccInsert({value : 1});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error.errorNum).toEqual(1210);
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Three documents, different values:
    c.mvccInsert({value : 2});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A thousand documents:
    var i;
    for (i = 3; i < 1001; i++) {
      c.mvccInsert({value : i});
    }
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 147 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A document with value: null
    c.mvccInsert({value : null});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Another document with value: null
    try {
      c.mvccInsert({value : null});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error).toEqual({});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A document without a bound value
    try {
      c.mvccInsert({});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error).toEqual({});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A document with unbound value:
    c.mvccInsert({});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // Another document with value: null
    try {
      c.mvccInsert({value : null});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error).toEqual({});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);

    // A document without a bound value
    try {
      c.mvccInsert({});
      error = {};
    }
    catch (e) {
      error = e;
    }
    expect(error).toEqual({});
    r = c.mvccByExampleHash(idx, { value : 1 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : 2 });
    expect(r.count).toEqual(1);
    r = c.mvccByExampleHash(idx, { value : null });
    expect(r.count).toEqual(0);
    r = c.mvccByExampleHash(idx, { });
    expect(r.count).toEqual(0);
  });
      
});

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
