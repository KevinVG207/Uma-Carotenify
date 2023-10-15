#pragma once

#include <cstdint>

enum Il2CppTypeEnum
{
	IL2CPP_TYPE_END = 0x00,       /* End of List */
	IL2CPP_TYPE_VOID = 0x01,
	IL2CPP_TYPE_BOOLEAN = 0x02,
	IL2CPP_TYPE_CHAR = 0x03,
	IL2CPP_TYPE_I1 = 0x04,
	IL2CPP_TYPE_U1 = 0x05,
	IL2CPP_TYPE_I2 = 0x06,
	IL2CPP_TYPE_U2 = 0x07,
	IL2CPP_TYPE_I4 = 0x08,
	IL2CPP_TYPE_U4 = 0x09,
	IL2CPP_TYPE_I8 = 0x0a,
	IL2CPP_TYPE_U8 = 0x0b,
	IL2CPP_TYPE_R4 = 0x0c,
	IL2CPP_TYPE_R8 = 0x0d,
	IL2CPP_TYPE_STRING = 0x0e,
	IL2CPP_TYPE_PTR = 0x0f,
	IL2CPP_TYPE_BYREF = 0x10,
	IL2CPP_TYPE_VALUETYPE = 0x11,
	IL2CPP_TYPE_CLASS = 0x12,
	IL2CPP_TYPE_VAR = 0x13,
	IL2CPP_TYPE_ARRAY = 0x14,
	IL2CPP_TYPE_GENERICINST = 0x15,
	IL2CPP_TYPE_TYPEDBYREF = 0x16,
	IL2CPP_TYPE_I = 0x18,
	IL2CPP_TYPE_U = 0x19,
	IL2CPP_TYPE_FNPTR = 0x1b,
	IL2CPP_TYPE_OBJECT = 0x1c,
	IL2CPP_TYPE_SZARRAY = 0x1d,
	IL2CPP_TYPE_MVAR = 0x1e,
	IL2CPP_TYPE_CMOD_REQD = 0x1f,
	IL2CPP_TYPE_CMOD_OPT = 0x20,
	IL2CPP_TYPE_INTERNAL = 0x21,

	IL2CPP_TYPE_MODIFIER = 0x40,
	IL2CPP_TYPE_SENTINEL = 0x41,
	IL2CPP_TYPE_PINNED = 0x45,

	IL2CPP_TYPE_ENUM = 0x55
};

typedef struct Il2CppType
{
	void* dummy;
	unsigned int attrs : 16;
	Il2CppTypeEnum type : 8;
	unsigned int num_mods : 6;
	unsigned int byref : 1;
	unsigned int pinned : 1;
} Il2CppType;

typedef struct Il2CppClass;

// not real Il2CppString class
struct Il2CppString
{
	void* Empty;
	void* WhiteChars;
	int32_t length;
	wchar_t start_char[1];
};

// UnityEngine.Color
struct Color_t
{
public:
	// System.Single UnityEngine.Color::r
	float r;
	// System.Single UnityEngine.Color::g
	float g;
	// System.Single UnityEngine.Color::b
	float b;
	// System.Single UnityEngine.Color::a
	float a;
};

// UnityEngine.Vector2
struct Vector2_t
{
public:
	// System.Single UnityEngine.Vector2::x
	float x;
	// System.Single UnityEngine.Vector2::y
	float y;
};

// UnityEngine.TextGenerationSettings
struct TextGenerationSettings_t
{
public:
	// UnityEngine.Font UnityEngine.TextGenerationSettings::font
	void* font;
	// UnityEngine.Color UnityEngine.TextGenerationSettings::color
	Color_t color;
	// System.Int32 UnityEngine.TextGenerationSettings::fontSize
	int32_t fontSize;
	// System.Single UnityEngine.TextGenerationSettings::lineSpacing
	float lineSpacing;
	// System.Boolean UnityEngine.TextGenerationSettings::richText
	bool richText;
	// System.Single UnityEngine.TextGenerationSettings::scaleFactor
	float scaleFactor;
	// UnityEngine.FontStyle UnityEngine.TextGenerationSettings::fontStyle
	int32_t fontStyle;
	// UnityEngine.TextAnchor UnityEngine.TextGenerationSettings::textAnchor
	int32_t textAnchor;
	// System.Boolean UnityEngine.TextGenerationSettings::alignByGeometry
	bool alignByGeometry;
	// System.Boolean UnityEngine.TextGenerationSettings::resizeTextForBestFit
	bool resizeTextForBestFit;
	// System.Int32 UnityEngine.TextGenerationSettings::resizeTextMinSize
	int32_t resizeTextMinSize;
	// System.Int32 UnityEngine.TextGenerationSettings::resizeTextMaxSize
	int32_t resizeTextMaxSize;
	// System.Boolean UnityEngine.TextGenerationSettings::updateBounds
	bool updateBounds;
	// UnityEngine.VerticalWrapMode UnityEngine.TextGenerationSettings::verticalOverflow
	int32_t verticalOverflow;
	// UnityEngine.HorizontalWrapMode UnityEngine.TextGenerationSettings::horizontalOverflow
	int32_t horizontalOverflow;
	// UnityEngine.Vector2 UnityEngine.TextGenerationSettings::generationExtents
	Vector2_t  generationExtents;
	// UnityEngine.Vector2 UnityEngine.TextGenerationSettings::pivot
	Vector2_t  pivot;
	// System.Boolean UnityEngine.TextGenerationSettings::generateOutOfBounds
	bool generateOutOfBounds;
};

