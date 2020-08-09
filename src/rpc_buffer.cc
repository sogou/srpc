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

  Authors: Wu Jiaxu (wujiaxu@sogou-inc.com)
*/

#include <errno.h>
#include <stdlib.h>
#include "rpc_buffer.h"

namespace sogou
{

size_t RPCBuffer::acquire(void **buf)
{
	if (last_piece_left_ > 0)
	{
		const auto it = buffer_list_.rbegin();
		size_t sz = last_piece_left_;

		*buf = (char *)it->buf + it->buflen;
		it->buflen += last_piece_left_;
		size_ += last_piece_left_;
		last_piece_left_ = 0;
		return sz;
	}

	*buf = malloc(piece_min_size_);
	if (*buf == NULL)
		return 0;

	append(*buf, piece_min_size_, BUFFER_MODE_GIFT_MALLOC);
	return piece_min_size_;
}

bool RPCBuffer::acquire(void **buf, size_t *size)
{
	if (last_piece_left_ == 0)
	{
		size_t sz = *size;

		if (sz > piece_max_size_)
			sz = piece_max_size_;

		if (sz < piece_min_size_)
			sz = piece_min_size_;

		*buf = malloc(sz);
		if (*buf == NULL)
		{
			*size = 0;
			return false;
		}

		append(*buf, 0, BUFFER_MODE_GIFT_MALLOC);
		last_piece_left_ = sz;
	}

	const auto it = buffer_list_.rbegin();

	*buf = (char *)it->buf + it->buflen;
	if (last_piece_left_ <= *size)
	{
		*size = last_piece_left_;
		last_piece_left_ = 0;
	}
	else
		last_piece_left_ -= *size;

	it->buflen += *size;
	size_ += *size;
	return true;
}

int RPCBuffer::merge_all(struct iovec& iov)
{
	size_t sz = 0;
	void *new_base = malloc(size_);

	if (!new_base)
		return -1;

	for (const auto& ele : buffer_list_)
	{
		memcpy((char *)new_base + sz, ele.buf, ele.buflen);
		sz += ele.buflen;
		if (!ele.is_nocopy)
		{
			if (ele.is_new)
				delete []((char *)ele.buf);
			else
				free(ele.buf);
		}
	}

	iov.iov_base = new_base;
	iov.iov_len = sz;
	buffer_list_.resize(1);
	auto head = buffer_list_.begin();

	head->buf = new_base;
	head->buflen = sz;
	head->is_nocopy = false;
	head->is_new = false;
	return 1;
}

int RPCBuffer::encode(struct iovec *iov, int count)
{
	if (count <= 0)
	{
		errno = EINVAL;
		return -1;
	}

	if (count == 1)
		return merge_all(iov[0]) == 0 ? 1 : -1;

	//if (buffer_list_.size() > count)
	//{
	//	// todo try merge all Adjacent copyed
	//	// nocopy copy nocopy copy copy nocopy ...
	//	// 0      1    0      1    1    0      ...
	//	// merge same, test if <= count
	//}

	while (buffer_list_.size() > (size_t)count)
	{
		//merge half
		auto next = buffer_list_.begin();
		auto cur = next;

		++next;
		while (next != buffer_list_.end())
		{
			size_t sz = cur->buflen + next->buflen;
			void *new_base = malloc(sz);

			if (!new_base)
				return -1;

			memcpy(new_base, cur->buf, cur->buflen);
			memcpy((char *)new_base + cur->buflen, next->buf, next->buflen);

			if (!cur->is_nocopy)
			{
				if (cur->is_new)
					delete []((char *)cur->buf);
				else
					free(cur->buf);
			}

			if (!next->is_nocopy)
			{
				if (next->is_new)
					delete []((char *)next->buf);
				else
					free(next->buf);
			}

			cur->buf = new_base;
			cur->buflen = sz;
			cur->is_nocopy = false;
			cur->is_new = false;

			buffer_list_.erase(next);
			++cur;
			next = cur;
			++next;
		}
	}

	int n = 0;

	for (const auto& ele : buffer_list_)
	{
		if (ele.buflen > 0)
		{
			iov[n].iov_base = ele.buf;
			iov[n].iov_len = ele.buflen;
			n++;
		}
	}

	return n;
}

bool RPCBuffer::fetch(const void **buf, size_t *size)
{
	if (!init_read_over_)
		rewind();

	while (cur_.first != buffer_list_.end() && cur_.second >= cur_.first->buflen)
	{
		++cur_.first;
		cur_.second = 0;
	}

	if (cur_.first != buffer_list_.end())
	{
		size_t n = cur_.first->buflen - cur_.second;

		*buf = (char *)cur_.first->buf + cur_.second;
		if (*size < n)
			cur_.second += *size;
		else
		{
			*size = n;
			++cur_.first;
			cur_.second = 0;
		}

		return true;
	}

	*buf = nullptr;
	*size = 0;
	return false;
}

size_t RPCBuffer::internal_fetch(const void **buf, bool move_or_stay)
{
	size_t sz = 0;

	if (!init_read_over_)
		rewind();

	while (cur_.first != buffer_list_.end() && cur_.second >= cur_.first->buflen)
	{
		++cur_.first;
		cur_.second = 0;
	}

	if (cur_.first == buffer_list_.end())
		*buf = nullptr;
	else
	{
		*buf = (char *)cur_.first->buf + cur_.second;
		sz = cur_.first->buflen - cur_.second;
		if (move_or_stay)
		{
			++cur_.first;
			cur_.second = 0;
		}
	}

	return sz;
}

long RPCBuffer::read_skip(long offset)
{
	long origin = offset;

	if (!init_read_over_)
		rewind();

	while (offset > 0 && cur_.first != buffer_list_.end())
	{
		if (cur_.second < cur_.first->buflen)
		{
			long n = cur_.first->buflen - cur_.second;

			if (offset < n)
			{
				cur_.second += offset;
				offset = 0;
				break;
			}

			offset -= n;
		}

		++cur_.first;
		cur_.second = 0;
	}

	return origin - offset;
}

long RPCBuffer::read_back(long offset)
{
	long origin = offset;

	if (!init_read_over_)
		rewind();

	if (cur_.first == buffer_list_.end())
	{
		if (buffer_list_.empty())
			return 0;
		else
		{
			--cur_.first;
			cur_.second = cur_.first->buflen;
		}
	}

	while (offset < 0)
	{
		if (cur_.second > 0)
		{
			long n = cur_.second;

			if (n + offset >= 0)
			{
				cur_.second += offset;
				offset = 0;
				break;
			}

			offset += n;
		}

 		if (cur_.first == buffer_list_.begin())
 			break;

		--cur_.first;
		cur_.second = cur_.first->buflen;
	}

	return origin - offset;
}

size_t RPCBuffer::cut(size_t offset, RPCBuffer *out)
{
	rewind();
	size_ = seek(offset);

	size_t cutsize = 0;
	const void *buf;
	size_t sz = fetch(&buf);

	if (sz > 0)
	{
		bool keep_left = false;
		void *left_buf;

		if (last_piece_left_ > 0)
		{
			const auto it = buffer_list_.rbegin();

			left_buf = (char *)it->buf + it->buflen;
		}

		out->last_piece_left_ = 0; // drop
		if (sz < cur_.second)
		{
			out->append(buf, sz, BUFFER_MODE_COPY);
			cutsize += sz;
			cur_.first->buflen = cur_.second;
			++cur_.first;
			if (std::next(cur_.first, 1) == buffer_list_.end())
				keep_left = true;
		}

		while (cur_.first != buffer_list_.end())
		{
			auto it = cur_.first;

			if (it->is_nocopy)
				out->append(it->buf, it->buflen, BUFFER_MODE_NOCOPY);
			else
				out->append(it->buf, it->buflen,
							it->is_new ? BUFFER_MODE_GIFT_NEW
									   : BUFFER_MODE_GIFT_MALLOC);

			cutsize += it->buflen;
			++cur_.first;
			buffer_list_.erase(it);
		}

		if (last_piece_left_ > 0)
		{
			if (keep_left)
			{
				buffer_t ele;

				ele.buflen = 0;
				ele.buf = left_buf;
				ele.is_nocopy = true;
				ele.is_new = false;
				buffer_list_.emplace_back(std::move(ele));
			}
			else
			{
				out->append(left_buf, last_piece_left_, BUFFER_MODE_NOCOPY);
				out->backup(last_piece_left_);
			}
		}
	}

	rewind();
	return cutsize;
}

} // namespace sogou

