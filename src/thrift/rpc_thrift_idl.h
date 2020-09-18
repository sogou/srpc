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

#ifndef __RPC_THRIFT_IDL_H__
#define __RPC_THRIFT_IDL_H__

#include <stddef.h>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "rpc_thrift_buffer.h"

namespace srpc
{

class ThriftDescriptor
{
public:
	int8_t data_type;

protected:
	using ReaderFunctionPTR = bool (*)(ThriftBuffer *, void *);
	using WriterFunctionPTR = bool (*)(const void *, ThriftBuffer *);
	virtual ~ThriftDescriptor() { }

	ThriftDescriptor():
		reader(nullptr),
		writer(nullptr),
		json_reader(nullptr),
		json_writer(nullptr)
	{}

	ThriftDescriptor(const ReaderFunctionPTR r,
					 const WriterFunctionPTR w,
					 const ReaderFunctionPTR jr,
					 const WriterFunctionPTR jw):
		reader(r),
		writer(w),
		json_reader(jr),
		json_writer(jw)
	{}

public:
	const ReaderFunctionPTR reader;
	const WriterFunctionPTR writer;
	const ReaderFunctionPTR json_reader;
	const WriterFunctionPTR json_writer;
};

struct struct_element
{
	const ThriftDescriptor *desc;
	const char *name;
	ptrdiff_t isset_offset;
	ptrdiff_t data_offset;
	int16_t field_id;
	int8_t required_state;
};

template<class T>
class ThriftElementsImpl
{
public:
	static const std::list<struct_element> *get_elements_instance()
	{
		static const ThriftElementsImpl<T> kInstance;

		return &kInstance.elements;
	}

private:
	ThriftElementsImpl<T>()
	{
		T::StaticElementsImpl(&this->elements);
	}

	std::list<struct_element> elements;
};

class ThriftIDLMessage
{
public:
	const ThriftDescriptor *descriptor = nullptr;
	const std::list<struct_element> *elements = nullptr;

	std::string debug_string() const { return ""; }

	virtual ~ThriftIDLMessage() { }
};

} // end namespace srpc

#include "rpc_thrift_idl.inl"

#endif

