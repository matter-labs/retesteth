#pragma once
#include <retesteth/Socket.h>
#include <retesteth/ethObjects/object.h>
#include <boost/asio.hpp>
#include <mutex>
#include <string>
namespace fs = boost::filesystem;

namespace
{
bool validateIP(std::string const& _ip)
{
    size_t pos = _ip.find_last_of(':');
    string address = _ip.substr(0, pos);
    int port = atoi(_ip.substr(pos + 1).c_str());
    boost::system::error_code ec;
    boost::asio::ip::address::from_string(address, ec);
    if (port <= 1024 || port > 49151 || ec)
        return false;
    return true;
}
}

namespace test
{

struct ClientConfigID
{
    /// ClientConfigID handles the unique id logic so not to store it inside int and accedentially change
    /// or mistake with some other value. ???possibly a class for unique object ids???
    ClientConfigID();
    bool operator == (ClientConfigID const& _right) const { return m_id == _right.id(); }
    bool operator != (ClientConfigID const& _right) const { return m_id != _right.id(); }
    unsigned id() const { return m_id; }
private:
    unsigned m_id;
};

class ClientConfig : public object
{
public:
    ClientConfig(DataObject const& _obj, ClientConfigID const& _id, fs::path _shell = fs::path())
      : object(_obj), m_shellPath(_shell), m_id(_id)
    {
        requireJsonFields(_obj, "ClientConfig ",
            {{"name", {DataType::String}}, {"socketType", {DataType::String}},
                {"socketAddress", {DataType::String, DataType::Array}},
                {"forks", {DataType::Array}}},
            true);

        for (auto const& name : m_data.atKey("forks").getSubObjects())
            m_networks.push_back(name.asString());

        std::string const& socketTypeStr = _obj.atKey("socketType").asString();
        if (socketTypeStr == "ipc")
        {
            ETH_FAIL_REQUIRE_MESSAGE(
                getAddress() == "local", "A client socket of type ipc must be deployed locally!");
            m_socketType = Socket::SocketType::IPC;
            ETH_FAIL_REQUIRE_MESSAGE(fs::exists(_shell),
                std::string("Client shell script not found: ") + _shell.c_str());
        }
        else if (socketTypeStr == "tcp")
        {
            ETH_FAIL_REQUIRE_MESSAGE(validateIP(getAddress()) == true,
                "A client tcp socket must be a correct ipv4 address! (" + getAddress() + ")");
            m_socketType = Socket::SocketType::TCP;
            if (getAddressObject().type() == DataType::Array)
            {
                for (auto const& addr : getAddressObject().getSubObjects())
                    ETH_FAIL_REQUIRE_MESSAGE(validateIP(addr.asString()) == true,
                        "A client tcp socket must be a correct ipv4 address! (" + addr.asString() +
                            ")");
            }
        }
        else if (socketTypeStr == "ipc-debug")
        {
            m_socketType = Socket::SocketType::IPCDebug;
            ETH_FAIL_REQUIRE_MESSAGE(fs::exists(getAddress()),
                std::string("Client IPC socket file not found: ") + getAddress());
        }
        else
            ETH_FAIL_MESSAGE(
                "Incorrect client socket type: " + socketTypeStr + " in client named '" +
                getName() +
                "' Allowed socket configs [type, \"address\"]: [ipc, \"local\"], [ipc-debug, "
                "\"path to .ipc socket\"], [tcp, \"address:port\"]");
    }

    fs::path const& getShellPath() const { return m_shellPath; }
    std::string const& getName() const { return m_data.atKey("name").asString(); }
    Socket::SocketType getSocketType() const { return m_socketType; }
    std::string const& getAddress() const
    {
        if (m_data.atKey("socketAddress").type() == DataType::String)
            return m_data.atKey("socketAddress").asString();
        else
            return m_data.atKey("socketAddress").at(0).asString();
    }
    DataObject const& getAddressObject() const { return m_data.atKey("socketAddress"); }
    ClientConfigID const& getId() const { return m_id; }
    std::vector<string> const& getNetworks() const { return m_networks; }

private:
    Socket::SocketType m_socketType;  ///< Connection type
    fs::path m_shellPath;             ///< Script to start new instance of a client (for ipc)
    ClientConfigID m_id;              ///< Internal id
    std::vector<string> m_networks;   ///< Allowed forks as network name
};
}
