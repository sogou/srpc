#include "Json.h"

namespace wfrest
{

// ------------------------ Constructor -------------------------
Json::Json()
    : node_(json_value_create(JSON_VALUE_NULL)), parent_(nullptr),
      allocated_(true)
{
}

Json::Json(const std::string& str)
    : node_(json_value_create(JSON_VALUE_STRING, str.c_str())),
      parent_(nullptr), allocated_(true)
{
}

Json::Json(const char* str)
    : node_(json_value_create(JSON_VALUE_STRING, str)), parent_(nullptr),
      allocated_(true)
{
}

Json::Json(std::nullptr_t null)
    : node_(json_value_create(JSON_VALUE_NULL)), parent_(nullptr),
      allocated_(true)
{
}

Json::Json(bool val)
    : node_(val ? json_value_create(JSON_VALUE_TRUE)
                : json_value_create(JSON_VALUE_FALSE)),
      parent_(nullptr), allocated_(true)
{
}

Json::Json(const std::vector<std::string>& val)
    : node_(json_value_create(JSON_VALUE_ARRAY)),
      parent_(nullptr), allocated_(true)
{
    json_array_t* arr = json_value_array(node_);
    for (const auto& str : val)
    {
        json_array_append(arr, JSON_VALUE_STRING, str.c_str());
    }
}

// for parse
Json::Json(const std::string& str, bool parse_flag) : parent_(nullptr)
{
    node_ = json_value_parse(str.c_str());
    allocated_ = node_ == nullptr ? false : true;
}

Json::~Json()
{
    destroy_node(&node_);
}

// watcher constructor
Json::Json(const json_value_t* node, const json_value_t* parent,
           std::string&& key)
    : node_(const_cast<json_value_t*>(node)), parent_(parent),
      allocated_(false), parent_key_(std::move(key))
{
}

Json::Json(const json_value_t* node, const json_value_t* parent,
           const std::string& key)
    : node_(const_cast<json_value_t*>(node)), parent_(parent),
      allocated_(false), parent_key_(key)
{
}

Json::Json(JsonType type) : allocated_(true)
{
    if (type == JsonType::Object)
    {
        node_ = json_value_create(JSON_VALUE_OBJECT);
    }
    else
    {
        node_ = json_value_create(JSON_VALUE_ARRAY);
    }
}
Json::Json(const Json& other)
{
    node_ = json_value_copy(other.node_);
    allocated_ = true;
}

Json& Json::operator=(const Json& other)
{
    if (this == &other)
    {
        return *this;
    }
    destroy_node(&node_);
    node_ = json_value_copy(other.node_);
    parent_ = nullptr;
    allocated_ = true;
    parent_key_.clear();

    return *this;
}

Json::Json(Json&& other)
    : node_(other.node_), parent_(other.parent_), allocated_(other.allocated_),
      parent_key_(std::move(other.parent_key_))
{
    other.node_ = nullptr;
    other.parent_ = nullptr;
    other.allocated_ = false;
}

Json& Json::operator=(Json&& other)
{
    if (this == &other)
    {
        return *this;
    }
    destroy_node(&node_);
    node_ = other.node_;
    other.node_ = nullptr;
    parent_ = other.parent_;
    other.parent_ = nullptr;
    allocated_ = other.allocated_;
    other.allocated_ = false;
    parent_key_ = std::move(other.parent_key_);
    return *this;
}

Json Json::parse(const std::string& str)
{
    return Json(str, true);
}

Json Json::parse(const std::ifstream& stream)
{
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return Json(buffer.str(), true);
}

Json Json::parse(FILE* fp)
{
    if (fp == nullptr)
    {
        return Json();
    }
    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* buffer = (char*)malloc(length + 1);
    buffer[length] = '\0';
    long ret = fread(buffer, 1, length, fp);
    Json js;
    if (ret != length)
    {
        js = Json();
    }
    else
    {
        js = Json(buffer, true);
    }
    free(buffer);
    return js;
}

std::string Json::dump() const
{
    return dump(0);
}

std::string Json::dump(int spaces) const
{
    std::string str;
    str.reserve(64);
    value_convert(node_, spaces, 0, &str);
    return str;
}

Json Json::operator[](const char* key)
{
    if (is_null() && is_root())
    {
        // todo : need is_root here?
        to_object();
    }
    else if (is_object())
    {
        // if exists
        json_object_t* obj = json_value_object(node_);
        const json_value_t* res = json_object_find(key, obj);
        if (res != nullptr)
        {
            return Json(res, node_, std::string(key));
        }
    }
    if (is_placeholder())
    {
        destroy_node(&node_);
        json_object_t* parent_obj = json_value_object(parent_);
        node_ = const_cast<json_value_t*>(json_object_append(
            parent_obj, parent_key_.c_str(), JSON_VALUE_OBJECT));
    }
    if (!is_object())
    {
        return Json();
    }
    // (null, parent(node_), key)
    Json js = Json();
    js.set_parent(node_, std::string(key));
    return js;
}

Json Json::operator[](const char* key) const
{
    if (!is_object())
    {
        return Json();
    }
    const json_value_t* val = node_;
    json_object_t* obj = json_value_object(val);
    const json_value_t* res = json_object_find(key, obj);
    if (res != nullptr)
    {
        return Json(res, node_, "");
    }
    return Json();
}

Json Json::operator[](const std::string& key)
{
    return this->operator[](key.c_str());
}

Json Json::operator[](const std::string& key) const
{
    return this->operator[](key.c_str());
}

bool Json::has(const std::string& key) const
{
    if (!is_object())
        return false;
    json_object_t* obj = json_value_object(node_);
    const json_value_t* find = json_object_find(key.c_str(), obj);
    return find != nullptr;
}

void Json::erase(const std::string& key)
{
    if (!is_object())
        return;
    json_object_t* obj = json_value_object(node_);
    const json_value_t* find = json_object_find(key.c_str(), obj);
    if (find == nullptr)
        return;
    json_value_t* remove_val = json_object_remove(find, obj);
    json_value_destroy(remove_val);
}

Json Json::operator[](int index)
{
    if (!is_array() || index < 0 || index > this->size())
    {
        return Json();
    }
    const json_value_t* val;
    json_array_t* arr = json_value_array(node_);
    json_array_for_each(val, arr)
    {
        if (index == 0)
        {
            return Json(val, node_, "");
        }
        index--;
    }
    return Json();
}

void Json::erase(int index)
{
    if (!is_array())
        return;
    int cnt = 0;
    json_array_t* arr = json_value_array(node_);
    const json_value_t* arr_cursor = nullptr;
    json_array_for_each(arr_cursor, arr)
    {
        if (cnt++ == index)
            break;
    }
    json_value_t* remove_val = json_array_remove(arr_cursor, arr);
    json_value_destroy(remove_val);
}

Json Json::operator[](int index) const
{
    if (!is_array() || index < 0 || index > this->size())
    {
        return Json();
    }
    const json_value_t* val;
    json_array_t* arr = json_value_array(node_);
    json_array_for_each(val, arr)
    {
        if (index == 0)
        {
            return Json(val, node_, "");
        }
        index--;
    }
    return Json();
}

bool Json::can_obj_push_back()
{
    if (is_placeholder() ||
        (parent_ != nullptr && json_value_type(parent_) == JSON_VALUE_OBJECT))
    {
        return true;
    }
    if (is_root() && is_null())
    {
        to_object();
    }
    return is_object();
}

void Json::push_back(const std::string& key, bool val)
{
    if (is_placeholder())
    {
        *this = Json::Object{{key, val}};
        return;
    }
    if (!can_obj_push_back())
    {
        return;
    }
    json_object_t* obj = json_value_object(node_);
    int type = val ? JSON_VALUE_TRUE : JSON_VALUE_FALSE;
    json_object_append(obj, key.c_str(), type);
}

void Json::push_back(const std::string& key, std::nullptr_t val)
{
    if (is_placeholder())
    {
        *this = Json::Object{{key, val}};
        return;
    }
    if (!can_obj_push_back())
    {
        return;
    }
    json_object_t* obj = json_value_object(node_);
    json_object_append(obj, key.c_str(), JSON_VALUE_NULL);
}

void Json::push_back(const std::string& key, const std::string& val)
{
    push_back(key, val.c_str());
}

void Json::push_back(const std::string& key, const char* val)
{
    if (is_placeholder())
    {
        *this = Json::Object{{key, val}};
        return;
    }
    if (!can_obj_push_back())
    {
        return;
    }
    json_object_t* obj = json_value_object(node_);
    json_object_append(obj, key.c_str(), JSON_VALUE_STRING, val);
}

void Json::push_back(const std::string& key, const std::vector<std::string>& val)
{
    if (is_placeholder())
    {
        *this = Json::Object{{key, val}};
        return;
    }
    if (!can_obj_push_back())
    {
        return;
    }
    json_object_t* obj = json_value_object(node_);
    const json_value_t* v = json_object_append(obj, key.c_str(),
                                               JSON_VALUE_ARRAY);
    json_array_t* arr = json_value_array(v);
    for (const auto& str : val)
    {
        json_array_append(arr, JSON_VALUE_STRING, str.c_str());
    }
}

void Json::push_back(const std::string& key, const Json& val)
{
    if (is_placeholder())
    {
        *this = Json::Object{{key, val}};
        return;
    }
    if (!can_obj_push_back())
    {
        return;
    }
    json_object_t* obj = json_value_object(node_);
    Json copy_json = val;
    json_object_append(obj, key.c_str(), 0, copy_json.node_);
    copy_json.reset();
}

void Json::placeholder_push_back(const std::string& key, bool val)
{
    json_object_t* obj = json_value_object(parent_);
    destroy_node(&node_);
    if (val)
    {
        node_ = const_cast<json_value_t*>(
            json_object_append(obj, key.c_str(), JSON_VALUE_TRUE));
    }
    else
    {
        node_ = const_cast<json_value_t*>(
            json_object_append(obj, key.c_str(), JSON_VALUE_FALSE));
    }
}

void Json::placeholder_push_back(const std::string& key, std::nullptr_t val)
{
    json_object_t* obj = json_value_object(parent_);
    destroy_node(&node_);
    node_ = const_cast<json_value_t*>(
        json_object_append(obj, key.c_str(), JSON_VALUE_NULL));
}

void Json::placeholder_push_back(const std::string& key, const std::string& val)
{
    placeholder_push_back(key, val.c_str());
}

void Json::placeholder_push_back(const std::string& key, const char* val)
{
    json_object_t* obj = json_value_object(parent_);
    destroy_node(&node_);
    node_ = const_cast<json_value_t*>(
        json_object_append(obj, key.c_str(), JSON_VALUE_STRING, val));
}

void Json::placeholder_push_back(const std::string& key,
                                 const std::vector<std::string>& val)
{
    json_object_t* obj = json_value_object(parent_);
    destroy_node(&node_);
    node_ = const_cast<json_value_t*>(
        json_object_append(obj, key.c_str(), JSON_VALUE_ARRAY));
    json_array_t* arr = json_value_array(node_);
    for (const auto& str : val)
        json_array_append(arr, JSON_VALUE_STRING, str.c_str());
}

void Json::placeholder_push_back(const std::string& key, const Json& val)
{
    json_object_t* obj = json_value_object(parent_);
    destroy_node(&node_);
    Json copy_json = val;
    node_ = const_cast<json_value_t*>(
        json_object_append(obj, key.c_str(), 0, copy_json.node_));
    copy_json.reset();
}

void Json::normal_push_back(const std::string& key, bool val)
{
    json_object_t* obj = json_value_object(parent_);
    const json_value_t* find = json_object_find(key.c_str(), obj);
    int type = val ? JSON_VALUE_TRUE : JSON_VALUE_FALSE;
    if (find == nullptr)
    {
        json_object_append(obj, key.c_str(), type);
        return;
    }
    json_object_insert_before(find, obj, key.c_str(), type);
    json_value_t* remove_val = json_object_remove(find, obj);
    json_value_destroy(remove_val);
}

void Json::normal_push_back(const std::string& key, std::nullptr_t val)
{
    json_object_t* obj = json_value_object(parent_);
    const json_value_t* find = json_object_find(key.c_str(), obj);
    if (find == nullptr)
    {
        json_object_append(obj, key.c_str(), JSON_VALUE_NULL);
        return;
    }
    json_object_insert_before(find, obj, key.c_str(), JSON_VALUE_NULL);
    json_value_t* remove_val = json_object_remove(find, obj);
    json_value_destroy(remove_val);
}

void Json::normal_push_back(const std::string& key, const std::string& val)
{
    normal_push_back(key, val.c_str());
}

void Json::normal_push_back(const std::string& key, const char* val)
{
    json_object_t* obj = json_value_object(parent_);
    const json_value_t* find = json_object_find(key.c_str(), obj);
    if (find == nullptr)
    {
        json_object_append(obj, key.c_str(), JSON_VALUE_STRING, val);
        return;
    }
    json_object_insert_before(find, obj, key.c_str(), JSON_VALUE_STRING, val);
    json_value_t* remove_val = json_object_remove(find, obj);
    json_value_destroy(remove_val);
}

void Json::normal_push_back(const std::string& key,
                            const std::vector<std::string>& val)
{
    json_object_t* obj = json_value_object(parent_);
    const json_value_t* find = json_object_find(key.c_str(), obj);
    const json_value_t* v;
    if (find == nullptr)
    {
        v = json_object_append(obj, key.c_str(), JSON_VALUE_ARRAY);
    }
    else
    {
        v = json_object_insert_before(find, obj, key.c_str(),
                                      JSON_VALUE_ARRAY);
        json_value_t* remove_val = json_object_remove(find, obj);
        json_value_destroy(remove_val);
    }
    json_array_t* arr = json_value_array(v);
    for (const auto& str : val)
        json_array_append(arr, JSON_VALUE_STRING, str.c_str());
}

void Json::normal_push_back(const std::string& key, const Json& val)
{
    json_object_t* obj = json_value_object(parent_);
    const json_value_t* find = json_object_find(key.c_str(), obj);
    Json copy_json = val;
    if (find == nullptr)
    {
        json_object_append(obj, key.c_str(), 0, copy_json.node_);
        copy_json.node_ = nullptr;
        return;
    }
    json_object_insert_before(find, obj, key.c_str(), 0, copy_json.node_);
    copy_json.reset();
    json_value_t* remove_val = json_object_remove(find, obj);
    json_value_destroy(remove_val);
}

bool Json::can_arr_push_back()
{
    if (is_root() && is_null())
    {
        to_array();
    }
    return is_array();
}

Json Json::copy() const
{
    return *this;
}

void Json::push_back(bool val)
{
    if (is_placeholder())
    {
        *this = Json::Array{{val}};
        return;
    }
    if (!can_arr_push_back())
    {
        return;
    }
    json_array_t* arr = json_value_array(node_);
    int type = val ? JSON_VALUE_TRUE : JSON_VALUE_FALSE;
    json_array_append(arr, type);
}

void Json::push_back(std::nullptr_t val)
{
    if (is_placeholder())
    {
        *this = Json::Array{{val}};
        return;
    }
    if (!can_arr_push_back())
    {
        return;
    }
    json_array_t* arr = json_value_array(node_);
    json_array_append(arr, JSON_VALUE_NULL);
}

void Json::push_back(const std::string& val)
{
    push_back(val.c_str());
}
void Json::push_back(const std::vector<std::string>& val)
{
    for (const auto& str : val)
    {
        push_back(str.c_str());
    }
}

void Json::push_back(const char* val)
{
    if (is_placeholder())
    {
        *this = Json::Array{{val}};
        return;
    }
    if (!can_arr_push_back())
    {
        return;
    }
    json_array_t* arr = json_value_array(node_);
    json_array_append(arr, JSON_VALUE_STRING, val);
}

void Json::push_back(const Json& val)
{
    if (is_placeholder())
    {
        *this = Json::Array{{val}};
        return;
    }
    if (!can_arr_push_back())
    {
        return;
    }
    json_array_t* arr = json_value_array(node_);
    Json copy_json = val;
    json_array_append(arr, 0, copy_json.node_);
    copy_json.reset();
}

void Json::update_arr(bool val)
{
    json_array_t* arr = json_value_array(parent_);
    int type = val ? JSON_VALUE_TRUE : JSON_VALUE_FALSE;
    json_array_insert_before(node_, arr, type);
    json_value_t* remove_val = json_array_remove(node_, arr);
    json_value_destroy(remove_val);
}

void Json::update_arr(std::nullptr_t val)
{
    json_array_t* arr = json_value_array(parent_);
    json_array_insert_before(node_, arr, JSON_VALUE_NULL);
    json_value_t* remove_val = json_array_remove(node_, arr);
    json_value_destroy(remove_val);
}

void Json::update_arr(const std::string& val)
{
    update_arr(val.c_str());
}

void Json::update_arr(const char* val)
{
    json_array_t* arr = json_value_array(parent_);
    json_array_insert_before(node_, arr, JSON_VALUE_STRING, val);
    json_value_t* remove_val = json_array_remove(node_, arr);
    json_value_destroy(remove_val);
}

void Json::update_arr(const std::vector<std::string>& val)
{
    json_array_t* arr = json_value_array(parent_);
    const json_value_t* v = json_array_insert_before(node_, arr, JSON_VALUE_ARRAY);
    json_value_t* remove_val = json_array_remove(node_, arr);
    json_value_destroy(remove_val);
    arr = json_value_array(v);
    for (const auto& str : val)
    {
        json_array_append(arr, JSON_VALUE_STRING, str.c_str());
    }
}

void Json::update_arr(const Json& val)
{
    json_array_t* arr = json_value_array(parent_);
    Json copy_json = val;
    json_array_insert_before(node_, arr, 0, copy_json.node_);
    copy_json.reset();
    json_value_t* remove_val = json_array_remove(node_, arr);
    json_value_destroy(remove_val);
}

std::string Json::type_str() const
{
    switch (type())
    {
        case JSON_VALUE_STRING:
            return "string";
        case JSON_VALUE_NUMBER:
            return "number";
        case JSON_VALUE_OBJECT:
            return "object";
        case JSON_VALUE_ARRAY:
            return "array";
        case JSON_VALUE_TRUE:
            return "true";
        case JSON_VALUE_FALSE:
            return "false";
        case JSON_VALUE_NULL:
            return "null";
    }
    return "unknown";
}

int Json::size() const
{
    if (type() == JSON_VALUE_ARRAY)
    {
        json_array_t* array = json_value_array(node_);
        return json_array_size(array);
    }
    else if (type() == JSON_VALUE_OBJECT)
    {
        json_object_t* obj = json_value_object(node_);
        return json_object_size(obj);
    }
    return 1;
}

bool Json::empty() const
{
    switch (type())
    {
        case JSON_VALUE_NULL:
        {
            // null values are empty
            return true;
        }
        case JSON_VALUE_ARRAY:
        case JSON_VALUE_OBJECT:
        {
            return size() == 0;
        }
        default:
            // all other types are nonempty
            return false;
    }
}

void Json::clear()
{
    int type = json_value_type(node_);
    destroy_node(&node_);
    parent_ = nullptr;
    allocated_ = true;
    parent_key_.clear();
    if (type == JSON_VALUE_STRING)
    {
        node_ = json_value_create(JSON_VALUE_STRING, "");
    }
    else if (type == JSON_VALUE_NUMBER)
    {

        node_ = json_value_create(JSON_VALUE_NUMBER, 0.0);
    }
    else
    {
        node_ = json_value_create(type);
    }
}

bool Json::to_object()
{
    if (!allocated_ || !is_null())
    {
        // watcher and non-null type can't change type
        return false;
    }
    destroy_node(&node_);
    node_ = json_value_create(JSON_VALUE_OBJECT);
    allocated_ = true;
    return true;
}

bool Json::to_array()
{
    if (!allocated_ || !is_null())
    {
        // watcher and non-null type can't change type
        return false;
    }
    destroy_node(&node_);
    node_ = json_value_create(JSON_VALUE_ARRAY);
    allocated_ = true;
    return true;
}

void Json::value_convert(const json_value_t* val, int spaces, int depth,
                         std::string* out_str)
{
    if (val == nullptr || out_str == nullptr)
        return;
    switch (json_value_type(val))
    {
        case JSON_VALUE_STRING:
            string_convert(json_value_string(val), out_str);
            break;
        case JSON_VALUE_NUMBER:
            number_convert(json_value_number(val), out_str);
            break;
        case JSON_VALUE_OBJECT:
            object_convert(json_value_object(val), spaces, depth, out_str);
            break;
        case JSON_VALUE_ARRAY:
            array_convert(json_value_array(val), spaces, depth, out_str);
            break;
        case JSON_VALUE_TRUE:
            out_str->append("true");
            break;
        case JSON_VALUE_FALSE:
            out_str->append("false");
            break;
        case JSON_VALUE_NULL:
            out_str->append("null");
            break;
    }
}

void Json::string_convert(const char* str, std::string* out_str)
{
    out_str->append("\"");
    while (*str)
    {
        switch (*str)
        {
            case '\r':
                out_str->append("\\r");
                break;
            case '\n':
                out_str->append("\\n");
                break;
            case '\f':
                out_str->append("\\f");
                break;
            case '\b':
                out_str->append("\\b");
                break;
            case '\"':
                out_str->append("\\\"");
                break;
            case '\t':
                out_str->append("\\t");
                break;
            case '\\':
                out_str->append("\\\\");
                break;
            default:
                if ((unsigned char)*str < 0x20)
                {
                    char buf[8];
                    snprintf(buf, 8, "\\u00%02x", *str);
                    out_str->append(buf);
                }
                else
                    out_str->push_back(*str);
                break;
        }
        str++;
    }
    out_str->append("\"");
}

void Json::number_convert(double number, std::string* out_str)
{
    std::ostringstream oss;
    long long integer = number;
    if (integer == number)
        oss << integer;
    else
        oss << number;

    out_str->append(oss.str());
}

void Json::array_convert_not_format(const json_array_t* arr,
                                    std::string* out_str)
{
    const json_value_t* val;
    int n = 0;

    out_str->append("[");
    json_array_for_each(val, arr)
    {
        if (n != 0)
        {
            out_str->append(",");
        }
        n++;
        value_convert(val, 0, 0, out_str);
    }
    out_str->append("]");
}

void Json::array_convert(const json_array_t* arr, int spaces, int depth,
                         std::string* out_str)
{
    if (spaces == 0)
    {
        return array_convert_not_format(arr, out_str);
    }
    const json_value_t* val;
    int n = 0;
    int i;
    std::string padding(spaces, ' ');
    out_str->append("[\n");
    json_array_for_each(val, arr)
    {
        if (n != 0)
        {
            out_str->append(",\n");
        }
        n++;
        for (i = 0; i < depth + 1; i++)
        {
            out_str->append(padding);
        }
        value_convert(val, spaces, depth + 1, out_str);
    }

    out_str->append("\n");
    for (i = 0; i < depth; i++)
    {
        out_str->append(padding);
    }
    out_str->append("]");
}

void Json::object_convert_not_format(const json_object_t* obj,
                                     std::string* out_str)
{
    const char* name;
    const json_value_t* val;
    int n = 0;

    out_str->append("{");
    json_object_for_each(name, val, obj)
    {
        if (n != 0)
        {
            out_str->append(",");
        }
        n++;
        string_convert(name, out_str);
        out_str->append(":");
        value_convert(val, 0, 0, out_str);
    }
    out_str->append("}");
}

void Json::object_convert(const json_object_t* obj, int spaces, int depth,
                          std::string* out_str)
{
    if (spaces == 0)
    {
        return object_convert_not_format(obj, out_str);
    }
    const char* name;
    const json_value_t* val;
    int n = 0;
    int i;
    std::string padding(spaces, ' ');
    out_str->append("{\n");
    json_object_for_each(name, val, obj)
    {
        if (n != 0)
        {
            out_str->append(",\n");
        }
        n++;
        for (i = 0; i < depth + 1; i++)
        {
            out_str->append(padding);
        }
        string_convert(name, out_str);
        out_str->append(": ");
        value_convert(val, spaces, depth + 1, out_str);
    }

    out_str->append("\n");
    for (i = 0; i < depth; i++)
    {
        out_str->append(padding);
    }
    out_str->append("}");
}

} // namespace wfrest
