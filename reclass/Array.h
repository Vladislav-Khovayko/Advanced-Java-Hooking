/*
 * Copyright 2022 https://github.com/ctsmax
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

template <typename T>
class Array {
protected:
	int _length;                                 // the number of array elements
	T   _data[1];                                // the array memory

	void initialize(int length) {
		_length = length;
	}
public:
	int  length() const { return _length; }
	T* data() { return _data; }
	bool is_empty() const { return length() == 0; }

	int index_of(const T& x) const {
		int i = length();
		while (i-- > 0 && _data[i] != x);

		return i;
	}

	bool contains(const T& x) const { return index_of(x) >= 0; }

	T    at(int i) const { return _data[i]; }
	void at_put(const int i, const T& x) { _data[i] = x; }
	T* adr_at(const int i) { return &_data[i]; }
	int  find(const T& x) { return index_of(x); }
};