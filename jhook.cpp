/*
 * Copyright (C) 2025 Vladislav Khovayko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include <iostream>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <windows.h>

#include "libs/jni/jni.h"
#include "myDefinedClass.h"

#include "reclass/Method.h" //has hard-coded offsets, wont work for other jvms


#include "get_offsets.h" //can get offsets from the jvm, but not all, not jnienv or adapters

int c2i_adapter_offset = -1;



// needed for safe JNI usage, this function digs into the method's constant pool and edits the sig
//  so that the wrapper's sig matches that of the hooked function
// todo: dont use reclass files, use the jvm's exported offsets
void attempt_copy(void* replacement, void* orig){
    Method* meth1 = (Method*)replacement;
    Method* meth2 = (Method*)orig;

    //old code that works:
    meth1->_constMethod->_max_locals = meth2->_constMethod->_max_locals; //needed
    meth1->_constMethod->_size_of_parameters = meth2->_constMethod->_size_of_parameters; //needed

    Symbol* s = meth2->_constMethod->_constants->symbol_at(meth2->_constMethod->_signature_index);
    meth1->_constMethod->_constants->set_symbol(meth1->_constMethod->_signature_index, s);

    //the gc fix:
    // 1. if the target function is static, the replacement function must also be static, same with being non-static
    // 2. copy sig symbol from target function to replacement function
    // 3. copy fields from target to replacement: _max_locals, _size_of_parameters
}

//bridge distrubution system
std::mutex bridge_mutex; //todo: make it so every bridge has its own mutex
struct wrapper_entry{
    void* wrapper;
    std::string current_sig;
    char ret_type;
    bool is_static;
    volatile int use_count = 0; //0 = available for sig change, 1+ = in use by X threads
};
std::vector<wrapper_entry*> wrapper_entries;

void add_bridge(void* ptr, char return_type, bool is_static){
    wrapper_entry *ent = new wrapper_entry;
    ent->wrapper = ptr;
    ent->ret_type = return_type;
    ent->is_static = is_static;
    wrapper_entries.push_back(ent);
}

void* request_bridge(void* orig_method, std::string sig, char return_type, bool is_static){
    bridge_mutex.lock();
    bool found = false;
    wrapper_entry* result = 0;

    //once a wrapper is found, perform the attempt_copy thing on it before returning its

    for(wrapper_entry *ent : wrapper_entries){
        if(ent->is_static == is_static && ent->ret_type == return_type){
            //same static and return value, meaning its ok to use this wrapper
            if(ent->use_count > 0){//first check the in-use wrappers and see if they can be reused
                if(ent->current_sig == sig){
                    //use this entry
                    found = true;
                    result = ent;
                    break;
                }
            }
        }
    }
    
    if(!found)
    for(wrapper_entry *ent : wrapper_entries){
        if(ent->is_static == is_static && ent->ret_type == return_type){
            if(ent->use_count == 0){ //no in-use wrapper found, just take the first available one
                found = true;
                result = ent;
                break;
            }
        }
    }

    if(!found){
        printf("Error: failed to find an available bridge\n");
        Sleep(30000); //assume that a crash will take place after, so provide time for error message to be seen
    }else{
        result->use_count++;
        result->current_sig = sig;
    }
    attempt_copy(result->wrapper, orig_method);
    bridge_mutex.unlock();
    return result->wrapper;
}
void release_bridge(void* wrapper_instance){
    bridge_mutex.lock();
    for(wrapper_entry *ent : wrapper_entries){
        if(ent->wrapper == wrapper_instance){
            ent->use_count--;
            break;
        }
    }
    bridge_mutex.unlock();
}



jobject oop_to_jobject(JNIEnv *env, uint64_t oop) {
    if (oop == 0) { 
        printf("Error: tried converting a NULL oop to a jobject\n");
        return NULL;
        Sleep(30000);//assume a crash will take place, so sleep to give the user time to see the message
    }
    // HotSpot specific trick (assuming oop is a valid pointer to an object)
    return env->NewLocalRef((jobject)&oop);
}



//basic:    no reentry,  unsafe jni, can cancel
//advanced: has reentry, safe jni,   must cancel, performance overhead of around 10% slower than basic hook
enum HOOK_TYPE{BASIC_JAVA_HOOK=1, ADVANCED_JAVA_HOOK=2};

std::mutex rebuild_flag_lock;
std::mutex reentry_flag_lock;
//TODO: make it so every hooked_func has its own mutex
struct hooked_func{ //cant hold the mutexes inside for some reason
    HOOK_TYPE hook_type;

    std::string class_name = "";
    std::string method_name = "";
    std::string method_sig = "";
    std::string adjusted_sig = "";
    int adjusted_sig_len = 0;
    bool is_method_static = false;

    void* method_ptr = nullptr;//need to filter calls, check rehooking
    void* externalDetour = nullptr;//detour the user provides
    void* i2i_saved = nullptr;
    void* interpreted_saved = nullptr;

    bool inited = false;

    volatile int exit_counter = 0;

    //tail injection support:
    //when doing tail injection, the hook must be disabled temporarily, then the original method should be called with jni, place the hook back,
    //  and have the detour cancel the call. This isnt thread safe, and its possible some calls will go past the detour.
    //Solution: before calling the orig method, set a flag "on the next call, if the thread is the same, do nothing and let the interpreter run"
    //Since multiple threads may be doing this at once, the flags must be in a list, and the list should be thread safe
    //usage: when rebuilding, push the thread id into rebuild_flags. 
    // std::vector<std::thread::id> rebuild_flags; //holds list of thread ids of callers
    //if(current_thread_id == rebuild_flags[i]) { remove rebuild_flags[i], send call to interpreter}
    int *rebuild_flags;
    int rebuild_flags_size = 100;
    //rebuild and re-entry flags must be pre-alloced, this is due to mem allocs in the detour causing fake overflow crashes in some cases

    void prepare_rebuild(){
        int id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        bool found = false;
        rebuild_flag_lock.lock();
            // rebuild_flags.push_back(std::this_thread::get_id());
            for (int i = 0; i < rebuild_flags_size; i++){
                if(rebuild_flags[i] == -1){ // -1 = empty
                    rebuild_flags[i] = id;
                    found = true;
                    exit_counter++;
                    break;
                }
            }
        if(!found){
            printf("ERROR: OUT OF SPACE FOR REBUILD\n");
            Sleep(30000);//assume a crash will take place, so sleep to give the user time to see the message
        }
        rebuild_flag_lock.unlock();
    }
    bool should_rebuild(){ //mutex placement could be better
        int this_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        bool result = false;
        rebuild_flag_lock.lock();
            for(int i = 0; i < rebuild_flags_size; i++){
                if(this_id == rebuild_flags[i]){
                    // rebuild_flags.erase(rebuild_flags.begin() + i);
                    rebuild_flags[i] = -1;
                    result = true;
                    exit_counter--;
                    break;
                }
            }
        rebuild_flag_lock.unlock();
        return result;
    }


    // std::vector<std::pair<std::thread::id, void*>> reentry_flags; //holds list of thread ids of callers
    // cant use vectors due to dynamic mem allocation
    int *reentry_flags1;//thread ids
    void* *reentry_flags2;//contexts
    int reentry_flags_size = 100;
    void prepare_reentry(void* ctx){
        int id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        bool found = false;
        reentry_flag_lock.lock();
            // reentry_flags.push_back({std::this_thread::get_id(), ctx}); //so the {} makes an std::pair...
            for (int i = 0; i < reentry_flags_size; i++){
                if(reentry_flags1[i] == -1){ // -1 = empty
                    reentry_flags1[i] = id;
                    reentry_flags2[i] = ctx;
                    found = true;
                    exit_counter++;
                    break;
                }
            }
        if(!found){
            printf("ERROR: OUT OF SPACE FOR RE-ENTRY\n");
            Sleep(30000);//assume a crash will take place, so sleep to give the user time to see the message
        }
        reentry_flag_lock.unlock();
    }
    void* should_reenter(){ //returns an instance of hook_context
        // std::thread::id this_id = std::this_thread::get_id();
        int this_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        void* result = 0;
        reentry_flag_lock.lock();
            for(int i = 0; i < reentry_flags_size; i++){
                if(this_id == reentry_flags1[i]){
                    // rebuild_flags.erase(rebuild_flags.begin() + i);
                    result = reentry_flags2[i];
                    reentry_flags1[i] = -1;
                    reentry_flags2[i] = (void*)-1;
                    exit_counter--;
                    break;
                }
            }
        reentry_flag_lock.unlock();
        return result;
    }

    void init(){
        if (inited){
            //error
            return;
        }
        //alloc mem
        rebuild_flags = new int[rebuild_flags_size];
        reentry_flags1 = new int[reentry_flags_size];
        reentry_flags2 = new void*[reentry_flags_size];
        for(int i = 0; i < reentry_flags_size; i++){
            reentry_flags1[i] = -1;
            reentry_flags2[i] = (void*)-1;
        }
        for(int i = 0; i < rebuild_flags_size; i++){
            rebuild_flags[i] = -1;
        }
        
        inited = true;
    }

    //need to make a destructor to free mem
    void destroy(){
        while(exit_counter > 0){
            Sleep(1);
        }

        delete rebuild_flags;
        delete reentry_flags1;
        delete reentry_flags2;
    }
};

std::vector<hooked_func> hooks;






std::string make_adjusted_sig(const std::string sig, const bool is_method_static){
    std::string adjusted_sig = "";
    if(!is_method_static){
        adjusted_sig = "O"; //Lthis;
    }
    bool skip = false;
    for(int i = 1; i < sig.length(); i++){ //start at i=1 to skip first char which is '('
        char c = sig[i];
        if(c == ')'){ //end of sig
            break;
        }
        if(c == 'L'){ //start of java object signature
            skip = true;
            continue;
        }
        if(c == ';'){ //end of java object signature
            skip = false;
            adjusted_sig += "O";
            continue;
        }
        if(c == '['){ //"array of", an oop
            adjusted_sig += c;
            //dont add the next char, also enable skip if the next char is L
            i++; //skip next char
            if(sig[i] == 'L') //if next char is L, enable skipping
                skip = true;
            continue;
        }
        if(c == 'D' || c == 'J'){
            adjusted_sig += "_";
            adjusted_sig += c;
            continue;
        }
        if(!skip)
            adjusted_sig += c;
    }
    std::string reversed;
    reversed.reserve(adjusted_sig.length());
    for(int i = adjusted_sig.length() - 1; i >= 0; i--){
        reversed += adjusted_sig[i];
    }
    return reversed;
}


enum REGS{ RBX, RAX, RCX, RDX, RBP, RSI, RDI, R15, R14, R13, R12, R11, R10, R9, R8};

//this is passed into the user-provided detour
struct hook_context{
    uint64_t *regs;
    bool cancel = false;//set to true to cancel function call
    // int ret_type = 0;
    uint64_t ret_val = 0;
    hooked_func *hf;
    JNIEnv *env = NULL;
    void* bridge_wrapper_used = 0;


    uint64_t get_register(REGS reg){
        return regs[reg];
    }
    void set_register(REGS reg, uint64_t val){
        regs[reg] = val;
        return;
    }

    //returns arguments in the order of:     this_oop, arg0, arg1, ...    OR    arg0, arg1, ...
    //result must be cast to target variable type
    uint64_t get_arg(int pos){
        //ISSUE: doubles have garbage padding, meaning type of args passed in affect positions
        //Entity.moveEntity(DDD) = (obj,D1,D2,D3) = D3, junk, D2, junk, D1, junk, obj
        
        for(int i = hf->adjusted_sig_len; i >= 0; i--){
            if(hf->adjusted_sig[i] == '_')
                continue;
            if(pos == 0){
                return regs[14+2 + i];//14 = the regs pushed by asm, 2 = other stuff like return addresses or something
            }
            pos--;
        }
        printf("getArg failed, requested arg out of range\n");
        return 0;
    }

    int get_arg_int(const int pos){
        return (uint32_t)get_arg(pos);
    }
    float get_arg_float(const int pos){
        uint64_t temp = get_arg(pos);
        return *(float*)&temp;
    }
    double get_arg_double(const int pos){
        uint64_t temp = get_arg(pos);
        return *(double*)&temp;
    }
    jobject get_arg_jobject(const int pos){
        if(env == NULL){ //might be null, for example when doing a non-jni head injection
            printf("Error: JNI cannot be used in this context, returning NULL\n");
            return NULL;
        }
        uint64_t oop = get_arg(pos);
        return oop_to_jobject(env, oop);
    }
    uint64_t get_arg_oop(const int pos){
        return get_arg(pos);
    }
    bool get_arg_bool(const int pos){
        return (bool)get_arg(pos);
    }
    long get_arg_long(const int pos){
        uint64_t temp = get_arg(pos);
        return *(long*)&temp;
    }

    jchar get_arg_char(int pos){ //NOT TESTED
        return (jchar)get_arg(pos);
    }
    jbyte get_arg_byte(int pos){//NOT TESTED
        return (jbyte)get_arg(pos);
    }
    jshort get_arg_short(int pos){//NOT TESTED
        return (jshort)get_arg(pos);
    }

    //working with [ arrays not implemented or tested

    void set_arg(int pos, uint64_t val){
        for(int i = hf->adjusted_sig_len; i >= 0; i--){
            if(hf->adjusted_sig[i] == '_')
                continue;
            if(pos == 0){
                regs[14+2 + i] = val;
                return;
            }
            pos--;
        }

        printf("setArg failed, requested arg out of range\n");
    }
    void set_arg_int(int pos, int val){ //works
        set_arg(pos, val);
    }
    void set_arg_float(int pos, float val){
        uint64_t temp = *(uint64_t*)&val;
        set_arg(pos, temp);
    }
    void set_arg_double(int pos, double val){
        uint64_t temp = *(uint64_t*)&val;
        set_arg(pos, temp);
    }
    void set_arg_jobject(int pos, jobject obj){
        jobject* temp1 = (*(jobject**)obj);
        uint64_t oop = *(uint64_t*)&temp1;
        set_arg(pos, oop);
    }
    void set_arg_oop(int pos, uint64_t oop){
        set_arg(pos, oop);
    }
    void set_arg_bool(int pos, bool val){ //works
        set_arg(pos, val);
    }
    void set_arg_long(int pos, long val){
        uint64_t temp = *(uint64_t*)&val;
        set_arg(pos, temp);
    }
    //TODO: set arg jchar, jbyte, short NOT DONE

    
    JNIEnv* get_JNIEnv(){
        if(env == NULL){ //might be null, for example when doing a non-jni head injection
            printf("Error: JNI cannot be used in this context, returning NULL\n");
            Sleep(30000);//assume a crash will take place, so sleep to give the user time to see the message
        }
        return env;
    }

    //set a flag saying you plan on calling the original java function, and want the next call from this thread to go past the detour
    void prepare_rebuild(){
        hf->prepare_rebuild();
    }

    //when overriding, need to have a value to return
    //NOT TESTED
    void set_return_float(float f){
        float f2 = f;
        ret_val = *(float*)&f2;
    }
    void set_return_double(double d){
        double d2 = d;
        ret_val = *(double*)&d2;
    }

    ~hook_context(){
        if(bridge_wrapper_used != 0){
            release_bridge(bridge_wrapper_used);
        }
    }
};









JNIEXPORT void JNICALL Java_myDefinedClass_nativeBridgeVoid(JNIEnv *env, jclass cls) {
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            delete ctx;

            // void_reentry_lock.unlock();

            //ok to change sigs here
        //     jclass cls1 = env->FindClass("myDefinedClass");
        //     jmethodID testfunc = env->GetStaticMethodID(cls1, "dummy_func", "()V");
        //     Method* pt = *(Method**)testfunc;
        //     attempt_copy(nativeWrapperVoid_notStatic, pt);
		// jclass syscls = env->FindClass("java/lang/System");
		// jmethodID systemGCMethod = env->GetStaticMethodID(syscls, "gc", "()V");
		// env->CallStaticVoidMethod(syscls, systemGCMethod);

            return;
        }
    }
    printf("ERROR: A void re-entry took place and failed to find where to go\n");
}

JNIEXPORT jboolean JNICALL Java_myDefinedClass_nativeBridgeBool(JNIEnv *env, jclass cls){
    bool result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = ctx->ret_val;
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jbool re-entry took place and failed to find where to go\n");
    return result;
}

JNIEXPORT jbyte JNICALL Java_myDefinedClass_nativeBridgeByte(JNIEnv *env, jclass cls){
    jbyte result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = ctx->ret_val;
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jbyte re-entry took place and failed to find where to go\n");
    return result;
}

JNIEXPORT jchar JNICALL Java_myDefinedClass_nativeBridgeChar(JNIEnv *env, jclass cls){
    jchar result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = ctx->ret_val;
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jchar re-entry took place and failed to find where to go\n");
    return result;
}

JNIEXPORT jshort JNICALL Java_myDefinedClass_nativeBridgeShort(JNIEnv *env, jclass cls){
    jshort result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = ctx->ret_val;
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jshort re-entry took place and failed to find where to go\n");
    return result;
}

JNIEXPORT jint JNICALL Java_myDefinedClass_nativeBridgeInt(JNIEnv *env, jclass cls){
    int result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = ctx->ret_val;
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jint re-entry took place and failed to find where to go\n");
    return result;
}

JNIEXPORT jlong JNICALL Java_myDefinedClass_nativeBridgeLong(JNIEnv *env, jclass cls){
    long result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = ctx->ret_val;
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jlong re-entry took place and failed to find where to go\n");
    return result;
}

JNIEXPORT jfloat JNICALL Java_myDefinedClass_nativeBridgeFloat(JNIEnv *env, jclass cls){
    float result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = *(uint64_t*)&(ctx->ret_val); //NOTE: THIS TYPE OF OVERRIDING HASNT BEEN TESTED YET
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jfloat re-entry took place and failed to find where to go\n");
    return result;
}

JNIEXPORT jdouble JNICALL Java_myDefinedClass_nativeBridgeDouble(JNIEnv *env, jclass cls){
    double result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = *(uint64_t*)&(ctx->ret_val); //NOTE: THIS TYPE OF OVERRIDING HASNT BEEN TESTED YET
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jdouble re-entry took place and failed to find where to go\n");
    return result;
}

JNIEXPORT jobject JNICALL Java_myDefinedClass_nativeBridgeObject(JNIEnv *env, jclass cls){
    jobject result = 0;
    for(hooked_func &hook : hooks){
        hook_context* ctx = (hook_context*)hook.should_reenter();
        if(ctx != 0){
            ctx->env = env;
            void (*funcPtr)(hook_context*);
            funcPtr = (void (*)(hook_context*))(hook.externalDetour);
            funcPtr(ctx);
            result = (jobject)ctx->ret_val;
            delete ctx;
            return result;
        }
    }
    printf("ERROR: A jobject re-entry took place and failed to find where to go\n");
    return result;
}




void* best_effort_int_entry = 0;
extern "C"{
    int __fastcall elevated_i2i_detour(uint64_t* regs){

        uint64_t rbx_reg = regs[RBX];
        void* orig = 0;

        hook_context *ctx;

        for(hooked_func &hook : hooks){
            if((uint64_t)(hook.method_ptr) == rbx_reg){
                best_effort_int_entry = hook.i2i_saved;

                if(hook.hook_type == BASIC_JAVA_HOOK){
                    
                    //make the hook context HERE
                    //call function based on its pointer
                    //get the ret val and ret type
                    //delete the hook context
                    //ret type ==  cancel=0  or  dont_cancel=1
                    //ret val needs to be placed in rax
                    //  for returning doubles/floats, the ret val must be placed in xmm0

                    ctx = new hook_context();
                    ctx->hf = &hook;
                    ctx->regs = regs;

                    void (*funcPtr)(hook_context*);
                    funcPtr = (void (*)(hook_context*))(hook.externalDetour);
                    funcPtr(ctx);

                    uint64_t ret_val = ctx->ret_val;
                    bool cancel = ctx->cancel; //ret type
                    delete ctx;
                    if(cancel){
                        regs[RAX] = ret_val;
                        //TODO:
                        //figure out a way to return a value through the xmm0 register WITHOUT modifying rax
                        //or is it 100% ok to modify rax in any function call?
                        //how about: return 1; == modify rax ONLY,    return 2; == modify xmm0
                        char ret_type = hook.method_sig[hook.method_sig.length() -1];
                        if(hook.method_sig.find(")[") != std::string::npos){ //an array, aka jobject
                            ret_type = ';';
                        }
                        if(ret_type == 'F' || ret_type == 'D'){//this might be wrong
                            return 2;//modify xmm0 - NOT DONE
                        }

                        return 1; //ret value is in rax
                    }else{
                        regs[RAX] = *(uint64_t*)&(hook.i2i_saved);
                        return 0; //dont override
                    }

                }else{ //ADVANCED HOOK
                    //rebuild/tail injection support
                    if(hook.should_rebuild()){ //found entry
                        regs[RAX] = *(uint64_t*)&(hook.i2i_saved);
                        return 0; //dont override
                    }//else:

                    orig = hook.i2i_saved;

                    ctx = new hook_context();
                    ctx->hf = &hook;
                    ctx->regs = regs;

                    //if the function being overridden returns an array: need to make a case for this, what is returned? a jobject?
                    char ret_type = hook.method_sig[hook.method_sig.length() -1];
                    if(hook.method_sig.find(")[") != std::string::npos){ //an array, aka jobject
                        ret_type = ';';
                    }
                    void* method = request_bridge(hook.method_ptr, hook.method_sig, ret_type, hook.is_method_static);
                    ctx->bridge_wrapper_used = method;
                    regs[RBX] = (uint64_t)method;
                    hook.prepare_reentry((void*)ctx);
                    break; //trick interpreter into running a different function, going into the JNIEXPORT above
                }
                
            }
        }

        if(orig == 0){
            printf("ERROR: detour ran for a non-hooked function\n");
            regs[RAX] = *(uint64_t*)&best_effort_int_entry; //interpreter is usually the same so this should work 99% of the time
        }else{
            regs[RAX] = *(uint64_t*)&orig; //original i2i entry
        }
        return 0; //dont override
    }


    //asm functions:
    void asm_i2i_detour(){
        __asm__ __volatile__ (

            "push %%r8\n"//arg 14
            "push %%r9\n"
            "push %%r10\n"
            "push %%r11\n"
            "push %%r12\n"
            "push %%r13\n"
            "push %%r14\n"
            "push %%r15\n"
            "push %%rdi\n"
            "push %%rsi\n"
            "push %%rbp\n"
            "push %%rdx\n"
            "push %%rcx\n"
            "push %%rax\n"
            "push %%rbx\n"//arg 0

            "movq %%rsp, %%rcx\n"		//rcx is the first argument in a function call, this way we pass in a pointer to all the saved registers
            //we must prepare the arg here because we modify rsp below

            //allign stack for xmm saving
            //make sure stack is alligned before saving xmm
            "mov %%rsp, %%rax\n"
            "mov $8, %%rbx\n"
            "not %%rbx\n"
            "and %%rbx, %%rsp\n"
            "push %%rax\n"
            "sub $8,%%rsp\n"

            //xmm0 to xmm7 preservation
            "sub $16,%%rsp\n"
            "movaps %%xmm0,(%%rsp)\n"
            "sub $16,%%rsp\n"
            "movaps %%xmm1,(%%rsp)\n"
            "sub $16,%%rsp\n"
            "movaps %%xmm2,(%%rsp)\n"
            "sub $16,%%rsp\n"
            "movaps %%xmm3,(%%rsp)\n"
            "sub $16,%%rsp\n"
            "movaps %%xmm4,(%%rsp)\n"
            "sub $16,%%rsp\n"
            "movaps %%xmm5,(%%rsp)\n"
            "sub $16,%%rsp\n"
            "movaps %%xmm6,(%%rsp)\n"
            "sub $16,%%rsp\n"
            "movaps %%xmm7,(%%rsp)\n"


            "call elevated_i2i_detour\n"
            //restore xmm before doing cmp on returned bool, so less asm code is repeated

            //pop back xmm regs
            "movaps (%%rsp),%%xmm7\n"
            "add $16,%%rsp\n"
            "movaps (%%rsp),%%xmm6\n"
            "add $16,%%rsp\n"
            "movaps (%%rsp),%%xmm5\n"
            "add $16,%%rsp\n"
            "movaps (%%rsp),%%xmm4\n"
            "add $16,%%rsp\n"
            "movaps (%%rsp),%%xmm3\n"
            "add $16,%%rsp\n"
            "movaps (%%rsp),%%xmm2\n"
            "add $16,%%rsp\n"
            "movaps (%%rsp),%%xmm1\n"
            "add $16,%%rsp\n"
            "movaps (%%rsp),%%xmm0\n"
            "add $16,%%rsp\n"

            "add $8,%%rsp\n"//reallign stack
            "pop %%rsp\n"
            //more registers exist, like ymm and zmm, dosnt seem like theyr important

/*cmp*/
            //branch based on return type
    		"test %%rax, %%rax\n"//test bool returned
	    	"jnz no_call\n"//jump if not zero, if returned true then cancel the java call
            //order of saving and getting registers could be changed to get rax back at the end, allowing for much less duplicate code
            
            //todo: redo how the cmp is done, check if its 0, 1, or 2
            //0 = call orig - done
            //1 = no call - done
            //2 = no call + mov rax --> xmm0 - not done


