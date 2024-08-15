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

	il2cpp_object_get_class_t il2cpp_object_get_class;
	il2cpp_object_get_size_t il2cpp_object_get_size;
	il2cpp_class_get_name_t il2cpp_class_get_name;
	il2cpp_class_get_namespace_t il2cpp_class_get_namespace;

	HHOOK keyboard_hook;

	std::map<int, std::string> text_id_to_string;
	std::map<std::string, std::string> text_id_string_to_translation;
	std::map<std::string, std::string> text_id_string_to_original;
	bool tl_first_check = true;
	std::filesystem::file_time_type tl_last_modified;
	std::set<Il2CppString*> stringid_pointers;
	int debug_level = 0;
	int max_debug_level = 3;
	void* last_text_list_ptr = nullptr;
	uintptr_t last_story_timeline_controller = 0;

	std::set<std::string> do_not_replace_strings = {
		"SingleMode418033"
	};

	const std::vector<std::string> training_result_array = {
		"at Gallop.SingleModeMainTrainingCuttController.GetTrainingEffectMessageWindowTextList",
		"at Gallop.TrainingParamChangeUI.GetMessageText",
		"at Gallop.StoryEventConclusion",
		"at Gallop.SingleModeScenarioCookCookedDishModel.CreateChangeParameterInfoAtCooked",
		"at Gallop.SingleModeScenarioCookDishModel+SingleModeCookDishEffect.get_TrainingParamChangeUIText",
		"at Gallop.TrainingParamChangeUI.OnNextTypewrite",
		"at Gallop.TrainingParamChangeUI.StartTypewrite",
		"at Gallop.PartsSingleModeScenarioCookGetMaterialIconFlashPlayer.PlayGetMaterialIcon"
	};

	const std::vector<std::string> valid_textcommon_classes = {
		"TextCommon",
		"BitmapTextCommon"
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
		replaceAll(in_str, "<story>", "");
		replaceAll(in_str, "<rbr>", "");
		replaceAll(in_str, "<br>", "");
		replaceAll(in_str, "<force>", "");
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
		removePropertyTag(in_str, "p");
		removePropertyTag(in_str, "ord");
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
		std::string file_name = "translations.txt";

		if (!file_exists(file_name))
		{
			// printf("No translations.txt found\n");
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
				// printf("No changes to translations.txt\n");
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
		const int ret = reinterpret_cast<decltype(LZ4_decompress_safe_ext_hook)*>(LZ4_decompress_safe_ext_orig)(
			src, dst, compressedSize, dstCapacity);

		std::filesystem::create_directory("CarrotJuicer");
		const auto out_path = std::string("CarrotJuicer\\").append(current_time()).append("R.msgpack");
		write_file(out_path, dst, ret);
		std::cout << "wrote response to " << out_path << "\n";

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

		std::filesystem::create_directory("CarrotJuicer");
		const auto out_path = std::string("CarrotJuicer\\").append(current_time()).append("Q.msgpack");
		write_file(out_path, src, srcSize);
		std::cout << "wrote request to " << out_path << "\n";

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
		while (str.find("<ord=") != std::string::npos)
		{
			auto start = str.find("<ord=");
			auto end = str.find(">", start);

			std::string param = getPropertyTag(str, "ord");

			std::string substr = param;
			std::string out_str = "th";

			if (substr == "1")
			{
				out_str = "st";
			}
			else if (substr == "2")
			{
				out_str = "nd";
			}
			else if (substr == "3")
			{
				out_str = "rd";
			}

			else if (substr.length() > 1)
			{
				if (substr[substr.length() - 2] == '1')
				{
					out_str = "th";
				}
				else
				{
					if (substr.back() == '1')
					{
						out_str = "st";
					}
					else if (substr.back() == '2')
					{
						out_str = "nd";
					}
					else if (substr.back() == '3')
					{
						out_str = "rd";
					}
					else
					{
						out_str = "th";
					}
				}
			}

			str.replace(start, end - start + 1, out_str);
		}

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


	std::string handle_plurals(std::string str){
		// Plurals are like this <p=turn,turns>10</p>
		while (str.find("<p=") != std::string::npos)
		{
			auto start = str.find("<p=");
			auto end = str.find(">", start);

			std::string param = getPropertyTag(str, "p");

			// Split the param into singular and plural
			std::string singular;
			std::string plural;
			std::string num_str;
			std::istringstream iss(param);
			std::getline(iss, singular, ',');
			std::getline(iss, plural, ',');
			// The rest is the number. Go all the way to the end of the string
			std::getline(iss, num_str, '\0');

			std::string new_part;
			if (num_str.back() == '1')
			{
				new_part = singular;
			} else {
				new_part = plural;
			}

			str.replace(start, end - start + 1, new_part);
		}

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
		str_utf8 = handle_plurals(str_utf8);

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
			Vector2_t tmp_pivot = settings->pivot;
			tmp_pivot.y += 0.08f;
			tmp_pivot.x -= 0.10f;
			settings->pivot = tmp_pivot;
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

		if (debug_level == 3)
		{
			return reinterpret_cast<decltype(populate_with_errors_hook)*>(populate_with_errors_orig)(_this, str, settings, context);
		}

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

		// printf("Draw b: %s\n", str_utf8.c_str());  // Before

		str_utf8 = handle_tags(str_utf8, settings);

		if (debug_level > 0)
		{
			printf("Draw: %s\n", str_utf8.c_str());  // After
			// printf("horizonalOverflow: %d\n", settings->horizontalOverflow);
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
		// printf("textcommon_gettextid_string_hook: %p\n", _this);
		return reinterpret_cast<decltype(textcommon_gettextid_string_hook)*>(textcommon_gettextid_string_orig)(_this);
	}

	void* localize_jp_get_orig = nullptr;
	Il2CppString* localize_jp_get_hook(int id)
	{
		// printf("localize_jp_get_hook\n");

		Il2CppString* out_text = reinterpret_cast<decltype(localize_jp_get_hook)*>(localize_jp_get_orig)(id);

		if (debug_level == 3)
		{
			return out_text;
		}

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
		std::string translation;

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
				if (debug_level > 1)
				{
					out_text = il2cpp_string_new((textid_string + "<debug>" + il2cppstring_to_utf8(out_text->start_char)).data());
				}
			} else
			{
				translation = text_id_string_to_translation[textid_string];
				// printf("Translation: %s\n", translation.c_str());

				if (debug_level > 1)
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
			if (debug_level > 1){
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

		if (debug_level > 1 && no_print_ids.find(id) == no_print_ids.end())
		{
			// translation = il2cppstring_to_utf8(out_text->start_char);
			// if (translation.find("<force>") != std::string::npos)
			// {
			// 	stacktrace();
			// }
			printf("Fetch %d: %s\n", id, il2cppstring_to_utf8(out_text->start_char).c_str());
		}

		// TODO: This kinda sucks, maybe there's a better way?
		std::string utf8_str = il2cppstring_to_utf8(out_text->start_char);

		if (in_stacktrace("at Gallop.SingleModeMainTrainingController.OnSelectTraining "))
		{
			remove_all_tags(utf8_str);
			out_text = il2cpp_string_new(utf8_str.data());
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
		debug_level = 0;
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

		if (_this == nullptr)
		{
			return orig_text;
		}

		if (debug_level == 3)
		{
			return orig_text;
		}

		// printf("textcommon_gettext_hook: %s\n", il2cppstring_to_utf8(orig_text->start_char).c_str());

		// return orig_text;
		// stacktrace();

		// printf("a\n");
		
		std::string orig_text_utf8 = il2cppstring_to_utf8(orig_text->start_char);
		std::string orig_text_json = il2cppstring_to_jsonstring(orig_text->start_char);

		auto* this_class = il2cpp_object_get_class(_this);
		char* this_class_name = il2cpp_class_get_name(this_class);
		// printf("Class: %s\n", this_class_name);

		if (std::find(valid_textcommon_classes.begin(), valid_textcommon_classes.end(), this_class_name) == valid_textcommon_classes.end())
		{
			// printf("SKIP\n");
			return orig_text;
		}

		// printf("GO\n");

		// Check if _this is valid
		auto bad = IsBadReadPtr(_this, 8);
		if (bad)
		{
			printf("Bad read ptr\n");
			return orig_text;
		}

		Il2CppString* textid_string = nullptr;
		try
		{
			textid_string = textcommon_gettextid_string_hook(_this);
		}
		catch (...)
		{
			printf("Exception caught\n");
			return orig_text;
		}

		if (textid_string == nullptr)
		{
			return orig_text;
		}
		

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
		
		if (debug_level > 1)
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
				// printf("Found translation for %s\n", textid_string.c_str());
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

	// void* antext_settext_orig = nullptr;
	// void* antext_settext_hook(void* _this, Il2CppString* text)
	// {
	// 	printf("antext_settext_hook: %s\n", il2cppstring_to_utf8(text->start_char).c_str());
	// 	return reinterpret_cast<decltype(antext_settext_hook)*>(antext_settext_orig)(_this, text);
	// }

	// void* antext_gettext_orig = nullptr;
	// void* antext_gettext_hook(void* _this)
	// {
	// 	// printf("antext_gettext_hook\n");
	// 	void* ret = reinterpret_cast<decltype(antext_gettext_hook)*>(antext_gettext_orig)(_this);
	// 	return ret;
	
	// }

	// void* antext_getfixtext_orig = nullptr;
	// void* antext_getfixtext_hook(void* _this)
	// {
	// 	// printf("antext_getfixtext_hook\n");
	// 	void* ret = reinterpret_cast<decltype(antext_getfixtext_hook)*>(antext_getfixtext_orig)(_this);
	// 	return ret;
	
	// }

	// void* antext_getfixtext_wrt_orig = nullptr;
	// void* antext_getfixtext_wrt_hook(void* _this)
	// {
	// 	// printf("antext_getfixtext_wrt_hook\n");
	// 	void* ret = reinterpret_cast<decltype(antext_getfixtext_wrt_hook)*>(antext_getfixtext_wrt_orig)(_this);
	// 	return ret;
	
	// }

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

	void* stc_initialize_orig = nullptr;
	void* stc_initialize_hook(uintptr_t _this)
	{
		// printf("stc_initialize_hook\n");
		void* ret = reinterpret_cast<decltype(stc_initialize_hook)*>(stc_initialize_orig)(_this);
		return ret;
	}

	void* stc_release_orig = nullptr;
	void* stc_release_hook(uintptr_t _this)
	{
		// printf("stc_release_hook\n");
		if (last_story_timeline_controller == _this){
			last_story_timeline_controller = 0;
		}
		void* ret = reinterpret_cast<decltype(stc_release_hook)*>(stc_release_orig)(_this);
		return ret;
	}

	void* stc_onendstory_orig = nullptr;
	void* stc_onendstory_hook(uintptr_t _this)
	{
		// printf("stc_onendstory_hook\n");
		if (last_story_timeline_controller == _this){
			last_story_timeline_controller = 0;
		}
		void* ret = reinterpret_cast<decltype(stc_onendstory_hook)*>(stc_onendstory_orig)(_this);
		return ret;
	}

	void* stc_gotoblock_orig = nullptr;
	void* stc_gotoblock_hook(uintptr_t _this, int block_id, bool weakenCySpring, bool isUpdate, bool isChoice)
	{
		// printf("stc_gotoblock_hook\n");
		// printf("block_id: %d\n", block_id);
		// printf("weakenCySpring: %d\n", weakenCySpring);
		// printf("isUpdate: %d\n", isUpdate);
		// printf("isChoice: %d\n", isChoice);
		last_story_timeline_controller = _this;
		void* ret = reinterpret_cast<decltype(stc_gotoblock_hook)*>(stc_gotoblock_orig)(_this, block_id, weakenCySpring, isUpdate, isChoice);
		return ret;
	}

	void* stc_gotoblockforskip_orig = nullptr;
	void* stc_gotoblockforskip_hook(void* _this, int block_id, void* weakenCySpring)
	{
		// printf("stc_gotoblockforskip_hook\n");
		void* ret = reinterpret_cast<decltype(stc_gotoblockforskip_hook)*>(stc_gotoblockforskip_orig)(_this, block_id, weakenCySpring);
		return ret;
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
			// If folder CarrotJuicer exists, delete it recursively
			std::string folder_name = "CarrotJuicer";
			if (std::filesystem::exists(folder_name))
			{
				std::filesystem::remove_all(folder_name);
			}
			std::filesystem::create_directory(folder_name);


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
			il2cpp_object_get_class = reinterpret_cast<il2cpp_object_get_class_t>(GetProcAddress(game_assembly_module, "il2cpp_object_get_class"));
 			il2cpp_object_get_size = reinterpret_cast<il2cpp_object_get_size_t>(GetProcAddress(game_assembly_module, "il2cpp_object_get_size"));
			il2cpp_class_get_name = reinterpret_cast<il2cpp_class_get_name_t>(GetProcAddress(game_assembly_module, "il2cpp_class_get_name"));
			il2cpp_class_get_namespace = reinterpret_cast<il2cpp_class_get_namespace_t>(GetProcAddress(game_assembly_module, "il2cpp_class_get_namespace"));

			auto domain = il2cpp_domain_get();
			// Print domain
			printf("Domain: %p\n", domain);


			auto mscorlib_assembly = il2cpp_domain_assembly_open(domain, "mscorlib.dll");
			// printf("mscorlib Assembly: %p\n", mscorlib_assembly);
			auto mscorlib_image = il2cpp_assembly_get_image(mscorlib_assembly);
			// printf("mscorlib Image: %p\n", mscorlib_image);

			auto environment_class = il2cpp_class_from_name(mscorlib_image, "System", "Environment");
			// printf("Environment: %p\n", environment_class);

			auto stack_trace_addr = il2cpp_class_get_method_from_name(environment_class, "get_StackTrace", 0)->methodPointer;
			environment_get_stacktrace = reinterpret_cast<decltype(environment_get_stacktrace)>(stack_trace_addr);


			auto ienumerator_class = il2cpp_class_from_name(mscorlib_image, "System.Collections", "IEnumerator");
			// printf("IEnumerator: %p\n", ienumerator_class);

			auto move_next_addr = il2cpp_class_get_method_from_name(ienumerator_class, "MoveNext", 0)->methodPointer;
			// printf("MoveNext: %p\n", move_next_addr);

			auto move_next_addr_offset = reinterpret_cast<void*>(move_next_addr);
			// printf("MoveNext Offset: %p\n", move_next_addr_offset);

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
			// printf("Unity Core Image: %p\n", unity_core_image);

			auto object_class = il2cpp_class_from_name(unity_core_image, "UnityEngine", "Object");
			// printf("Object: %p\n", object_class);

			auto to_string_addr = il2cpp_class_get_method_from_name(object_class, "ToString", 0)->methodPointer;
			// printf("to_string_addr: %p\n", to_string_addr);

			auto to_string_addr_offset = reinterpret_cast<void*>(to_string_addr);
			printf("to_string_addr_offset: %p\n", to_string_addr_offset);

			MH_CreateHook(to_string_addr_offset, to_string_hook, &to_string_orig);
			MH_EnableHook(to_string_addr_offset);


			// auto set_name_addr = il2cpp_class_get_method_from_name(object_class, "set_name", 1)->methodPointer;
			// printf("set_name_addr: %p\n", set_name_addr);

			// auto set_name_addr_offset = reinterpret_cast<void*>(set_name_addr);
			// printf("set_name_addr_offset: %p\n", set_name_addr_offset);

			// MH_CreateHook(set_name_addr_offset, set_name_hook, &set_name_orig);
			// MH_EnableHook(set_name_addr_offset);




			// auto plugins_assembly = il2cpp_domain_assembly_open(domain, "Plugins.dll");
			// // printf("Plugins Assembly: %p\n", plugins_assembly);
			// auto plugins_image = il2cpp_assembly_get_image(plugins_assembly);
			// // printf("Plugins Image: %p\n", plugins_image);

			// auto antext_class = il2cpp_class_from_name(plugins_image, "AnimateToUnity", "AnText");
			// // printf("AnText: %p\n", antext_class);

			// auto antext_settext_addr = il2cpp_class_get_method_from_name(antext_class, "SetText", 1)->methodPointer;
			// // printf("antext_settext_addr: %p\n", antext_settext_addr);

			// auto antext_settext_addr_offset = reinterpret_cast<void*>(antext_settext_addr);
			// // printf("antext_settext_addr_offset: %p\n", antext_settext_addr_offset);

			// MH_CreateHook(antext_settext_addr_offset, antext_settext_hook, &antext_settext_orig);
			// MH_EnableHook(antext_settext_addr_offset);


			// auto antext_gettext_addr = il2cpp_class_get_method_from_name(antext_class, "get_Text", 0)->methodPointer;
			// // printf("antext_gettext_addr: %p\n", antext_gettext_addr);

			// auto antext_gettext_addr_offset = reinterpret_cast<void*>(antext_gettext_addr);
			// // printf("antext_gettext_addr_offset: %p\n", antext_gettext_addr_offset);

			// MH_CreateHook(antext_gettext_addr_offset, antext_gettext_hook, &antext_gettext_orig);
			// MH_EnableHook(antext_gettext_addr_offset);


			// auto antext_getfixtext_addr = il2cpp_class_get_method_from_name(antext_class, "get_FixText", 0)->methodPointer;
			// // printf("antext_getfixtext_addr: %p\n", antext_getfixtext_addr);

			// auto antext_getfixtext_addr_offset = reinterpret_cast<void*>(antext_getfixtext_addr);
			// // printf("antext_getfixtext_addr_offset: %p\n", antext_getfixtext_addr_offset);

			// MH_CreateHook(antext_getfixtext_addr_offset, antext_getfixtext_hook, &antext_getfixtext_orig);
			// MH_EnableHook(antext_getfixtext_addr_offset);


			// auto antext_getfixtext_wrt_addr = il2cpp_class_get_method_from_name(antext_class, "get_FixTextWithoutRichText", 0)->methodPointer;
			// // printf("antext_getfixtext_wrt_addr: %p\n", antext_getfixtext_wrt_addr);

			// auto antext_getfixtext_wrt_addr_offset = reinterpret_cast<void*>(antext_getfixtext_wrt_addr);
			// // printf("antext_getfixtext_wrt_addr_offset: %p\n", antext_getfixtext_wrt_addr_offset);

			// MH_CreateHook(antext_getfixtext_wrt_addr_offset, antext_getfixtext_wrt_hook, &antext_getfixtext_wrt_orig);
			// MH_EnableHook(antext_getfixtext_wrt_addr_offset);




			auto assembly2 = il2cpp_domain_assembly_open(domain, "UnityEngine.TextRenderingModule.dll");
			printf("TextRenderingModule Assembly: %p\n", assembly2);
			auto image2 = il2cpp_assembly_get_image(assembly2);
			// printf("Image2: %p\n", image2);
			auto text_generator_class = il2cpp_class_from_name(image2, "UnityEngine", "TextGenerator");
			// printf("TextGenerator: %p\n", text_generator_class);
			auto populate_with_errors_addr = il2cpp_class_get_method_from_name(text_generator_class, "PopulateWithErrors", 3)->methodPointer;
			printf("populate_with_errors_addr: %p\n", populate_with_errors_addr);
			auto populate_with_errors_addr_offset = reinterpret_cast<void*>(populate_with_errors_addr);

			MH_CreateHook(populate_with_errors_addr_offset, populate_with_errors_hook, &populate_with_errors_orig);
			MH_EnableHook(populate_with_errors_addr_offset);


			auto populate_addr = il2cpp_class_get_method_from_name(text_generator_class, "Populate", 2)->methodPointer;
			// printf("populate_addr: %p\n", populate_addr);
			auto populate_addr_offset = reinterpret_cast<void*>(populate_addr);

			MH_CreateHook(populate_addr_offset, populate_hook, &populate_orig);
			MH_EnableHook(populate_addr_offset);


			// Uma Assembly
			auto uma_assembly = il2cpp_domain_assembly_open(domain, "umamusume.dll");
			printf("uma_assembly: %p\n", uma_assembly);

			auto uma_image = il2cpp_assembly_get_image(uma_assembly);
			// printf("uma_image: %p\n", uma_image);


			auto uimanager_class = il2cpp_class_from_name(uma_image, "Gallop", "UIManager");
			// printf("UIManager: %p\n", uimanager_class);

			// print_class_methods_with_types(uimanager_class);

			const Il2CppTypeEnum types[2] = { Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE, Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE };
			auto uimanager_SetHeaderTitleText1_addr = find_class_method_with_name_and_types(uimanager_class, "SetHeaderTitleText", types);
			// printf("uimanager_SetHeaderTitleText1_addr: %p\n", uimanager_SetHeaderTitleText1_addr);

			auto uimanager_SetHeaderTitleText1_addr_offset = reinterpret_cast<void*>(uimanager_SetHeaderTitleText1_addr);
			// printf("uimanager_SetHeaderTitleText1_addr_offset: %p\n", uimanager_SetHeaderTitleText1_addr_offset);

			MH_CreateHook(uimanager_SetHeaderTitleText1_addr_offset, uimanager_SetHeaderTitleText1_hook, &uimanager_SetHeaderTitleText1_orig);
			MH_EnableHook(uimanager_SetHeaderTitleText1_addr_offset);


			const Il2CppTypeEnum types2[2] = { Il2CppTypeEnum::IL2CPP_TYPE_STRING, Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE };
			auto uimanager_SetHeaderTitleText2_addr = find_class_method_with_name_and_types(uimanager_class, "SetHeaderTitleText", types2);
			// printf("uimanager_SetHeaderTitleText2_addr: %p\n", uimanager_SetHeaderTitleText2_addr);

			auto uimanager_SetHeaderTitleText2_addr_offset = reinterpret_cast<void*>(uimanager_SetHeaderTitleText2_addr);
			// printf("uimanager_SetHeaderTitleText2_addr_offset: %p\n", uimanager_SetHeaderTitleText2_addr_offset);

			MH_CreateHook(uimanager_SetHeaderTitleText2_addr_offset, uimanager_SetHeaderTitleText2_hook, &uimanager_SetHeaderTitleText2_orig);
			MH_EnableHook(uimanager_SetHeaderTitleText2_addr_offset);


			// LocalizeJP
			const auto localize_class = il2cpp_class_from_name(uma_image, "Gallop", "Localize");
			printf("LocalizeJP: %p\n", localize_class);

			const char* name = "JP";
			const auto localize_jp_class = find_nested_class_by_name(localize_class, name);
			printf("LocalizeJP: %p\n", localize_jp_class);

			auto localize_jp_get_addr = il2cpp_class_get_method_from_name(localize_jp_class, "Get", 1)->methodPointer;
			// printf("localize_jp_get_addr: %p\n", localize_jp_get_addr);

			auto localize_jp_get_addr_offset = reinterpret_cast<void*>(localize_jp_get_addr);
			printf("localize_jp_get_addr_offset: %p\n", localize_jp_get_addr_offset);

			MH_CreateHook(localize_jp_get_addr_offset, localize_jp_get_hook, &localize_jp_get_orig);
			MH_EnableHook(localize_jp_get_addr_offset);



			// const auto turncountera2u_class = il2cpp_class_from_name(uma_image, "Gallop", "SingleModeMainViewHeaderTurnCounterA2U");
			// printf("turncountera2u_class: %p\n", turncountera2u_class);

			// const char* name = "TurnCounter";
			// const auto turncountera2u_turncounter_class = find_nested_class_by_name(turncountera2u_class, name);
			

			const auto single_header_model_class = il2cpp_class_from_name(uma_image, "Gallop", "SingleModeMainViewHeaderModel");
			// printf("single_header_model_class: %p\n", single_header_model_class);

			auto get_scen_race_name_addr = il2cpp_class_get_method_from_name(single_header_model_class, "GetScenarioRaceName", 0)->methodPointer;
			// printf("get_scen_race_name_addr: %p\n", get_scen_race_name_addr);

			auto get_scen_race_name_addr_offset = reinterpret_cast<void*>(get_scen_race_name_addr);
			// printf("get_scen_race_name_addr_offset: %p\n", get_scen_race_name_addr_offset);

			MH_CreateHook(get_scen_race_name_addr_offset, get_scen_race_name_hook, &get_scen_race_name_orig);
			MH_EnableHook(get_scen_race_name_addr_offset);


			// TextCommon
			const auto textcommon_class = il2cpp_class_from_name(uma_image, "Gallop", "TextCommon");
			printf("TextCommon: %p\n", textcommon_class);

			// set_text
			auto textcommon_settext_addr = il2cpp_class_get_method_from_name(textcommon_class, "set_text", 1)->methodPointer;
			// printf("textcommon_settext_addr: %p\n", textcommon_settext_addr);

			auto textcommon_settext_addr_offset = reinterpret_cast<void*>(textcommon_settext_addr);
			printf("textcommon_settext_addr_offset: %p\n", textcommon_settext_addr_offset);

			MH_CreateHook(textcommon_settext_addr_offset, textcommon_settext_hook, &textcommon_settext_orig);
			MH_EnableHook(textcommon_settext_addr_offset);


			// get_textid
			auto textcommon_gettextid_addr = il2cpp_class_get_method_from_name(textcommon_class, "get_TextId", 0)->methodPointer;
			// printf("textcommon_gettextid_addr: %p\n", textcommon_gettextid_addr);

			auto textcommon_gettextid_addr_offset = reinterpret_cast<void*>(textcommon_gettextid_addr);
			printf("textcommon_gettextid_addr_offset: %p\n", textcommon_gettextid_addr_offset);

			MH_CreateHook(textcommon_gettextid_addr_offset, textcommon_gettextid_hook, &textcommon_gettextid_orig);
			MH_EnableHook(textcommon_gettextid_addr_offset);


			// set_textid
			auto textcommon_settextid_addr = il2cpp_class_get_method_from_name(textcommon_class, "set_TextId", 1)->methodPointer;
			// printf("textcommon_settextid_addr: %p\n", textcommon_settextid_addr);

			auto textcommon_settextid_addr_offset = reinterpret_cast<void*>(textcommon_settextid_addr);
			printf("textcommon_settextid_addr_offset: %p\n", textcommon_settextid_addr_offset);

			MH_CreateHook(textcommon_settextid_addr_offset, textcommon_settextid_hook, &textcommon_settextid_orig);
			MH_EnableHook(textcommon_settextid_addr_offset);


			// get_textidstring
			auto textcommon_gettextid_string_addr = il2cpp_class_get_method_from_name(textcommon_class, "get_TextIdString", 0)->methodPointer;
			// printf("textcommon_gettextid_string_addr: %p\n", textcommon_gettextid_string_addr);

			auto textcommon_gettextid_string_addr_offset = reinterpret_cast<void*>(textcommon_gettextid_string_addr);
			printf("textcommon_gettextid_string_addr_offset: %p\n", textcommon_gettextid_string_addr_offset);

			MH_CreateHook(textcommon_gettextid_string_addr_offset, textcommon_gettextid_string_hook, &textcommon_gettextid_string_orig);
			MH_EnableHook(textcommon_gettextid_string_addr_offset);


			// get_text
			auto textcommon_gettext_addr = il2cpp_class_get_method_from_name(textcommon_class, "get_text", 0)->methodPointer;
			// printf("textcommon_gettext_addr: %p\n", textcommon_gettext_addr);

			auto textcommon_gettext_addr_offset = reinterpret_cast<void*>(textcommon_gettext_addr);
			printf("textcommon_gettext_addr_offset: %p\n", textcommon_gettext_addr_offset);

			MH_CreateHook(textcommon_gettext_addr_offset, textcommon_gettext_hook, &textcommon_gettext_orig);
			MH_EnableHook(textcommon_gettext_addr_offset);

			auto trainingparamchangea2u_class = il2cpp_class_from_name(uma_image, "Gallop", "TrainingParamChangeA2U");
			// printf("trainingparamchangea2u_class: %p\n", trainingparamchangea2u_class);

			auto tpca2u_getcaptiontext_addr = il2cpp_class_get_method_from_name(trainingparamchangea2u_class, "GetCaptionText", 1)->methodPointer;
			// printf("tpca2u_getcaptiontext_addr: %p\n", tpca2u_getcaptiontext_addr);

			auto tpca2u_getcaptiontext_addr_offset = reinterpret_cast<void*>(tpca2u_getcaptiontext_addr);
			// printf("tpca2u_getcaptiontext_addr_offset: %p\n", tpca2u_getcaptiontext_addr_offset);

			MH_CreateHook(tpca2u_getcaptiontext_addr_offset, tpca2u_getcaptiontext_hook, &tpca2u_getcaptiontext_orig);
			MH_EnableHook(tpca2u_getcaptiontext_addr_offset);


			auto tpca2u_getskillcaptiontext_addr = il2cpp_class_get_method_from_name(trainingparamchangea2u_class, "GetSkillCaptionText", 2)->methodPointer;
			// printf("tpca2u_getskillcaptiontext_addr: %p\n", tpca2u_getskillcaptiontext_addr);

			auto tpca2u_getskillcaptiontext_addr_offset = reinterpret_cast<void*>(tpca2u_getskillcaptiontext_addr);
			// printf("tpca2u_getskillcaptiontext_addr_offset: %p\n", tpca2u_getskillcaptiontext_addr_offset);

			MH_CreateHook(tpca2u_getskillcaptiontext_addr_offset, tpca2u_getskillcaptiontext_hook, &tpca2u_getskillcaptiontext_orig);
			MH_EnableHook(tpca2u_getskillcaptiontext_addr_offset);


			auto storyTimelineController_class = il2cpp_class_from_name(uma_image, "Gallop", "StoryTimelineController");

			auto stc_initialize = il2cpp_class_get_method_from_name(storyTimelineController_class, "Initialize", 0)->methodPointer;
			// printf("stc_initialize: %p\n", stc_initialize);

			auto stc_initialize_offset = reinterpret_cast<void*>(stc_initialize);
			// printf("stc_initialize_offset: %p\n", stc_initialize_offset);

			MH_CreateHook(stc_initialize_offset, stc_initialize_hook, &stc_initialize_orig);
			MH_EnableHook(stc_initialize_offset);


			auto stc_release = il2cpp_class_get_method_from_name(storyTimelineController_class, "Release", 0)->methodPointer;
			// printf("stc_release: %p\n", stc_release);

			auto stc_release_offset = reinterpret_cast<void*>(stc_release);
			// printf("stc_release_offset: %p\n", stc_release_offset);

			MH_CreateHook(stc_release_offset, stc_release_hook, &stc_release_orig);
			MH_EnableHook(stc_release_offset);


			auto stc_onendstory_addr = il2cpp_class_get_method_from_name(storyTimelineController_class, "OnEndStory", 0)->methodPointer;

			auto stc_onendstory_offset = reinterpret_cast<void*>(stc_onendstory_addr);

			MH_CreateHook(stc_onendstory_offset, stc_onendstory_hook, &stc_onendstory_orig);
			MH_EnableHook(stc_onendstory_offset);


			auto stc_gotoblock_addr = il2cpp_class_get_method_from_name(storyTimelineController_class, "GotoBlock", 4)->methodPointer;

			auto stc_gotoblock_offset = reinterpret_cast<void*>(stc_gotoblock_addr);

			MH_CreateHook(stc_gotoblock_offset, stc_gotoblock_hook, &stc_gotoblock_orig);
			MH_EnableHook(stc_gotoblock_offset);


			auto stc_gotoblockforskip_addr = il2cpp_class_get_method_from_name(storyTimelineController_class, "GotoBlockForSkip", 2)->methodPointer;

			auto stc_gotoblockforskip_offset = reinterpret_cast<void*>(stc_gotoblockforskip_addr);

			MH_CreateHook(stc_gotoblockforskip_offset, stc_gotoblockforskip_hook, &stc_gotoblockforskip_orig);
			MH_EnableHook(stc_gotoblockforskip_offset);


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
			// printf("training_cutscene_controller_class: %p\n", training_cutscene_controller_class);

			auto tcc_get_text_list_addr = il2cpp_class_get_method_from_name(training_cutscene_controller_class, "GetTrainingEffectMessageWindowTextList", 0)->methodPointer;
			// printf("tcc_get_text_list_addr: %p\n", tcc_get_text_list_addr);

			auto tcc_get_text_list_addr_offset = reinterpret_cast<void*>(tcc_get_text_list_addr);
			// printf("tcc_get_text_list_addr_offset: %p\n", tcc_get_text_list_addr_offset);

			MH_CreateHook(tcc_get_text_list_addr_offset, tcc_get_text_list_hook, &tcc_get_text_list_orig);
			MH_EnableHook(tcc_get_text_list_addr_offset);


			auto tcc_play_cut_addr = il2cpp_class_get_method_from_name(training_cutscene_controller_class, "PlayTrainingCut", 1)->methodPointer;
			// printf("tcc_play_cut_addr: %p\n", tcc_play_cut_addr);

			auto tcc_play_cut_addr_offset = reinterpret_cast<void*>(tcc_play_cut_addr);
			// printf("tcc_play_cut_addr_offset: %p\n", tcc_play_cut_addr_offset);

			MH_CreateHook(tcc_play_cut_addr_offset, tcc_play_cut_hook, &tcc_play_cut_orig);
			MH_EnableHook(tcc_play_cut_addr_offset);


			import_translations();

			bootstrap_carrot_juicer();

			MH_DisableHook(LoadLibraryW);
			MH_RemoveHook(LoadLibraryW);

			printf("=== Carotene successfully initialized ===\n");

			return LoadLibraryW(path);
		}

		return reinterpret_cast<decltype(LoadLibraryW)*>(load_library_w_orig)(path);
	}
}

const int ctrl_key = 0xA2;
const int shift_key = 0xA0;
const int alt_key = 0xA4;
const int d_key = 0x44;
const int r_key = 0x52;

const std::set<int> keys_to_check = {
	ctrl_key,
	shift_key,
	alt_key,
	d_key,
	r_key
};
std::set<int> pressed_keys;

void add_key(int key)
{
	// Check if this key should be checked
	if (keys_to_check.find(key) == keys_to_check.end())
	{
		return;
	}

	pressed_keys.insert(key);
}

bool is_pressed(int key)
{
	return pressed_keys.find(key) != pressed_keys.end();
}

void rem_key(int key)
{
	if (is_pressed(key))
	{
		pressed_keys.erase(key);
	}
}

LRESULT CALLBACK keyboard_hook_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
		{
			KBDLLHOOKSTRUCT hooked_key = *((KBDLLHOOKSTRUCT*)lParam);
			int key = hooked_key.vkCode;
			add_key(key);

			// Keyboard shortcuts go here

			// Debug mode
			if (is_pressed(ctrl_key) && is_pressed(shift_key) && is_pressed(alt_key) && is_pressed(d_key))
			{
				debug_level++;
				if (debug_level > max_debug_level)
				{
					debug_level = 0;
				}
				printf("Debug level: %d\n", debug_level);
			}

			// Reload translations
			if (is_pressed(ctrl_key) && is_pressed(shift_key) && is_pressed(alt_key) && is_pressed(r_key))
			{
				import_translations();
			}
		}

		if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
		{
			KBDLLHOOKSTRUCT hooked_key = *((KBDLLHOOKSTRUCT*)lParam);
			int key = hooked_key.vkCode;
			rem_key(key);
		}
	}

	return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}



void keyboard_input_thread()
{
	HINSTANCE handle = GetModuleHandle(NULL);
	keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_proc, handle, 0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) != 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnhookWindowsHookEx(keyboard_hook);
}


void handle_keyboard_input()
{
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)keyboard_input_thread, NULL, 0, NULL);
}

