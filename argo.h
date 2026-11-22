#ifndef ARGO_LEXER_H
#define ARGO_LEXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define Argo_NoShortFlag NULL
#define Argo_NoLongFlag NULL

typedef enum ArgoReturnType
{
	ArgoReturnType_Failure,
	ArgoReturnType_Success,
} ArgoReturnType;

typedef enum ArgoOptionType
{
	ArgoOptionType_Boolean,
	ArgoOptionType_Integer,
	ArgoOptionType_Float,
	ArgoOptionType_String,
} ArgoOptionType;

// Tokenizer only. Bit zero preserves the meaning of the old true argument.
#define ARGO_IGNORE_UNKNOWN_FLAGS UINT32_C(1)

// Tokenizer defaults or per-option overrides for repeated values.
// With neither bit set, options inherit the tokenizer default (last wins).
// Within either mask, LAST_IN takes precedence if both bits are supplied.
#define ARGO_NO_FLAGS UINT32_C(0)
#define ARGO_LAST_IN UINT32_C(2)  // Keep the last occurrence.
#define ARGO_FIRST_IN UINT32_C(4) // Keep the first occurrence.
#define ARGO_LAST_OUT ARGO_FIRST_IN

// Per-option help annotation. The caller checks whether the option was supplied.
#define ARGO_REQUIRED UINT32_C(8)

typedef struct ArgoOption
{
	char* short_name;
	char* long_name;
	ArgoOptionType type;
	char* description;
	bool found;
	char* value;
	uint32_t flags;
} ArgoOption;

#define Argo_Set(_short_name, _long_name, _type, _description)                                 \
	Argo_SetFlags(_short_name, _long_name, _type, _description, 0)

#define Argo_SetWithFlags(_short_name, _long_name, _type, _description, _flags)                \
	{                                                                                          \
		.short_name = _short_name, .long_name = _long_name, .type = _type,                     \
		.description = _description, .found = false, .value = NULL, .flags = (_flags),         \
	}

typedef struct ArgoInstance
{
	ArgoOption* options;
	size_t size;
	char** unformatted;
	size_t unformatted_size;

	// Optional caller-owned array for positional arguments before --.
	// Entries borrow argv pointers. The first unused slot is set to NULL.
	// Excess positional arguments are ignored.
	char** positionals;
	size_t positional_size;     // Read only. Number of pointers stored.
	size_t positional_capacity; // Number of pointer slots, including any terminator.
} ArgoInstance;

/*
Initialize the instance before first use, e.g. `ArgoInstance instance = {0};`. If you don't,
Argo may think `positionals` points to valid memory which will result in a segfault. That's a
no-no.

Options that take values accept `--name=value`, `-x=value`, or a separate value argument. Argo
doesn't check what's actually after the equal sign so always double-check! Argo doesn't allocate
memory but just borrows it from `argv`, and it doesn't modify `argv` at all.

Argo relies on the system to resolve `--input-file="path/to/my file.txt"` to
`--input-file=path/to/my file.txt`. But this is a standard convention so there's *probably*
nothing to worry about.

Each call starts with fresh found/value results. Pass 0 for the standard tokenizer behavior.
*/
ArgoReturnType Argo_Tokenize(ArgoInstance* instance, ArgoOption* options, size_t size,
                             size_t argc, char** argv, uint32_t argo_flags);
ArgoReturnType Argo_PrintHelp(ArgoInstance* instance);
void Argo_PrintError(void);

#ifdef __cplusplus
}
#endif

#endif // ARGO_LEXER_H
