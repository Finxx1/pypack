// this file functions as a launcher of sorts. it will download any
// dependencies if needed (eg python or libs) and then launch the main
// python script in src/ with the same name as the executable

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "json.h"

#ifdef _WIN32
#define xstrdup _strdup
const char* python_exe = "python.exe";
#define CANCEL_STDOUT " > NUL"
#else
#define xstrdup strdup
const char* python_exe = "python3";
#define CANCEL_STDOUT " > /dev/null 2> /dev/null"
#endif

static char* build_python_cmdline(char* prefix, char* name, char* suffix);
static void get_deps(char* prefix);

int main(int argc, char** argv) {
	char* name = xstrdup(argv[0]);

	// remove leading directories
	char* s1 = strrchr(name, '/');
	if (s1) name = s1 + 1;

	// remove windows-style leading directories
	s1 = strrchr(name, '\\');
	if (s1) name = s1 + 1;

	// remove file extension
	s1 = strrchr(name, '.');
	if (s1) s1[0] = 0;

	// if python is in the path, just use that
	if (system(build_python_cmdline("", "-V", CANCEL_STDOUT)) == 0) {
		get_deps("");
		return system(build_python_cmdline("", name, ".py"));
	}

	// python only provides prebuilt binaries for windows, so give up if not on windows
#ifndef _WIN32
	printf("Python could not be found. Please install Python, see www.python.org for details.");
	return 1;
#endif

	// check if python is already downloaded
	FILE* fp = fopen("python/python.exe", "r");
	if (fp) {
		fclose(fp);
		get_deps("python/");
		return system(build_python_cmdline("python/", name, ".py"));
	}

	// download python embeddable
#define PYTHON_URL "https://www.python.org/ftp/python/3.12.8/python-3.12.8-embed-amd64.zip"
	printf("Downloading Python...\n");
	system("curl -o python.zip -Ls " PYTHON_URL CANCEL_STDOUT);
	printf("Unpacking Python...\n");
	system("mkdir -p python" CANCEL_STDOUT);
	system("tar -xf python.zip -C python/" CANCEL_STDOUT);
	system("del python.zip" CANCEL_STDOUT);
	system("del python/python312._pth" CANCEL_STDOUT); // enables pip
	
	fp = fopen("python/python.exe", "r");
	if (fp) {
		fclose(fp);
		get_deps("python/");
		return system(build_python_cmdline("python/", name, ".py"));
	} else {
		printf("Python download failed! Try manually installing python, see www.python.org for more details\n");
	}
}

static char* build_python_cmdline(char* prefix, char* name, char* suffix) {
	// format: "{prefix}${python_exe} ${name}${suffix}"
	char* r = calloc(strlen(prefix) + strlen(python_exe) + 1 + strlen(name) + strlen(suffix), 1);
	strcpy(r, prefix);
	strcat(r, python_exe);
	strcat(r, " ");
	strcat(r, name);
	strcat(r, suffix);
	return r;
}

static void get_deps(char* prefix) {
	FILE* fp = fopen("pypack.json", "rb");
	if (!fp) {
		printf("WARNING: pypack.json not found. The program may not work correctly.");
		return;
	}

	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char* fd = malloc(size + 1);
	fd[size] = 0;
	fread(fd, 1, size, fp);

	struct json_value_s* root = json_parse(fd, size);
	if (root->type != json_type_object) return;

	struct json_object_s* root_object = root->payload;
	struct json_object_element_s* deps = root_object->start;
	if (strcmp(deps->name->string, "deps") != 0) return;
	struct json_array_s* deparray = deps->value->payload;

	struct json_array_element_s* dep = deparray->start;
	for (int i = 0; i < deparray->length; i++) {
		struct json_string_s* depname = dep->value->payload;
		char* s1 = malloc(strlen(depname->string) + strlen(CANCEL_STDOUT) + 1);
		s1[0] = 0;
		strcat(s1, depname->string);
		strcat(s1, CANCEL_STDOUT);
		if (system(build_python_cmdline(prefix, "-m pip show ", s1)) != 0) {
			if (system(build_python_cmdline(prefix, "-m pip install ", s1)) != 0) {
				printf("Failed to install a dependency!");
				exit(1);
			}
		}

		dep = dep->next;
	}
}

