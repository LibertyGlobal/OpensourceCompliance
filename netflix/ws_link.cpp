/*
 * Copyright (c) 2017, LIBERTY GLOBAL all rights reserved.
 *
 * Implementation based on a snapshot of the WebSocket++ utility client tutorial.
 * Original Copyright below.
 */


/*
 * Copyright (c) 2014, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// **NOTE:** This file is a snapshot of the WebSocket++ utility client tutorial.
// Additional related material can be found in the tutorials/utility_client
// directory of the WebSocket++ repository.

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <semaphore.h>
#include "ws_link.h"

#include <nrd/AppLog.h>

#define DEBUG(message, ...) netflix::Log::trace(netflix::TRACE_LOG, "[WS_LINK] %s(): " message, __FUNCTION__, ##__VA_ARGS__)
#define ERROR(message, ...) netflix::Log::error(netflix::TRACE_LOG, "[WS_LINK] %s(): " message, __FUNCTION__, ##__VA_ARGS__)

namespace {
typedef websocketpp::client<websocketpp::config::asio_client> client;

class connection_metadata {
public:
    typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;

    connection_metadata(int id, websocketpp::connection_hdl hdl, std::string uri, ws_response_handler_fn response_handler, void* response_data_ptr )
      : m_id(id)
      , m_hdl(hdl)
      , m_status("Connecting")
      , m_response_handler(response_handler)
      , m_response_data_ptr(response_data_ptr)
    {
      sem_init(&m_operation_sem, 0, 0);
    }

    void on_open(client * c, websocketpp::connection_hdl hdl) {
        DEBUG("\n");

        m_status = "Open";

        client::connection_ptr con = c->get_con_from_hdl(hdl);
        sem_post(&m_operation_sem);
    }

    void on_fail(client * c, websocketpp::connection_hdl hdl) {
        DEBUG("\n");

        m_status = "Failed";

        sem_post(&m_operation_sem);
    }

    void on_close(client * c, websocketpp::connection_hdl hdl) {
        DEBUG("\n");

        m_status = "Closed";

    }

    void on_message(websocketpp::connection_hdl, client::message_ptr msg) {
        DEBUG("\n");

        m_response_handler(msg->get_payload().c_str(), msg->get_payload().length(), m_response_data_ptr);
        sem_post(&m_operation_sem);
    }

    websocketpp::connection_hdl get_hdl() const {
        return m_hdl;
    }

    void operation_wait(bool exit_on_fail = false) {
      sem_wait(&m_operation_sem);
      if ((exit_on_fail) && (m_status == "Failed")) {
        exit(1);
      }
    }

    int get_id() const {
        return m_id;
    }

    std::string get_status() const {
        return m_status;
    }

private:
    int                         m_id;
    websocketpp::connection_hdl m_hdl;
    std::string                 m_status;
    ws_response_handler_fn      m_response_handler;
    void*                       m_response_data_ptr;
    sem_t                       m_operation_sem;
};


class websocket_endpoint {
public:
    websocket_endpoint () : m_next_id(0) {
        m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

        m_endpoint.init_asio();
        m_endpoint.start_perpetual();

        m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&client::run, &m_endpoint);
    }

    ~websocket_endpoint() {
        m_endpoint.stop_perpetual();

        for (con_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it) {
            if (it->second->get_status() != "Open") {
                // Only close open connections
                continue;
            }

            DEBUG("Closing connection %d\n", it->second->get_id());

            websocketpp::lib::error_code ec;
            m_endpoint.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
            if (ec) {
                DEBUG("> Error closing connection %d:%s\n",it->second->get_id(), ec.message().c_str());
            }
        }

        m_thread->join();
    }

    void cleanup_closed() {

      for (con_list::iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it) {
          if (it->second->get_status() == "Closed") {
            m_connection_list.erase(it);
          }
      }
    }


    int connect(std::string const & uri, ws_response_handler_fn response_handler, void* response_data_ptr) {
        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_endpoint.get_connection(uri, ec);

        if (ec) {
            DEBUG("> Connect initialization error: %s\n", ec.message().c_str());
            return -1;
        }

        int new_id = m_next_id++;
        connection_metadata::ptr metadata_ptr = websocketpp::lib::make_shared<connection_metadata>(new_id, con->get_handle(), uri, response_handler, response_data_ptr);
        m_connection_list[new_id] = metadata_ptr;

        con->set_open_handler(websocketpp::lib::bind(
            &connection_metadata::on_open,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));
        con->set_fail_handler(websocketpp::lib::bind(
            &connection_metadata::on_fail,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));
        con->set_close_handler(websocketpp::lib::bind(
            &connection_metadata::on_close,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));
        con->set_message_handler(websocketpp::lib::bind(
            &connection_metadata::on_message,
            metadata_ptr,
            websocketpp::lib::placeholders::_1,
            websocketpp::lib::placeholders::_2
        ));

        m_endpoint.connect(con);
        metadata_ptr->operation_wait(false);
        DEBUG("New connection: %d, ; total connections count: %d\n", new_id, m_connection_list.size() );
        return (0 == metadata_ptr->get_status().compare("Open")) ? new_id : -1;
    }


    void close(int id, websocketpp::close::status::value code, std::string reason) {
        websocketpp::lib::error_code ec;

        con_list::iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            DEBUG("> No connection found with id %d\n", id);
            return;
        }

        m_endpoint.close(metadata_it->second->get_hdl(), code, reason, ec);
        if (ec) {
          DEBUG("> Error initiating close: %s\n", ec.message().c_str());
        }
    }

    void send(int id, const void* buffer, size_t size) {
        websocketpp::lib::error_code ec;

        con_list::iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            DEBUG("> No connection found with id %d\n", id);
            return;
        }

        m_endpoint.send(metadata_it->second->get_hdl(), buffer, size, websocketpp::frame::opcode::text, ec);
        if (ec) {
            DEBUG("> Error sending message: %s\n", ec.message().c_str());
            return;
        }
        metadata_it->second->operation_wait(false);
    }

    void send(int id, std::string message) {
        websocketpp::lib::error_code ec;

        con_list::iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            DEBUG("> No connection found with id %d\n", id);
            return;
        }

        m_endpoint.send(metadata_it->second->get_hdl(), message, websocketpp::frame::opcode::text, ec);
        if (ec) {
            DEBUG("> Error sending message: %s\n", ec.message().c_str());
            return;
        }
        metadata_it->second->operation_wait(false);
    }

    connection_metadata::ptr get_metadata(int id) const {
        con_list::const_iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            return connection_metadata::ptr();
        } else {
            return metadata_it->second;
        }
    }
private:
    typedef std::map<int,connection_metadata::ptr> con_list;

    client m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

    con_list m_connection_list;
    int m_next_id;
};

} //namespace


static websocket_endpoint* endpoint_ptr = NULL;

int WSConnect(const char* url, ws_response_handler_fn response_handler, void* response_data_ptr) {
  DEBUG("WSConnect entry \"%s\"\n", url);
  endpoint_ptr = endpoint_ptr ? endpoint_ptr : new websocket_endpoint();
  int id = endpoint_ptr->connect( url, response_handler, response_data_ptr);

  endpoint_ptr->cleanup_closed();
  DEBUG("Connecting \"%s\"\n", url);
  if (id != -1) {
    DEBUG("> Created connection with id %d\n", id);
  }
  return id;
}

void WSSendBuffer(int ws_connection, const void* data_buffer, size_t data_size) {
  endpoint_ptr = endpoint_ptr ? endpoint_ptr : new websocket_endpoint();
  endpoint_ptr->send(ws_connection, data_buffer, data_size);
}

void WSClose(int ws_connection) {
  endpoint_ptr = endpoint_ptr ? endpoint_ptr : new websocket_endpoint();
  endpoint_ptr->close(ws_connection, websocketpp::close::status::normal, "close");
}

