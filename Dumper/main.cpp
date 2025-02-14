#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include <Windows.h>
#include <iostream>
#include <chrono>
#include <fstream>

#include "CppGenerator.h"
#include "MappingGenerator.h"
#include "IDAMappingGenerator.h"
#include "DumpspaceGenerator.h"

#include "StructManager.h"
#include "EnumManager.h"

#include "Generator.h"


#include "UnicodeNames.h"
BOOL StartUEDump(std::string DumpLocation, HANDLE hModule);

/*
This file (Plugin.cpp) is licensed under the MIT license and is separate from the rest of the UEVR codebase.

Copyright (c) 2023 praydog

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include <sstream>
#include <mutex>
#include <memory>
#include <locale>
#include <codecvt>
#include <iostream>
#include <fstream>

#include <Windows.h>

// only really necessary if you want to render to the screen

#include "uevr/Plugin.hpp"
#define MAX_PATH_SIZE 512
#define TOUCH_PROXIMITY 0.15f

typedef struct _HAPTIC_TIMER_STRUCT
{
	const UEVR_VRData* vr;
	int MillisecondsDelay;
}   HAPTIC_TIMER_STRUCT;


using namespace uevr;

#define PLUGIN_LOG_ONCE(...) \
    static bool _logged_ = false; \
    if (!_logged_) { \
        _logged_ = true; \
        API::get()->log_info(__VA_ARGS__); \
    }



class DumpPlugin : public uevr::Plugin {
public:
	API::UGameEngine* m_UEngine;
	HAPTIC_TIMER_STRUCT m_Timer;
	const UEVR_PluginInitializeParam* m_Param;
	const UEVR_VRData* m_VR;
	int m_DumpCount;
	std::string m_PersistentDir;
	std::string m_DumperDllLocation;
	HANDLE mHandle;

	DumpPlugin() = default;


	void on_dllmain(HANDLE handle) override {
		m_DumpCount = 0;
		m_UEngine = NULL;
		mHandle = handle;
	}

	void on_initialize() override {
		wchar_t PersistentDir[MAX_PATH_SIZE];
		ZeroMemory(PersistentDir, MAX_PATH_SIZE * sizeof(wchar_t));
		std::wstring WSDir;
		m_Param = API::get()->param();
		m_VR = m_Param->vr;


		API::get()->param()->functions->get_persistent_dir(PersistentDir, MAX_PATH_SIZE);

		WSDir = PersistentDir;

		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		m_PersistentDir = converter.to_bytes(WSDir);
		m_DumperDllLocation = m_PersistentDir + std::string("\\Dumper7SDK");

		API::get()->log_info("dump.dll: dumper7.dll location: %s", m_DumperDllLocation.c_str());

		ZeroMemory(&m_Timer, sizeof(HAPTIC_TIMER_STRUCT));
		m_Timer.vr = API::get()->param()->vr;
		m_Timer.MillisecondsDelay = 0;
	}




	void on_present(){
	    	static bool Dumped = false;


		if (Dumped == true) return;

    		if (GetAsyncKeyState(VK_F8) & 1)
		{
		    Dumped = true;
				API::get()->log_info("dump.dll: dumping values");
				print_all_objects();
				API::get()->log_info("dump.dll: Loading dumper7.dll");
				StartUEDump(m_DumperDllLocation, mHandle);
			}
		}



        

	//*******************************************************************************************
	// This is the controller input routine. Everything happens here.
	//*******************************************************************************************
	void on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state) {
		static bool Dumped = false;


		if (Dumped == true) return;

		if (state != NULL) {
			if (state->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB &&
				state->Gamepad.bLeftTrigger >= 200 &&
				state->Gamepad.bRightTrigger >= 200) {
				Dumped = true;

				UEVR_InputSourceHandle LeftController = m_VR->get_left_joystick_source();
				UEVR_InputSourceHandle RightController = m_VR->get_right_joystick_source();

				API::get()->log_info("dump.dll: dumping values");
				m_VR->trigger_haptic_vibration(0.0f, 0.05f, 1.0f, 1.0f, LeftController);
				m_VR->trigger_haptic_vibration(0.0f, 0.05f, 1.0f, 1.0f, RightController);
				print_all_objects();
				m_VR->trigger_haptic_vibration(0.0f, 0.05f, 1.0f, 1.0f, LeftController);
				m_VR->trigger_haptic_vibration(0.0f, 0.05f, 1.0f, 1.0f, RightController);
				API::get()->log_info("dump.dll: Loading dumper7.dll");
				StartUEDump(m_DumperDllLocation, mHandle);
			}
		}
	}

	void print_all_objects() {
		m_DumpCount++;
		std::string File = m_PersistentDir + "\\object_dump_" + std::to_string(m_DumpCount) + ".txt";
		API::get()->log_info("Printing all objects to %s", File.c_str());

		std::ofstream file(File);
		if (file.is_open()) {
			file << "Chunked: " << API::FUObjectArray::is_chunked() << "\n";
			file << "Inlined: " << API::FUObjectArray::is_inlined() << "\n";
			file << "Objects offset: " << API::FUObjectArray::get_objects_offset() << "\n";
			file << "Item distance: " << API::FUObjectArray::get_item_distance() << "\n";
			file << "Object count: " << API::FUObjectArray::get()->get_object_count() << "\n";
			file << "------------\n";

			const auto objects = API::FUObjectArray::get();

			if (objects == nullptr) {
				file << "Failed to get FUObjectArray\n";
				file.close();
				return;
			}

			std::string Categories[2] = { "Class", "Function" };
			for (int32_t i = 0; i < objects->get_object_count(); ++i) {
				const auto object = objects->get_object(i);

				if (object == nullptr) {
					continue;
				}

				const auto name = object->get_full_name();

				if (name.empty()) {
					continue;
				}

				std::string name_narrow{ std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(name) };

				file << i << " " << name_narrow << "\n";
			}

			file.close();
		}
	}


};

// Actually creates the plugin. Very important that this global is created.
// The fact that it's using std::unique_ptr is not important, as long as the constructor is called in some way.
std::unique_ptr<DumpPlugin> g_plugin{ new DumpPlugin() };


enum class EFortToastType : uint8
{
        Default                        = 0,
        Subdued                        = 1,
        Impactful                      = 2,
        EFortToastType_MAX             = 3,
};

DWORD MainThread(HMODULE Module)
{
	AllocConsole();
	FILE* Dummy;
	freopen_s(&Dummy, "CONOUT$", "w", stdout);
	freopen_s(&Dummy, "CONIN$", "r", stdin);

	auto t_1 = std::chrono::high_resolution_clock::now();

	std::cout << "Started Generation [Dumper-7]!\n";

	Generator::InitEngineCore();
	Generator::InitInternal();

	if (Settings::Generator::GameName.empty() && Settings::Generator::GameVersion.empty())
	{
		// Only Possible in Main()
		FString Name;
		FString Version;
		UEClass Kismet = ObjectArray::FindClassFast("KismetSystemLibrary");
		UEFunction GetGameName = Kismet.GetFunction("KismetSystemLibrary", "GetGameName");
		UEFunction GetEngineVersion = Kismet.GetFunction("KismetSystemLibrary", "GetEngineVersion");

		Kismet.ProcessEvent(GetGameName, &Name);
		Kismet.ProcessEvent(GetEngineVersion, &Version);

		Settings::Generator::GameName = Name.ToString();
		Settings::Generator::GameVersion = Version.ToString();
	}

	std::cout << "GameName: " << Settings::Generator::GameName << "\n";
	std::cout << "GameVersion: " << Settings::Generator::GameVersion << "\n\n";

	Generator::Generate<CppGenerator>();
	Generator::Generate<MappingGenerator>();
	Generator::Generate<IDAMappingGenerator>();
	Generator::Generate<DumpspaceGenerator>();


	auto t_C = std::chrono::high_resolution_clock::now();

	auto ms_int_ = std::chrono::duration_cast<std::chrono::milliseconds>(t_C - t_1);
	std::chrono::duration<double, std::milli> ms_double_ = t_C - t_1;

	std::cout << "\n\nGenerating SDK took (" << ms_double_.count() << "ms)\n\n\n";

#if 0
	while (true)
	{
		if (GetAsyncKeyState(VK_F6) & 1)
		{
			fclose(stdout);
			if (Dummy) fclose(Dummy);
			FreeConsole();

			FreeLibraryAndExitThread(Module, 0);
		}

		Sleep(100);
	}
#endif
    fclose(stdout);
    if (Dummy) fclose(Dummy);
    FreeConsole();
	ExitThread(0);
    //FreeLibraryAndExitThread(Module, 0);

	return 0;
}

BOOL StartUEDump(std::string DumpLocation, HANDLE hModule)
{
	Generator::SDKFolder = DumpLocation;
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);

	return TRUE;
}