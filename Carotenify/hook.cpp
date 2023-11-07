#include <filesystem>
#include <iostream>
#include <fstream>
#include <locale>
#include <string>
#include <thread>
#include <Windows.h>
#include <MinHook.h>
#include <map>
#include <sstream>
#include <set>
#include <math.h>

#include "hook.hpp"
#include "sha256.h"

using namespace std::literals;

namespace
{
	il2cpp_domain_get_t il2cpp_domain_get;
	il2cpp_domain_assembly_open_t il2cpp_domain_assembly_open;
	il2cpp_assembly_get_image_t il2cpp_assembly_get_image;
	il2cpp_class_from_name_t il2cpp_class_from_name;
	il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name;
	il2cpp_class_get_nested_types_t il2cpp_class_get_nested_types;
	il2cpp_class_get_field_from_name_t il2cpp_class_get_field_from_name;
	il2cpp_class_enum_basetype_t il2cpp_class_enum_basetype;
	il2cpp_class_get_methods_t il2cpp_class_get_methods;
	il2cpp_string_new_t il2cpp_string_new;
	il2cpp_method_get_param_count_t il2cpp_method_get_param_count;
	il2cpp_type_get_name_t il2cpp_type_get_name;
	il2cpp_method_get_param_t il2cpp_method_get_param;



	std::map<int, std::string> text_id_to_string;
	std::map<std::string, std::string> text_id_string_to_translation;
	bool tl_first_check = true;
	std::filesystem::file_time_type tl_last_modified;
	std::set<Il2CppString*> stringid_pointers;
	bool debug_mode = false;

	uintptr_t find_class_method_with_name_and_types(void* class_ptr, const char* method_name, const Il2CppTypeEnum method_types[])
	{
		uintptr_t return_ptr = 0;

		void* iter = nullptr;
		MethodInfo* method = nullptr;
		while ((method = il2cpp_class_get_methods(class_ptr, &iter)) != nullptr)
		{
			// Check if method name matches
			const char* compare_method_name = method->name;
			if (strcmp(method_name, compare_method_name) != 0)
			{
				continue;
			}

			// Check if method types matches
			uint32_t param_count = il2cpp_method_get_param_count(method);
			// Quick check if param count matches
			if (param_count != sizeof(method_types) / sizeof(method_types[0]))
			{
				continue;
			}

			bool match = true;
			for (uint32_t i = 0; i < param_count; i++)
			{
				auto param = il2cpp_method_get_param(method, i);
				auto param_type = param->type;
				auto compare_type = method_types[i];
				if (param_type != compare_type)
				{
					match = false;
					break;
				}
			}

			if (!match)
			{
				continue;
			}

			// Found method
			return_ptr = method->methodPointer;
			break;

		}

		return return_ptr;
	}

	void print_class_methods_with_types(void* class_ptr)
	{
		void* iter = nullptr;
		MethodInfo* method = nullptr;
		while ((method = il2cpp_class_get_methods(class_ptr, &iter)) != nullptr)
		{
			printf("Method: %s\n", method->name);

			uint32_t param_count = il2cpp_method_get_param_count(method);
			printf("Param count: %d\n", param_count);

			for (uint32_t i = 0; i < param_count; i++)
			{
				auto param = il2cpp_method_get_param(method, i);
				auto param_type = param->type;
				// Print as hexadecimal
				printf("Param type: %x\n", param_type);
			}
		}
	}

	std::string remove_all_tags(std::string in_str)
	{
		// Remove all <> tags
		while (true)
		{
			size_t start_pos = in_str.find('<');
			if (start_pos == std::string::npos)
			{
				break;
			}
			size_t end_pos = in_str.find('>', start_pos);
			if (end_pos == std::string::npos)
			{
				break;
			}
			in_str.erase(start_pos, end_pos - start_pos + 1);
		}

		return in_str;
	}

	Il2CppString* (*environment_get_stacktrace)();

	void stacktrace()
	{
		printf("%ls", environment_get_stacktrace()->start_char);
	}

	void* find_nested_class_by_name(void* klass, const char* name)
	{
		void* iter{};
		while (const auto curNestedClass = il2cpp_class_get_nested_types(klass, &iter))
		{
			if (static_cast<bool>(std::string_view(name) ==
								static_cast<Il2CppClassHead*>(curNestedClass)->name))
			{
				return curNestedClass;
			}
		}
		return nullptr;
	}

	bool file_exists(std::string file_path)
	{
		std::ifstream infile(file_path);
		bool file_exists = infile.good();
		infile.close();
		return file_exists;
	}


