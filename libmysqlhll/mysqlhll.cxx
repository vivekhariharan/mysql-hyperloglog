#ifdef WIN32
#include <winsock.h>
typedef signed char int8_t;
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#include <mysql.h>
#include <math.h>

#include "constants.hpp"
#include "SerializedHyperLogLog.hpp"

extern "C" {

#ifndef NDEBUG
#define LOG(...) fprintf(stderr, __VA_ARGS__);
#else
#define LOG(...)
#endif

my_bool EXPORT hll_create_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void EXPORT hll_create_deinit(UDF_INIT *initid);
char EXPORT *hll_create(UDF_INIT *initid, UDF_ARGS *args, char *result,
		unsigned long *length, char *is_null, char *error);
void EXPORT hll_create_clear(UDF_INIT* initid, char* is_null, char* message);
void EXPORT hll_create_add(UDF_INIT* initid, UDF_ARGS* args, char* is_null,
		char* message);

my_bool EXPORT hll_compute_init(UDF_INIT *initid, UDF_ARGS *args,
		char *message);
void EXPORT hll_compute_deinit(UDF_INIT *initid);
long long EXPORT hll_compute(UDF_INIT *initid, UDF_ARGS *args, char *result,
		unsigned long *length, char *is_null, char *error);
void EXPORT hll_compute_clear(UDF_INIT* initid, char* is_null, char* message);
void EXPORT hll_compute_add(UDF_INIT* initid, UDF_ARGS* args, char* is_null,
		char* message);

my_bool EXPORT hll_merge_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void EXPORT hll_merge_deinit(UDF_INIT *initid);
char EXPORT *hll_merge(UDF_INIT *initid, UDF_ARGS *args, char *result,
		unsigned long *length, char *is_null, char *error);
void EXPORT hll_merge_clear(UDF_INIT* initid, char* is_null, char* message);
void EXPORT hll_merge_add(UDF_INIT* initid, UDF_ARGS* args, char* is_null,
		char* message);

my_bool EXPORT hll_merge_compute_init(UDF_INIT *initid, UDF_ARGS *args,
		char *message);
void EXPORT hll_merge_compute_deinit(UDF_INIT *initid);
long long EXPORT hll_merge_compute(UDF_INIT *initid, UDF_ARGS *args,
		char *result, unsigned long *length, char *is_null, char *error);
void EXPORT hll_merge_compute_clear(UDF_INIT* initid, char* is_null,
		char* message);
void EXPORT hll_merge_compute_add(UDF_INIT* initid, UDF_ARGS* args,
		char* is_null, char* message);

class Data {
public:
	SerializedHyperLogLog* shll;
	char* result;

	Data(bool need_result) {
		shll = new SerializedHyperLogLog(HLL_BIT_WIDTH);

		//the base64 string is 4*ceil(n+2/3) where n is the number of bytes in the original string(2^HLL_BIT_WIDTH)
		int bytesToAllocate = ceil(((1 << HLL_BIT_WIDTH) + 2) / 3) * 4;
		//add padding
		bytesToAllocate += 10;

		if (need_result) {
			result = (char*) malloc(bytesToAllocate);
		} else {
			result = NULL;
		}
	}

	~Data() {
		delete shll;
		if (result != NULL) {
			free(result);
		}
	}
};

my_bool init(UDF_INIT *initid, UDF_ARGS *args, char *message, bool need_result,
		const char* function_name) {
	if (args->arg_count == 0) {
		sprintf(message,
				"Wrong arguments to %s();  Must have at least 1 argument",
				function_name);
		return 1;
	}

	for (int i = 0; i < args->arg_count; ++i) {
		args->arg_type[i] = STRING_RESULT;
	}

	initid->ptr = (char*) new Data(need_result);
	return 0;
}

my_bool EXPORT hll_create_init(UDF_INIT *initid, UDF_ARGS *args,
		char *message) {
	return init(initid, args, message, true, "HLL_CREATE");
}

Data* data(UDF_INIT *initid) {
	return (Data*) initid->ptr;
}

SerializedHyperLogLog* shll(UDF_INIT *initid) {
	return data(initid)->shll;
}

void EXPORT hll_create_deinit(UDF_INIT *initid) {
	delete data(initid);
}

char EXPORT *hll_create(UDF_INIT *initid, UDF_ARGS *args, char *result,
		unsigned long *length, char *is_null, char *error) {

	char* hll_result = data(initid)->result;
	shll(initid)->toString(hll_result);
	*length = strlen(hll_result);

	LOG("hll %s\n", hll_result);

	return hll_result;
}

void EXPORT hll_create_clear(UDF_INIT* initid, char* is_null, char* message) {
	LOG("hll clear\n");
	shll(initid)->clear();
}

void get_value_and_length(UDF_ARGS* args, int i, const char** value,
		uint32_t* length) {
	*value = (args->args[i] == NULL ? "" : args->args[i]);
	*length = (args->args[i] == NULL ? 0 : args->lengths[i]);
}

void EXPORT hll_create_add(UDF_INIT* initid, UDF_ARGS* args, char* is_null,
		char* message) {
	for (int i = 0; i < args->arg_count; ++i) {
		const char* value;
		uint32_t length;
		get_value_and_length(args, i, &value, &length);
		shll(initid)->add(value, length);
	}
}

my_bool EXPORT hll_compute_init(UDF_INIT *initid, UDF_ARGS *args,
		char *message) {
	return init(initid, args, message, false, "HLL_COMPUTE");
}

void EXPORT hll_compute_deinit(UDF_INIT *initid) {
	return hll_create_deinit(initid);
}

long long EXPORT hll_compute(UDF_INIT *initid, UDF_ARGS *args, char *result,
		unsigned long *length, char *is_null, char *error) {
	LOG("hll_compute\n");
	return shll(initid)->estimate();
}

void EXPORT hll_compute_clear(UDF_INIT* initid, char* is_null, char* message) {
	LOG("hll_compute_clear\n");
	hll_create_clear(initid, is_null, message);
}

void EXPORT hll_compute_add(UDF_INIT* initid, UDF_ARGS* args, char* is_null,
		char* message) {
	LOG("hll_compute_add\n");
	hll_create_add(initid, args, is_null, message);
}

my_bool merge_init(UDF_INIT *initid, UDF_ARGS *args, char *message,
		bool need_result, const char* function_name) {
	if (args->arg_count == 0) {
		sprintf(message,
				"Wrong arguments to %s();  Must have at least 1 argument",
				function_name);
		return 1;
	}

	for (int i = 0; i < args->arg_count; ++i) {
		if (args->arg_type[i] != STRING_RESULT) {
			sprintf(message,
					"Wrong arguments to %s();  All arguments must be of type string",
					function_name);
			return 1;
		}
	}

	initid->ptr = (char*) new Data(need_result);
	return 0;
}

my_bool EXPORT hll_merge_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
	return merge_init(initid, args, message, true, "HLL_MERGE");
}

