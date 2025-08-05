#include <stdio.h>
#include <syslog.h>

int main(int argc, char* argv[]) {
	openlog(NULL, 0, LOG_USER);
	syslog(LOG_INFO, "writer application started");
	if (argc != 3) {
		syslog(LOG_ERR, "Invalid number of arguments! Number of given arguments: %d. Usage: %s <arg1> <arg2>\n", argc, argv[0]);
		return 1;
	}

	FILE* file_ptr;
	file_ptr = fopen(argv[1], "w");
	if (file_ptr == NULL) {
		syslog(LOG_ERR, "Error occured while opening file");
		return 1;
	}
	syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
	fputs(argv[2], file_ptr);

	fclose(file_ptr);

	return 0;
}