	// copy-pasted from https://stackoverflow.com/questions/3418231/replace-part-of-a-string-with-another-string
	void replaceAll(std::string& str, const std::string& from, const std::string& to)
	{
		if (from.empty())
			return;
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos)
		{
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
		}
	}

	std::string il2cppstring_to_utf8(std::wstring str)
	{
		std::string result;
		result.resize(str.length() * 4);

		int len = WideCharToMultiByte(CP_UTF8, 0, str.data(), str.length(), result.data(), result.size(), nullptr, nullptr);

		result.resize(len);

		return result;
	}

	std::string il2cppstring_to_jsonstring(std::wstring str)
	{
		auto unicode = il2cppstring_to_utf8(str);
		replaceAll(unicode, "\n", "\\n");
		replaceAll(unicode, "\r", "\\r");
		replaceAll(unicode, "\"", "\\\"");

		return unicode;
	}


	void import_translations()
	{
		std::string file_name = "translations.txt";

		if (!file_exists(file_name))
		{
			printf("No translations.txt found\n");
			return;
		}

		if (tl_first_check)
		{
			tl_first_check = false;
			tl_last_modified = std::filesystem::last_write_time(file_name);
		}
		else
		{
			auto last_modified = std::filesystem::last_write_time(file_name);

			if (last_modified == tl_last_modified)
			{
				printf("No changes to translations.txt\n");
				return;
			}

			tl_last_modified = last_modified;
		}

		printf("Importing translations\n");

		std::ifstream infile(file_name);
		std::string line;
		while (std::getline(infile, line))
		{
			std::istringstream iss(line);

			std::string textidstring;
			std::string translation;
			std::getline(iss, textidstring, '\t');
			std::getline(iss, translation, '\t');

			if (translation.empty() || translation.length() == 0)
			{
				continue;
			}

			// printf("Importing %s: %s\n", textidstring.c_str(), translation.c_str());

			replaceAll(translation, "\\n", "\n");
			replaceAll(translation, "\\r", "\r");
			replaceAll(translation, "\\\"", "\"");

			if (translation.find('{') != std::string::npos)
			{
				translation = "<force>" + translation;
			}

			text_id_string_to_translation[textidstring] = translation;
		}

		printf("Importing translations done\n");
		return;
	}


	void create_debug_console()
	{
		AllocConsole();

		FILE* _;
		// open stdout stream
		freopen_s(&_, "CONOUT$", "w", stdout);
		freopen_s(&_, "CONOUT$", "w", stderr);
		freopen_s(&_, "CONIN$", "r", stdin);

		SetConsoleTitle(L"Umapyoi");

		// set this to avoid turn japanese texts into question mark
		SetConsoleOutputCP(CP_UTF8);
		std::locale::global(std::locale(""));

		const HANDLE handle = CreateFile(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		                                 NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		DWORD mode;
		if (!GetConsoleMode(handle, &mode))
		{
			std::cout << "GetConsoleMode " << GetLastError() << "\n";
		}
		mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (!SetConsoleMode(handle, mode))
		{
			std::cout << "SetConsoleMode " << GetLastError() << "\n";
		}
	}

	std::string current_time()
	{
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch());
		return std::to_string(ms.count());
	}

	void write_file(const std::string& file_name, const char* buffer, const int len)
	{
		FILE* fp;
		fopen_s(&fp, file_name.c_str(), "wb");
		if (fp != nullptr)
		{
			fwrite(buffer, 1, len, fp);
			fclose(fp);
		}
	}


	void* LZ4_decompress_safe_ext_orig = nullptr;

	int LZ4_decompress_safe_ext_hook(
		char* src,
		char* dst,
		int compressedSize,
		int dstCapacity)
	{
		// printf("LZ4_decompress_safe_ext_hook\n");
		import_translations();

		const int ret = reinterpret_cast<decltype(LZ4_decompress_safe_ext_hook)*>(LZ4_decompress_safe_ext_orig)(
			src, dst, compressedSize, dstCapacity);

		return ret;
	}

	void* LZ4_compress_default_ext_orig = nullptr;

	int LZ4_compress_default_ext_hook(
		char* src,
		char* dst,
		int srcSize,
		int dstCapacity)
	{
		// printf("LZ4_compress_default_ext_hook\n");

		const int ret = reinterpret_cast<decltype(LZ4_compress_default_ext_hook)*>(LZ4_compress_default_ext_orig)(
			src, dst, srcSize, dstCapacity);

		return ret;
	}


	void* populate_orig = nullptr;
	bool populate_hook(void* _this, Il2CppString* str, TextGenerationSettings_t* settings){
		printf("populate_hook: %s\n", il2cppstring_to_utf8(str->start_char).c_str());

		return reinterpret_cast<decltype(populate_hook)*>(populate_orig)(_this, str, settings);
	}

	void* populate_with_errors_orig = nullptr;
	bool populate_with_errors_hook(void* _this, Il2CppString* str, TextGenerationSettings_t* settings, void* context)
	{
		// printf("populate_with_errors_hook\n");

		// Resize font
		// settings->fontSize = round(settings->fontSize * 0.9f);

		std::string str_utf8 = il2cppstring_to_utf8(str->start_char);
		std::string str_json = il2cppstring_to_jsonstring(str->start_char);
		
		size_t debug_pos = str_utf8.find("<debug>");
		if (debug_pos != std::string::npos)
		{
			// Remove all until <debug>
			str_utf8 = str_utf8.substr(debug_pos + 7);
		}

		SHA256 sha256;
		auto str_hash = sha256(str_utf8);
		// TODO: Turn this back on
		// printf("PopulateWithErrors: %s\n", str_json.c_str());
		// printf("^ Hash: %s\n", str_hash.c_str());


		if (text_id_string_to_translation.find(str_hash) != text_id_string_to_translation.end())
		{
			// printf("Found hashed translation for %s\n", str_json.c_str());
			str_utf8 = text_id_string_to_translation[str_hash];
		}

		if (str_utf8.find("<a7>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a7>", "");
			settings->textAnchor = 0;
		}
		if (str_utf8.find("<a8>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a8>", "");
			settings->textAnchor = 1;
		}
		if (str_utf8.find("<a9>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a9>", "");
			settings->textAnchor = 2;
		}
		if (str_utf8.find("<a4>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a4>", "");
			settings->textAnchor = 3;
		}
		if (str_utf8.find("<a5>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a5>", "");
			settings->textAnchor = 4;
		}
		if (str_utf8.find("<a6>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a6>", "");
			settings->textAnchor = 5;
		}
		if (str_utf8.find("<a1>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a1>", "");
			settings->textAnchor = 6;
		}
		if (str_utf8.find("<a2>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a2>", "");
			settings->textAnchor = 7;
		}
		if (str_utf8.find("<a3>") != std::string::npos)
		{
			replaceAll(str_utf8, "<a3>", "");
			settings->textAnchor = 8;
		}
		if (str_utf8.find("<nb>") != std::string::npos)
		{
			replaceAll(str_utf8, "<nb>", "");
			settings->horizontalOverflow = 1;
			settings->generateOutOfBounds = true;
		}
		if (str_utf8.find("<ho>") != std::string::npos)
		{
			replaceAll(str_utf8, "<ho>", "");
			settings->horizontalOverflow = 0;
		}
		if (str_utf8.find("<nho>") != std::string::npos)
		{
			replaceAll(str_utf8, "<nho>", "");
			settings->horizontalOverflow = 1;
		}
		if (str_utf8.find("<vo>") != std::string::npos)
		{
			replaceAll(str_utf8, "<vo>", "");
			settings->verticalOverflow = 0;
		}
		if (str_utf8.find("<nvo>") != std::string::npos)
		{
			replaceAll(str_utf8, "<nvo>", "");
			settings->verticalOverflow = 1;
		}
		if (str_utf8.find("<oob>") != std::string::npos)
		{
			replaceAll(str_utf8, "<oob>", "");
			settings->generateOutOfBounds = true;
		}
		if (str_utf8.find("<fit>") != std::string::npos)
		{
			replaceAll(str_utf8, "<fit>", "");
			settings->resizeTextForBestFit = true;
		}
		if (str_utf8.find("<ub>") != std::string::npos)
		{
			replaceAll(str_utf8, "<ub>", "");
			settings->updateBounds = true;
		}
		if (str_utf8.find("<ag>") != std::string::npos)
		{
			replaceAll(str_utf8, "<ag>", "");
			settings->alignByGeometry = true;
		}
		if (str_utf8.find("<slogan>") != std::string::npos)
		{
			replaceAll(str_utf8, "<slogan>", "");
			replaceAll(str_utf8, "\n", "");
			settings->horizontalOverflow = 0;
		}
		if (str_utf8.find("<br>") != std::string::npos)
		{
			replaceAll(str_utf8, "<br>", "\n");
			if (str_utf8.back() == '\n')
			{
				str_utf8.pop_back();
			}
		}
		if (str_utf8.find("<force>") != std::string::npos)
		{
			replaceAll(str_utf8, "<force>", "");
		}

		Il2CppString* new_str = il2cpp_string_new(str_utf8.data());
		settings->richText = true;

		return reinterpret_cast<decltype(populate_with_errors_hook)*>(populate_with_errors_orig)(_this, new_str, settings, context);
	}



	void* textcommon_gettextid_orig = nullptr;
	int textcommon_gettextid_hook (void* _this)
	{
		// printf("textcommon_gettextid_hook\n");
		return reinterpret_cast<decltype(textcommon_gettextid_hook)*>(textcommon_gettextid_orig)(_this);
	}


	void* textcommon_settextid_orig = nullptr;
	void* textcommon_settextid_hook (void* _this, int id)
	{
		// printf("textcommon_settextid_hook\n");
		return reinterpret_cast<decltype(textcommon_settextid_hook)*>(textcommon_settextid_orig)(_this, id);
	}


	void* textcommon_gettextid_string_orig = nullptr;
	Il2CppString* textcommon_gettextid_string_hook (void* _this)
	{
		// printf("textcommon_gettextid_string_hook\n");
		return reinterpret_cast<decltype(textcommon_gettextid_string_hook)*>(textcommon_gettextid_string_orig)(_this);
	}

	void* localize_jp_get_orig = nullptr;
	Il2CppString* localize_jp_get_hook(int id)
	{
		// printf("localize_jp_get_hook\n");

		// std::string textid_string = text_id_to_string[id];
		// if (textid_string == "Outgame0100")
		// {
		// 	printf("!!!Found Outgame0100!!!");
		// 	stacktrace();
		// }

		Il2CppString* orig_text = reinterpret_cast<decltype(localize_jp_get_hook)*>(localize_jp_get_orig)(id);

		// printf("=== JP GET ===");
		// printf("ID: %d\n", id);
		if (text_id_to_string.find(id) == text_id_to_string.end())
		{
			// printf("ID not found\n");
			return orig_text;
		}

		std::string textid_string = text_id_to_string[id];
		// printf("TextIdString: %s\n", textid_string.c_str());
		
		if (text_id_string_to_translation.find(textid_string) == text_id_string_to_translation.end())
		{
			// printf("Translation not found\n");
			if (debug_mode)
			{
				return il2cpp_string_new((textid_string + "<debug>" + il2cppstring_to_utf8(orig_text->start_char)).data());
			}
			return orig_text;
		}

		std::string translation = text_id_string_to_translation[textid_string];
		// printf("Translation: %s\n", translation.c_str());

		if (debug_mode)
		{
			return il2cpp_string_new((textid_string + "<debug>" + translation).data());
		}
		return il2cpp_string_new(translation.data());
	}


	void index_text(void* textcommon_obj)
	{
		printf("Indexing text\n");
		std::string file_name = "assembly_dump.json";
		if (file_exists(file_name))
		{
			debug_mode = true;
			printf("Dumping text to file.\n");
		}

		std::ofstream outfile;
		if (debug_mode)
		{
			outfile.open(file_name, std::ios_base::trunc);
			outfile << "{\n";
		}

		bool first = true;
		for (int i = 1; i <= 7000; i++)
		{
			textcommon_settextid_hook(textcommon_obj, i);
			Il2CppString* textid_string = textcommon_gettextid_string_hook(textcommon_obj);

			stringid_pointers.insert(textid_string);

			Il2CppString* jp_text = localize_jp_get_hook(i);

			if (jp_text->length == 0){
				continue;
			}

			std::string textid_string_utf8 = il2cppstring_to_jsonstring(textid_string->start_char);
			std::string jp_text_utf8 = il2cppstring_to_jsonstring(jp_text->start_char);

			// printf("index %s: %s\n", textid_string_utf8.c_str(), jp_text_utf8.c_str());

			text_id_to_string[i] = textid_string_utf8;

			if (debug_mode)
			{
				if (!first)
				{
					outfile << ",\n";
				}
				first = false;

				outfile << "\t\"" << textid_string_utf8 << "\": \"" << jp_text_utf8 << "\"";
			}
		}

		if (debug_mode)
		{
			outfile << "\n}";
			outfile.close();
		}

		printf("Indexing text done\n");
		return;
	}


	bool first_textcommon = true;

	void* textcommon_settext_orig = nullptr;
	void* textcommon_settext_hook (void* _this, Il2CppString* str)
	{
		// printf("textcommon_settext_hook\n");

		// std::string str_utf8 = il2cppstring_to_jsonstring(str->start_char);
		// printf("TextCommon.set_text: %s\n", str_utf8.c_str());

		int textid = textcommon_gettextid_hook(_this);
		// printf("TextCommon.set_text: %d\n", textid);

		if (first_textcommon)
		{
			// Index text
			first_textcommon = false;
			index_text(_this);
			textcommon_settextid_hook(_this, textid);
		}


		return reinterpret_cast<decltype(textcommon_settext_hook)*>(textcommon_settext_orig)(_this, str);
	}

	void* textcommon_gettext_orig = nullptr;
	Il2CppString* textcommon_gettext_hook (void* _this)
	{
		// printf("textcommon_gettext_hook\n");

		Il2CppString* orig_text = reinterpret_cast<decltype(textcommon_gettext_hook)*>(textcommon_gettext_orig)(_this);

		// return orig_text;
		
		std::string orig_text_utf8 = il2cppstring_to_utf8(orig_text->start_char);
		std::string orig_text_json = il2cppstring_to_jsonstring(orig_text->start_char);


		Il2CppString* textid_string = textcommon_gettextid_string_hook(_this);
		
		int textid = textcommon_gettextid_hook(_this);

		if (textid_string == nullptr || stringid_pointers.find(textid_string) == stringid_pointers.end() || textid_string->length == 0)
		{
			return orig_text;
		}

		std::string textid_string_utf8 = il2cppstring_to_jsonstring(textid_string->start_char);

		if (orig_text_json.find("<force>") != std::string::npos)
		{
			return orig_text;
		}

		if (text_id_string_to_translation.find(textid_string_utf8) == text_id_string_to_translation.end())
		{
			return orig_text;
		}
		std::string translation = text_id_string_to_translation[textid_string_utf8];
		
		if (debug_mode)
		{
			return il2cpp_string_new((textid_string_utf8 + "<debug>" + translation).data());
		}
		return il2cpp_string_new(translation.data());
	}


	void* to_string_orig = nullptr;
	Il2CppString* to_string_hook(void* _this)
	{
		return reinterpret_cast<decltype(to_string_hook)*>(to_string_orig)(_this);
	}

	void* set_name_orig = nullptr;
	void* set_name_hook(void* _this, Il2CppString* str)
	{
		printf("set_name_hook: %s\n", il2cppstring_to_utf8(str->start_char).c_str());

		Il2CppString* obj_str = to_string_hook(_this);
		printf("obj_str: %s\n", il2cppstring_to_utf8(obj_str->start_char).c_str());

		return reinterpret_cast<decltype(set_name_hook)*>(set_name_orig)(_this, str);
	}


	void* uimanager_SetHeaderTitleText2_orig = nullptr;
	void* uimanager_SetHeaderTitleText2_hook(void* _this, Il2CppString* text, void* guide_id)
	{
		// printf("uimanager_SetHeaderTitleText2_hook: %s\n", il2cppstring_to_utf8(text->start_char).c_str());
		return reinterpret_cast<decltype(uimanager_SetHeaderTitleText2_hook)*>(uimanager_SetHeaderTitleText2_orig)(_this, text, guide_id);
	}


	void* uimanager_SetHeaderTitleText1_orig = nullptr;
	void* uimanager_SetHeaderTitleText1_hook(void* _this, int text_id, void* guide_id)
	{
		// printf("uimanager_SetHeaderTitleText1_hook: %d\n", text_id);
		// If text_id in text_id_to_string, then use that instead
		if (text_id_to_string.find(text_id) != text_id_to_string.end())
		{
			std::string textid_string = text_id_to_string[text_id];
			if (text_id_string_to_translation.find(textid_string) != text_id_string_to_translation.end())
			{
				printf("Found translation for %s\n", textid_string.c_str());
				std::string translation = text_id_string_to_translation[textid_string];
				translation = remove_all_tags(translation);
				return uimanager_SetHeaderTitleText2_hook(_this, il2cpp_string_new(translation.data()), guide_id);
			}
		}

		return reinterpret_cast<decltype(uimanager_SetHeaderTitleText1_hook)*>(uimanager_SetHeaderTitleText1_orig)(_this, text_id, guide_id);
	}


	void* load_library_w_orig = nullptr;

	HMODULE __stdcall load_library_w_hook(const wchar_t* path)
	{
		printf("Saw %ls\n", path);

		// GameAssembly.dll code must be loaded and decrypted while loading criware library
		if (path == L"cri_ware_unity.dll"s)
		{
			const auto game_assembly_module = GetModuleHandle(L"GameAssembly.dll");


			il2cpp_domain_get = reinterpret_cast<il2cpp_domain_get_t>(GetProcAddress(game_assembly_module, "il2cpp_domain_get"));
			il2cpp_domain_assembly_open = reinterpret_cast<il2cpp_domain_assembly_open_t>(GetProcAddress(game_assembly_module, "il2cpp_domain_assembly_open"));
			il2cpp_assembly_get_image = reinterpret_cast<il2cpp_assembly_get_image_t>(GetProcAddress(game_assembly_module, "il2cpp_assembly_get_image"));
			il2cpp_class_from_name = reinterpret_cast<il2cpp_class_from_name_t>(GetProcAddress(game_assembly_module, "il2cpp_class_from_name"));
			il2cpp_class_get_method_from_name = reinterpret_cast<il2cpp_class_get_method_from_name_t>(GetProcAddress(game_assembly_module, "il2cpp_class_get_method_from_name"));
			il2cpp_class_get_nested_types = reinterpret_cast<il2cpp_class_get_nested_types_t>(GetProcAddress(game_assembly_module, "il2cpp_class_get_nested_types"));
			il2cpp_class_get_field_from_name = reinterpret_cast<il2cpp_class_get_field_from_name_t>(GetProcAddress(game_assembly_module, "il2cpp_class_get_field_from_name"));
			il2cpp_class_enum_basetype = reinterpret_cast<il2cpp_class_enum_basetype_t>(GetProcAddress(game_assembly_module, "il2cpp_class_enum_basetype"));
			il2cpp_class_get_methods = reinterpret_cast<il2cpp_class_get_methods_t>(GetProcAddress(game_assembly_module, "il2cpp_class_get_methods"));
			il2cpp_string_new = reinterpret_cast<il2cpp_string_new_t>(GetProcAddress(game_assembly_module, "il2cpp_string_new"));
			il2cpp_method_get_param_count = reinterpret_cast<il2cpp_method_get_param_count_t>(GetProcAddress(game_assembly_module, "il2cpp_method_get_param_count"));
			il2cpp_type_get_name = reinterpret_cast<il2cpp_type_get_name_t>(GetProcAddress(game_assembly_module, "il2cpp_type_get_name"));
			il2cpp_method_get_param = reinterpret_cast<il2cpp_method_get_param_t>(GetProcAddress(game_assembly_module, "il2cpp_method_get_param"));

			printf("1\n");

			auto domain = il2cpp_domain_get();
			// Print domain
			printf("Domain: %p\n", domain);


			auto mscorlib_assembly = il2cpp_domain_assembly_open(domain, "mscorlib.dll");
			printf("mscorlib Assembly: %p\n", mscorlib_assembly);
			auto mscorlib_image = il2cpp_assembly_get_image(mscorlib_assembly);
			printf("mscorlib Image: %p\n", mscorlib_image);

			auto environment_class = il2cpp_class_from_name(mscorlib_image, "System", "Environment");
			printf("Environment: %p\n", environment_class);

			auto stack_trace_addr = il2cpp_class_get_method_from_name(environment_class, "get_StackTrace", 0)->methodPointer;
			environment_get_stacktrace = reinterpret_cast<decltype(environment_get_stacktrace)>(stack_trace_addr);



			auto unity_core_assembly = il2cpp_domain_assembly_open(domain, "UnityEngine.CoreModule.dll");
			printf("Unity Core Assembly: %p\n", unity_core_assembly);
			auto unity_core_image = il2cpp_assembly_get_image(unity_core_assembly);
			printf("Unity Core Image: %p\n", unity_core_image);

			auto object_class = il2cpp_class_from_name(unity_core_image, "UnityEngine", "Object");
			printf("Object: %p\n", object_class);

			auto to_string_addr = il2cpp_class_get_method_from_name(object_class, "ToString", 0)->methodPointer;
			printf("to_string_addr: %p\n", to_string_addr);

			auto to_string_addr_offset = reinterpret_cast<void*>(to_string_addr);
			printf("to_string_addr_offset: %p\n", to_string_addr_offset);

			MH_CreateHook(to_string_addr_offset, to_string_hook, &to_string_orig);
			MH_EnableHook(to_string_addr_offset);


			auto set_name_addr = il2cpp_class_get_method_from_name(object_class, "set_name", 1)->methodPointer;
			printf("set_name_addr: %p\n", set_name_addr);

			auto set_name_addr_offset = reinterpret_cast<void*>(set_name_addr);
			printf("set_name_addr_offset: %p\n", set_name_addr_offset);

			// MH_CreateHook(set_name_addr_offset, set_name_hook, &set_name_orig);
			// MH_EnableHook(set_name_addr_offset);



			auto assembly2 = il2cpp_domain_assembly_open(domain, "UnityEngine.TextRenderingModule.dll");
			printf("Assembly2: %p\n", assembly2);
			auto image2 = il2cpp_assembly_get_image(assembly2);
			printf("Image2: %p\n", image2);
			auto text_generator_class = il2cpp_class_from_name(image2, "UnityEngine", "TextGenerator");
			printf("TextGenerator: %p\n", text_generator_class);
			auto populate_with_errors_addr = il2cpp_class_get_method_from_name(text_generator_class, "PopulateWithErrors", 3)->methodPointer;
			printf("populate_with_errors_addr: %p\n", populate_with_errors_addr);
			auto populate_with_errors_addr_offset = reinterpret_cast<void*>(populate_with_errors_addr);

			MH_CreateHook(populate_with_errors_addr_offset, populate_with_errors_hook, &populate_with_errors_orig);
			MH_EnableHook(populate_with_errors_addr_offset);


			auto populate_addr = il2cpp_class_get_method_from_name(text_generator_class, "Populate", 2)->methodPointer;
			printf("populate_addr: %p\n", populate_addr);
			auto populate_addr_offset = reinterpret_cast<void*>(populate_addr);

			MH_CreateHook(populate_addr_offset, populate_hook, &populate_orig);
			MH_EnableHook(populate_addr_offset);


			// Uma Assembly
			auto uma_assembly = il2cpp_domain_assembly_open(domain, "umamusume.dll");
			printf("uma_assembly: %p\n", uma_assembly);

			auto uma_image = il2cpp_assembly_get_image(uma_assembly);
			printf("uma_image: %p\n", uma_image);


			auto uimanager_class = il2cpp_class_from_name(uma_image, "Gallop", "UIManager");
			printf("UIManager: %p\n", uimanager_class);

			// print_class_methods_with_types(uimanager_class);

			const Il2CppTypeEnum types[2] = { Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE, Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE };
			auto uimanager_SetHeaderTitleText1_addr = find_class_method_with_name_and_types(uimanager_class, "SetHeaderTitleText", types);
			printf("uimanager_SetHeaderTitleText1_addr: %p\n", uimanager_SetHeaderTitleText1_addr);

			auto uimanager_SetHeaderTitleText1_addr_offset = reinterpret_cast<void*>(uimanager_SetHeaderTitleText1_addr);
			printf("uimanager_SetHeaderTitleText1_addr_offset: %p\n", uimanager_SetHeaderTitleText1_addr_offset);

			MH_CreateHook(uimanager_SetHeaderTitleText1_addr_offset, uimanager_SetHeaderTitleText1_hook, &uimanager_SetHeaderTitleText1_orig);
			MH_EnableHook(uimanager_SetHeaderTitleText1_addr_offset);


			const Il2CppTypeEnum types2[2] = { Il2CppTypeEnum::IL2CPP_TYPE_STRING, Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE };
			auto uimanager_SetHeaderTitleText2_addr = find_class_method_with_name_and_types(uimanager_class, "SetHeaderTitleText", types2);
			printf("uimanager_SetHeaderTitleText2_addr: %p\n", uimanager_SetHeaderTitleText2_addr);

			auto uimanager_SetHeaderTitleText2_addr_offset = reinterpret_cast<void*>(uimanager_SetHeaderTitleText2_addr);
			printf("uimanager_SetHeaderTitleText2_addr_offset: %p\n", uimanager_SetHeaderTitleText2_addr_offset);

			MH_CreateHook(uimanager_SetHeaderTitleText2_addr_offset, uimanager_SetHeaderTitleText2_hook, &uimanager_SetHeaderTitleText2_orig);
			MH_EnableHook(uimanager_SetHeaderTitleText2_addr_offset);


			// LocalizeJP
			const auto localize_class = il2cpp_class_from_name(uma_image, "Gallop", "Localize");
			printf("LocalizeJP: %p\n", localize_class);

			const char* name = "JP";
			const auto localize_jp_class = find_nested_class_by_name(localize_class, name);
			printf("LocalizeJP: %p\n", localize_jp_class);

			auto localize_jp_get_addr = il2cpp_class_get_method_from_name(localize_jp_class, "Get", 1)->methodPointer;
			printf("localize_jp_get_addr: %p\n", localize_jp_get_addr);

			auto localize_jp_get_addr_offset = reinterpret_cast<void*>(localize_jp_get_addr);
			printf("localize_jp_get_addr_offset: %p\n", localize_jp_get_addr_offset);

			MH_CreateHook(localize_jp_get_addr_offset, localize_jp_get_hook, &localize_jp_get_orig);
			MH_EnableHook(localize_jp_get_addr_offset);


			// TextCommon
			const auto textcommon_class = il2cpp_class_from_name(uma_image, "Gallop", "TextCommon");
			printf("TextCommon: %p\n", textcommon_class);

			// set_text
			auto textcommon_settext_addr = il2cpp_class_get_method_from_name(textcommon_class, "set_text", 1)->methodPointer;
			printf("textcommon_settext_addr: %p\n", textcommon_settext_addr);

			auto textcommon_settext_addr_offset = reinterpret_cast<void*>(textcommon_settext_addr);
			printf("textcommon_settext_addr_offset: %p\n", textcommon_settext_addr_offset);

			MH_CreateHook(textcommon_settext_addr_offset, textcommon_settext_hook, &textcommon_settext_orig);
			MH_EnableHook(textcommon_settext_addr_offset);


			// get_textid
			auto textcommon_gettextid_addr = il2cpp_class_get_method_from_name(textcommon_class, "get_TextId", 0)->methodPointer;
			printf("textcommon_gettextid_addr: %p\n", textcommon_gettextid_addr);

			auto textcommon_gettextid_addr_offset = reinterpret_cast<void*>(textcommon_gettextid_addr);
			printf("textcommon_gettextid_addr_offset: %p\n", textcommon_gettextid_addr_offset);

			MH_CreateHook(textcommon_gettextid_addr_offset, textcommon_gettextid_hook, &textcommon_gettextid_orig);
			MH_EnableHook(textcommon_gettextid_addr_offset);


			// set_textid
			auto textcommon_settextid_addr = il2cpp_class_get_method_from_name(textcommon_class, "set_TextId", 1)->methodPointer;
			printf("textcommon_settextid_addr: %p\n", textcommon_settextid_addr);

			auto textcommon_settextid_addr_offset = reinterpret_cast<void*>(textcommon_settextid_addr);
			printf("textcommon_settextid_addr_offset: %p\n", textcommon_settextid_addr_offset);

			MH_CreateHook(textcommon_settextid_addr_offset, textcommon_settextid_hook, &textcommon_settextid_orig);
			MH_EnableHook(textcommon_settextid_addr_offset);


			// get_textidstring
			auto textcommon_gettextid_string_addr = il2cpp_class_get_method_from_name(textcommon_class, "get_TextIdString", 0)->methodPointer;
			printf("textcommon_gettextid_string_addr: %p\n", textcommon_gettextid_string_addr);

			auto textcommon_gettextid_string_addr_offset = reinterpret_cast<void*>(textcommon_gettextid_string_addr);
			printf("textcommon_gettextid_string_addr_offset: %p\n", textcommon_gettextid_string_addr_offset);

			MH_CreateHook(textcommon_gettextid_string_addr_offset, textcommon_gettextid_string_hook, &textcommon_gettextid_string_orig);
			MH_EnableHook(textcommon_gettextid_string_addr_offset);


			// get_text
			auto textcommon_gettext_addr = il2cpp_class_get_method_from_name(textcommon_class, "get_text", 0)->methodPointer;
			printf("textcommon_gettext_addr: %p\n", textcommon_gettext_addr);

			auto textcommon_gettext_addr_offset = reinterpret_cast<void*>(textcommon_gettext_addr);
			printf("textcommon_gettext_addr_offset: %p\n", textcommon_gettext_addr_offset);

			MH_CreateHook(textcommon_gettext_addr_offset, textcommon_gettext_hook, &textcommon_gettext_orig);
			MH_EnableHook(textcommon_gettext_addr_offset);



			import_translations();

			MH_DisableHook(LoadLibraryW);
			MH_RemoveHook(LoadLibraryW);

			return LoadLibraryW(path);
		}

		return reinterpret_cast<decltype(LoadLibraryW)*>(load_library_w_orig)(path);
	}
}

void attach()
{
	create_debug_console();

	if (MH_Initialize() != MH_OK)
	{
		printf("Failed to initialize MinHook.\n");
		return;
	}
	printf("MinHook initialized.\n");

	MH_CreateHook(LoadLibraryW, load_library_w_hook, &load_library_w_orig);
	MH_EnableHook(LoadLibraryW);

	// If 'carrotjuicer.dll' exists, loadlibraryw
	if (file_exists("carrotjuicer.dll"))
	{
		printf("carrotjuicer.dll found, loading...\n");
		LoadLibraryW(L"carrotjuicer.dll");
	}
}

void detach()
{
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}
