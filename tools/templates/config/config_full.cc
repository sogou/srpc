#include <fstream>
#include "config.h"
#include "workflow/WFGlobal.h"
#include "workflow/UpstreamManager.h"
#include "workflow/UpstreamPolicies.h"
#include "srpc/rpc_metrics_filter.h"
#include "srpc/rpc_trace_filter.h"

using namespace srpc;

// default upstream_route_t

static unsigned int default_consistent_hash(const char *path, const char *query,
                                              const char *fragment)
{
    return 0;
}

static unsigned int default_select_route(const char *path, const char *query,
                                         const char *fragment)
{
    return 0;
}

static void set_endpoint_params(const wfrest::Json& data,
                                struct EndpointParams *params)
{
    *params = ENDPOINT_PARAMS_DEFAULT;

    for (const auto& it : data)
    {
        if (it.key() == "max_connections")
            params->max_connections = data["max_connections"];
        else if (it.key() == "connect_timeout")
            params->connect_timeout = data["connect_timeout"];
        else if (it.key() == "response_timeout")
            params->response_timeout = data["response_timeout"];
        else if (it.key() == "ssl_connect_timeout")
            params->ssl_connect_timeout = data["ssl_connect_timeout"];
        else if (it.key() == "use_tls_sni")
            params->use_tls_sni = data["use_tls_sni"];
        else
        {
            printf("[INFO][set_endpoint_params] Unknown key : %s\n",
                   it.key().c_str());
        }
    }
}

static void load_global(const wfrest::Json& data)
{
    struct WFGlobalSettings settings = GLOBAL_SETTINGS_DEFAULT;
    std::string resolv_conf_path;
    std::string hosts_path;

    for (const auto& it : data)
    {
        if (it.key() == "endpoint_params")
        {
            set_endpoint_params(data["endpoint_params"],
                                &settings.endpoint_params);
        }
        else if (it.key() == "dns_server_params")
        {
            set_endpoint_params(data["dns_server_params"],
                                &settings.dns_server_params);
        }
        else if (it.key() == "dns_ttl_default")
            settings.dns_ttl_default = data["dns_ttl_default"];
        else if (it.key() == "dns_ttl_min")
            settings.dns_ttl_min = data["dns_ttl_min"];
        else if (it.key() == "dns_threads")
            settings.dns_threads = data["dns_threads"];
        else if (it.key() == "poller_threads")
            settings.poller_threads = data["poller_threads"];
        else if (it.key() == "handler_threads")
            settings.handler_threads = data["handler_threads"];
        else if (it.key() == "compute_threads")
            settings.compute_threads = data["compute_threads"];
        else if (it.key() == "resolv_conf_path")
        {
            resolv_conf_path = data["resolv_conf_path"].get<std::string>();
            settings.resolv_conf_path = resolv_conf_path.c_str();
        }
        else if (it.key() == "hosts_path")
        {
            hosts_path = data["hosts_path"].get<std::string>();
            settings.hosts_path = hosts_path.c_str();
        }
        else
            printf("[INFO][load_global] Unknown key : %s\n", it.key().c_str());
    }

    WORKFLOW_library_init(&settings);
}

static bool load_upstream_server(const wfrest::Json& data,
                                 std::vector<std::string>& hosts,
                                 std::vector<AddressParams>& params)
{
    AddressParams param;

    hosts.clear();
    params.clear();

    for (const auto& server : data)
    {
        if (server.has("host") == false)
        {
            printf("[ERROR][load_upstream] Invalid upstream server\n");
            continue;
        }

        param = ADDRESS_PARAMS_DEFAULT;
        if (server.has("params"))
        {
            for (const auto& p : server["params"])
            {
                if (p.key() == "endpoint_params")
                    set_endpoint_params(p.value(), &param.endpoint_params);
                else if (p.key() == "weight")
                    param.weight = p.value().get<unsigned short>();
                else if (p.key() == "max_fails")
                    param.max_fails = p.value().get<unsigned int>();
                else if (p.key() == "dns_ttl_default")
                    param.dns_ttl_default = p.value().get<unsigned int>();
                else if (p.key() == "dns_ttl_min")
                    param.dns_ttl_min = p.value().get<unsigned int>();
                else if (p.key() == "server_type")
                    param.server_type = p.value().get<int>();
                else if (p.key() == "group_id")
                    param.group_id = p.value().get<int>();
                else
                    printf("[ERROR][load_upstream] Invalid params: %s\n",
                           p.key().c_str());
            }
        }

        hosts.push_back(server["host"]);
        params.push_back(param);
    }
    
    if (hosts.size() == 0)
        return false;
    else
        return true;
}

