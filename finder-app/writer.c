#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    openlog("writer", LOG_PID, LOG_USER);

    if (argc < 3) {
        fprintf(stderr, "Error: Insufficient arguments. Usage: %s <writefile> <writestr>\n",
				argv[0]);
        syslog(LOG_ERR, "Insufficient arguments. Expected 2, got %d", argc - 1);
        closelog();
        exit(1);
    }

    const char *writefile = argv[1];
    const char *writestr  = argv[2];

    FILE *fp = fopen(writefile, "w");
    if (fp == NULL) {
        fprintf(stderr, "Error: Could not open file %s for writing. Error: %s\n",
                writefile, strerror(errno));
        syslog(LOG_ERR, "Could not open file %s for writing: %s", writefile, strerror(errno));
        closelog();
        exit(1);
    }

    size_t bytes = fwrite(writestr, sizeof(char), strlen(writestr), fp);
    if (bytes < strlen(writestr)) {
        fprintf(stderr, "Error: Failed to write complete string to %s.\n", writefile);
        syslog(LOG_ERR, "Failed to write complete string to %s", writefile);
        fclose(fp);
        closelog();
        exit(1);
    }

    fclose(fp);
    syslog(LOG_DEBUG, "Writing \"%s\" to \"%s\". Success.", writestr, writefile);
    closelog();
    return 0;
}
