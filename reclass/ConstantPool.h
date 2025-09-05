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
#include "../libs/jni/jni.h"
#include "Array.h"
#include "Symbol.h"
#include <cstring>

class Klass;
class ConstantPoolCache;
class Monitor;
class ConstantPool {
public:
	char pad[8];
	Array<unsigned char>* _tags;
	ConstantPoolCache* _constantPoolCache;
	Klass* _pool_holder;
	Array<unsigned short>*           _operands;    // for variable-sized (InvokeDynamic) nodes, usually empty
	
	// Array of resolved objects from the constant pool and map from resolved
	// object index to original constant pool index
	jobject              _resolved_references;
	Array<unsigned short>*           _reference_map;

	enum {
		_has_preresolution = 1,           // Flags
	    	_on_stack          = 2
	};

	int                  _flags;  // old fashioned bit twiddling
	int                  _length; // number of elements in the array

	union {
		// set for CDS to restore resolved references
		int                _resolved_reference_length;
		// keeps version number for redefined classes (used in backtrace)
		int                _version;
	} _saved;

	Monitor*             _lock;

	intptr_t* base() const {
		return (intptr_t*)(((char*)this) + sizeof(ConstantPool));
	}

	Symbol** symbol_at_addr(int idx) const {
		return (Symbol**)&base()[idx];
	}

	Symbol* symbol_at(int idx) const {
		return *symbol_at_addr(idx);
	}

	void set_symbol(int idx, Symbol* sym){
		Symbol** symbAddr = symbol_at_addr(idx);
		// Replace the existing Symbol* with the new Symbol*
		*symbAddr = sym;
	}


};
