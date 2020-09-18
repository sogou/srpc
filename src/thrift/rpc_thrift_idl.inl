/*
  Copyright (c) 2020 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

namespace srpc
{

class ThriftJsonUtil
{
public:
	static bool skip_whitespace(ThriftBuffer *buffer);
	static bool skip_character(ThriftBuffer *buffer, char ch);
	static bool peek_first_meaningful_char(ThriftBuffer *buffer, char& ch);
	static bool skip_simple_string(ThriftBuffer *buffer, const std::string& str);
	static bool read_int64(ThriftBuffer *buffer, int64_t& intv);
	static bool read_uint64(ThriftBuffer *buffer, uint64_t& intv);
	static bool read_double(ThriftBuffer *buffer, double& d);
	static bool read_string(ThriftBuffer *buffer, std::string *str);
	static bool skip_one_element(ThriftBuffer *buffer);
	static bool escape_string(const std::string& str, std::string& escape_str);
};

template<class T, int8_t DATA, class KEYIMPL, class VALIMPL>
class ThriftDescriptorImpl : public ThriftDescriptor
{
public:
	static const ThriftDescriptor *get_instance()
	{
		static const ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL> kInstance;

		return &kInstance;
	}

public:
	static bool read(ThriftBuffer *buffer, void *data);
	static bool write(const void *data, ThriftBuffer *buffer);
	static bool read_json(ThriftBuffer *buffer, void *data);
	static bool write_json(const void *data, ThriftBuffer *buffer);

private:
	ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>():
		ThriftDescriptor(ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>::read,
						 ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>::write,
						 ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>::read_json,
						 ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>::write_json)
	{
		this->data_type = DATA;
		//this->reader = ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>::read;
		//this->writer = ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>::write;
		//this->json_reader = ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>::read_json;
		//this->json_writer = ThriftDescriptorImpl<T, DATA, KEYIMPL, VALIMPL>::write_json;
	}
};

template<class T, class VALIMPL>
class ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL> : public ThriftDescriptor
{
public:
	static const ThriftDescriptor *get_instance()
	{
		static const ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL> kInstance;

		return &kInstance;
	}

	static bool read(ThriftBuffer *buffer, void *data)
	{
		T *list = static_cast<T *>(data);
		const auto *val_desc = VALIMPL::get_instance();
		int8_t field_type;
		int32_t count;

		if (!buffer->readI08(field_type))
			return false;

		if (!buffer->readI32(count))
			return false;

		list->resize(count);
		for (auto& ele : *list)
		{
			if (!val_desc->reader(buffer, &ele))
				return false;
		}

		return true;
	}

	static bool write(const void *data, ThriftBuffer *buffer)
	{
		const T *list = static_cast<const T *>(data);
		const auto *val_desc = VALIMPL::get_instance();

		if (!buffer->writeI08(val_desc->data_type))
			return false;

		if (!buffer->writeI32(list->size()))
			return false;

		for (const auto& ele : *list)
		{
			if (!val_desc->writer(&ele, buffer))
				return false;
		}

		return true;
	}

	static bool read_json(ThriftBuffer *buffer, void *data)
	{
		T *list = static_cast<T *>(data);
		const auto *val_desc = VALIMPL::get_instance();
		bool is_first = true;
		typename T::value_type ele;
		char ch;

		if (!ThriftJsonUtil::skip_character(buffer, '['))
			return false;

		while (ThriftJsonUtil::peek_first_meaningful_char(buffer, ch))
		{
			if (ch == ']')
			{
				buffer->buffer->seek(1);
				return true;
			}

			if (is_first)
				is_first = false;
			else
			{
				buffer->buffer->seek(1);
				if (ch != ',')
					break;
			}

			if (!val_desc->json_reader(buffer, &ele))
				break;

			list->emplace_back(std::move(ele));
		}

		return false;
	}

	static bool write_json(const void *data, ThriftBuffer *buffer)
	{
		const T *list = static_cast<const T *>(data);
		const auto *val_desc = VALIMPL::get_instance();
		bool is_first = true;

		if (!buffer->writeI08('['))
			return false;

		for (const auto& ele : *list)
		{
			if (is_first)
				is_first = false;
			else if (!buffer->writeI08(','))
				return false;

			if (!val_desc->json_writer(&ele, buffer))
				return false;
		}

		return buffer->writeI08(']');
	}

private:
	ThriftDescriptorImpl():
		ThriftDescriptor(ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL>::read,
						 ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL>::write,
						 ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL>::read_json,
						 ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL>::write_json)
	{
		this->data_type = TDT_LIST;
		//this->reader = ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL>::read;
		//this->writer = ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL>::write;
		//this->json_reader = ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL>::read_json;
		//this->json_writer = ThriftDescriptorImpl<T, TDT_LIST, void, VALIMPL>::write_json;
	}
};

template<class T, class VALIMPL>
class ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL> : public ThriftDescriptor
{
public:
	static const ThriftDescriptor *get_instance()
	{
		static const ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL> kInstance;

		return &kInstance;
	}

	static bool read(ThriftBuffer *buffer, void *data)
	{
		T *set = static_cast<T *>(data);
		const auto *val_desc = VALIMPL::get_instance();
		int8_t field_type;
		int32_t count;
		typename T::value_type ele;

		if (!buffer->readI08(field_type))
			return false;

		if (!buffer->readI32(count))
			return false;

		set->clear();
		for (int i = 0; i < count; i++)
		{
			if (!val_desc->reader(buffer, &ele))
				return false;

			set->insert(std::move(ele));
		}

		return true;
	}

	static bool write(const void *data, ThriftBuffer *buffer)
	{
		const T *set = static_cast<const T *>(data);
		const auto *val_desc = VALIMPL::get_instance();

		if (!buffer->writeI08(val_desc->data_type))
			return false;

		if (!buffer->writeI32(set->size()))
			return false;

		for (const auto& ele : *set)
		{
			if (!val_desc->writer(&ele, buffer))
				return false;
		}

		return true;
	}

	static bool read_json(ThriftBuffer *buffer, void *data)
	{
		T *set = static_cast<T *>(data);
		const auto *val_desc = VALIMPL::get_instance();
		bool is_first = true;
		typename T::value_type ele;
		char ch;

		if (!ThriftJsonUtil::skip_character(buffer, '['))
			return false;

		while (ThriftJsonUtil::peek_first_meaningful_char(buffer, ch))
		{
			if (ch == ']')
			{
				buffer->buffer->seek(1);
				return true;
			}

			if (is_first)
				is_first = false;
			else
			{
				buffer->buffer->seek(1);
				if (ch != ',')
					break;
			}

			if (!val_desc->json_reader(buffer, &ele))
				break;

			set->insert(std::move(ele));
		}

		return false;
	}

	static bool write_json(const void *data, ThriftBuffer *buffer)
	{
		const T *set = static_cast<const T *>(data);
		const auto *val_desc = VALIMPL::get_instance();
		bool is_first = true;

		if (!buffer->writeI08('['))
			return false;

		for (const auto& ele : *set)
		{
			if (is_first)
				is_first = false;
			else if (!buffer->writeI08(','))
				return false;

			if (!val_desc->json_writer(&ele, buffer))
				return false;
		}

		return buffer->writeI08(']');
	}

private:
	ThriftDescriptorImpl():
		ThriftDescriptor(ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL>::read,
						 ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL>::write,
						 ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL>::read_json,
						 ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL>::write_json)
	{
		this->data_type = TDT_SET;
		//this->reader = ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL>::read;
		//this->writer = ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL>::write;
		//this->json_reader = ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL>::read_json;
		//this->json_writer = ThriftDescriptorImpl<T, TDT_SET, void, VALIMPL>::write_json;
	}
};

template<class T, class KEYIMPL, class VALIMPL>
class ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL> : public ThriftDescriptor
{
public:
	static const ThriftDescriptor *get_instance()
	{
		static const ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL> kInstance;

		return &kInstance;
	}

	static bool read(ThriftBuffer *buffer, void *data)
	{
		T *map = static_cast<T *>(data);
		const auto *key_desc = KEYIMPL::get_instance();
		const auto *val_desc = VALIMPL::get_instance();
		int8_t field_type;
		int32_t count;
		typename T::key_type key;
		typename T::mapped_type val;

		if (!buffer->readI08(field_type))
			return false;

		if (!buffer->readI08(field_type))
			return false;

		if (!buffer->readI32(count))
			return false;

		map->clear();
		for (int i = 0; i < count; i++)
		{
			if (!key_desc->reader(buffer, &key))
				return false;

			if (!val_desc->reader(buffer, &val))
				return false;

			map->insert(std::make_pair(std::move(key), std::move(val)));
		}

		return true;
	}

	static bool write(const void *data, ThriftBuffer *buffer)
	{
		const T *map = static_cast<const T *>(data);
		const auto *key_desc = KEYIMPL::get_instance();
		const auto *val_desc = VALIMPL::get_instance();

		if (!buffer->writeI08(key_desc->data_type))
			return false;

		if (!buffer->writeI08(val_desc->data_type))
			return false;

		if (!buffer->writeI32(map->size()))
			return false;

		for (const auto& kv : *map)
		{
			if (!key_desc->writer(&kv.first, buffer))
				return false;

			if (!val_desc->writer(&kv.second, buffer))
				return false;
		}

		return true;
	}

	static bool read_json(ThriftBuffer *buffer, void *data)
	{
		T *map = static_cast<T *>(data);
		const auto *key_desc = KEYIMPL::get_instance();
		const auto *val_desc = VALIMPL::get_instance();
		bool is_first = true;
		typename T::key_type key;
		typename T::mapped_type val;
		char ch;

		if (!ThriftJsonUtil::skip_character(buffer, '['))
			return false;

		while (ThriftJsonUtil::peek_first_meaningful_char(buffer, ch))
		{
			if (ch == ']')
			{
				buffer->buffer->seek(1);
				return true;
			}

			if (is_first)
				is_first = false;
			else
			{
				buffer->buffer->seek(1);
				if (ch != ',')
					break;
			}

			if (!ThriftJsonUtil::skip_character(buffer, '{'))
				return false;

			if (!ThriftJsonUtil::skip_simple_string(buffer, "key"))
				return false;

			if (!ThriftJsonUtil::skip_character(buffer, ':'))
				return false;

			if (!key_desc->json_reader(buffer, &key))
				break;

			if (!ThriftJsonUtil::skip_character(buffer, ','))
				return false;

			if (!ThriftJsonUtil::skip_simple_string(buffer, "value"))
				return false;

			if (!ThriftJsonUtil::skip_character(buffer, ':'))
				return false;

			if (!val_desc->json_reader(buffer, &val))
				break;

			ThriftJsonUtil::skip_character(buffer, '}');
			map->insert(std::make_pair(std::move(key), std::move(val)));
		}

		return false;
	}

	static bool write_json(const void *data, ThriftBuffer *buffer)
	{
		const T *map = static_cast<const T *>(data);
		const auto *key_desc = KEYIMPL::get_instance();
		const auto *val_desc = VALIMPL::get_instance();
		bool is_first = true;

		if (!buffer->writeI08('['))
			return false;

		for (const auto& kv : *map)
		{
			if (is_first)
				is_first = false;
			else if (!buffer->writeI08(','))
				return false;

			if (!buffer->writeStringBody("{\"key\":"))
				return false;

			if (!key_desc->json_writer(&kv.first, buffer))
				return false;

			if (!buffer->writeStringBody(",\"value\":"))
				return false;

			if (!val_desc->json_writer(&kv.second, buffer))
				return false;

			if (!buffer->writeI08('}'))
				return false;
		}

		return buffer->writeI08(']');
	}

private:
	ThriftDescriptorImpl():
		ThriftDescriptor(ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL>::read,
						 ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL>::write,
						 ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL>::read_json,
						 ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL>::write_json)
	{
		this->data_type = TDT_MAP;
		//this->reader = ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL>::read;
		//this->writer = ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL>::write;
		//this->json_reader = ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL>::read_json;
		//this->json_writer = ThriftDescriptorImpl<T, TDT_MAP, KEYIMPL, VALIMPL>::write_json;
	}
};

template<class T>
class ThriftDescriptorImpl<T, TDT_STRUCT, void, void> : public ThriftDescriptor
{
public:
	static const ThriftDescriptor *get_instance()
	{
		static const ThriftDescriptorImpl<T, TDT_STRUCT, void, void> kInstance;

		return &kInstance;
	}

public:
	static bool read(ThriftBuffer *buffer, void *data)
	{
		T *st = static_cast<T *>(data);
		char *base = (char *)data;
		int8_t field_type;
		int16_t field_id;
		auto it = st->elements->cbegin();

		while (true)
		{
			if (!buffer->readFieldBegin(field_type, field_id))
				return false;

			if (field_type == TDT_STOP)
				return true;

			while (it != st->elements->cend() && it->field_id < field_id)
				++it;

			if (it != st->elements->cend()
				&& it->field_id == field_id
				&& it->desc->data_type == field_type)
			{
				if (it->required_state != THRIFT_STRUCT_FIELD_REQUIRED)
					*((bool *)(base + it->isset_offset)) = true;

				if (!it->desc->reader(buffer, base + it->data_offset))
					return false;
			}
			else
			{
				if (!buffer->skip(field_type))
					return false;
			}
		}

		return true;
	}

	static bool write(const void *data, ThriftBuffer *buffer)
	{
		const T *st = static_cast<const T *>(data);
		const char *base = (const char *)data;

		for (const auto& ele : *(st->elements))
		{
			if (ele.required_state != THRIFT_STRUCT_FIELD_OPTIONAL || *((bool *)(base + ele.isset_offset)) == true)
			{
				if (!buffer->writeFieldBegin(ele.desc->data_type, ele.field_id))
					return false;

				if (!ele.desc->writer(base + ele.data_offset, buffer))
					return false;
			}
		}

		return buffer->writeFieldStop();
	}

	static bool read_json(ThriftBuffer *buffer, void *data)
	{
		T *st = static_cast<T *>(data);
		char *base = (char *)data;
		bool is_first = true;
		std::string name;
		char ch;

		if (!ThriftJsonUtil::skip_character(buffer, '{'))
			return false;

		while (ThriftJsonUtil::peek_first_meaningful_char(buffer, ch))
		{
			if (ch == '}')
			{
				buffer->buffer->seek(1);
				return true;
			}

			if (is_first)
				is_first = false;
			else
			{
				buffer->buffer->seek(1);
				if (ch != ',')
					break;
			}

			if (!ThriftJsonUtil::read_string(buffer, &name))
				return false;

			if (!ThriftJsonUtil::skip_character(buffer, ':'))
				return false;

			bool is_find = false;

			for (const auto& ele : *(st->elements))
			{
				if (name == ele.name)
				{
					is_find = true;
					if (ele.required_state != THRIFT_STRUCT_FIELD_REQUIRED)
						*((bool *)(base + ele.isset_offset)) = true;

					if (!ele.desc->json_reader(buffer, base + ele.data_offset))
						return false;

					break;
				}
			}

			if (!is_find)
			{
				if (!ThriftJsonUtil::skip_one_element(buffer))
					return false;
			}
		}

		return false;
	}

	static bool write_json(const void *data, ThriftBuffer *buffer)
	{
		const T *st = static_cast<const T *>(data);
		const char *base = (const char *)data;
		bool is_first = true;

		if (!buffer->writeI08('{'))
			return false;

		for (const auto& ele : *(st->elements))
		{
			if (ele.required_state != THRIFT_STRUCT_FIELD_OPTIONAL || *((bool *)(base + ele.isset_offset)) == true)
			{
				if (is_first)
					is_first = false;
				else if (!buffer->writeI08(','))
					return false;

				if (!buffer->writeStringBody(std::string("\"") + ele.name + "\":"))
					return false;

				if (!ele.desc->json_writer(base + ele.data_offset, buffer))
					return false;
			}
		}

		return buffer->writeI08('}');
	}

private:
	ThriftDescriptorImpl<T, TDT_STRUCT, void, void>():
		ThriftDescriptor(ThriftDescriptorImpl<T, TDT_STRUCT, void, void>::read,
						 ThriftDescriptorImpl<T, TDT_STRUCT, void, void>::write,
						 ThriftDescriptorImpl<T, TDT_STRUCT, void, void>::read_json,
						 ThriftDescriptorImpl<T, TDT_STRUCT, void, void>::write_json)
	{
		this->data_type = TDT_STRUCT;
		//this->reader = ThriftDescriptorImpl<T, TDT_STRUCT, void, void>::read;
		//this->writer = ThriftDescriptorImpl<T, TDT_STRUCT, void, void>::write;
		//this->json_reader = ThriftDescriptorImpl<T, TDT_STRUCT, void, void>::read_json;
		//this->json_writer = ThriftDescriptorImpl<T, TDT_STRUCT, void, void>::write_json;
	}
};

template<>
inline bool ThriftDescriptorImpl<bool, TDT_BOOL, void, void>::read(ThriftBuffer *buffer, void *data)
{
	int8_t *p = static_cast<int8_t *>(data);

	return buffer->readI08(*p);
}

template<>
inline bool ThriftDescriptorImpl<int8_t, TDT_I08, void, void>::read(ThriftBuffer *buffer, void *data)
{
	int8_t *p = static_cast<int8_t *>(data);

	return buffer->readI08(*p);
}

template<>
inline bool ThriftDescriptorImpl<int16_t, TDT_I16, void, void>::read(ThriftBuffer *buffer, void *data)
{
	int16_t *p = static_cast<int16_t *>(data);

	return buffer->readI16(*p);
}

template<>
inline bool ThriftDescriptorImpl<int32_t, TDT_I32, void, void>::read(ThriftBuffer *buffer, void *data)
{
	int32_t *p = static_cast<int32_t *>(data);

	return buffer->readI32(*p);
}

template<>
inline bool ThriftDescriptorImpl<int64_t, TDT_I64, void, void>::read(ThriftBuffer *buffer, void *data)
{
	int64_t *p = static_cast<int64_t *>(data);

	return buffer->readI64(*p);
}

template<>
inline bool ThriftDescriptorImpl<uint64_t, TDT_U64, void, void>::read(ThriftBuffer *buffer, void *data)
{
	uint64_t *p = static_cast<uint64_t *>(data);

	return buffer->readU64(*p);
}

template<>
inline bool ThriftDescriptorImpl<double, TDT_DOUBLE, void, void>::read(ThriftBuffer *buffer, void *data)
{
	uint64_t *p = static_cast<uint64_t *>(data);

	return buffer->readU64(*p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_STRING, void, void>::read(ThriftBuffer *buffer, void *data)
{
	std::string *p = static_cast<std::string *>(data);

	return buffer->readString(*p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_UTF8, void, void>::read(ThriftBuffer *buffer, void *data)
{
	std::string *p = static_cast<std::string *>(data);

	return buffer->readString(*p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_UTF16, void, void>::read(ThriftBuffer *buffer, void *data)
{
	std::string *p = static_cast<std::string *>(data);

	return buffer->readString(*p);
}

template<>
inline bool ThriftDescriptorImpl<bool, TDT_BOOL, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const int8_t *p = static_cast<const int8_t *>(data);

	return buffer->writeI08(*p);
}

template<>
inline bool ThriftDescriptorImpl<int8_t, TDT_I08, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const int8_t *p = static_cast<const int8_t *>(data);

	return buffer->writeI08(*p);
}

template<>
inline bool ThriftDescriptorImpl<int16_t, TDT_I16, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const int16_t *p = static_cast<const int16_t *>(data);

	return buffer->writeI16(*p);
}

template<>
inline bool ThriftDescriptorImpl<int32_t, TDT_I32, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const int32_t *p = static_cast<const int32_t *>(data);

	return buffer->writeI32(*p);
}

template<>
inline bool ThriftDescriptorImpl<int64_t, TDT_I64, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const int64_t *p = static_cast<const int64_t *>(data);

	return buffer->writeI64(*p);
}

template<>
inline bool ThriftDescriptorImpl<uint64_t, TDT_U64, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const uint64_t *p = static_cast<const uint64_t *>(data);

	return buffer->writeU64(*p);
}

template<>
inline bool ThriftDescriptorImpl<double, TDT_DOUBLE, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const uint64_t *p = static_cast<const uint64_t *>(data);

	return buffer->writeU64(*p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_STRING, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const std::string *p = static_cast<const std::string *>(data);

	return buffer->writeString(*p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_UTF8, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const std::string *p = static_cast<const std::string *>(data);

	return buffer->writeString(*p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_UTF16, void, void>::write(const void *data, ThriftBuffer *buffer)
{
	const std::string *p = static_cast<const std::string *>(data);

	return buffer->writeString(*p);
}

template<>
inline bool ThriftDescriptorImpl<bool, TDT_BOOL, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	int8_t *p = static_cast<int8_t *>(data);
	int64_t intv;

	if (!ThriftJsonUtil::read_int64(buffer, intv))
		return false;

	*p = (int8_t)intv;
	return true;
}

template<>
inline bool ThriftDescriptorImpl<int8_t, TDT_I08, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	int8_t *p = static_cast<int8_t *>(data);
	int64_t intv;

	if (!ThriftJsonUtil::read_int64(buffer, intv))
		return false;

	*p = (int8_t)intv;
	return true;
}

template<>
inline bool ThriftDescriptorImpl<int16_t, TDT_I16, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	int16_t *p = static_cast<int16_t *>(data);
	int64_t intv;

	if (!ThriftJsonUtil::read_int64(buffer, intv))
		return false;

	*p = (int16_t)intv;
	return true;
}

template<>
inline bool ThriftDescriptorImpl<int32_t, TDT_I32, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	int32_t *p = static_cast<int32_t *>(data);
	int64_t intv;

	if (!ThriftJsonUtil::read_int64(buffer, intv))
		return false;

	*p = (int32_t)intv;
	return true;
}

template<>
inline bool ThriftDescriptorImpl<int64_t, TDT_I64, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	int64_t *p = static_cast<int64_t *>(data);

	return ThriftJsonUtil::read_int64(buffer, *p);
}

template<>
inline bool ThriftDescriptorImpl<uint64_t, TDT_U64, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	uint64_t *p = static_cast<uint64_t *>(data);

	return ThriftJsonUtil::read_uint64(buffer, *p);
}

template<>
inline bool ThriftDescriptorImpl<double, TDT_DOUBLE, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	double *p = static_cast<double *>(data);

	return ThriftJsonUtil::read_double(buffer, *p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_STRING, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	std::string *p = static_cast<std::string *>(data);

	return ThriftJsonUtil::read_string(buffer, p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_UTF8, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	std::string *p = static_cast<std::string *>(data);

	return ThriftJsonUtil::read_string(buffer, p);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_UTF16, void, void>::read_json(ThriftBuffer *buffer, void *data)
{
	std::string *p = static_cast<std::string *>(data);

	return ThriftJsonUtil::read_string(buffer, p);
}

template<>
inline bool ThriftDescriptorImpl<bool, TDT_BOOL, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const int8_t *p = static_cast<const int8_t *>(data);

	return buffer->writeStringBody(std::to_string(*p));
}

template<>
inline bool ThriftDescriptorImpl<int8_t, TDT_I08, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const int8_t *p = static_cast<const int8_t *>(data);

	return buffer->writeStringBody(std::to_string(*p));
}

template<>
inline bool ThriftDescriptorImpl<int16_t, TDT_I16, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const int16_t *p = static_cast<const int16_t *>(data);

	return buffer->writeStringBody(std::to_string(*p));
}

template<>
inline bool ThriftDescriptorImpl<int32_t, TDT_I32, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const int32_t *p = static_cast<const int32_t *>(data);

	return buffer->writeStringBody(std::to_string(*p));
}

template<>
inline bool ThriftDescriptorImpl<int64_t, TDT_I64, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const int64_t *p = static_cast<const int64_t *>(data);

	return buffer->writeStringBody(std::to_string(*p));
}

template<>
inline bool ThriftDescriptorImpl<uint64_t, TDT_U64, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const uint64_t *p = static_cast<const uint64_t *>(data);

	return buffer->writeStringBody(std::to_string(*p));
}

template<>
inline bool ThriftDescriptorImpl<double, TDT_DOUBLE, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const double *p = static_cast<const double *>(data);

	return buffer->writeStringBody(std::to_string(*p));
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_STRING, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const std::string *p = static_cast<const std::string *>(data);
	std::string str;

	if (!ThriftJsonUtil::escape_string(*p, str))
		return false;

	return buffer->writeStringBody(str);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_UTF8, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const std::string *p = static_cast<const std::string *>(data);
	std::string str;

	if (!ThriftJsonUtil::escape_string(*p, str))
		return false;

	return buffer->writeStringBody(str);
}

template<>
inline bool ThriftDescriptorImpl<std::string, TDT_UTF16, void, void>::write_json(const void *data, ThriftBuffer *buffer)
{
	const std::string *p = static_cast<const std::string *>(data);
	std::string str;

	if (!ThriftJsonUtil::escape_string(*p, str))
		return false;

	return buffer->writeStringBody(str);
}

} // end namespace srpc

