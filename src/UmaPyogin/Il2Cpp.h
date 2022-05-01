#ifndef UMAPYOGIN_IL2CPP_H
#define UMAPYOGIN_IL2CPP_H

#include <cstdint>

#include "Misc.h"

namespace UmaPyogin
{
	struct Il2CppDomain;

	struct Il2CppAssemblyName
	{
		const char* name;
		const char* culture;
		const uint8_t* public_key;
		uint32_t hash_alg;
		int32_t hash_len;
		uint32_t flags;
		int32_t major;
		int32_t minor;
		int32_t build;
		int32_t revision;
		uint8_t public_key_token[8];
	};

	struct Il2CppAssembly
	{
		void* image;
		uint32_t token;
		int32_t referencedAssemblyStart;
		int32_t referencedAssemblyCount;
		Il2CppAssemblyName aname;
	};

	struct Il2CppImage;

	using Il2CppMethodPointer = void (*)();
	struct MethodInfo;
	using InvokerMethod = void* (*) (Il2CppMethodPointer, const MethodInfo*, void*, void**);

	struct Il2CppClass;

	struct Il2CppClassHead
	{
		// The following fields are always valid for a Il2CppClass structure
		const void* image;
		void* gc_desc;
		const char* name;
		const char* namespaze;
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

	using Il2CppChar = char16_t;

	struct Il2CppString
	{
		Il2CppObject object;
		int32_t length; ///< Length of string *excluding* the trailing null (which is included in
		                ///< 'chars').
		Il2CppChar chars[1];
	};

	struct Il2CppType;

	struct Il2CppReflectionType
	{
		Il2CppObject object;
		const Il2CppType* type;
	};

	struct MethodInfo
	{
		Il2CppMethodPointer methodPointer;
		InvokerMethod invoker_method;
		const char* name;
		void* klass;
		const void* return_type;
		const void* parameters;

		union
		{
			const void* rgctx_data; /* is_inflated is true and is_generic is false, i.e. a generic
			                           instance method */
			void* methodMetadataHandle;
		};

		/* note, when is_generic == true and is_inflated == true the method represents an uninflated
		 * generic method on an inflated type. */
		union
		{
			const void* genericMethod;          /* is_inflated is true */
			void* genericContainerHandle;       /* is_inflated is false and is_generic is true */
			Il2CppMethodPointer nativeFunction; /* if is_marshaled_from_native is true */
		};

		uint32_t token;
		uint16_t flags;
		uint16_t iflags;
		uint16_t slot;
		uint8_t parameters_count;
		uint8_t is_generic : 1;   /* true if method is a generic method definition */
		uint8_t is_inflated : 1;  /* true if declaring_type is a generic instance or if method is a
		                             generic instance*/
		uint8_t wrapper_type : 1; /* always zero (MONO_WRAPPER_NONE) needed for the debugger */
		uint8_t
		    is_marshaled_from_native : 1; /* a fake MethodInfo wrapping a native function pointer */
	};

	struct FieldInfo
	{
		const char* name;
		const void* type;
		void* parent;
		int32_t offset; // If offset is -1, then it's thread static
		uint32_t token;
	};

// 接受 X(returnType, name, params)
#define LOAD_FUNCTIONS(X)                                                                          \
	X(Il2CppDomain*, il2cpp_domain_get, ())                                                        \
	X(const Il2CppAssembly*, il2cpp_domain_assembly_open,                                          \
	  (Il2CppDomain * domain, const char* name))                                                   \
	X(const Il2CppImage*, il2cpp_get_corlib, ())                                                   \
	X(const Il2CppImage*, il2cpp_assembly_get_image, (const Il2CppAssembly* assembly))             \
	X(Il2CppClass*, il2cpp_class_from_name,                                                        \
	  (const Il2CppImage* image, const char* name_space, const char* name))                        \
	X(const MethodInfo*, il2cpp_class_get_methods, (Il2CppClass * klass, void** iter))             \
	X(const MethodInfo*, il2cpp_class_get_method_from_name,                                        \
	  (Il2CppClass * klass, const char* name, int paramCount))                                     \
	X(const Il2CppType*, il2cpp_class_get_type, (Il2CppClass * klass))                             \
	X(Il2CppString*, il2cpp_string_new, (const char* str))                                         \
	X(Il2CppString*, il2cpp_string_new_utf16, (const Il2CppChar* text, int32_t len))               \
	X(Il2CppClass*, il2cpp_class_get_nested_types, (Il2CppClass * klass, void** iter))             \
	X(FieldInfo*, il2cpp_class_get_field_from_name, (Il2CppClass * klass, const char* name))       \
	X(void, il2cpp_field_get_value, (Il2CppObject * obj, FieldInfo * field, void* value))          \
	X(Il2CppObject*, il2cpp_field_get_value_object, (FieldInfo * field, Il2CppObject * obj))       \
	X(void, il2cpp_field_set_value, (Il2CppObject * obj, FieldInfo * field, void* value))          \
	X(void, il2cpp_field_set_value_object,                                                         \
	  (Il2CppObject * instance, FieldInfo * field, Il2CppObject * value))                          \
	X(Il2CppClass*, il2cpp_object_get_class, (Il2CppObject * obj))                                 \
	X(uint32_t, il2cpp_gchandle_new, (Il2CppObject * obj, bool pinned))                            \
	X(Il2CppObject*, il2cpp_gchandle_get_target, (uint32_t gchandle))                              \
	X(void, il2cpp_gchandle_free, (uint32_t gchandle))                                             \
	X(Il2CppObject*, il2cpp_type_get_object, (const Il2CppType* type))                             \
	X(bool, il2cpp_class_is_assignable_from, (Il2CppClass * klass, Il2CppClass * oklass))          \
	X(const char*, il2cpp_class_get_name, (Il2CppClass * klass))

	namespace Il2CppSymbols
	{
#define DECLARE_FUNCTION_POINTERS(returnType, name, params) extern returnType(*name) params;

		LOAD_FUNCTIONS(DECLARE_FUNCTION_POINTERS)

#undef DECLARE_FUNCTION_POINTERS

		void LoadIl2CppSymbols();
	} // namespace Il2CppSymbols
} // namespace UmaPyogin

#endif
