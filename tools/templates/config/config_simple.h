#ifndef _RPC_CONFIG_H_
#define _RPC_CONFIG_H_

#include <string>
#include <vector>
#include <unordered_map>
#include "Json.h"

namespace srpc
{

class RPCConfig
{
public:
    using ErrorPageMap = std::unordered_map<int, std::string>;

    bool load(const char *file);

    unsigned short server_port() const { return this->s_port; }
    const char *server_cert_file() const { return this->s_cert_file.c_str(); }
    const char *server_file_key() const { return this->s_file_key.c_str(); }
    unsigned short client_port() const { return this->c_port; }
    const char *client_host() const { return this->c_host.c_str(); }
    int redirect_max() const { return this->c_redirect_max; }
    int retry_max() const { return this->c_retry_max; }
    const char *client_user_name() const { return this->c_user_name.c_str(); }
    const char *client_password() const { return this->c_password.c_str(); }
    const char *get_root_path() const { return this->root_path.c_str(); }
    const ErrorPageMap& get_error_page() const { return this->error_page; }

public:
    RPCConfig() : s_port(0), c_port(0), c_redirect_max(0), c_retry_max(0) { }
    ~RPCConfig();

private:
    void load_server();
    void load_client();

    wfrest::Json data;
    unsigned short s_port;
    std::string s_cert_file;
    std::string s_file_key;
    std::string c_host;
    unsigned short c_port;
    int c_redirect_max;
    int c_retry_max;
    std::string c_user_name;
    std::string c_password;
    std::string root_path;
    ErrorPageMap error_page;
};

}

#endif