static void load_upstream(const wfrest::Json& data)
{
    std::string name;
    std::string type;
    bool try_another;
    std::vector<std::string> hosts;
    std::vector<AddressParams> params;

    for (const auto& it : data)
    {
        if (it.has("name") == false ||
            it.has("type") == false ||
            it.has("server") == false || 
            load_upstream_server(it["server"], hosts, params) == false)
        {
            printf("[ERROR][load_upstream] Invalid upstream\n");
            continue;
        }

        name = it["name"].get<std::string>();
        type = it["type"].get<std::string>();

        if (it.has("try_another"))
            try_another = it["try_another"];
        else
            try_another = false;

        if (type == "weighted_random")
        {
            UpstreamManager::upstream_create_weighted_random(name, try_another);
        }
        else if (type == "consistent_hash")
        {
            UpstreamManager::upstream_create_consistent_hash(name,
                                                    default_consistent_hash);
        }
        else if (type == "round_robin")
        {
            UpstreamManager::upstream_create_round_robin(name, try_another);
        }
        else if (type == "manual")
        {
            UpstreamManager::upstream_create_manual(name,
                                                    default_select_route,
                                                    try_another,
                                                    default_consistent_hash);
        }
        else if (type == "vnswrr")
        {
            UpstreamManager::upstream_create_vnswrr(name);
        }
        else
        {
            printf("[INFO][load_upstream] Unknown type : %s\n", type.c_str());
            continue;
        }

        for (size_t i = 0; i < hosts.size(); i++)
            UpstreamManager::upstream_add_server(name, hosts[i], &params[i]);
    }
}

void RPCConfig::load_server()
{
    if (this->data["server"].has("port"))
        this->s_port = this->data["server"]["port"];

    if (this->data["server"].has("root"))
        this->root_path = this->data["server"]["root"].get<std::string>();

    if (this->data["server"].has("cert_file"))
        this->s_cert_file = this->data["server"]["cert_file"].get<std::string>();

    if (this->data["server"].has("file_key"))
        this->s_file_key = this->data["server"]["file_key"].get<std::string>();

    if (this->data["server"].has("error_page"))
    {
        for (const auto& it : this->data["server"]["error_page"])
        {
            std::string page;

            if (it.has("error") == true && it.has("error") == true)
            {
                page = it["page"].get<std::string>();
                for (const auto& e : it["error"])
                    this->error_page.insert(std::make_pair(e.get<int>(), page));
            }
            else
            {
                printf("[ERROR][load_file_service] Invalid error_page\n");
                continue;
            }
        }
    }
}

void RPCConfig::load_client()
{
    if (this->data["client"].has("transport_type"))
    {
        std::string type = this->data["client"]["transport_type"].get<std::string>();
        if (type == "TT_SCTP")
            this->c_transport_type = TT_SCTP;
        else if (type == "TT_UDP")
            this->c_transport_type = TT_UDP;
    }

    if (this->data["client"].has("remote_host"))
        this->c_host = this->data["client"]["remote_host"].get<std::string>();

    if (this->data["client"].has("remote_port"))
        this->c_port = this->data["client"]["remote_port"];

    if (this->data["client"].has("is_ssl"))
        this->c_is_ssl = this->data["client"]["is_ssl"];

    if (this->data["client"].has("callee_timeout"))
        this->c_callee_timeout = this->data["client"]["callee_timeout"];

    if (this->data["client"].has("redirect_max"))
        this->c_redirect_max = this->data["client"]["redirect_max"];

    if (this->data["client"].has("retry_max"))
        this->c_retry_max = this->data["client"]["retry_max"];

    if (this->data["client"].has("user_name"))
        this->c_user_name = this->data["client"]["user_name"].get<std::string>();

    if (this->data["client"].has("password"))
        this->c_password = this->data["client"]["password"].get<std::string>();
}

bool RPCConfig::load(const char *file)
{
    FILE *fp = fopen(file, "r");
    if (!fp)
        return false;

    this->data = wfrest::Json::parse(fp);
    fclose(fp);
    
    if (this->data.is_valid() == false)
        return false;

    for (const auto& it : this->data)
    {
        if (it.key() == "server")
            this->load_server();
        else if (it.key() == "client")
            this->load_client();
        else if (it.key() == "global")
            load_global(it.value());
        else if (it.key() == "upstream")
            load_upstream(it.value());
        else if (it.key() == "metrics")
            this->load_metrics();
        else if (it.key() == "trace")
            this->load_trace();
        else
            printf("[INFO][RPCConfig::load] Unknown key: %s\n", it.key().c_str());
    }

    return true;
};