/*call orig*/
            "pop %%rbx\n"
            "pop %%rax\n"
            "pop %%rcx\n"
            "pop %%rdx\n"
            "pop %%rbp\n"
            "pop %%rsi\n"
            "pop %%rdi\n"
            "pop %%r15\n"
            "pop %%r14\n"
            "pop %%r13\n"
            "pop %%r12\n"
            "pop %%r11\n"
            "pop %%r10\n"
            "pop %%r9\n"
            "pop %%r8\n"


            "push %%rax\n"
            "ret\n"


/*return to JVM, not interpreter, ret val should be passed through rax*/
    "no_call:\n"
            "pop %%rbx\n"
            "pop %%rax\n"
            "pop %%rcx\n"
            "pop %%rdx\n"
            "pop %%rbp\n"
            "pop %%rsi\n"
            "pop %%rdi\n"
            "pop %%r15\n"
            "pop %%r14\n"
            "pop %%r13\n"
            "pop %%r12\n"
            "pop %%r11\n"
            "pop %%r10\n"
            "pop %%r9\n"
            "pop %%r8\n"

            "pop %%rdi\n" //rdi holds the return address from the stack
            "mov %%r13, %%rsp\n" //restore stack before returning
            "jmp *%%rdi\n" //jump to return address

            
/*return to JVM, not interpreter, ret val should be passed through xmm0*/
    "no_call2:\n"
    //not implemented yet
            
            : : :"memory"
        );
    }
}   //extern C














