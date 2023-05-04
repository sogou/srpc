#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "workflow/WFTaskError.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/HttpUtil.h"
#include "workflow/StringUtil.h"
#include "rpc_module.h"
#include "http_task.h"

namespace srpc
{
using namespace protocol;

#define HTTP_KEEPALIVE_MAX		(300 * 1000)

HttpClientTask::HttpClientTask(int redirect_max,
							   int retry_max,
							   std::list<RPCModule *>&& modules,
							   http_callback_t&& callback) :
	WFComplexClientTask(retry_max, std::move(callback)),
	redirect_max_(redirect_max),
	redirect_count_(0),
	modules_(std::move(modules))
{
	protocol::HttpRequest *client_req = this->get_req();

	client_req->set_method(HttpMethodGet);
	client_req->set_http_version("HTTP/1.1");
}

std::string HttpClientTask::get_uri_host() const
{
	if (uri_.state == URI_STATE_SUCCESS)
		return uri_.host;

	return "";
}

std::string HttpClientTask::get_uri_port() const
{
	if (uri_.state == URI_STATE_SUCCESS)
		return uri_.port;

	return "";
}

std::string HttpClientTask::get_url() const
{
	if (url_.empty())
		return url_;

	return ""; //TODO: fill with uri and other info
}

std::string HttpClientTask::get_uri_scheme() const
{
	if (uri_.state == URI_STATE_SUCCESS)
		return uri_.scheme;

	return "";
}

CommMessageOut *HttpClientTask::message_out()
{
	HttpRequest *req = this->get_req();
	struct HttpMessageHeader header;
	bool is_alive;
	HttpServerTask::ModuleSeries *series;
	RPCModuleData *data = NULL;

	// prepare module_data from series to request
	series = dynamic_cast<HttpServerTask::ModuleSeries *>(series_of(this));

	if (series)
	{
		data = series->get_module_data();
		if (data != NULL)
			this->set_module_data(*data);
	}

	data = this->mutable_module_data();

	for (auto *module : modules_)
		module->client_task_begin(this, *data);

	http_set_header_module_data(*data, req);

	// from ComplexHttpTask::message_out()
	if (!req->is_chunked() && !req->has_content_length_header())
	{
		size_t body_size = req->get_output_body_size();
		const char *method = req->get_method();

		if (body_size != 0 || strcmp(method, "POST") == 0 ||
							  strcmp(method, "PUT") == 0)
		{
			char buf[32];
			header.name = "Content-Length";
			header.name_len = strlen("Content-Length");
			header.value = buf;
			header.value_len = sprintf(buf, "%zu", body_size);
			req->add_header(&header);
		}
	}

	if (req->has_connection_header())
		is_alive = req->is_keep_alive();
	else
	{
		header.name = "Connection";
		header.name_len = strlen("Connection");
		is_alive = (this->keep_alive_timeo != 0);
		if (is_alive)
		{
			header.value = "Keep-Alive";
			header.value_len = strlen("Keep-Alive");
		}
		else
		{
			header.value = "close";
			header.value_len = strlen("close");
		}

		req->add_header(&header);
	}

	if (!is_alive)
		this->keep_alive_timeo = 0;
	else if (req->has_keep_alive_header())
	{
		HttpHeaderCursor req_cursor(req);

		//req---Connection: Keep-Alive
		//req---Keep-Alive: timeout=0,max=100
		header.name = "Keep-Alive";
		header.name_len = strlen("Keep-Alive");
		if (req_cursor.find(&header))
		{
			std::string keep_alive((const char *)header.value, header.value_len);
			std::vector<std::string> params = StringUtil::split(keep_alive, ',');

			for (const auto& kv : params)
			{
				std::vector<std::string> arr = StringUtil::split(kv, '=');
				if (arr.size() < 2)
					arr.emplace_back("0");

				std::string key = StringUtil::strip(arr[0]);
				std::string val = StringUtil::strip(arr[1]);
				if (strcasecmp(key.c_str(), "timeout") == 0)
				{
					this->keep_alive_timeo = 1000 * atoi(val.c_str());
					break;
				}
			}
		}

		if ((unsigned int)this->keep_alive_timeo > HTTP_KEEPALIVE_MAX)
			this->keep_alive_timeo = HTTP_KEEPALIVE_MAX;
	}

	return this->WFComplexClientTask::message_out();
}

CommMessageIn *HttpClientTask::message_in()
{
	HttpResponse *resp = this->get_resp();

	if (strcmp(this->get_req()->get_method(), HttpMethodHead) == 0)
		resp->parse_zero_body();

	return this->WFComplexClientTask::message_in();
}

int HttpClientTask::keep_alive_timeout()
{
	return this->resp.is_keep_alive() ? this->keep_alive_timeo : 0;
}

void HttpClientTask::set_empty_request()
{
	HttpRequest *client_req = this->get_req();
	client_req->set_request_uri("/");
	client_req->set_header_pair("Host", "");
}

void HttpClientTask::init_failed()
{
	this->set_empty_request();
}

bool HttpClientTask::init_success()
{
	HttpRequest *client_req = this->get_req();
	std::string request_uri;
	std::string header_host;
	bool is_ssl;

	if (uri_.scheme && strcasecmp(uri_.scheme, "http") == 0)
		is_ssl = false;
	else if (uri_.scheme && strcasecmp(uri_.scheme, "https") == 0)
		is_ssl = true;
	else
	{
		this->state = WFT_STATE_TASK_ERROR;
		this->error = WFT_ERR_URI_SCHEME_INVALID;
		this->set_empty_request();
		return false;
	}

	//todo http+unix
	//https://stackoverflow.com/questions/26964595/whats-the-correct-way-to-use-a-unix-domain-socket-in-requests-framework
	//https://stackoverflow.com/questions/27037990/connecting-to-postgres-via-database-url-and-unix-socket-in-rails

	if (uri_.path && uri_.path[0])
		request_uri = uri_.path;
	else
		request_uri = "/";

	if (uri_.query && uri_.query[0])
	{
		request_uri += "?";
		request_uri += uri_.query;
	}

	if (uri_.host && uri_.host[0])
		header_host = uri_.host;

	if (uri_.port && uri_.port[0])
	{
		int port = atoi(uri_.port);

		if (is_ssl)
		{
			if (port != 443)
			{
				header_host += ":";
				header_host += uri_.port;
			}
		}
		else
		{
			if (port != 80)
			{
				header_host += ":";
				header_host += uri_.port;
			}
		}
	}

	this->WFComplexClientTask::set_transport_type(is_ssl ? TT_TCP_SSL : TT_TCP);
	client_req->set_request_uri(request_uri.c_str());
	client_req->set_header_pair("Host", header_host.c_str());
	return true;
}

bool HttpClientTask::redirect_url(HttpResponse *client_resp, ParsedURI& uri)
{
	if (redirect_count_ < redirect_max_)
	{
		redirect_count_++;
		std::string url;
		HttpHeaderCursor cursor(client_resp);

		if (!cursor.find("Location", url) || url.empty())
		{
			this->state = WFT_STATE_TASK_ERROR;
			this->error = WFT_ERR_HTTP_BAD_REDIRECT_HEADER;
			return false;
		}

		if (url[0] == '/')
		{
			if (url[1] != '/')
			{
				if (uri.port)
					url = ':' + (uri.port + url);

				url = "//" + (uri.host + url);
			}

			url = uri.scheme + (':' + url);
		}

		URIParser::parse(url, uri);
		return true;
	}

	return false;
}

bool HttpClientTask::need_redirect(ParsedURI& uri)
{
	HttpRequest *client_req = this->get_req();
	HttpResponse *client_resp = this->get_resp();
	const char *status_code_str = client_resp->get_status_code();
	const char *method = client_req->get_method();

	if (!status_code_str || !method)
		return false;

	int status_code = atoi(status_code_str);

	switch (status_code)
	{
	case 301:
	case 302:
	case 303:
		if (redirect_url(client_resp, uri))
		{
			if (strcasecmp(method, HttpMethodGet) != 0 &&
				strcasecmp(method, HttpMethodHead) != 0)
			{
				client_req->set_method(HttpMethodGet);
			}

			return true;
		}
		else
			break;

	case 307:
	case 308:
		if (redirect_url(client_resp, uri))
			return true;
		else
			break;

	default:
		break;
	}

	return false;
}

void HttpClientTask::check_response()
{
	HttpResponse *resp = this->get_resp();

	resp->end_parsing();
	if (this->state == WFT_STATE_SYS_ERROR && this->error == ECONNRESET)
	{
		/* Servers can end the message by closing the connection. */
		if (resp->is_header_complete() &&
			!resp->is_keep_alive() &&
			!resp->is_chunked() &&
			!resp->has_content_length_header())
		{
			this->state = WFT_STATE_SUCCESS;
			this->error = 0;
		}
	}
}

bool HttpClientTask::finish_once()
{
	if (this->state != WFT_STATE_SUCCESS)
		this->check_response();

	if (this->state == WFT_STATE_SUCCESS)
	{
		if (this->need_redirect(uri_))
			this->set_redirect(uri_);
		else if (this->state != WFT_STATE_SUCCESS)
			this->disable_retry();
	}

	return true;
}

/**********Server**********/

void HttpServerTask::handle(int state, int error)
{
	if (state == WFT_STATE_TOREPLY)
	{
		HttpRequest *req = this->get_req();

		// from WFHttpServerTask::handle()
		req_is_alive_ = req->is_keep_alive();
		if (req_is_alive_ && req->has_keep_alive_header())
		{
			HttpHeaderCursor req_cursor(req);
			struct HttpMessageHeader header;

			header.name = "Keep-Alive";
			header.name_len = strlen("Keep-Alive");
			req_has_keep_alive_header_ = req_cursor.find(&header);
			if (req_has_keep_alive_header_)
			{
				req_keep_alive_.assign((const char *)header.value,
										header.value_len);
			}
		}

		this->state = WFT_STATE_TOREPLY;
		this->target = this->get_target();

		// fill module data from request to series
		ModuleSeries *series = new ModuleSeries(this);

		http_get_header_module_data(req, this->module_data_);
		for (auto *module : this->modules_)
		{
			if (module)
				module->server_task_begin(this, this->module_data_);
		}

		series->set_module_data(this->mutable_module_data());
		series->start();
	}
	else if (this->state == WFT_STATE_TOREPLY)
	{
		this->state = state;
		this->error = error;
		if (error == ETIMEDOUT)
			this->timeout_reason = TOR_TRANSMIT_TIMEOUT;

		// prepare module_data from series to response
		for (auto *module : modules_)
			module->server_task_end(this, this->module_data_);

		http_set_header_module_data(this->module_data_, this->get_resp());

		this->subtask_done();
	}
	else
		delete this;
}

CommMessageOut *HttpServerTask::message_out()
{
	HttpResponse *resp = this->get_resp();
	struct HttpMessageHeader header;

	if (!resp->get_http_version())
		resp->set_http_version("HTTP/1.1");

	const char *status_code_str = resp->get_status_code();
	if (!status_code_str || !resp->get_reason_phrase())
	{
		int status_code;

		if (status_code_str)
			status_code = atoi(status_code_str);
		else
			status_code = HttpStatusOK;

		HttpUtil::set_response_status(resp, status_code);
	}

	if (!resp->is_chunked() && !resp->has_content_length_header())
	{
		char buf[32];
		header.name = "Content-Length";
		header.name_len = strlen("Content-Length");
		header.value = buf;
		header.value_len = sprintf(buf, "%zu", resp->get_output_body_size());
		resp->add_header(&header);
	}

	bool is_alive;

	if (resp->has_connection_header())
		is_alive = resp->is_keep_alive();
	else
		is_alive = req_is_alive_;

	if (!is_alive)
		this->keep_alive_timeo = 0;
	else
	{
		//req---Connection: Keep-Alive
		//req---Keep-Alive: timeout=5,max=100

		if (req_has_keep_alive_header_)
		{
			int flag = 0;
			std::vector<std::string> params = StringUtil::split(req_keep_alive_, ',');

			for (const auto& kv : params)
			{
				std::vector<std::string> arr = StringUtil::split(kv, '=');
				if (arr.size() < 2)
					arr.emplace_back("0");

				std::string key = StringUtil::strip(arr[0]);
				std::string val = StringUtil::strip(arr[1]);
				if (!(flag & 1) && strcasecmp(key.c_str(), "timeout") == 0)
				{
					flag |= 1;
					// keep_alive_timeo = 5000ms when Keep-Alive: timeout=5
					this->keep_alive_timeo = 1000 * atoi(val.c_str());
					if (flag == 3)
						break;
				}
				else if (!(flag & 2) && strcasecmp(key.c_str(), "max") == 0)
				{
					flag |= 2;
					if (this->get_seq() >= atoi(val.c_str()))
					{
						this->keep_alive_timeo = 0;
						break;
					}

					if (flag == 3)
						break;
				}
			}
		}

		if ((unsigned int)this->keep_alive_timeo > HTTP_KEEPALIVE_MAX)
			this->keep_alive_timeo = HTTP_KEEPALIVE_MAX;
		//if (this->keep_alive_timeo < 0 || this->keep_alive_timeo > HTTP_KEEPALIVE_MAX)

	}

	if (!resp->has_connection_header())
	{
		header.name = "Connection";
		header.name_len = 10;
		if (this->keep_alive_timeo == 0)
		{
			header.value = "close";
			header.value_len = 5;
		}
		else
		{
			header.value = "Keep-Alive";
			header.value_len = 10;
		}

		resp->add_header(&header);
	}

	return this->WFServerTask::message_out();
}

} // end namespace srpc

