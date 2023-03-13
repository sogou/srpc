#include "file_service.h"

SubTask *FileService::create_error_task(int code,
                                        struct FileService::ModuleCtx *ctx)
{
    if (ctx->is_error_set == false)
    {
        ctx->resp->set_status_code(std::to_string(code));
        ctx->is_error_set = true;
        const auto it = this->error_page.find(code);
        if (it != this->error_page.end())
            return this->create_file_task(it->second, ctx);
    }

    return WFTaskFactory::create_empty_task();
}

void FileService::pread_callback(WFFileIOTask *task)
{
    FileIOArgs *args = task->get_args();
    long ret = task->get_retval();
    struct ModuleCtx *ctx = (struct ModuleCtx *)series_of(task)->get_context();
    protocol::HttpResponse *resp = ctx->resp;

    if (task->get_state() == WFT_STATE_SUCCESS)
    {
        resp->append_output_body_nocopy(args->buf, ret);
    }
    else
    {
        auto *error_task = this->create_error_task(503, ctx);
        series_of(task)->push_back(error_task);
    }
}

SubTask *FileService::create_file_task(const std::string& path,
                                       struct FileService::ModuleCtx *ctx)
{
    SubTask *task;
    struct stat st;
    std::string abs_path = this->root + path;
    if (abs_path.back() == '/')
        abs_path += "index.html";

    if (stat(abs_path.c_str(), &st) >= 0)
    {
        size_t size = st.st_size;
        void *buf = malloc(size);
        if (buf)
        {
            ctx->buf = buf;
            auto&& cb = std::bind(&FileService::pread_callback,
                                  this, std::placeholders::_1);
            task = WFTaskFactory::create_pread_task(abs_path, buf, size, 0, cb);
        }
        else
        {
            task = this->create_error_task(503, ctx);
        }
    }
    else
    {
        task = this->create_error_task(404, ctx);
    }

    return task;
}

WFModuleTask *FileService::create_module(WFHttpTask *server_task,
                                         const std::string& path)
{
    fprintf(stderr, "file service get request: %s\n", path.c_str());
    struct ModuleCtx *ctx = new ModuleCtx(server_task->get_resp());
    SubTask *next = this->create_file_task(path, ctx);
    WFModuleTask *module = WFTaskFactory::create_module_task(next,
                                        [server_task](const WFModuleTask *mod)
    {
        struct ModuleCtx *ctx;
        ctx = (struct ModuleCtx *)mod->sub_series()->get_context();
        void *buf = ctx->buf;
        server_task->set_callback([buf](WFHttpTask *t){ free(buf); });
        delete ctx;
    });

    module->sub_series()->set_context(ctx);
    return module;
}

void FileService::process(WFHttpTask *server_task)
{
    protocol::HttpRequest *req = server_task->get_req();
    protocol::HttpResponse *resp = server_task->get_resp();
    std::string path = req->get_request_uri();
    auto pos = path.find_first_of('?');
    if (pos != std::string::npos)
        path = path.substr(0, pos);    

    resp->add_header_pair("Server", "SRPC HTTP File Server");

    WFModuleTask *module = this->create_module(server_task, path);
    series_of(server_task)->push_back(module);
}