void init_java_hook(JNIEnv *env){
    static bool java_hook_init = false;
    if(java_hook_init)
        return;
    java_hook_init = true;

    if(c2i_adapter_offset == -1){
        //attempt to calc the offset, call some other func for that, maybe have it be in get_offsets.h
        //on failure set to -2
        c2i_adapter_offset = calc_c2i_offset(env);
    }

    jclass cls1 = env->FindClass("myDefinedClass");

    //new bridge registration:
    std::string sigs[] = {"IIFC", "IICF", "IFIC", "IFCI", "ICIF", "ICFI", "FIIC", "FICI", "FCII", "CIIF"};
    std::string ret_types[] = {"V", "Z", "B", "C", "S", "I", "J", "F", "D", "Ljava/lang/Object;"};
    std::string names[] = {"nativeWrapperVoid", "nativeWrapperBool", "nativeWrapperByte", "nativeWrapperChar", "nativeWrapperShort", "nativeWrapperInt", "nativeWrapperLong", "nativeWrapperFloat", "nativeWrapperDouble", "nativeWrapperObject"};
    
    for(int i = 0; i < 10; i++){ //number of entry point types (DONT CHANGE)

        for(int j = 1; j <= 10; j++){ //number of sigs (can be increased)
            //once j > 5, use static, subtract 5 from j when converting to string
            std::string sig = "("+ sigs[j-1] +")"+ret_types[i];
            std::string name;
            jmethodID meth = 0;
            if(j < 6){
                name = names[i] + std::to_string(j);
                meth = env->GetMethodID(cls1, name.c_str(), sig.c_str());
            }else{
                name = names[i] + std::to_string(j-5) + "S";//static
                meth = env->GetStaticMethodID(cls1, name.c_str(), sig.c_str());
            }
            if(meth != NULL){
                if(j < 6){
                    add_bridge(*(void**)meth, ret_types[i][ret_types[i].length()-1], false);
                }else{
                    add_bridge(*(void**)meth, ret_types[i][ret_types[i].length()-1], true);
                }
            }else{
                printf("ERROR: CANT FIND %s.%s\n", name.c_str(), sig.c_str());
                printf("init_java_hook: There is an issue with the defined class not having the expected entries in the defined class, init failed\n");
                Sleep(30000);//assume a crash will take place, so sleep to give the user time to see the message
            }
        }
    }


    //connect native funcs in java code to the jniexports here
    JNINativeMethod methods[] = { 
        {(char*)"nativeBridgeVoid", (char*)"()V", (void *)&Java_myDefinedClass_nativeBridgeVoid},
        {(char*)"nativeBridgeBool", (char*)"()Z", (void *)&Java_myDefinedClass_nativeBridgeBool},
        {(char*)"nativeBridgeByte", (char*)"()B", (void *)&Java_myDefinedClass_nativeBridgeByte},
        {(char*)"nativeBridgeChar", (char*)"()C", (void *)&Java_myDefinedClass_nativeBridgeChar},
        {(char*)"nativeBridgeShort", (char*)"()S", (void *)&Java_myDefinedClass_nativeBridgeShort},
        {(char*)"nativeBridgeInt", (char*)"()I", (void *)&Java_myDefinedClass_nativeBridgeInt},
        {(char*)"nativeBridgeLong", (char*)"()J", (void *)&Java_myDefinedClass_nativeBridgeLong},
        {(char*)"nativeBridgeFloat", (char*)"()F", (void *)&Java_myDefinedClass_nativeBridgeFloat},
        {(char*)"nativeBridgeDouble", (char*)"()D", (void *)&Java_myDefinedClass_nativeBridgeDouble},
        {(char*)"nativeBridgeObject", (char*)"()Ljava/lang/Object;", (void *)&Java_myDefinedClass_nativeBridgeObject}
    };
    if (env->RegisterNatives(cls1, methods, sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
        printf("ERROR: RegisterNatives failed\n");
        Sleep(30000);//assume a crash will take place, so sleep to give the user time to see the message
    }

    env->DeleteLocalRef(cls1);
}





void prevent_compile(JNIEnv* env, std::string class_name, std::string method_name, std::string method_sig){
    init_java_hook(env);

    jclass klass = env->FindClass(class_name.c_str());
    if(klass == NULL){
        printf("prevent_compile error: jclass is null: %s\n", class_name.c_str());
        return;
    }

    jmethodID method;
	bool isStatic = false;
	method = env->GetMethodID(klass, method_name.c_str(), method_sig.c_str());
	if(method == 0){
		method = env->GetStaticMethodID(klass, method_name.c_str(), method_sig.c_str());
		if(method != 0){  //method has been found
			isStatic = true;
		}else{  //error out
            printf("prevent_compile error: method not found: %s.%s\n", class_name.c_str(), method_sig.c_str());
            return;
		}
	}//else: method has been found
    unsigned char* method_ptr = *(unsigned char**)method;

    if(method_ptr == NULL){
        printf("prevent_compile error: target method not found\n");
        return;
    }else{
        uint64_t access_flags_val = (uint64_t)get_field(method_ptr, "Method", "_access_flags");
        set_field(method_ptr, "Method", "_access_flags", (access_flags_val |(0x01000000 | 0x02000000 | 0x04000000 | 0x08000000)));
        //also need to have comp entry point to c2i
        if(c2i_adapter_offset != -1 && c2i_adapter_offset != -2){ //redirect comp_entry to c2i
            printf("attempting to set comp entry to c2i\n");
			unsigned char* adapter = get_field(method_ptr, "Method", "_adapter");
            unsigned char* c2i = *(unsigned char**)(adapter + c2i_adapter_offset);
            set_field(method_ptr, "Method", "_from_compiled_entry", (uint64_t)c2i);
        }else{
            printf("placing unreliable hook, no c2i adapter offset found, some calls might get past the hook\n");
        }

        set_field(method_ptr, "Method", "_code", 0); //uncompile, should be done last

    }
    env->DeleteLocalRef(klass);
}




void create_hook(JNIEnv* env, std::string class_name, std::string method_name, std::string method_sig, void* detour, HOOK_TYPE hook_type=ADVANCED_JAVA_HOOK){

    init_java_hook(env);

    for(hooked_func &hf : hooks){
        if(hf.class_name == class_name && hf.method_name == method_name && hf.method_sig == method_sig){
            printf("ERROR: This function is already hooked: %s.%s%s\n", class_name.c_str(), method_name.c_str(), method_sig.c_str());
            return;
        }
    }

    //cant take a direct jmethodID since this needs to know if the method is static or not
    jclass klass = env->FindClass(class_name.c_str());
    if(klass == NULL){
        printf("Hooking error: failed to find class: %s\n", class_name.c_str());
        return;
    }

    jmethodID method;
	bool isStatic = false;
	method = env->GetMethodID(klass, method_name.c_str(), method_sig.c_str());
	if(method == 0){
		method = env->GetStaticMethodID(klass, method_name.c_str(), method_sig.c_str());
		if(method != 0){
			isStatic = true;
			//method has been found
		}else{
            printf("Hooking error: method not found: %s.%s\n", class_name.c_str(), method_sig.c_str());
            return;
		}
	}//else: method has been found
    
    unsigned char* method_ptr = *(unsigned char**)method;
    printf("Method found: %p\n", method_ptr);

    hooked_func hf;
    hf.init();
    hf.class_name = class_name;
    hf.method_name = method_name;
    hf.method_sig = method_sig;
    hf.adjusted_sig = make_adjusted_sig(method_sig, isStatic);
    hf.adjusted_sig_len = hf.adjusted_sig.length() -1;
    hf.externalDetour = detour;
    hf.method_ptr = method_ptr;
    hf.i2i_saved = get_field(method_ptr, "Method", "_i2i_entry"); //when accessing a field's value, need to depointer
    hf.interpreted_saved = get_field(method_ptr, "Method", "_from_interpreted_entry");// need to do *(void**) when working with pointers
    hf.is_method_static = isStatic;
    hf.hook_type = hook_type;

    hooks.push_back(hf); //must be reach to process the hook before actually placing it
    
    set_field(method_ptr, "Method", "_i2i_entry", (uint64_t)&asm_i2i_detour);
    set_field(method_ptr, "Method", "_from_interpreted_entry", (uint64_t)&asm_i2i_detour);

    //uncompile:
    prevent_compile(env, class_name, method_name, method_sig);

    //cant get offset of adapter fields
    //AdapterHandlerEntry fields arent exported, somehow this still works without setting comp_entry to c2i,
    //my guess is the JVM sees comp_entry is pointing to some garbage, and decides to go to c2i manually, but is this the case for all JVMs?
    //would be much better to somehow get c2i rather than hope for the best here.

    //todo: calcing adapter c2i entry should be possible if i can do jni->DefineClass, need to do it

    env->DeleteLocalRef(klass);
}








void unhook(std::string class_name, std::string method_name, std::string method_sig){
    for(int i = 0; i < hooks.size(); i++){
        if(hooks[i].class_name == class_name && hooks[i].method_name == method_name && hooks[i].method_sig == method_sig){
            set_field((unsigned char*)hooks[i].method_ptr, "Method", "_i2i_entry", (uint64_t)hooks[i].i2i_saved);
            set_field((unsigned char*)hooks[i].method_ptr, "Method", "_from_interpreted_entry", (uint64_t)hooks[i].interpreted_saved);
            Sleep(500); //in case the jvm is right about to call and hasnt been updated yet
            hooks[i].destroy(); //assumes no more calls can ever come in, will wait for all running detours to finish
            hooks.erase(hooks.begin() + i);
            break;
        }
    }
}


void unhook_ALL(){
    if(hooks.size() == 0){
        printf("unhook_ALL: No hooks exist, nothing to unhook\n");
    }else{
        for(hooked_func &hf : hooks){
            set_field((unsigned char*)hf.method_ptr, "Method", "_i2i_entry", (uint64_t)hf.i2i_saved);
            set_field((unsigned char*)hf.method_ptr, "Method", "_from_interpreted_entry", (uint64_t)hf.interpreted_saved);
            Sleep(500); //in case the jvm is right about to call and hasnt been updated yet
            hf.destroy(); //assumes no more calls can ever come in
        }
    }
    hooks.clear();
}


void shutdown_hooking(JNIEnv *env){
    unhook_ALL();
    Sleep(1000);
    jclass c = env->FindClass("myDefinedClass");
    env->UnregisterNatives(c);
    env->DeleteLocalRef(c);
}
