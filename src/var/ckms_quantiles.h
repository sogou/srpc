#ifndef __CKMS_QUANTILE_H__
#define __CKMS_QUANTILE_H__

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace srpc
{

struct Quantile
{
	Quantile(double q, double err)
	{
		quantile = q;
		error = err;
		u = 2.0 * err / (1.0 - q);
 		v = 2.0 * err / q;
	}

	double quantile;
	double error;
	double u;
	double v;
};

template<typename TYPE>
class CKMSQuantiles
{
private:
	struct Item
	{
		Item(TYPE value, int lower_delta, int delta);

		TYPE value;
		int g;
		int delta;
	};

 public:
	CKMSQuantiles(const std::vector<Quantile> *quantiles);
	CKMSQuantiles<TYPE>& operator= (const CKMSQuantiles<TYPE>& copy);
	CKMSQuantiles(const CKMSQuantiles<TYPE>& copy);

	void insert(TYPE value);
	TYPE get(double q);
	void reset();
	size_t get_count() const { return this->count; }
	TYPE get_sum() const { return this->sum; }

 private:
	double allowable_error(int rank);
	bool insert_batch();
	void compress();

 private:
	const std::vector<Quantile> *quantiles;

	TYPE sum;
	size_t count;
	std::vector<Item> sample;
	std::array<TYPE, 500> buffer;
	size_t buffer_count;
};

////////// impl

template<typename TYPE>
CKMSQuantiles<TYPE>::Item::Item(TYPE val, int lower_del, int del)
{
	value = val;
	g = lower_del;
	delta = del;
}

template<typename TYPE>
CKMSQuantiles<TYPE>::CKMSQuantiles(const std::vector<Quantile> *q) :
	quantiles(q)
{
	this->count = 0;
	this->sum = 0;
	this->buffer_count = 0;
}

template<typename TYPE>
CKMSQuantiles<TYPE>::CKMSQuantiles(const CKMSQuantiles<TYPE>& copy)
{
	this->operator= (copy);
}

template<typename TYPE>
CKMSQuantiles<TYPE>& CKMSQuantiles<TYPE>::operator= (const CKMSQuantiles<TYPE>& copy)
{
	if (this != &copy)
	{
		this->quantiles = copy.quantiles;
		this->count = copy.count;
		this->sum = copy.sum;
		this->sample = copy.sample;
		this->buffer = copy.buffer;
		this->buffer_count = copy.buffer_count;
	}

	return *this;
}

template<typename TYPE>
void CKMSQuantiles<TYPE>::insert(TYPE value)
{
	this->sum += value;
	this->buffer[this->buffer_count] = value;
	++this->buffer_count;

 	if (this->buffer_count == this->buffer.size())
	{
		this->insert_batch();
		this->compress();
	}
}

template<typename TYPE>
TYPE CKMSQuantiles<TYPE>::get(double q)
{
	this->insert_batch();
	this->compress();

	if (this->sample.empty())
		return std::numeric_limits<TYPE>::quiet_NaN();

	int rank_min = 0;
	const auto desired = static_cast<int>(q * this->count);
	const auto bound = desired + (allowable_error(desired) / 2);

	auto it = this->sample.begin();
	decltype(it) prev;
	auto cur = it++;

	while (it != this->sample.end())
	{
		prev = cur;
		cur = it++;

		rank_min += prev->g;

		if (rank_min + cur->g + cur->delta > bound)
			return prev->value;
	}

	return this->sample.back().value;
}

template<typename TYPE>
void CKMSQuantiles<TYPE>::reset()
{
	this->count = 0;
	this->sum = 0;
	this->sample.clear();
	this->buffer_count = 0;
}

template<typename TYPE>
double CKMSQuantiles<TYPE>::allowable_error(int rank)
{
	auto size = this->sample.size();
	double min_error = size + 1;

	for (const auto& q : *(this->quantiles))
	{
		double error;
		if (rank <= q.quantile * size)
			error = q.u * (size - rank);
		else
			error = q.v * rank;

		if (error < min_error)
			min_error = error;
	}

	return min_error;
}

template<typename TYPE>
bool CKMSQuantiles<TYPE>::insert_batch()
{
	if (this->buffer_count == 0)
		return false;

	std::sort(this->buffer.begin(), this->buffer.begin() + this->buffer_count);

	size_t start = 0;
	size_t idx = 0;
	size_t item = idx++;
	TYPE v;
	int delta;

	if (this->sample.empty())
	{
		this->sample.emplace_back(this->buffer[0], 1, 0);
		++start;
		++this->count;
	}

	for (size_t i = start; i < this->buffer_count; ++i)
	{
		v = this->buffer[i];

		while (idx < this->sample.size() && this->sample[item].value < v)
			item = idx++;

		if (this->sample[item].value > v)
			--idx;

		if (idx - 1 == 0 || idx + 1 == this->sample.size())
			delta = 0;
		else
			delta = static_cast<int>(std::floor(allowable_error(idx + 1))) + 1;

		this->sample.emplace(this->sample.begin() + idx, v, 1, delta);
		this->count++;
		item = idx++;
	}

	this->buffer_count = 0;
	return true;
}

template<typename TYPE>
void CKMSQuantiles<TYPE>::compress()
{
	if (this->sample.size() < 2)
		return;

	size_t idx = 0;
	size_t prev;
	size_t next = idx++;

	while (idx < this->sample.size())
	{
		prev = next;
		next = idx++;

		if (this->sample[prev].g + this->sample[next].g +
			this->sample[next].delta <= allowable_error(idx - 1))
		{
			this->sample[next].g += this->sample[prev].g;
			this->sample.erase(this->sample.begin() + prev);
		}
	}
}

} // end namespace srpc

#endif