void EXPORT hll_merge_deinit(UDF_INIT *initid) {
	delete data(initid);
}

char EXPORT *hll_merge(UDF_INIT *initid, UDF_ARGS *args, char *result,
		unsigned long *length, char *is_null, char *error) {
	LOG("hll_merge\n");
	return hll_create(initid, args, result, length, is_null, error);
}

void EXPORT hll_merge_clear(UDF_INIT* initid, char* is_null, char* message) {
	LOG("hll_merge_clear\n");
	shll(initid)->clear();
}

void EXPORT hll_merge_add(UDF_INIT* initid, UDF_ARGS* args, char* is_null,
		char* message) {
	for (int i = 0; i < args->arg_count; ++i) {
		LOG("hll_merge_add %d\n", i);

		uint32_t length;
		const char* arg;
		get_value_and_length(args, i, &arg, &length);
		if (length == 0)
			continue; // NULL handling

		LOG("hll_add %.*s %d\n", (int )length, arg, (int )length);
		char* hll_str = (char*) malloc(length + 1);

		strncpy(hll_str, arg, length);
		hll_str[length] = '\0';

		SerializedHyperLogLog* current_shll = SerializedHyperLogLog::fromString(
				hll_str);
		free(hll_str);

		if (current_shll != NULL) {
			shll(initid)->merge(*current_shll);
			delete current_shll;
		}
	}
}

my_bool EXPORT hll_merge_compute_init(UDF_INIT *initid, UDF_ARGS *args,
		char *message) {
	return merge_init(initid, args, message, false, "HLL_MERGE_COMPUTE");
}

void EXPORT hll_merge_compute_deinit(UDF_INIT *initid) {
	hll_merge_deinit(initid);
}

long long EXPORT hll_merge_compute(UDF_INIT *initid, UDF_ARGS *args,
		char *result, unsigned long *length, char *is_null, char *error) {
	return shll(initid)->estimate();
}

void EXPORT hll_merge_compute_clear(UDF_INIT* initid, char* is_null,
		char* message) {
	hll_merge_clear(initid, is_null, message);
}

void EXPORT hll_merge_compute_add(UDF_INIT* initid, UDF_ARGS* args,
		char* is_null, char* message) {
	hll_merge_add(initid, args, is_null, message);
}

}