void file_check_thread()
{
	typedef std::multimap<std::filesystem::file_time_type, std::filesystem::directory_entry> result_set_t;
	std::wstring tmp_folder_raw = L"%TEMP%\\carotenify";
	// Expand the environment variable
	std::wstring tmp_folder_expanded;
	tmp_folder_expanded.resize(MAX_PATH);
	DWORD result = ExpandEnvironmentStringsW(tmp_folder_raw.c_str(), &tmp_folder_expanded[0], MAX_PATH);
	if (result == 0)
	{
		printf("Failed to expand environment string\n");
		return;
	}
	tmp_folder_expanded.resize(result - 1);
	std::filesystem::path tmp_folder(tmp_folder_expanded);

	// std::filesystem::path tmp_folder("%TEMP%\\carotene");
	while (true)
	{
		if (!std::filesystem::exists(tmp_folder))
		{
			std::filesystem::create_directory(tmp_folder);
		}

		std::filesystem::directory_iterator end_iter;
		result_set_t results;

		// Iterate through files in tmp folder, ordered by date
		for(std::filesystem::directory_iterator dir_iter(tmp_folder) ; dir_iter != end_iter ; ++dir_iter)
		{
			if (std::filesystem::is_regular_file(dir_iter->status()))
			{
				auto last_time = std::filesystem::last_write_time(dir_iter->path());
				results.insert(result_set_t::value_type(last_time, *dir_iter));
			}
		}

		// If there are files, load them
		if (!results.empty())
		{
			for(auto it = results.begin() ; it != results.end() ; ++it)
			{
				auto file_path = it->second.path();
				auto file_name = file_path.filename().string();
				printf("Loading %s\n", file_name.c_str());
				try {
					// Read file as string
					std::ifstream file(file_path);
					std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
					file.close();

					std::filesystem::remove(file_path);

					printf("File contents: %s\n", file_contents.c_str());


					std::istringstream iss(file_path.string());

					std::string file_rest;
					std::string file_type;
					std::getline(iss, file_rest, '.');
					std::getline(iss, file_type, '.');

					// printf("File time: %s\n", file_rest.c_str());
					// printf("File type: %s\n", file_type.c_str());

					if (file_type == "gotoBlock")
					{
						if (last_story_timeline_controller != 0)
						{
							// Split file_contents on newline
							std::istringstream iss(file_contents);
							std::string line;
							while (std::getline(iss, line))
							{
								// Convert line to int
								int num = -1;
								try
								{
									num = std::stoi(line);
								}
								catch (const std::invalid_argument const& ex)
								{
									printf("Invalid argument: %s\n", ex.what());
								}

								if (num != -1)
								{
									printf("Goto block %d\n", num);
									stc_gotoblock_hook(last_story_timeline_controller, num, false, false, false);
								}
							}

							/*
							// Convert file_contents to int
							int num = -1;

							try
							{
								num = std::stoi(file_contents);
							}
							catch (const std::invalid_argument const& ex)
							{
								
							}

							if (num != -1)
							{
								printf("Goto block %d\n", num);
								stc_gotoblock_hook(last_story_timeline_controller, num, false, false, false);
							}
							*/
						}
					}
				}
				catch (std::exception& e)
				{
					printf("File read error: %s\n", e.what());
				}
			}
		}
		

		Sleep(200);
	}
}


void attach()
{
	create_debug_console();

	printf("=== Starting Carotene ===\n");

	if (MH_Initialize() != MH_OK)
	{
		printf("Failed to initialize MinHook.\n");
		return;
	}
	printf("MinHook initialized.\n");

	MH_CreateHook(LoadLibraryW, load_library_w_hook, &load_library_w_orig);
	MH_EnableHook(LoadLibraryW);

	// if (file_exists("carrotjuicer.dll"))
	// {
	// 	printf("carrotjuicer.dll found, loading...\n");
	// 	LoadLibraryW(L"carrotjuicer.dll");
	// }

	if (file_exists("tlg.dll"))
	{
		printf("tlg.dll found, loading...\n");
		LoadLibraryW(L"tlg.dll");
	}

	handle_keyboard_input();

	// file_check_thread();
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)file_check_thread, NULL, 0, NULL);
}

void detach()
{
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}