void RPCConfig::load_metrics()
{
    for (const auto& it : this->data["metrics"])
    {
        if (it.has("filter") == false)
            continue;

        std::string filter_name = it["filter"];

        if (filter_name.compare("prometheus") == 0)
        {
            if (it.has("port") == false)
                continue;

            RPCMetricsPull *filter = new RPCMetricsPull();
            unsigned short port = it["port"];
            filter->init(port);
            this->filters.push_back(filter);
        }
        else if (filter_name.compare("opentelemetry") == 0)
        {
            if (it.has("address") == false)
                continue;

            std::string name = it["filter_name"];
            std::string url = it["address"];

            unsigned int redirect_max = OTLP_HTTP_REDIRECT_MAX;
            unsigned int retry_max = OTLP_HTTP_RETRY_MAX;
            size_t report_threshold = RPC_REPORT_THREHOLD_DEFAULT;
            size_t report_interval = RPC_REPORT_INTERVAL_DEFAULT;

            if (it.has("redirect_max"))
                redirect_max = it["redirect_max"];
            if (it.has("retry_max"))
                retry_max = it["retry_max"];
            if (it.has("report_threshold"))
                report_threshold = it["report_threshold"];
            if (it.has("report_interval_ms"))
                report_interval = it["report_interval_ms"];

            RPCMetricsOTel *filter = new RPCMetricsOTel(name,
                                                        url,
                                                        redirect_max,
                                                        retry_max,
                                                        report_threshold,
                                                        report_interval);

            if (it.has("attributes"))
            {
                for (const auto& kv : it["attributes"])
                {
                    if (kv.has("key") == false || kv.has("value") == false)
                        continue;

                    filter->add_attributes(kv["key"], kv["value"]);
                }
            }

            this->filters.push_back(filter);
        }
        else
        {
            printf("[ERROR][RPCConfig::load_metrics] Unknown metrics: %s\n",
                   filter_name.c_str());
        }
    }
}

void RPCConfig::load_trace()
{
    for (const auto& it : this->data["trace"])
    {
        if (it.has("filter") == false)
            continue;

        std::string filter_name = it["filter"];
        size_t spans_per_second = SPANS_PER_SECOND_DEFAULT;

        if (filter_name.compare("default") == 0)
        {
            if (it.has("spans_per_second"))
                spans_per_second = it["spans_per_second"];

            auto *filter = new RPCTraceDefault(spans_per_second);
            this->filters.push_back(filter);
        }
        else if (filter_name.compare("opentelemetry") == 0)
        {
            if (it.has("address") == false)
                continue;

            std::string url = it["address"];
            unsigned int redirect_max = OTLP_HTTP_REDIRECT_MAX;
            unsigned int retry_max = OTLP_HTTP_RETRY_MAX;
            size_t report_threshold = RPC_REPORT_THREHOLD_DEFAULT;
            size_t report_interval = RPC_REPORT_INTERVAL_DEFAULT;

            if (it.has("redirect_max"))
                redirect_max = it["redirect_max"];
            if (it.has("retry_max"))
                retry_max = it["retry_max"];
            if (it.has("report_threshold"))
                report_threshold = it["report_threshold"];
            if (it.has("report_interval_ms"))
                report_interval = it["report_interval_ms"];

            auto *filter = new RPCTraceOpenTelemetry(url,
                                                     OTLP_TRACES_PATH,
                                                     redirect_max,
                                                     retry_max,
                                                     spans_per_second,
                                                     report_threshold,
                                                     report_interval);

            if (it.has("attributes"))
            {
                for (const auto& kv : it["attributes"])
                {
                    if (kv.has("key") == false || kv.has("value") == false)
                        continue;

                    filter->add_attributes(kv["key"], kv["value"]);
                }
            }

            this->filters.push_back(filter);
        }
        else
        {
            printf("[ERROR][RPCConfig::load_metrics] Unknown metrics: %s\n",
                   filter_name.c_str());
        }
    }
}

void RPCConfig::load_filter(SRPCServer& server)
{
    for (auto *filter : this->filters)
        server.add_filter(filter);
}

void RPCConfig::load_filter(SRPCClient& client)
{
    for (auto *filter : this->filters)
        client.add_filter(filter);
}

void RPCConfig::load_filter(SRPCHttpServer& server)
{
    for (auto *filter : this->filters)
        server.add_filter(filter);
}

void RPCConfig::load_filter(SRPCHttpClient& client)
{
    for (auto *filter : this->filters)
        client.add_filter(filter);
}

void RPCConfig::load_filter(BRPCServer& server)
{
    for (auto *filter : this->filters)
        server.add_filter(filter);
}

void RPCConfig::load_filter(BRPCClient& client)
{
    for (auto *filter : this->filters)
        client.add_filter(filter);
}

void RPCConfig::load_filter(ThriftServer& server)
{
    for (auto *filter : this->filters)
        server.add_filter(filter);
}

void RPCConfig::load_filter(ThriftClient& client)
{
    for (auto *filter : this->filters)
        client.add_filter(filter);
}

void RPCConfig::load_filter(ThriftHttpServer& server)
{
    for (auto *filter : this->filters)
        server.add_filter(filter);
}

void RPCConfig::load_filter(ThriftHttpClient& client)
{
    for (auto *filter : this->filters)
        client.add_filter(filter);
}

void RPCConfig::load_filter(TRPCServer& server)
{
    for (auto *filter : this->filters)
        server.add_filter(filter);
}

void RPCConfig::load_filter(TRPCClient& client)
{
    for (auto *filter : this->filters)
        client.add_filter(filter);
}

void RPCConfig::load_filter(TRPCHttpServer& server)
{
    for (auto *filter : this->filters)
        server.add_filter(filter);
}

void RPCConfig::load_filter(TRPCHttpClient& client)
{
    for (auto *filter : this->filters)
        client.add_filter(filter);
}

RPCConfig::~RPCConfig()
{
    for (size_t i = 0; i < this->filters.size(); i++)
        delete this->filters[i];
}

