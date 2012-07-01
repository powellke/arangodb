////////////////////////////////////////////////////////////////////////////////
/// @brief application http server feature
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_HTTP_SERVER_APPLICATION_HTTP_SERVER_H
#define TRIAGENS_HTTP_SERVER_APPLICATION_HTTP_SERVER_H 1

#include "ApplicationServer/ApplicationFeature.h"

#include "Rest/AddressPort.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class ApplicationDispatcher;
    class ApplicationScheduler;
    class HttpHandlerFactory;
    class HttpServer;

// -----------------------------------------------------------------------------
// --SECTION--                                       class ApplicationHttpServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief application http server feature
////////////////////////////////////////////////////////////////////////////////

    class ApplicationHttpServer : public ApplicationFeature {
      private:
        ApplicationHttpServer (ApplicationHttpServer const&);
        ApplicationHttpServer& operator= (ApplicationHttpServer const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ApplicationHttpServer (ApplicationScheduler*, ApplicationDispatcher*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ApplicationHttpServer ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief shows the port options
////////////////////////////////////////////////////////////////////////////////

        void showPortOptions (bool value);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a http address:port
////////////////////////////////////////////////////////////////////////////////

        AddressPort addPort (string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the http server
////////////////////////////////////////////////////////////////////////////////

        HttpServer* buildServer ();

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the http server
////////////////////////////////////////////////////////////////////////////////

        HttpServer* buildServer (vector<AddressPort> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the http server
///
/// Note that the server claims ownership of the server.
////////////////////////////////////////////////////////////////////////////////

        HttpServer* buildServer (HttpServer*, vector<AddressPort> const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void setupOptions (map<string, basics::ProgramOptionsDescription>&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool parsePhase2 (basics::ProgramOptions&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool open ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void close ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void stop ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief build an http server
////////////////////////////////////////////////////////////////////////////////

        HttpServer* buildHttpServer (HttpServer*, vector<AddressPort> const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief application scheduler
////////////////////////////////////////////////////////////////////////////////

        ApplicationScheduler* _applicationScheduler;

////////////////////////////////////////////////////////////////////////////////
/// @brief application dispatcher or null
////////////////////////////////////////////////////////////////////////////////

        ApplicationDispatcher* _applicationDispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief show port options
////////////////////////////////////////////////////////////////////////////////

        bool _showPort;

////////////////////////////////////////////////////////////////////////////////
/// @brief is keep-alive required to keep the connection open
////////////////////////////////////////////////////////////////////////////////

        bool _requireKeepAlive;

////////////////////////////////////////////////////////////////////////////////
/// @brief all constructed http servers
////////////////////////////////////////////////////////////////////////////////

        vector<HttpServer*> _httpServers;

////////////////////////////////////////////////////////////////////////////////
/// @brief all default ports
////////////////////////////////////////////////////////////////////////////////

        vector<string> _httpPorts;

////////////////////////////////////////////////////////////////////////////////
/// @brief all used addresses
////////////////////////////////////////////////////////////////////////////////

        vector<AddressPort> _httpAddressPorts;
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