struct ParameterInfo
{
	const char* name;
	int32_t position;
	uint32_t token;
	const Il2CppType* parameter_type;
};

struct Il2CppObject
{
	union
	{
		void* klass;
		void* vtable;
	};
	void* monitor;
};

struct MethodInfo
{
	uintptr_t methodPointer;
	uintptr_t invoker_method;
	const char* name;
	uintptr_t klass;
	const Il2CppType* return_type;
	const ParameterInfo* parameters;
	uintptr_t methodDefinition;
	uintptr_t genericContainer;
	uint32_t token;
	uint16_t flags;
	uint16_t iflags;
	uint16_t slot;
	uint8_t parameters_count;
	uint8_t is_generic : 1;
	uint8_t is_inflated : 1;
	uint8_t wrapper_type : 1;
	uint8_t is_marshaled_from_native : 1;
};

struct FieldInfo
{
	const char* name;
	const Il2CppType* type;
	uintptr_t parent;
	int32_t offset;
	uint32_t token;
};

template <typename T>
struct TypedField
{
	FieldInfo* Field;

	constexpr FieldInfo* operator->() const noexcept
	{
		return Field;
	}
};

struct Il2CppReflectionType
{
	Il2CppObject object;
	const Il2CppType* type;
};

struct Il2CppClassHead
{
	const void* image;
	void* gc_desc;
	const char* name;
	const char* namespaze;
};

// function types
typedef Il2CppString* (*il2cpp_string_new_utf16_t)(const wchar_t* str, unsigned int len);
typedef Il2CppString* (*il2cpp_string_new_t)(const char* str);
typedef void* (*il2cpp_domain_get_t)();
typedef void* (*il2cpp_domain_assembly_open_t)(void* domain, const char* name);
typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namespaze, const char* name);
typedef MethodInfo* (*il2cpp_class_get_methods_t)(void* klass, void** iter);
typedef MethodInfo* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_method_get_param_t)(const MethodInfo* method, uint32_t index);
typedef void* (*il2cpp_object_new_t)(void* klass);
typedef void* (*il2cpp_resolve_icall_t)(const char* name);
typedef void* (*il2cpp_array_new_t)(void* klass, uintptr_t count);
typedef void* (*il2cpp_thread_attach_t)(void* domain);
typedef void (*il2cpp_thread_detach_t)(void* thread);
typedef FieldInfo* (*il2cpp_class_get_field_from_name_t)(void* klass, const char* name);
typedef bool (*il2cpp_class_is_assignable_from_t)(void* klass, void* oklass);
typedef void (*il2cpp_class_for_each_t)(void(*klassReportFunc)(void* klass, void* userData), void* userData);
typedef void* (*il2cpp_class_get_nested_types_t)(void* klass, void** iter);
typedef void* (*il2cpp_class_get_type_t)(void* klass);
typedef Il2CppReflectionType* (*il2cpp_type_get_object_t)(const void* type);
typedef uint32_t (*il2cpp_gchandle_new_t)(void* obj, bool pinned);
typedef void (*il2cpp_gchandle_free_t)(uint32_t gchandle);
typedef void* (*il2cpp_gchandle_get_target_t)(uint32_t gchandle);
typedef void* (*il2cpp_class_from_type_t)(const Il2CppType* type);
typedef void (*il2cpp_runtime_class_init_t)(void* klass);
typedef void* (*il2cpp_runtime_invoke_t)(MethodInfo* method, void* obj, void** params, Il2CppObject** exc);
typedef Il2CppType* (*il2cpp_class_enum_basetype_t)(void * klass);

// function defines
extern il2cpp_string_new_utf16_t il2cpp_string_new_utf16;
extern il2cpp_string_new_t il2cpp_string_new;
extern il2cpp_domain_get_t il2cpp_domain_get;
extern il2cpp_domain_assembly_open_t il2cpp_domain_assembly_open;
extern il2cpp_assembly_get_image_t il2cpp_assembly_get_image;
extern il2cpp_class_from_name_t il2cpp_class_from_name;
extern il2cpp_class_get_methods_t il2cpp_class_get_methods;
extern il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name;
extern il2cpp_method_get_param_t il2cpp_method_get_param;
extern il2cpp_object_new_t il2cpp_object_new;
extern il2cpp_resolve_icall_t il2cpp_resolve_icall;
extern il2cpp_array_new_t il2cpp_array_new;
extern il2cpp_thread_attach_t il2cpp_thread_attach;
extern il2cpp_thread_detach_t il2cpp_thread_detach;
extern il2cpp_class_get_field_from_name_t il2cpp_class_get_field_from_name;
extern il2cpp_class_is_assignable_from_t il2cpp_class_is_assignable_from;
extern il2cpp_class_for_each_t il2cpp_class_for_each;
extern il2cpp_class_get_nested_types_t il2cpp_class_get_nested_types;
extern il2cpp_class_get_type_t il2cpp_class_get_type;
extern il2cpp_type_get_object_t il2cpp_type_get_object;
extern il2cpp_gchandle_new_t il2cpp_gchandle_new;
extern il2cpp_gchandle_free_t il2cpp_gchandle_free;
extern il2cpp_gchandle_get_target_t il2cpp_gchandle_get_target;
extern il2cpp_class_from_type_t il2cpp_class_from_type;
extern il2cpp_runtime_class_init_t il2cpp_runtime_class_init;
extern il2cpp_runtime_invoke_t il2cpp_runtime_invoke;
extern il2cpp_class_enum_basetype_t il2cpp_class_enum_basetype;