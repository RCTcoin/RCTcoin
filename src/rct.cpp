// Copyright (c) 2014-2014 YSWS
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "util.h"

using boost::asio::ip::tcp;

class client
{
public:
  client(boost::asio::io_service& io_service,
      const std::string& server, const std::string& path)
    : resolver_(io_service),
      socket_(io_service)
  {
    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    std::ostream request_stream(&request_);
    request_stream << "GET " << path << " HTTP/1.1\r\n";
    request_stream << "Host: " << server << "\r\n";
    request_stream << "User-Agent: Mozilla/5.0 (Windows NT 6.1; rv:32.0) Gecko/20100101 Firefox/32.0\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Accept-Encoding: identity\r\n";
    request_stream << "Connection: close\r\n";
    request_stream << "Cache-Control: max-age=0\r\n\r\n";

    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    tcp::resolver::query query(server, "http");
    resolver_.async_resolve(query,
        boost::bind(&client::handle_resolve, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::iterator));
  }

private:
  void handle_resolve(const boost::system::error_code& err,
      tcp::resolver::iterator endpoint_iterator)
  {
    if (!err)
    {
      // Attempt a connection to each endpoint in the list until we
      // successfully establish a connection.
      boost::asio::async_connect(socket_, endpoint_iterator,
          boost::bind(&client::handle_connect, this,
            boost::asio::placeholders::error));
    }
    else
    {
      printf("HTTP Error1: %s\n", err.message().c_str());
    }
  }

  void handle_connect(const boost::system::error_code& err)
  {
    if (!err)
    {
      // The connection was successful. Send the request.
      boost::asio::async_write(socket_, request_,
          boost::bind(&client::handle_write_request, this,
            boost::asio::placeholders::error));
    }
    else
    {
      printf("HTTP Error2: %s\n", err.message().c_str());
    }
  }

  void handle_write_request(const boost::system::error_code& err)
  {
    if (!err)
    {
      // Read the response status line. The response_ streambuf will
      // automatically grow to accommodate the entire line. The growth may be
      // limited by passing a maximum size to the streambuf constructor.
      boost::asio::async_read_until(socket_, response_, "\r\n",
          boost::bind(&client::handle_read_status_line, this,
            boost::asio::placeholders::error));
    }
    else
    {
      printf("HTTP Error3: %s\n", err.message().c_str());
    }
  }

  void handle_read_status_line(const boost::system::error_code& err)
  {
    if (!err)
    {
      // Check that response is OK.
      std::istream response_stream(&response_);
      std::string http_version;
      response_stream >> http_version;
      unsigned int status_code;
      response_stream >> status_code;
      std::string status_message;
      std::getline(response_stream, status_message);
      if (!response_stream || http_version.substr(0, 5) != "HTTP/")
      {
        printf("HTTP Error: Invalid response\n");
        return;
      }
      if (status_code != 200)
      {
        printf("HTTP Response returned with status code: %d\n", status_code);
        return;
      }

      // Read the response headers, which are terminated by a blank line.
      boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
          boost::bind(&client::handle_read_headers, this,
            boost::asio::placeholders::error));
    }
    else
    {
      printf("HTTP Error5: %s\n", err.message().c_str());
    }
  }

  void handle_read_headers(const boost::system::error_code& err)
  {
    if (!err)
    {
      // Process the response headers.
      std::istream response_stream(&response_);
      std::string header;
      while (std::getline(response_stream, header) && header != "\r")
        printf("HTTP header: %s\n", header.c_str());

      // Write whatever content we already have to output.
      if (response_.size() > 0) printf("HTTP header: response_ have content\n");
      ///  std::cout << &response_;

      // Start reading remaining data until EOF.
      boost::asio::async_read(socket_, response_,
          boost::asio::transfer_at_least(1),
          boost::bind(&client::handle_read_content, this,
            boost::asio::placeholders::error));
    }
    else
    {
      printf("HTTP Error6: %s\n", err.message().c_str());
    }
  }

  void handle_read_content(const boost::system::error_code& err)
  {
    if (!err)
    {
      // Write all of the data that has been read so far.
      //std::cout << &response_;
      std::istream response_stream(&response_);
      std::string sdata;
      while (std::getline(response_stream, sdata))
          printf("HTTP data: %s\n", sdata.c_str());

      // Continue reading remaining data until EOF.
      boost::asio::async_read(socket_, response_,
          boost::asio::transfer_at_least(1),
          boost::bind(&client::handle_read_content, this,
            boost::asio::placeholders::error));
    }
    else if (err != boost::asio::error::eof)
    {
      printf("HTTP Error7: %s\n", err.message().c_str());
    }
  }

  tcp::resolver resolver_;
  tcp::socket socket_;
  boost::asio::streambuf request_;
  boost::asio::streambuf response_;
};

const std::string HTTPClient(const std::string& pszUrl, const std::string& pszFile)
{
  try
  {
    printf("HTTP start for: %s\n", pszUrl.c_str());
    boost::asio::io_service io_service;
    //const std::string& szUrl = "lgpk.sinaapp.com";
    //const std::string& szFile="/t.php";
    //pszUrl="lgpk.sinaapp.com"; pszFile="/t.php";
    client c(io_service, pszUrl, pszFile);
    io_service.run();
  }
  catch (std::exception& e)
  {
    printf("HTTP failed: %s\n", e.what());
  }

  return pszUrl;
}
