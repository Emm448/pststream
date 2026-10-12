#include "pst.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <inttypes.h>

typedef enum {
    MODE_NONE = 0,
    MODE_HEADERS,
    MODE_FULL,
    MODE_BODY,
    MODE_FULL_SINGLE,
    MODE_FOLDER,
    MODE_ATTACHMENTS
} pst_mode_t;

void print_help(const char *prog) {
    printf(
        "Usage: %s -f <file.pst> [MODE] [OPTIONS]\n\n"
        "Modes (choose one):\n"
        "  -H            Print all email headers\n"
        "  -F            Print all emails full info\n"
        "  -B            Print single email body (requires -n)\n"
        "  -I            Print single email full info (requires -n)\n"
        "  -D            Print single email folder (requires -n)\n"
        "  -A            Print email attachments (requires -n)\n\n"
        "Options:\n"
        "  -f <file>     PST file (mandatory)\n"
        "  -n <nid>      Node ID (required for most modes)\n"
        "  -h, --help    Show this help message\n\n",
        prog
    );
}

static uint64_t parse_nid(const char *arg) {
    char *end = NULL;
    errno = 0;

    uint64_t val = strtoumax(arg, &end, 10);

    if (errno != 0 || end == arg || *end != '\0') {
        fprintf(stderr, "Error: invalid NID '%s'\n", arg);
        exit(EXIT_FAILURE);
    }

    return val;
}

int main(int argc, char *argv[]) {
    int opt;

    char *file = NULL;
    uint64_t nid = 0;
    int nid_provided = 0;

    pst_mode_t mode = MODE_NONE;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "f:n:HFBIDAh", long_options, NULL)) != -1) {
        switch (opt) {

            case 'f':
                file = optarg;
                break;

            case 'n':
                nid = parse_nid(optarg);
                nid_provided = 1;
                break;

            case 'H':
                mode = MODE_HEADERS;
                break;

            case 'F':
                mode = MODE_FULL;
                break;

            case 'B':
                mode = MODE_BODY;
                break;

            case 'I':
                mode = MODE_FULL_SINGLE;
                break;

            case 'D':
                mode = MODE_FOLDER;
                break;

            case 'A':
                mode = MODE_ATTACHMENTS;
                break;

            case 'h':
                print_help(argv[0]);
                return EXIT_SUCCESS;

            default:
                print_help(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (!file) {
        fprintf(stderr, "Error: -f (file) is mandatory\n");
        return EXIT_FAILURE;
    }

    if (mode == MODE_NONE) {
        fprintf(stderr, "Error: you must specify one mode\n");
        return EXIT_FAILURE;
    }

    if ((mode == MODE_BODY ||
         mode == MODE_FULL_SINGLE ||
         mode == MODE_FOLDER ||
         mode == MODE_ATTACHMENTS) && !nid_provided) {

        fprintf(stderr, "Error: -n (nid) is required for this mode\n");
        return EXIT_FAILURE;
    }

    switch (mode) {

        case MODE_HEADERS:
            pst_stdout_stream_email_header(file);
            break;

        case MODE_FULL:
            pst_stdout_stream_emails_full_info(file);
            break;

        case MODE_BODY:
            pst_stdout_stream_email_body(file, nid);
            break;

        case MODE_FULL_SINGLE:
            pst_stdout_stream_email_full_info(file, nid);
            break;

        case MODE_FOLDER:
            pst_stdout_email_folder(file, nid);
            break;

        case MODE_ATTACHMENTS:
            pst_extracxt_email_attachments(file, nid);
            break;

        default:
            fprintf(stderr, "Invalid mode\n");
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}