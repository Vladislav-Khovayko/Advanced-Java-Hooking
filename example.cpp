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
#include "libs/jni/jni.h"

#include "jhook.cpp"//all hooking code

#include <chrono>



// from stackoverflow
std::string jstring2string(JNIEnv *env, jstring jStr){
    const char *cstr = env->GetStringUTFChars(jStr, NULL);
    std::string str = std::string(cstr);
    env->ReleaseStringUTFChars(jStr, cstr);//cstr or c_str()
    return str;
}







void nametags(hook_context *ctx){
	static int call_counter = 0;
    call_counter++;
	JNIEnv *env = ctx->env;
    if(call_counter % 100 == 0){
        // printf("hook ran %d times\n", call_counter);
    }

//Render.renderLivingLabel: - edit nametags
	// create_hook(env, "biv", "a", "(Lpk;Ljava/lang/String;DDDI)V", (void*)myDetour);

	jobject a1 = ctx->get_arg_jobject(0);
	jobject a2 = ctx->get_arg_jobject(1);
	jstring msg = (jstring)ctx->get_arg_jobject(2); //0 = this, 1 = some entity, 2 = jstring
	double a4 = ctx->get_arg_double(3);
	double a5 = ctx->get_arg_double(4);
	double a6 = ctx->get_arg_double(5);
	int a7 = ctx->get_arg_int(6);
	std::string normal = jstring2string(env, msg);
	jstring jstrBuf = env->NewStringUTF(std::string(normal + " §4§lHELP I CANT FIND A JOB").c_str());
	// jstring jstrBuf = env->NewStringUTF("§4§lreplacing this string");
	// ctx->setArgJObject(2, jstrBuf);
	jclass owner = env->FindClass("biv");
	jmethodID meth = env->GetMethodID(owner, "a", "(Lpk;Ljava/lang/String;DDDI)V");
	ctx->prepare_rebuild();
	env->CallVoidMethod(a1, meth, a2, jstrBuf, a4, a5 , a6, a7);
	
	return;
}





//start of main entry point 

void thread(HMODULE hModule)
{
	AllocConsole();
	FILE* fStream;
	freopen_s(&fStream, "conin$", "r", stdin);
	freopen_s(&fStream, "conout$", "w", stdout);

	JavaVM* vm;
	JNIEnv* env;

    //create env
    jsize vmCount;
    if (JNI_GetCreatedJavaVMs(&vm, 1, &vmCount) != JNI_OK || vmCount == 0) {
        return;
    }
    jint res = vm->GetEnv((void**)&env, JNI_VERSION_1_8);
    if (res == JNI_EDETACHED) {
        res = vm->AttachCurrentThread((void**)&env, nullptr);
    }
    if (res != JNI_OK) {
        return;
    }



//nametags editing
	//Render.java 
	//protected void renderLivingLabel(Entity p_147906_1_, String p_147906_2_, double p_147906_3_, double p_147906_5_, double p_147906_7_, int p_147906_9_)
	//renderLivingLabel
	create_hook(env, "biv", "a", "(Lpk;Ljava/lang/String;DDDI)V", (void*)nametags);



	// while(true){
    while((GetAsyncKeyState(VK_END) & 0x8000) == 0){
     	Sleep(10);
    }

   	printf("exiting\n");

	// unhook_ALL();
	shutdown_hooking(env);

// exit:

	Sleep(1000);
	vm->DetachCurrentThread();
	printf("hooking has been shut down\n");

	FreeConsole();
	FreeLibraryAndExitThread(hModule, 0);
}




BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		auto handle = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)thread, hModule, 0, nullptr);
		if (handle != NULL)
			CloseHandle(handle);
	}
    return TRUE;
}

