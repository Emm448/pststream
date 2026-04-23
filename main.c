#include "pst.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <inttypes.h>


void print_help(const char *prog) {
    printf(
        "Usage: %s -f <file.pst> [MODE] [OPTIONS]\n\n"
        "Modes (choose one):\n"
        "  -H            Print all email headers\n"
        "  -F            Print all emails full info\n"
        "  -B            Print single email body (requires -n)\n"
        "  -I            Print single email full info (requires -n)\n"
        "  -D            Print single email folder (requires -n)\n\n"
        "Options:\n"
        "  -f <file>     PST file (mandatory)\n"
        "  -n <nid>      Node ID (required for -B, -I, -D)\n"
        "  -h, --help    Show this help message\n\n"
        "Examples:\n"
        "  %s -f mail.pst -H\n"
        "  %s -f mail.pst -F\n"
        "  %s -f mail.pst -B -n 12345\n"
        "  %s -f mail.pst -I -n 12345\n"
        "  %s -f mail.pst -D -n 12345\n",
        prog, prog, prog, prog, prog, prog
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

    int flag_headers_all = 0;
    int flag_full_all = 0;
    int flag_body_single = 0;
    int flag_full_single = 0;
    int flag_folder = 0;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "f:n:HFBIDh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f':
                file = optarg;
                break;

            case 'n':
                nid = parse_nid(optarg);
                nid_provided = 1;
                break;

            case 'H':
                flag_headers_all = 1;
                break;

            case 'F':
                flag_full_all = 1;
                break;

            case 'B':
                flag_body_single = 1;
                break;

            case 'I':
                flag_full_single = 1;
                break;

            case 'D':
                flag_folder = 1;
                break;

            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);

            default:
                print_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!file) {
        fprintf(stderr, "Error: -f (file) is mandatory\n\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    int modes = flag_headers_all + flag_full_all +
                flag_body_single + flag_full_single +
                flag_folder;

    if (modes == 0) {
        fprintf(stderr, "Error: you must specify one mode\n\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (modes > 1) {
        fprintf(stderr, "Error: only one mode at a time\n\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    if ((flag_body_single || flag_full_single || flag_folder) && !nid_provided) {
        fprintf(stderr, "Error: -n (nid) is required for this mode\n\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (flag_headers_all) {
		pst_stdout_stream_email_header(file);
    }
    else if (flag_full_all) {
        pst_stdout_stream_emails_full_info(file);
    }
    else if (flag_body_single) {
        pst_stdout_stream_email_body(file, nid);
    }
    else if (flag_full_single) {
        pst_stdout_stream_email_full_info(file, nid);
    }
    else if (flag_folder) {
        pst_stdout_email_folder(file, nid);
    }

    return EXIT_SUCCESS;
}
