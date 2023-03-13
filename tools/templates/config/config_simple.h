#ifndef _RPC_CONFIG_H_
#define _RPC_CONFIG_H_

#include "Json.h"

namespace srpc
{

class RPCConfig
{
public:
    bool load(const char *file);

    unsigned short server_port() const { return this->s_port; }
    unsigned short client_port() const { return this->c_port; }
    const char *client_host() const { return this->c_host.c_str(); }
    int redirect_max() const { return this->c_redirect_max; }
    int retry_max() const { return this->c_retry_max; }
    const char *client_user_name() const { return this->c_user_name.c_str(); }
    const char *client_password() const { return this->c_password.c_str(); }

public:
    RPCConfig() : s_port(0), c_port(0), c_redirect_max(0), c_retry_max(0) { }
    ~RPCConfig();

private:
    void load_server();
    void load_client();

    wfrest::Json data;
    unsigned short s_port;
    std::string c_host;
    unsigned short c_port;
    int c_redirect_max;
    int c_retry_max;
    std::string c_user_name;
    std::string c_password;
};

}

#endif
