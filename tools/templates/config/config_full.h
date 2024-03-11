#ifndef _RPC_CONFIG_H_
#define _RPC_CONFIG_H_

#include <string>
#include <vector>
#include <unordered_map>
#include "Json.h"
#include "srpc/rpc_types.h"
#include "srpc/rpc_define.h"
#include "srpc/rpc_filter.h"

namespace srpc
{

class RPCConfig
{
public:
    using ErrorPageMap = std::unordered_map<int, std::string>;

    bool load(const char *file);

    void load_filter(SRPCServer& server);
    void load_filter(SRPCClient& client);

    void load_filter(SRPCHttpServer& server);
    void load_filter(SRPCHttpClient& client);

    void load_filter(BRPCServer& server);
    void load_filter(BRPCClient& client);

    void load_filter(ThriftServer& server);
    void load_filter(ThriftClient& client);

    void load_filter(ThriftHttpServer& server);
    void load_filter(ThriftHttpClient& client);

    void load_filter(TRPCServer& server);
    void load_filter(TRPCClient& client);

    void load_filter(TRPCHttpServer& server);
    void load_filter(TRPCHttpClient& client);

    unsigned short server_port() const { return this->s_port; }
    const char *server_cert_file() const { return this->s_cert_file.c_str(); }
    const char *server_file_key() const { return this->s_file_key.c_str(); }
    enum TransportType client_transport_type() const { return this->c_transport_type; }
    const char *client_host() const { return this->c_host.c_str(); }
    unsigned short client_port() const { return this->c_port; }
    bool client_is_ssl() const { return this->c_is_ssl; }
    const char *client_url() const { return this->c_url.c_str(); }
    int client_callee_timeout() const { return this->c_callee_timeout; }
    const char *client_caller() const { return this->c_caller.c_str(); }
    int redirect_max() const { return this->c_redirect_max; }
    int retry_max() const { return this->c_retry_max; }
    const char *client_user_name() const { return this->c_user_name.c_str(); }
    const char *client_password() const { return this->c_password.c_str(); }
    const char *get_root_path() const { return this->root_path.c_str(); }
    const ErrorPageMap& get_error_page() const { return this->error_page; }

public:
    RPCConfig() :
        s_port(0), c_port(0), c_is_ssl(false), c_redirect_max(0), c_retry_max(0)
    { }
    ~RPCConfig();

private:
    void load_server();
    void load_client();
    void load_metrics();
    void load_trace();

    wfrest::Json data;
    std::vector<RPCFilter *> filters;
    unsigned short s_port;
    std::string s_cert_file;
    std::string s_file_key;
    enum TransportType c_transport_type;
    std::string c_host;
    unsigned short c_port;
    bool c_is_ssl;
    std::string c_url;
    int c_callee_timeout;
    std::string c_caller;
    int c_redirect_max;
    int c_retry_max;
    std::string c_user_name;
    std::string c_password;
    std::string root_path;
    ErrorPageMap error_page;
};

}

#endif
