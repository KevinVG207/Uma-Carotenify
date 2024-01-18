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
	std::map<std::string, std::string> text_id_string_to_original;
	bool tl_first_check = true;
	std::filesystem::file_time_type tl_last_modified;
	std::set<Il2CppString*> stringid_pointers;
	bool debug_mode = false;
	void* last_text_list_ptr = nullptr;

	std::set<std::string> do_not_replace_strings = {
		"SingleMode418033"
	};

	const std::vector<std::string> training_result_array = {
		"at Gallop.SingleModeMainTrainingCuttController.GetTrainingEffectMessageWindowTextList",
		"at Gallop.TrainingParamChangeUI.GetMessageText",
		"at Gallop.StoryEventConclusion"
	};

	// TrainingParamChangeUI.FitMessageSize

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


	void removePropertyTag(std::string& in_str, std::string tag)
	{
		// Property Tags look like <tag=xxx>
		// We want to remove the whole tag, including the =xxx part
		// There is no end tag.
		std::string start_tag = "<" + tag + "=";
		while (in_str.find(start_tag) != std::string::npos)
		{
			auto start = in_str.find(start_tag);
			auto end = in_str.find(">", start);

			if (end == std::string::npos)
			{
				// No end tag found
				break;
			}

			in_str.replace(start, end - start + 1, "");
		}

	}
	

	std::string getPropertyTag(std::string& in_str, std::string tag)
	{
		// Property Tags look like <tag=xxx>
		// Determine if a tag is found, and return the xxx part.

		std::string ret = "";

		std::string start_tag = "<" + tag + "=";
		while (in_str.find(start_tag) != std::string::npos)
		{
			auto start = in_str.find(start_tag);
			auto end = in_str.find(">", start);

			if (end == std::string::npos)
			{
				// No end tag found
				break;
			}

			ret = in_str.substr(start + start_tag.length(), end - start - start_tag.length());
			break;
		}

		return ret;
	}


	void remove_all_tags(std::string& in_str)
	{
		// Remove all <> tags
		replaceAll(in_str, "<res>", "");
		replaceAll(in_str, "<log>", "");
		replaceAll(in_str, "<a7>", "");
		replaceAll(in_str, "<a8>", "");
		replaceAll(in_str, "<a9>", "");
		replaceAll(in_str, "<a4>", "");
		replaceAll(in_str, "<a5>", "");
		replaceAll(in_str, "<a6>", "");
		replaceAll(in_str, "<a1>", "");
		replaceAll(in_str, "<a2>", "");
		replaceAll(in_str, "<a3>", "");
		replaceAll(in_str, "<nb>", "");
		replaceAll(in_str, "<ho>", "");
		replaceAll(in_str, "<nho>", "");
		replaceAll(in_str, "<vo>", "");
		replaceAll(in_str, "<nvo>", "");
		replaceAll(in_str, "<oob>", "");
		replaceAll(in_str, "<ub>", "");
		replaceAll(in_str, "<mon>", "");
		replaceAll(in_str, "<slogan>", "");
		replaceAll(in_str, "<rbr>", "");
		replaceAll(in_str, "<br>", "");
		replaceAll(in_str, "<force>", "");
		replaceAll(in_str, "<ords>", "");  // Ordinal numeral start
		replaceAll(in_str, "<orde>", "");  // Ordinal numeral end
		replaceAll(in_str, "<sy>", "");
		replaceAll(in_str, "<sd>", "");
		replaceAll(in_str, "<sh>", "");
		replaceAll(in_str, "<sm>", "");
		replaceAll(in_str, "<ss>", "");
		replaceAll(in_str, "<st>", "");
		replaceAll(in_str, "<ey>", "");
		replaceAll(in_str, "<ed>", "");
		replaceAll(in_str, "<eh>", "");
		replaceAll(in_str, "<em>", "");
		replaceAll(in_str, "<es>", "");
		replaceAll(in_str, "<et>", "");
		// replaceAll(in_str, "<ssc>", "");  // Scale tag start
		// replaceAll(in_str, "<esc>", "");  // Scale tag end
		removePropertyTag(in_str, "sc");
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

	Il2CppString* (*environment_get_stacktrace)();

	void stacktrace()
	{
		printf("%ls\n", environment_get_stacktrace()->start_char);
	}

	bool in_stacktrace(std::string str)
	{
		std::string stacktrace_str = il2cppstring_to_utf8(environment_get_stacktrace()->start_char);
		if (stacktrace_str.find(str) != std::string::npos)
		{
			return true;
		}
		return false;
	}

	bool in_stacktrace(std::vector<std::string> str)
	{
		std::string stacktrace_str = il2cppstring_to_utf8(environment_get_stacktrace()->start_char);
		for (auto& s : str)
		{
			if (stacktrace_str.find(s) != std::string::npos)
			{
				return true;
			}
		}
		return false;
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


	void import_translations()
	{
		std::string file_name = "assembly_dump.json";
		if (file_exists(file_name))
		{
			debug_mode = true;
		} else {
			debug_mode = false;
		}

		file_name = "translations.txt";

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

			// if (translation.find('{') != std::string::npos)
			// {
			// 	translation = "<force>" + translation;
			// }

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

		// TODO: Remove logging to file
		freopen_s(&_, "CONOUT$", "w", stdout);
		freopen_s(&_, "CONOUT$", "w", stderr);
		freopen_s(&_, "CONIN$", "r", stdin);
		// freopen_s(&_, "carotene_log.txt", "a", stdout);

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

	void* move_next_orig = nullptr;
	bool move_next_hook(void* _this)
	{
		printf("move_next_hook\n");
		if (last_text_list_ptr == _this)
		{
			// We are in the training cutscene enumerator
			printf("TRAINING CUTSCENE ENUMERATOR!!\n");
		}
		return reinterpret_cast<decltype(move_next_hook)*>(move_next_orig)(_this);
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


	void* populate_orig = nullptr;
	bool populate_hook(void* _this, Il2CppString* str, TextGenerationSettings_t* settings){
		printf("populate_hook: %s\n", il2cppstring_to_utf8(str->start_char).c_str());

		return reinterpret_cast<decltype(populate_hook)*>(populate_orig)(_this, str, settings);
	}

	std::string handle_early_late(std::string str, size_t idx, int len, std::string repl)
	{
		bool has_replaced = false;
		int full_len = len;
		std::string new_substr;
		std::string substr = str.substr(idx + len, 5);
		if (substr == "Early")
		{
			new_substr = "Early " + repl;
			has_replaced = true;
			full_len += 6;
		}

		substr = str.substr(idx + len, 4);
		if (substr == "Late")
		{
			new_substr = "Late " + repl;
			has_replaced = true;
			full_len += 5;
		}

		substr = str.substr(idx + len, 6);
		if (substr == " Early")
		{
			new_substr = "Early " + repl;
			has_replaced = true;
			full_len += 6;
		}

		substr = str.substr(idx + len, 5);
		if (substr == " Late")
		{
			new_substr = "Late " + repl;
			has_replaced = true;
			full_len += 5;
		}

		if (!has_replaced)
		{
			new_substr = repl;
		}

		if (idx > 0)
		{
			if (str[idx - 1] != ' ')
			{
				new_substr = " " + new_substr;
				full_len += 1;
			}
		}

		str.replace(idx, full_len, new_substr);

		return str;
	}

	std::string handle_months(std::string str)
	{
		while (str.find("<mon>") != std::string::npos)
		{
			bool has_replaced = false;

			// TODO: Rework this, this is awful!
			if (str.find("<mon>12") != std::string::npos)
			{
				auto idx = str.find("<mon>12");
				auto len = 7;
				auto repl = "Dec.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>11") != std::string::npos)
			{
				auto idx = str.find("<mon>11");
				auto len = 7;
				auto repl = "Nov.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>10") != std::string::npos)
			{
				auto idx = str.find("<mon>10");
				auto len = 7;
				auto repl = "Oct.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>9") != std::string::npos)
			{
				auto idx = str.find("<mon>9");
				auto len = 6;
				auto repl = "Sep.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>8") != std::string::npos)
			{
				auto idx = str.find("<mon>8");
				auto len = 6;
				auto repl = "Aug.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>7") != std::string::npos)
			{
				auto idx = str.find("<mon>7");
				auto len = 6;
				auto repl = "Jul.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>6") != std::string::npos)
			{
				auto idx = str.find("<mon>6");
				auto len = 6;
				auto repl = "Jun.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>5") != std::string::npos)
			{
				auto idx = str.find("<mon>5");
				auto len = 6;
				auto repl = "May.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>4") != std::string::npos)
			{
				auto idx = str.find("<mon>4");
				auto len = 6;
				auto repl = "Apr.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>3") != std::string::npos)
			{
				auto idx = str.find("<mon>3");
				auto len = 6;
				auto repl = "Mar.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>2") != std::string::npos)
			{
				auto idx = str.find("<mon>2");
				auto len = 6;
				auto repl = "Feb.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}
			if (str.find("<mon>1") != std::string::npos)
			{
				auto idx = str.find("<mon>1");
				auto len = 6;
				auto repl = "Jan.";
				str = handle_early_late(str, idx, len, repl);
				continue;
			}

			// If we reach here, we have not replaced anything
			replaceAll(str, "<mon>", "");
		}

		return str;
	}

	std::string handle_ordinal_numberals(std::string str)
	{
		while (str.find("<ords>") != std::string::npos)
		{
			auto start = str.find("<ords>");
			auto end = str.find("<orde>");

			if (end == std::string::npos)
			{
				// No end tag found
				break;
			}

			std::string substr = str.substr(start + 6, end - start - 6);

			if (substr == "1")
			{
				substr += "st";
			}
			else if (substr == "2")
			{
				substr += "nd";
			}
			else if (substr == "3")
			{
				substr += "rd";
			}

			else if (substr.length() > 1)
			{
				if (substr[substr.length() - 2] == '1')
				{
					substr += "th";
				}
				else
				{
					if (substr.back() == '1')
					{
						substr += "st";
					}
					else if (substr.back() == '2')
					{
						substr += "nd";
					}
					else if (substr.back() == '3')
					{
						substr += "rd";
					}
					else
					{
						substr += "th";
					}
				}
			}

			else
			{
				substr += "th";
			}

			str.replace(start, end - start + 6, substr);
		}
		replaceAll(str, "<orde>", "");

		return str;
	}

	std::string handle_timespan(std::string str, std::string start_tag, std::string end_tag, std::string singular, std::string plural){

		while (str.find(start_tag) != std::string::npos)
		{
			auto start = str.find(start_tag);
			auto end = str.find(end_tag);

			if (end == std::string::npos)
			{
				// No end tag found
				break;
			}

			std::string substr = str.substr(start + start_tag.length(), end - start - start_tag.length());

			int num = -1;

			try
			{
				num = std::stoi(substr);
			}
			catch (const std::invalid_argument const& ex)
			{
				
			}

			if (num == 1)
			{
				substr += " " + singular;
			} else {
				substr += " " + plural;
			}

			str.replace(start, end - start + end_tag.length(), substr);
		}

		return str;

	}

	std::tuple<std::string, float> handle_scale_tag(std::string str, float current_font_size)
	{
		// printf("handle_scale_tag\n");
		// printf("font size: %f\n", current_font_size);
		while (str.find("<ssc>") != std::string::npos)
		{
			auto start = str.find("<ssc>");
			auto end = str.find("<esc>");

			// printf("start: %d\n", start);
			// printf("end: %d\n", end);

			if (end == std::string::npos)
			{
				// No end tag found
				// printf("a\n");
				break;
			}

			if (start != 0)
			{
				// printf("b\n");
				str.replace(start, end - start + 5, "");
				break;
			}

			std::string substr = str.substr(start + 5, end - start - 5);

			float scale = 1.0f;
			try
			{
				scale = std::stoi(substr) / 100.0f;
			}
			catch (const std::invalid_argument const& ex)
			{
				
			}

			current_font_size *= scale;

			str.replace(start, end - start + 5, "");
			// printf("c\n");
		}

		// printf("font size: %f\n", current_font_size);
		// printf("handle_scale_tag done\n");

		return {str, current_font_size};
	}

	std::tuple<std::string, float> handle_scale_tag_v2(std::string str, float current_font_size)
	{
		auto value = getPropertyTag(str, "sc");

		float scale = 1.0f;
		try
		{
			scale = std::stoi(value) / 100.0f;
		}
		catch (const std::invalid_argument const& ex)
		{
			
		}

		current_font_size *= scale;

		removePropertyTag(str, "sc");

		return {str, current_font_size};
	}


	std::string handle_timespans(std::string str){
		str = handle_timespan(str, "<sy>", "<ey>", "year", "years");
		str = handle_timespan(str, "<sd>", "<ed>", "day", "days");
		str = handle_timespan(str, "<sh>", "<eh>", "hour", "hours");
		str = handle_timespan(str, "<sm>", "<em>", "min", "mins");
		str = handle_timespan(str, "<ss>", "<es>", "sec", "secs");
		str = handle_timespan(str, "<st>", "<et>", "turn", "turns");

		return str;
	}


	std::string handle_tags(std::string str_utf8, TextGenerationSettings_t* settings)
	{
		if (str_utf8.find("<log>") != std::string::npos)
		{
			replaceAll(str_utf8, "<log>", "");
			settings->horizontalOverflow = 0;
			replaceAll(str_utf8, "\n", "");

			// Remove only first <res> tag.
			size_t start = str_utf8.find("<res>");
			if (start != std::string::npos)
			{
				str_utf8.erase(start, 5);
			}

			replaceAll(str_utf8, "<res>", "\n");

			remove_all_tags(str_utf8);
			return str_utf8;
		}

		// Special case for training event result messages.
		if (str_utf8.find("<res>") != std::string::npos)
		{
			// // If <res> appears more than 1 time
			// if (str_utf8.find("<res>") != str_utf8.rfind("<res>"))
			// {
			// 	// Workaround for log screen. This may mess up in other places!
			// 	// TODO: Maybe find a more robust solution?
			// 	replaceAll(str_utf8, "\n", "");

			// 	// Remove only the first instance of <res>
			// 	size_t start = str_utf8.find("<res>");
			// 	str_utf8.erase(start, 5);

			// 	replaceAll(str_utf8, "<res>", "\n");
			// } else {
			// 	replaceAll(str_utf8, "<res>", "");
			// 	replaceAll(str_utf8, "\n", "");
			// }
			settings->horizontalOverflow = 0;
			replaceAll(str_utf8, "\n", "");
			remove_all_tags(str_utf8);
			return str_utf8;
		}

		std::tie(str_utf8, settings->fontSize) = handle_scale_tag_v2(str_utf8, settings->fontSize);

		str_utf8 = handle_ordinal_numberals(str_utf8);
		str_utf8 = handle_timespans(str_utf8);

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
			settings->horizontalOverflow = 1;
		}
		if (str_utf8.find("<nho>") != std::string::npos)
		{
			replaceAll(str_utf8, "<nho>", "");
			settings->horizontalOverflow = 0;
		}
		if (str_utf8.find("<vo>") != std::string::npos)
		{
			replaceAll(str_utf8, "<vo>", "");
			settings->verticalOverflow = 1;
		}
		if (str_utf8.find("<nvo>") != std::string::npos)
		{
			replaceAll(str_utf8, "<nvo>", "");
			settings->verticalOverflow = 0;
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
			//TODO: Phase out <slogan> tag and use <rbr> instead
			replaceAll(str_utf8, "<slogan>", "");
			replaceAll(str_utf8, "\n", "");
			settings->horizontalOverflow = 0;
		}
		if (str_utf8.find("<rbr>") != std::string::npos)
		{
			// Remove breaks
			replaceAll(str_utf8, "<rbr>", "");
			replaceAll(str_utf8, "\n", "");
			settings->horizontalOverflow = 0;
		}
		if (str_utf8.find("<story>") != std::string::npos)
		{
			replaceAll(str_utf8, "<story>", "");
			settings->fontSize *= 0.9f;
			settings->lineSpacing *= 0.8f;
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

		str_utf8 = handle_months(str_utf8);

		return str_utf8;
	}

	void* populate_with_errors_orig = nullptr;
	bool populate_with_errors_hook(void* _this, Il2CppString* str, TextGenerationSettings_t* settings, void* context)
	{
		// printf("populate_with_errors_hook\n");
		// Resize font
		// settings->fontSize = round(settings->fontSize * 0.9f);

		std::string str_utf8 = il2cppstring_to_utf8(str->start_char);
		std::string str_json = il2cppstring_to_jsonstring(str->start_char);
		
		// printf("Draw before: %s\n", str_utf8.c_str());
		// stacktrace();


		// if (str_utf8 == "育成中のデータを削除します")
		// {
		// 	stacktrace();
		// }
		
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

		str_utf8 = handle_tags(str_utf8, settings);

		printf("Draw: %s\n", str_utf8.c_str());
		// printf("horizonalOverflow: %d\n", settings->horizontalOverflow);

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
		// printf("textcommon_gettextid_string_hook: %p\n", _this);
		return reinterpret_cast<decltype(textcommon_gettextid_string_hook)*>(textcommon_gettextid_string_orig)(_this);
	}

	void* localize_jp_get_orig = nullptr;
	Il2CppString* localize_jp_get_hook(int id)
	{
		// printf("localize_jp_get_hook\n");

		Il2CppString* out_text = reinterpret_cast<decltype(localize_jp_get_hook)*>(localize_jp_get_orig)(id);

		// if (id == 4218)
		// {
		// 	stacktrace();
		// }

		// if (id == 946)
		// {
		// 	stacktrace();
		// }

		// printf("=== JP GET ===");
		// printf("ID: %d\n", id);
		if (text_id_to_string.find(id) != text_id_to_string.end())
		{
			std::string textid_string = text_id_to_string[id];
			// printf("TextIdString: %s\n", textid_string.c_str());

			// Do not replace some special cases
			if (do_not_replace_strings.find(textid_string) != do_not_replace_strings.end())
			{
				goto KeepOriginal;
			}


			// std::string compare_str = "CustomRace0003";
			// if (textid_string == compare_str)
			// {
			// 	printf("!!!Found CustomRace0003!!!\n");
			// 	stacktrace();
			// }


			
			if (text_id_string_to_translation.find(textid_string) == text_id_string_to_translation.end())
			{
				// printf("Translation not found\n");
				if (debug_mode)
				{
					out_text = il2cpp_string_new((textid_string + "<debug>" + il2cppstring_to_utf8(out_text->start_char)).data());
				}
			} else
			{
				std::string translation = text_id_string_to_translation[textid_string];
				// printf("Translation: %s\n", translation.c_str());

				if (debug_mode)
				{
					out_text = il2cpp_string_new((textid_string + "<debug>" + translation).data());
				} else {
					out_text = il2cpp_string_new(translation.data());
				}

				if (in_stacktrace(training_result_array))
				{
					// printf("Found\n");
					out_text = il2cpp_string_new(("<res>" + il2cppstring_to_utf8(out_text->start_char)).data());
				}
				else
				{
					// printf("Not found\n");
				}
			}

		} else {
			if (debug_mode){
				// Convert int id to string
				std::string textid_string = std::to_string(id);

				out_text = il2cpp_string_new((textid_string + "<debug>" + il2cppstring_to_utf8(out_text->start_char)).data());
			}
		}

		KeepOriginal:

		// Print ID
		std::set<int> no_print_ids = {
			1030, 1031, 1107, 1108
		};

		if (debug_mode && no_print_ids.find(id) == no_print_ids.end())
		{
			printf("Fetch %d: %s\n", id, il2cppstring_to_utf8(out_text->start_char).c_str());
		}

		// printf("Fetch %d: %s\n", id, il2cppstring_to_utf8(out_text->start_char).c_str());
		// stacktrace();

		return out_text;
	}


	void index_text(void* textcommon_obj)
	{
		printf("Indexing text\n");
		std::string file_name = "assembly_dump.json";
		bool print_flag = false;
		debug_mode = false;
		if (file_exists(file_name))
		{
			print_flag = true;
			printf("Dumping text to file.\n");
		}

		std::ofstream outfile;
		if (print_flag)
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
			text_id_string_to_original[textid_string_utf8] = jp_text_utf8;

			if (print_flag)
			{
				if (!first)
				{
					outfile << ",\n";
				}
				first = false;

				outfile << "\t\"" << textid_string_utf8 << "\": \"" << jp_text_utf8 << "\"";
			}
		}

		if (print_flag)
		{
			outfile << "\n}";
			outfile.close();
			debug_mode = true;
		}

		printf("Indexing text done\n");
		return;
	}


	bool first_textcommon = true;

	void* textcommon_settext_orig = nullptr;
	void* textcommon_settext_hook (void* _this, Il2CppString* str)
	{
		// printf("textcommon_settext_hook\n");
		// stacktrace();

		// std::string str_utf8 = il2cppstring_to_jsonstring(str->start_char);
		// printf("TextCommon.set_text: %s\n", str_utf8.c_str());

		// printf("TextCommon.set_text: %d\n", textid);
		if (str != nullptr && in_stacktrace("Gallop.SingleModeLogItem"))
		{
			// auto res = reinterpret_cast<decltype(textcommon_settext_hook)*>(textcommon_settext_orig)(_this, str);
			// auto new_str = textcommon_gettext_hook(_this);
			// new_str = il2cpp_string_new(("<log>" + il2cppstring_to_utf8(new_str->start_char)).data());
			// return reinterpret_cast<decltype(textcommon_settext_hook)*>(textcommon_settext_orig)(_this, new_str);
			// printf("a\n");
			str = il2cpp_string_new(("<log>" + il2cppstring_to_utf8(str->start_char)).data());
		}
		// printf("b\n");


		if (first_textcommon)
		{
			// Index text
			int textid = textcommon_gettextid_hook(_this);
			first_textcommon = false;
			index_text(_this);
			textcommon_settextid_hook(_this, textid);
		}


		return reinterpret_cast<decltype(textcommon_settext_hook)*>(textcommon_settext_orig)(_this, str);
	}

	void* textcommon_gettext_orig = nullptr;
	Il2CppString* textcommon_gettext_hook (void* _this)
	{
		Il2CppString* orig_text = reinterpret_cast<decltype(textcommon_gettext_hook)*>(textcommon_gettext_orig)(_this);
		// printf("textcommon_gettext_hook: %s\n", il2cppstring_to_utf8(orig_text->start_char).c_str());

		// return orig_text;
		// stacktrace();

		// printf("a\n");
		
		std::string orig_text_utf8 = il2cppstring_to_utf8(orig_text->start_char);
		std::string orig_text_json = il2cppstring_to_jsonstring(orig_text->start_char);

		// printf("_this: %p\n", _this);

		// Check if _this is valid
		auto bad = IsBadReadPtr(_this, 8);
		if (bad)
		{
			printf("Bad read ptr\n");
			return orig_text;
		}

		Il2CppString* textid_string = textcommon_gettextid_string_hook(_this);

		// printf("b\n");
		
		int textid = textcommon_gettextid_hook(_this);

		// printf("c\n");

		if (textid_string == nullptr || stringid_pointers.find(textid_string) == stringid_pointers.end() || textid_string->length == 0)
		{
			return orig_text;
		}

		std::string textid_string_utf8 = il2cppstring_to_jsonstring(textid_string->start_char);

		if (orig_text_json.find("<force>") != std::string::npos)
		{
			return orig_text;
		}

		// if (orig_text_utf8.find("{") != std::string::npos)
		// {
		// 	// Do not replace any format strings.
		// 	return orig_text;
		// }

		// The format string may have already been formatted, so we need to check the original text.
		if (text_id_string_to_original[textid_string_utf8].find("{") != std::string::npos)
		{
			// Do not replace any format strings.
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


	void* uimanager_SetHeaderTitleText2_orig = nullptr;
	void* uimanager_SetHeaderTitleText2_hook(void* _this, Il2CppString* text, void* guide_id)
	{
		// printf("uimanager_SetHeaderTitleText2_hook\n");
		// printf("uimanager_SetHeaderTitleText2_hook: %s\n", il2cppstring_to_utf8(text->start_char).c_str());
		return reinterpret_cast<decltype(uimanager_SetHeaderTitleText2_hook)*>(uimanager_SetHeaderTitleText2_orig)(_this, text, guide_id);
	}


	void* uimanager_SetHeaderTitleText1_orig = nullptr;
	void* uimanager_SetHeaderTitleText1_hook(void* _this, int text_id, void* guide_id)
	{
		// printf("uimanager_SetHeaderTitleText1_hook\n");
		// printf("uimanager_SetHeaderTitleText1_hook: %d\n", text_id);
		// If text_id in text_id_to_string, then use that instead
		if (text_id_to_string.find(text_id) != text_id_to_string.end())
		{
			std::string textid_string = text_id_to_string[text_id];
			if (text_id_string_to_translation.find(textid_string) != text_id_string_to_translation.end())
			{
				printf("Found translation for %s\n", textid_string.c_str());
				std::string translation = text_id_string_to_translation[textid_string];
				remove_all_tags(translation);
				return uimanager_SetHeaderTitleText2_hook(_this, il2cpp_string_new(translation.data()), guide_id);
			}
		}

		return reinterpret_cast<decltype(uimanager_SetHeaderTitleText1_hook)*>(uimanager_SetHeaderTitleText1_orig)(_this, text_id, guide_id);
	}

	void* tcc_get_text_list_orig = nullptr;
	void* tcc_get_text_list_hook(void* _this)
	{
		// printf("tcc_get_text_list_hook\n");
		void* ret = reinterpret_cast<decltype(tcc_get_text_list_hook)*>(tcc_get_text_list_orig)(_this);
		return ret;
	}

	void* antext_settext_orig = nullptr;
	void* antext_settext_hook(void* _this, Il2CppString* text)
	{
		printf("antext_settext_hook: %s\n", il2cppstring_to_utf8(text->start_char).c_str());
		return reinterpret_cast<decltype(antext_settext_hook)*>(antext_settext_orig)(_this, text);
	}

	void* antext_gettext_orig = nullptr;
	void* antext_gettext_hook(void* _this)
	{
		// printf("antext_gettext_hook\n");
		void* ret = reinterpret_cast<decltype(antext_gettext_hook)*>(antext_gettext_orig)(_this);
		return ret;
	
	}

	void* antext_getfixtext_orig = nullptr;
	void* antext_getfixtext_hook(void* _this)
	{
		// printf("antext_getfixtext_hook\n");
		void* ret = reinterpret_cast<decltype(antext_getfixtext_hook)*>(antext_getfixtext_orig)(_this);
		return ret;
	
	}

	void* antext_getfixtext_wrt_orig = nullptr;
	void* antext_getfixtext_wrt_hook(void* _this)
	{
		// printf("antext_getfixtext_wrt_hook\n");
		void* ret = reinterpret_cast<decltype(antext_getfixtext_wrt_hook)*>(antext_getfixtext_wrt_orig)(_this);
		return ret;
	
	}

	void* tcc_play_cut_orig = nullptr;
	void* tcc_play_cut_hook(void* _this, void* play_info)
	{
		// printf("tcc_play_cut_hook\n");
		void* enumerator = reinterpret_cast<decltype(tcc_play_cut_hook)*>(tcc_play_cut_orig)(_this, play_info);
		last_text_list_ptr = enumerator;
		return enumerator;
	}

	void* get_scen_race_name_orig = nullptr;
	void* get_scen_race_name_hook(void* _this)
	{
		void* ret = reinterpret_cast<decltype(get_scen_race_name_hook)*>(get_scen_race_name_orig)(_this);
		// printf("get_scen_race_name_hook: %s\n", il2cppstring_to_utf8(reinterpret_cast<Il2CppString*>(ret)->start_char).c_str());
		return ret;
	}

	void* tpca2u_getcaptiontext_orig = nullptr;
	Il2CppString* tpca2u_getcaptiontext_hook(void* _this, void* change_param_info)
	{
		// printf("tpca2u_getcaptiontext_hook\n");
		Il2CppString* ret = reinterpret_cast<decltype(tpca2u_getcaptiontext_hook)*>(tpca2u_getcaptiontext_orig)(_this, change_param_info);
		
		std::string ret_utf8 = il2cppstring_to_utf8(ret->start_char);

		remove_all_tags(ret_utf8);

		return il2cpp_string_new(ret_utf8.data());
	}

	void* tpca2u_getskillcaptiontext_orig = nullptr;
	Il2CppString* tpca2u_getskillcaptiontext_hook(void* _this, int skill_id, int skill_level)
	{
		// printf("tpca2u_getskillcaptiontext_hook\n");
		Il2CppString* ret = reinterpret_cast<decltype(tpca2u_getskillcaptiontext_hook)*>(tpca2u_getskillcaptiontext_orig)(_this, skill_id, skill_level);
		
		std::string ret_utf8 = il2cppstring_to_utf8(ret->start_char);

		remove_all_tags(ret_utf8);

		return il2cpp_string_new(ret_utf8.data());
	
	}

	void* manifest_exec_orig = nullptr;
	void* manifest_exec_hook(void* _this, Il2CppString* sql)
	{
		printf("manifest_exec_hook: %s\n", il2cppstring_to_utf8(sql->start_char).c_str());
		return reinterpret_cast<decltype(manifest_exec_hook)*>(manifest_exec_orig)(_this, sql);
	}

	void* db_adapter_query_orig = nullptr;
	void* db_adapter_query_hook(void* _this, Il2CppString* sql)
	{
		printf("db_adapter_query_hook: %s\n", il2cppstring_to_utf8(sql->start_char).c_str());
		return reinterpret_cast<decltype(db_adapter_query_hook)*>(db_adapter_query_orig)(_this, sql);
	}

	void* db_adapter_preparedquery_orig = nullptr;
	void* db_adapter_preparedquery_hook(void* _this, Il2CppString* sql)
	{
		printf("db_adapter_preparedquery_hook: %s\n", il2cppstring_to_utf8(sql->start_char).c_str());
		return reinterpret_cast<decltype(db_adapter_preparedquery_hook)*>(db_adapter_preparedquery_orig)(_this, sql);
	}


	void* connection_exec_orig = nullptr;
	void* connection_exec_hook(void* _this, Il2CppString* sql)
	{
		printf("connection_exec_hook: %s\n", il2cppstring_to_utf8(sql->start_char).c_str());
		return reinterpret_cast<decltype(connection_exec_hook)*>(connection_exec_orig)(_this, sql);
	}

	void* connection_query_orig = nullptr;
	void* connection_query_hook(void* _this, Il2CppString* sql)
	{
		printf("connection_query_hook: %s\n", il2cppstring_to_utf8(sql->start_char).c_str());
		return reinterpret_cast<decltype(connection_query_hook)*>(connection_query_orig)(_this, sql);
	}

	void* connection_preparedquery_orig = nullptr;
	void* connection_preparedquery_hook(void* _this, Il2CppString* sql)
	{
		printf("connection_preparedquery_hook: %s\n", il2cppstring_to_utf8(sql->start_char).c_str());
		return reinterpret_cast<decltype(connection_preparedquery_hook)*>(connection_preparedquery_orig)(_this, sql);
	}

	void* query_ctor_orig = nullptr;
	void* query_ctor_hook(void* _this, void* conn, Il2CppString* sql)
	{
		printf("query_ctor_hook: %s\n", il2cppstring_to_utf8(sql->start_char).c_str());
		return reinterpret_cast<decltype(query_ctor_hook)*>(query_ctor_orig)(_this, conn, sql);
	}



	void bootstrap_carrot_juicer()
	{
		const auto libnative_module = GetModuleHandle(L"libnative.dll");
		printf("libnative.dll at %p\n", libnative_module);
		if (libnative_module == nullptr)
		{
			return;
		}

		const auto LZ4_decompress_safe_ext_ptr = GetProcAddress(libnative_module, "LZ4_decompress_safe_ext");
		printf("LZ4_decompress_safe_ext at %p\n", LZ4_decompress_safe_ext_ptr);
		if (LZ4_decompress_safe_ext_ptr == nullptr)
		{
			return;
		}
		MH_CreateHook(LZ4_decompress_safe_ext_ptr, LZ4_decompress_safe_ext_hook, &LZ4_decompress_safe_ext_orig);
		MH_EnableHook(LZ4_decompress_safe_ext_ptr);

		const auto LZ4_compress_default_ext_ptr = GetProcAddress(libnative_module, "LZ4_compress_default_ext");
		printf("LZ4_compress_default_ext at %p\n", LZ4_compress_default_ext_ptr);
		if (LZ4_compress_default_ext_ptr == nullptr)
		{
			return;
		}
		MH_CreateHook(LZ4_compress_default_ext_ptr, LZ4_compress_default_ext_hook, &LZ4_compress_default_ext_orig);
		MH_EnableHook(LZ4_compress_default_ext_ptr);

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


			auto ienumerator_class = il2cpp_class_from_name(mscorlib_image, "System.Collections", "IEnumerator");
			printf("IEnumerator: %p\n", ienumerator_class);

			auto move_next_addr = il2cpp_class_get_method_from_name(ienumerator_class, "MoveNext", 0)->methodPointer;
			printf("MoveNext: %p\n", move_next_addr);

			auto move_next_addr_offset = reinterpret_cast<void*>(move_next_addr);
			printf("MoveNext Offset: %p\n", move_next_addr_offset);

			MH_CreateHook(move_next_addr_offset, move_next_hook, &move_next_orig);
			MH_EnableHook(move_next_addr_offset);



			// auto tmp_assembly = il2cpp_domain_assembly_open(domain, "Unity.TextMeshPro.dll");
			// printf("tmp_assembly: %p\n", tmp_assembly);
			// auto tmp_image = il2cpp_assembly_get_image(tmp_assembly);
			// printf("tmp_image: %p\n", tmp_image);

			// auto tmp_text_class = il2cpp_class_from_name(tmp_image, "TMPro", "TMP_Text");
			// printf("TMP_Text: %p\n", tmp_text_class);

			// print_class_methods_with_types(tmp_text_class);


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




			auto plugins_assembly = il2cpp_domain_assembly_open(domain, "Plugins.dll");
			printf("Plugins Assembly: %p\n", plugins_assembly);
			auto plugins_image = il2cpp_assembly_get_image(plugins_assembly);
			printf("Plugins Image: %p\n", plugins_image);

			auto antext_class = il2cpp_class_from_name(plugins_image, "AnimateToUnity", "AnText");
			printf("AnText: %p\n", antext_class);

			auto antext_settext_addr = il2cpp_class_get_method_from_name(antext_class, "SetText", 1)->methodPointer;
			printf("antext_settext_addr: %p\n", antext_settext_addr);

			auto antext_settext_addr_offset = reinterpret_cast<void*>(antext_settext_addr);
			printf("antext_settext_addr_offset: %p\n", antext_settext_addr_offset);

			MH_CreateHook(antext_settext_addr_offset, antext_settext_hook, &antext_settext_orig);
			MH_EnableHook(antext_settext_addr_offset);


			auto antext_gettext_addr = il2cpp_class_get_method_from_name(antext_class, "get_Text", 0)->methodPointer;
			printf("antext_gettext_addr: %p\n", antext_gettext_addr);

			auto antext_gettext_addr_offset = reinterpret_cast<void*>(antext_gettext_addr);
			printf("antext_gettext_addr_offset: %p\n", antext_gettext_addr_offset);

			MH_CreateHook(antext_gettext_addr_offset, antext_gettext_hook, &antext_gettext_orig);
			MH_EnableHook(antext_gettext_addr_offset);


			auto antext_getfixtext_addr = il2cpp_class_get_method_from_name(antext_class, "get_FixText", 0)->methodPointer;
			printf("antext_getfixtext_addr: %p\n", antext_getfixtext_addr);

			auto antext_getfixtext_addr_offset = reinterpret_cast<void*>(antext_getfixtext_addr);
			printf("antext_getfixtext_addr_offset: %p\n", antext_getfixtext_addr_offset);

			MH_CreateHook(antext_getfixtext_addr_offset, antext_getfixtext_hook, &antext_getfixtext_orig);
			MH_EnableHook(antext_getfixtext_addr_offset);


			auto antext_getfixtext_wrt_addr = il2cpp_class_get_method_from_name(antext_class, "get_FixTextWithoutRichText", 0)->methodPointer;
			printf("antext_getfixtext_wrt_addr: %p\n", antext_getfixtext_wrt_addr);

			auto antext_getfixtext_wrt_addr_offset = reinterpret_cast<void*>(antext_getfixtext_wrt_addr);
			printf("antext_getfixtext_wrt_addr_offset: %p\n", antext_getfixtext_wrt_addr_offset);

			MH_CreateHook(antext_getfixtext_wrt_addr_offset, antext_getfixtext_wrt_hook, &antext_getfixtext_wrt_orig);
			MH_EnableHook(antext_getfixtext_wrt_addr_offset);




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



			auto libnative_runtime_assembly = il2cpp_domain_assembly_open(domain, "LibNative.Runtime.dll");
			printf("libnative_runtime_assembly: %p\n", libnative_runtime_assembly);

			auto libnative_runtime_image = il2cpp_assembly_get_image(libnative_runtime_assembly);
			printf("libnative_runtime_image: %p\n", libnative_runtime_image);

			auto connection_class = il2cpp_class_from_name(libnative_runtime_image, "LibNative.Sqlite3", "Connection");
			printf("Connection: %p\n", connection_class);

			auto connection_exec_addr = il2cpp_class_get_method_from_name(connection_class, "Exec", 1)->methodPointer;
			printf("connection_exec_addr: %p\n", connection_exec_addr);

			auto connection_exec_addr_offset = reinterpret_cast<void*>(connection_exec_addr);
			printf("connection_exec_addr_offset: %p\n", connection_exec_addr_offset);

			MH_CreateHook(connection_exec_addr_offset, connection_exec_hook, &connection_exec_orig);
			MH_EnableHook(connection_exec_addr_offset);


			auto connection_query_addr = il2cpp_class_get_method_from_name(connection_class, "Query", 1)->methodPointer;
			printf("connection_query_addr: %p\n", connection_query_addr);

			auto connection_query_addr_offset = reinterpret_cast<void*>(connection_query_addr);
			printf("connection_query_addr_offset: %p\n", connection_query_addr_offset);

			MH_CreateHook(connection_query_addr_offset, connection_query_hook, &connection_query_orig);
			MH_EnableHook(connection_query_addr_offset);


			auto connection_preparedquery_addr = il2cpp_class_get_method_from_name(connection_class, "PreparedQuery", 1)->methodPointer;
			printf("connection_preparedquery_addr: %p\n", connection_preparedquery_addr);

			auto connection_preparedquery_addr_offset = reinterpret_cast<void*>(connection_preparedquery_addr);
			printf("connection_preparedquery_addr_offset: %p\n", connection_preparedquery_addr_offset);

			MH_CreateHook(connection_preparedquery_addr_offset, connection_preparedquery_hook, &connection_preparedquery_orig);
			MH_EnableHook(connection_preparedquery_addr_offset);

			// auto query_class = il2cpp_class_from_name(libnative_runtime_image, "LibNative.Sqlite3", "PreparedQuery");
			// printf("PreparedQuery: %p\n", query_class);

			// auto query_ctor_addr = il2cpp_class_get_method_from_name(query_class, ".ctor", 2)->methodPointer;
			// printf("query_ctor_addr: %p\n", query_ctor_addr);

			// auto query_ctor_addr_offset = reinterpret_cast<void*>(query_ctor_addr);
			// printf("query_ctor_addr_offset: %p\n", query_ctor_addr_offset);

			// MH_CreateHook(query_ctor_addr_offset, query_ctor_hook, &query_ctor_orig);
			// MH_EnableHook(query_ctor_addr_offset);


			auto cyan_assembly = il2cpp_domain_assembly_open(domain, "_Cyan.dll");
			printf("cyan_assembly: %p\n", cyan_assembly);

			auto cyan_image = il2cpp_assembly_get_image(cyan_assembly);
			printf("cyan_image: %p\n", cyan_image);

			auto manifestdb_class = il2cpp_class_from_name(cyan_image, "Cyan.Manifest", "ManifestDB");
			printf("ManifestDB: %p\n", manifestdb_class);

			auto manifest_exec_addr = il2cpp_class_get_method_from_name(manifestdb_class, "Exec", 1)->methodPointer;
			printf("manifest_exec_addr: %p\n", manifest_exec_addr);

			auto manifest_exec_addr_offset = reinterpret_cast<void*>(manifest_exec_addr);
			printf("manifest_exec_addr_offset: %p\n", manifest_exec_addr_offset);

			MH_CreateHook(manifest_exec_addr_offset, manifest_exec_hook, &manifest_exec_orig);
			MH_EnableHook(manifest_exec_addr_offset);


			auto manifestdbadapter_class = il2cpp_class_from_name(cyan_image, "Cyan.Manifest", "ManifestDBAdapter");
			printf("ManifestDBAdapter: %p\n", manifestdbadapter_class);

			auto db_adapter_query_addr = il2cpp_class_get_method_from_name(manifestdbadapter_class, "Query", 1)->methodPointer;
			printf("db_adapter_query_addr: %p\n", db_adapter_query_addr);

			auto db_adapter_query_addr_offset = reinterpret_cast<void*>(db_adapter_query_addr);
			printf("db_adapter_query_addr_offset: %p\n", db_adapter_query_addr_offset);

			MH_CreateHook(db_adapter_query_addr_offset, db_adapter_query_hook, &db_adapter_query_orig);
			MH_EnableHook(db_adapter_query_addr_offset);

			
			auto db_adapter_preparedquery_addr = il2cpp_class_get_method_from_name(manifestdbadapter_class, "PreparedQuery", 1)->methodPointer;
			printf("db_adapter_preparedquery_addr: %p\n", db_adapter_preparedquery_addr);

			auto db_adapter_preparedquery_addr_offset = reinterpret_cast<void*>(db_adapter_preparedquery_addr);
			printf("db_adapter_preparedquery_addr_offset: %p\n", db_adapter_preparedquery_addr_offset);

			MH_CreateHook(db_adapter_preparedquery_addr_offset, db_adapter_preparedquery_hook, &db_adapter_preparedquery_orig);
			MH_EnableHook(db_adapter_preparedquery_addr_offset);


			auto cyan_preparedquery_class = il2cpp_class_from_name(cyan_image, "Cyan.Manifest", "PreparedQuery");
			printf("PreparedQuery: %p\n", cyan_preparedquery_class);

			auto cyan_preparedquery_ctor_addr = il2cpp_class_get_method_from_name(cyan_preparedquery_class, ".ctor", 2)->methodPointer;
			printf("cyan_preparedquery_ctor_addr: %p\n", cyan_preparedquery_ctor_addr);

			auto cyan_preparedquery_ctor_addr_offset = reinterpret_cast<void*>(cyan_preparedquery_ctor_addr);
			printf("cyan_preparedquery_ctor_addr_offset: %p\n", cyan_preparedquery_ctor_addr_offset);

			MH_CreateHook(cyan_preparedquery_ctor_addr_offset, query_ctor_hook, &query_ctor_orig);
			MH_EnableHook(cyan_preparedquery_ctor_addr_offset);


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



			// const auto turncountera2u_class = il2cpp_class_from_name(uma_image, "Gallop", "SingleModeMainViewHeaderTurnCounterA2U");
			// printf("turncountera2u_class: %p\n", turncountera2u_class);

			// const char* name = "TurnCounter";
			// const auto turncountera2u_turncounter_class = find_nested_class_by_name(turncountera2u_class, name);
			

			const auto single_header_model_class = il2cpp_class_from_name(uma_image, "Gallop", "SingleModeMainViewHeaderModel");
			printf("single_header_model_class: %p\n", single_header_model_class);

			auto get_scen_race_name_addr = il2cpp_class_get_method_from_name(single_header_model_class, "GetScenarioRaceName", 0)->methodPointer;
			printf("get_scen_race_name_addr: %p\n", get_scen_race_name_addr);

			auto get_scen_race_name_addr_offset = reinterpret_cast<void*>(get_scen_race_name_addr);
			printf("get_scen_race_name_addr_offset: %p\n", get_scen_race_name_addr_offset);

			MH_CreateHook(get_scen_race_name_addr_offset, get_scen_race_name_hook, &get_scen_race_name_orig);
			MH_EnableHook(get_scen_race_name_addr_offset);


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

			auto trainingparamchangea2u_class = il2cpp_class_from_name(uma_image, "Gallop", "TrainingParamChangeA2U");
			printf("trainingparamchangea2u_class: %p\n", trainingparamchangea2u_class);

			auto tpca2u_getcaptiontext_addr = il2cpp_class_get_method_from_name(trainingparamchangea2u_class, "GetCaptionText", 1)->methodPointer;
			printf("tpca2u_getcaptiontext_addr: %p\n", tpca2u_getcaptiontext_addr);

			auto tpca2u_getcaptiontext_addr_offset = reinterpret_cast<void*>(tpca2u_getcaptiontext_addr);
			printf("tpca2u_getcaptiontext_addr_offset: %p\n", tpca2u_getcaptiontext_addr_offset);

			MH_CreateHook(tpca2u_getcaptiontext_addr_offset, tpca2u_getcaptiontext_hook, &tpca2u_getcaptiontext_orig);
			MH_EnableHook(tpca2u_getcaptiontext_addr_offset);


			auto tpca2u_getskillcaptiontext_addr = il2cpp_class_get_method_from_name(trainingparamchangea2u_class, "GetSkillCaptionText", 2)->methodPointer;
			printf("tpca2u_getskillcaptiontext_addr: %p\n", tpca2u_getskillcaptiontext_addr);

			auto tpca2u_getskillcaptiontext_addr_offset = reinterpret_cast<void*>(tpca2u_getskillcaptiontext_addr);
			printf("tpca2u_getskillcaptiontext_addr_offset: %p\n", tpca2u_getskillcaptiontext_addr_offset);

			MH_CreateHook(tpca2u_getskillcaptiontext_addr_offset, tpca2u_getskillcaptiontext_hook, &tpca2u_getskillcaptiontext_orig);
			MH_EnableHook(tpca2u_getskillcaptiontext_addr_offset);


			// auto icustomtextcomponent_class = il2cpp_class_from_name(uma_image, "Gallop", "ICustomTextComponent");
			// printf("ICustomTextComponent: %p\n", icustomtextcomponent_class);

			// // Check if tags need to be removed in these two functions
			// auto icustomtextcomponent_settext_addr = il2cpp_class_get_method_from_name(icustomtextcomponent_class, "set_text", 1)->methodPointer;
			// printf("icustomtextcomponent_settext_addr: %p\n", icustomtextcomponent_settext_addr);

			// auto icustomtextcomponent_settext_addr_offset = reinterpret_cast<void*>(icustomtextcomponent_settext_addr);
			// printf("icustomtextcomponent_settext_addr_offset: %p\n", icustomtextcomponent_settext_addr_offset);

			// MH_CreateHook(icustomtextcomponent_settext_addr_offset, icustomtextcomponent_settext_hook, &icustomtextcomponent_settext_orig);
			// MH_EnableHook(icustomtextcomponent_settext_addr_offset);

			// auto icustomtextcomponent_settextfrom_addr = il2cpp_class_get_method_from_name(icustomtextcomponent_class, "SetTextFromController", 1)->methodPointer;
			// printf("icustomtextcomponent_settextfrom_addr: %p\n", icustomtextcomponent_settextfrom_addr);

			// auto icustomtextcomponent_settextfrom_addr_offset = reinterpret_cast<void*>(icustomtextcomponent_settextfrom_addr);
			// printf("icustomtextcomponent_settextfrom_addr_offset: %p\n", icustomtextcomponent_settextfrom_addr_offset);

			// MH_CreateHook(icustomtextcomponent_settextfrom_addr_offset, icustomtextcomponent_settextfrom_hook, &icustomtextcomponent_settextfrom_orig);
			// MH_EnableHook(icustomtextcomponent_settextfrom_addr_offset);



			// auto icustomtextcomponent_gettext_addr = il2cpp_class_get_method_from_name(icustomtextcomponent_class, "get_text", 0)->methodPointer;
			// printf("icustomtextcomponent_gettext_addr: %p\n", icustomtextcomponent_gettext_addr);

			// auto icustomtextcomponent_gettext_addr_offset = reinterpret_cast<void*>(icustomtextcomponent_gettext_addr);
			// printf("icustomtextcomponent_gettext_addr_offset: %p\n", icustomtextcomponent_gettext_addr_offset);

			// MH_CreateHook(icustomtextcomponent_gettext_addr_offset, icustomtextcomponent_gettext_hook, &icustomtextcomponent_gettext_orig);
			// MH_EnableHook(icustomtextcomponent_gettext_addr_offset);



			auto training_cutscene_controller_class = il2cpp_class_from_name(uma_image, "Gallop", "SingleModeMainTrainingCuttController");
			printf("training_cutscene_controller_class: %p\n", training_cutscene_controller_class);

			auto tcc_get_text_list_addr = il2cpp_class_get_method_from_name(training_cutscene_controller_class, "GetTrainingEffectMessageWindowTextList", 0)->methodPointer;
			printf("tcc_get_text_list_addr: %p\n", tcc_get_text_list_addr);

			auto tcc_get_text_list_addr_offset = reinterpret_cast<void*>(tcc_get_text_list_addr);
			printf("tcc_get_text_list_addr_offset: %p\n", tcc_get_text_list_addr_offset);

			MH_CreateHook(tcc_get_text_list_addr_offset, tcc_get_text_list_hook, &tcc_get_text_list_orig);
			MH_EnableHook(tcc_get_text_list_addr_offset);


			auto tcc_play_cut_addr = il2cpp_class_get_method_from_name(training_cutscene_controller_class, "PlayTrainingCut", 1)->methodPointer;
			printf("tcc_play_cut_addr: %p\n", tcc_play_cut_addr);

			auto tcc_play_cut_addr_offset = reinterpret_cast<void*>(tcc_play_cut_addr);
			printf("tcc_play_cut_addr_offset: %p\n", tcc_play_cut_addr_offset);

			MH_CreateHook(tcc_play_cut_addr_offset, tcc_play_cut_hook, &tcc_play_cut_orig);
			MH_EnableHook(tcc_play_cut_addr_offset);


			import_translations();

			bootstrap_carrot_juicer();

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
