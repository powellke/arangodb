////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_ALIASES_H
#define VELOCYPACK_ALIASES_H 1

#include "velocypack/vpack.h"

using VPackArrayIterator      = arangodb::velocypack::ArrayIterator;
using VPackBuilder            = arangodb::velocypack::Builder;
using VPackCharBuffer         = arangodb::velocypack::CharBuffer;
using VPackCharBufferSink     = arangodb::velocypack::CharBufferSink;
using VPackCollection         = arangodb::velocypack::Collection;
using VPackDumper             = arangodb::velocypack::Dumper;
using VPackException          = arangodb::velocypack::Exception;
using VPackHexDump            = arangodb::velocypack::HexDump;
using VPackObjectIterator     = arangodb::velocypack::ObjectIterator;
using VPackOptions            = arangodb::velocypack::Options;
using VPackParser             = arangodb::velocypack::Parser;
using VPackSink               = arangodb::velocypack::Sink;
using VPackSlice              = arangodb::velocypack::Slice;
using VPackStringSink         = arangodb::velocypack::StringSink;
using VPackValue              = arangodb::velocypack::Value;
using VPackValueLength        = arangodb::velocypack::ValueLength;
using VPackValueType          = arangodb::velocypack::ValueType;
using VPackVersion            = arangodb::velocypack::Version;

#endif
