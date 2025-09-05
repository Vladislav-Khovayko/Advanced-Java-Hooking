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

#include <stdint.h>
#include "ConstantPool.h"

class ConstMethod {
public:
	unsigned long _fingerprint;
	ConstantPool* _constants;
	Array<unsigned char>* _stackmap_data;
	int _constMethod_size; //NOT size of parameters
	unsigned short _flags;
	unsigned char _result_type;
	unsigned short _name_index;
	unsigned short _signature_index;
	unsigned short _method_idnum;
	unsigned short _max_stack;
	unsigned short _max_locals;
	unsigned short _size_of_parameters;
	unsigned short _orig_method_idnum;
};


class AdapterHandlerEntry
{
public:
	char pad_0000[24]; //0x0000
	uint64_t _i2c_entry; //0x0018
	uint64_t _c2i_entry; //0x0020
	uint64_t _c2i_unverified_entry; //0x0028
	char pad_0030[16]; //0x0030
}; //Size: 0x0040



class Method
{
public:
	char pad_0000[0x8]; //0x0000 - RTTI
	ConstMethod* _constMethod; //0x0008
	// char pad_0010[0x10]; //0x0010
	void* _method_data; //0x0010
	void* _method_counters; //0x0018
	uint32_t _access_flags; //0x0020					- access_flags, NOT METHOD'S FLAGS, https://github.com/openjdk/jdk11/blob/master/src/hotspot/share/utilities/accessFlags.hpp
                                              //jvm.h contains more flags
  char pad_0024[0xC]; //0x0024	- reclass dosnt show anything at 0x24

  void* _i2i_entry; //0x0030
  AdapterHandlerEntry* _adapter;
	unsigned char* _from_compiled_entry; //0x0040
	unsigned char* _code; //0x0048 //_code
	unsigned char* _from_interpreted_entry; //0x0050
};









/*
access flags from
https://github.com/openjdk/jdk11/blob/master/src/hotspot/share/utilities/accessFlags.hpp


enum {
  // See jvm.h for shared JVM_ACC_XXX access flags

  // HotSpot-specific access flags

  // flags actually put in .class file
  JVM_ACC_WRITTEN_FLAGS           = 0x00007FFF,

  // Method* flags
  JVM_ACC_MONITOR_MATCH           = 0x10000000,     // True if we know that monitorenter/monitorexit bytecodes match
  JVM_ACC_HAS_MONITOR_BYTECODES   = 0x20000000,     // Method contains monitorenter/monitorexit bytecodes
  JVM_ACC_HAS_LOOPS               = 0x40000000,     // Method has loops
  JVM_ACC_LOOPS_FLAG_INIT         = (int)0x80000000,// The loop flag has been initialized
  JVM_ACC_QUEUED                  = 0x01000000,     // Queued for compilation
  JVM_ACC_NOT_C2_COMPILABLE       = 0x02000000,
  JVM_ACC_NOT_C1_COMPILABLE       = 0x04000000,
  JVM_ACC_NOT_C2_OSR_COMPILABLE   = 0x08000000,
  JVM_ACC_HAS_LINE_NUMBER_TABLE   = 0x00100000,
  JVM_ACC_HAS_CHECKED_EXCEPTIONS  = 0x00400000,
  JVM_ACC_HAS_JSRS                = 0x00800000,
  JVM_ACC_IS_OLD                  = 0x00010000,     // RedefineClasses() has replaced this method
  JVM_ACC_IS_OBSOLETE             = 0x00020000,     // RedefineClasses() has made method obsolete
  JVM_ACC_IS_PREFIXED_NATIVE      = 0x00040000,     // JVMTI has prefixed this native method
  JVM_ACC_ON_STACK                = 0x00080000,     // RedefineClasses() was used on the stack
  JVM_ACC_IS_DELETED              = 0x00008000,     // RedefineClasses() has deleted this method

  // Klass* flags
  JVM_ACC_HAS_MIRANDA_METHODS     = 0x10000000,     // True if this class has miranda methods in it's vtable
  JVM_ACC_HAS_VANILLA_CONSTRUCTOR = 0x20000000,     // True if klass has a vanilla default constructor
  JVM_ACC_HAS_FINALIZER           = 0x40000000,     // True if klass has a non-empty finalize() method
  JVM_ACC_IS_CLONEABLE_FAST       = (int)0x80000000,// True if klass implements the Cloneable interface and can be optimized in generated code
  JVM_ACC_HAS_FINAL_METHOD        = 0x01000000,     // True if klass has final method
  JVM_ACC_IS_SHARED_CLASS         = 0x02000000,     // True if klass is shared

  // Klass* and Method* flags
  JVM_ACC_HAS_LOCAL_VARIABLE_TABLE= 0x00200000,

  JVM_ACC_PROMOTED_FLAGS          = 0x00200000,     // flags promoted from methods to the holding klass

  // field flags
  // Note: these flags must be defined in the low order 16 bits because
  // InstanceKlass only stores a ushort worth of information from the
  // AccessFlags value.
  // These bits must not conflict with any other field-related access flags
  // (e.g., ACC_ENUM).
  // Note that the class-related ACC_ANNOTATION bit conflicts with these flags.
  JVM_ACC_FIELD_ACCESS_WATCHED            = 0x00002000, // field access is watched by JVMTI
  JVM_ACC_FIELD_MODIFICATION_WATCHED      = 0x00008000, // field modification is watched by JVMTI
  JVM_ACC_FIELD_INTERNAL                  = 0x00000400, // internal field, same as JVM_ACC_ABSTRACT
  JVM_ACC_FIELD_STABLE                    = 0x00000020, // @Stable field, same as JVM_ACC_SYNCHRONIZED and JVM_ACC_SUPER
  JVM_ACC_FIELD_INITIALIZED_FINAL_UPDATE  = 0x00000100, // (static) final field updated outside (class) initializer, same as JVM_ACC_NATIVE
  JVM_ACC_FIELD_HAS_GENERIC_SIGNATURE     = 0x00000800, // field has generic signature

  JVM_ACC_FIELD_INTERNAL_FLAGS       = JVM_ACC_FIELD_ACCESS_WATCHED |
                                       JVM_ACC_FIELD_MODIFICATION_WATCHED |
                                       JVM_ACC_FIELD_INTERNAL |
                                       JVM_ACC_FIELD_STABLE |
                                       JVM_ACC_FIELD_HAS_GENERIC_SIGNATURE,

                                                    // flags accepted by set_field_flags()
  JVM_ACC_FIELD_FLAGS                = JVM_RECOGNIZED_FIELD_MODIFIERS | JVM_ACC_FIELD_INTERNAL_FLAGS

};
*/