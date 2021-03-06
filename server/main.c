#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DynamicStringArray.h"
#include "EventDispatch.h"

//// Argument parser stuff
const char* argp_program_version = "DebuggerBootstrap 0.1";
const char* argp_program_bug_address = "https://github.com/FrankGoyens/DebuggerBootstrap/issues";

static char doc[] = "DebuggerBootstrap -- Automatically runs GDBServer when the right conditions are met.";

static char args_doc[] = "[-p PORT] [--gdbserver-binary PATH]";

static struct argp_option options[] = {{"verbose", 'v', 0, 0, "Produce verbose output"},
                                       {"quiet", 'q', 0, 0, "Don't produce any output"},
                                       {"silent", 's', 0, OPTION_ALIAS},
                                       {"port", 'p', "PORT", 0, "Run DebuggerBootstrap at the given PORT"},
                                       {"gdbserver-binary", 'g', "PATH", 0, "Use the GDBServer located at PATH"},
                                       {0}};

struct arguments {
    char* gdbserver_binary;
    int port;
    int verbose, silent;
};

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    struct arguments* arguments = state->input;

    switch (key) {
    case 'q':
    case 's':
        arguments->silent = 1;
        break;
    case 'v':
        arguments->verbose = 1;
        break;
    case 'g':
        arguments->gdbserver_binary = arg;
        break;
    case 'p': {
        errno = 0;
        arguments->port = (int)strtol(arg, NULL, 10);
        if (errno != 0)
            // The port could not be parsed as int
            argp_usage(state);
    }

    case ARGP_KEY_ARG:
        break;

    case ARGP_KEY_END:
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};
//// Argument parser stuff END

void SetDefaults(struct arguments* arguments) {
    arguments->gdbserver_binary = "/usr/bin/gdbserver";
    arguments->port = 0;
    arguments->silent = 0;
    arguments->verbose = 0;
}

static void RetrieveArguments(int argc, char** argv, struct arguments* arguments) {
    SetDefaults(arguments);

    argp_parse(&argp, argc, argv, 0, 0, arguments);

    printf("Chosen port %d\n", arguments->port);
    if (arguments->port == 0)
        printf("\t(port will be determined and reported later)\n");
    printf("GDBServer binary that will be used: %s\n", arguments->gdbserver_binary);
}

// Returns TRUE when the separator was found
static int FindPassthroughSeparator(int argc, char** argv, int* position) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--") == 0) {
            *position = i;
            return 1;
        }
    }
    return 0;
}

static void RetrieveArgumentsForDebugger(int argc, char** argv, int passthrough_separator,
                                         DebuggerParameters* debugger_parameters) {
    if (passthrough_separator >= argc)
        return;
    for (int i = passthrough_separator + 1; i < argc; ++i) {
        DynamicStringArrayAppend(&debugger_parameters->debugger_args, argv[i]);
    }
}

int main(int argc, char** argv) {
    DebuggerParameters debugger_arguments;
    DynamicStringArrayInit(&debugger_arguments.debugger_args);

    int passthrough_position;
    if (FindPassthroughSeparator(argc, argv, &passthrough_position) && passthrough_position > argc) {
        RetrieveArgumentsForDebugger(argc, argv, passthrough_position, &debugger_arguments);
        argc -= passthrough_position - 1; // Don't parse the passthrough arguments in our own parser
    }

    struct arguments arguments;
    RetrieveArguments(argc, argv, &arguments);
    debugger_arguments.debugger_path = arguments.gdbserver_binary;

    StartEventDispatch(arguments.port, &debugger_arguments);

    DynamicStringArrayDeinit(&debugger_arguments.debugger_args);
    return 0;
}
