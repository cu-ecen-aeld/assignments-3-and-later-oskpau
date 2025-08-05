#include <stdio.h>

int main(int argc, char* argv[]) {
	if (argc != 3) {
		printf("Usage: %s <arg1> <arg2>\n", argv[0]);
		exit(1);
	}

	FILE* file_ptr;
	file_ptr = fopen(argv[1], "w");
	if (file_ptr == NULL) {
		printf("Error occured while opening file");
		exit(1);
	}

	fputs(argv[2], file_ptr);

	fclose(file_ptr);

	return 0;
}
