#include "core/memory.hxx"
#include "core/protocol.hxx"
#include "core/uuid.hxx"
#include "core/tcp/tcp_session.hxx"

#include "core/io.hxx"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <system_error>

namespace CxxServer::Core::Tcp {
    class Server : public std::enable_shared_from_this<Server> {
        friend class Session;
    
        public:
            //! Init server with IO & port #
            /*!
             * \param io - IO service
             * \param port - port #
             * \param proto - Internet protocol to use (defaults to IPv4)
             */
            Server(const std::shared_ptr<Service> &service, unsigned int port, InternetProtocol proto = InternetProtocol::IPv4);

            //! Init server with IO, address & port to use
            /*!
             * \param io - IO service
             * \param addr - Address to use
             * \param port - port to use
             */
            Server(const std::shared_ptr<Service> &service, const std::string &addr, unsigned int port);

            //! Init server with IO & endpoint
            /*!
             * \param io - IO service
             * \param endpoint - endpoint to use
             */
            Server(const std::shared_ptr<Service> &service, const asio::ip::tcp::endpoint &endpoint);

            virtual ~Server() = default;

            Server(const Server&) = delete;
            Server(Server &&) = delete;
            Server &operator=(const Server &) = delete;
            Server &operator=(Server &&) = delete;

            //! Get server ID
            const Uuid &id() const noexcept { return _id; }

            //! Get service
            std::shared_ptr<Service> &service() noexcept { return _service; }

            //! Get IO service
            std::shared_ptr<asio::io_service> &io() noexcept { return _io; }

            //! Get strand
            asio::io_service::strand &strand() noexcept { return _strand; }

            //! Get server endpoint
            asio::ip::tcp::endpoint &endpoint() noexcept { return _endpoint; }

            //! Get server acceptor
            asio::ip::tcp::acceptor &acceptor() noexcept { return _acceptor; }

            //! Get server address
            const std::string &addr() const noexcept { return _addr; }

            //! Get server port number
            int port() const noexcept { return _port; }

            //! Get # of connected sessions
            int numConnectedSessions() const noexcept { return _sessions.size(); }

            //! Get # of bytes pending
            int numBytesPending() const noexcept { return _bytes_pending; }

            //! Get # of bytes sent
            int numBytesSent() const noexcept { return _bytes_sent; }
            
            //! Get # of bytes received
            int numBytesReceived() const noexcept { return _bytes_received; }

            //! Act as getter & setter for keep alive property
            bool &keepAlive() noexcept { return _keep_alive; }

            //! Act as getter & setter for no delay property
            bool &noDelay() noexcept { return _no_delay; }

            //! Act as getter & setter for reuse address property
            bool &reuseAddress() noexcept { return _reuse_addr; }

            //! Act as getter & setter for reuse port property
            bool &reusePort() noexcept { return _reuse_port; }

            //! Has server started
            bool isStarted() const noexcept { return _started; }

            //! Start the server
            /*!
             * \return true iff server successfully started
             */
            virtual bool start();

            //! Stop the server
            /*!
             * \return true iff server successfully stopped
             */
            virtual bool stop();

            //! Restart the server
            /*!
             * \return true iff server successfully restarted
             */
            virtual bool restart();
            
            //! Multicast data
            /*!
             * Send data to all connected sessions
             * \param buffer - data to multicast
             * \param size - size of buffer
             * \return true iff multicast was sucecessful
             */
            virtual bool multicast(const void* buffer, size_t size);

            //! Multicast string
            /*!
             * Send data to all connected sessions
             * \param text - text to multicast
             * \return true iff multicast was sucecessful
             */
            virtual bool multicast(std::string_view text) { return multicast(text.data(), text.size()); }

            //! Disconnect all sessions
            /*!
             * \return true iff all sessions disconnect successfully
             */
            virtual bool disconnectAll();

            //! Find a session from the uuid
            /*!
             * \param id - Uuid of session to find
             * \return session with the given id or nullptr
             */
            std::shared_ptr<Session> findSession(const Uuid &id);

        protected:

            virtual std::shared_ptr<Session> newSession(const std::shared_ptr<Server> &server) { return std::make_shared<Session>(server); }

            // Callbacks for events

            //! On server start
            virtual void onStart() {};

            //! On server stop
            virtual void onStop() {};

            //! On session connect
            /*!
             * \param session - Session which was connected
             */
            virtual void onConnect(std::shared_ptr<Session> &session) {}

            //! On session disconnect
            /*!
             * \param session - Session which was disconnected
             */
            virtual void onDisconnect(std::shared_ptr<Session> &session) {}

            //! On error
            /*!
             * \param code - Error code
             * \param category - Error category
             * \param message - Error message
             */
            virtual void onErr(int code, const std::string &category, const std::string &message) {}

            std::shared_mutex _sessions_lock;
            std::map<Uuid, std::shared_ptr<Session>> _sessions;

        private:
            //! Server Id
            Uuid _id;

            std::shared_ptr<Service> _service;
            std::shared_ptr<asio::io_service> _io;

            asio::io_service::strand _strand;
            bool _strand_needed;

            std::string _addr;
            unsigned int _port;

            std::shared_ptr<Session> _session;
            asio::ip::tcp::endpoint _endpoint;
            asio::ip::tcp::acceptor _acceptor;
            std::atomic<bool> _started;
            HandlerMemory<> _acceptor_storage;

            // Server stats
            uint64_t _bytes_pending;
            uint64_t _bytes_received;
            uint64_t _bytes_sent;

            // Server configuration options
            bool _keep_alive;
            bool _no_delay;
            bool _reuse_addr;
            bool _reuse_port;

            //! Handle acceptance of new connections
            void accept();

            //! Register a new session
            void registerSession();

            //! Unregister a session
            /*!
             * \param id - Id of session to be unregistered
             */
            void unregisterSession(const Uuid &id);

            //! Clear multicast buffers
            void clearMulticastBuffs();

            //! Propogate error notification
            void err(std::error_code);
    };
}