#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace asio = boost::asio;           // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

const std::string SERVER_IP = "localhost";
const int SERVER_PORT = 6666;

int main()
{
    try
    {
        // Check command line arguments.
        std::string const host = SERVER_IP;
        auto const port = std::to_string(SERVER_PORT);
        auto const text = "Hello, WebSocket!";

        // The io_context is required for all I/O
        asio::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver{ioc};
        beast::websocket::stream<tcp::socket> ws{ioc};

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        auto ep = asio::connect(ws.next_layer(), results);

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        std::string host_ = host + ':' + std::to_string(ep.port());

        // Set a decorator to change the User-Agent of the handshake
        ws.set_option(beast::websocket::stream_base::decorator(
            [](beast::websocket::request_type& req)
            {
                req.set(beast::http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-coro");
            }));

        // Perform the websocket handshake
        ws.handshake(host_, "/");

        // Send the message
        ws.write(asio::buffer(std::string(text)));

        // This buffer will hold the incoming message
        beast::flat_buffer buffer;

        // Read a message into our buffer
        ws.read(buffer);

        // Close the WebSocket connection
        ws.close(beast::websocket::close_code::normal);

        // If we get here then the connection is closed gracefully

        // The make_printable() function helps print a ConstBufferSequence
        std::cout << beast::make_printable(buffer.data()) << std::endl;
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}