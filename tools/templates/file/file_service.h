#ifndef _RPC_FILE_SERVICE_H_
#define _RPC_FILE_SERVICE_H_

#include <sys/stat.h>
#include <string>
#include <unordered_map>
#include "workflow/Workflow.h"
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
#include "workflow/WFHttpServer.h"
#include "workflow/WFFacilities.h"

// This is a simple exmaple for file service

class FileService
{
public:
    using ErrorPageMap = std::unordered_map<int, std::string>;

    void process(WFHttpTask *server_task);
    void pread_callback(WFFileIOTask *task);
protected:
    struct ModuleCtx
    {
        protocol::HttpResponse *resp;
        void *buf;
        bool is_error_set;
        ModuleCtx(protocol::HttpResponse * resp) :
            resp(resp),
            buf(NULL),
            is_error_set(false)
        {
        }
    };

private:
    WFModuleTask *create_module(WFHttpTask *task, const std::string& path);
    SubTask *create_file_task(const std::string& path, struct ModuleCtx *ctx);
    SubTask *create_error_task(int code, struct ModuleCtx *ctx);

public:
    FileService(std::string root, const ErrorPageMap& error_page) :
        root(std::move(root)),
        error_page(error_page)
    {
        if (this->root.empty())
            root = "./";
        else if (this->root.at(this->root.length() - 1) != '/')
            root += "/";
    }

private:
    std::string root;
    const ErrorPageMap& error_page;
};

#endif

