#include "../include/client.h"
#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s server|client [options]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if(strcmp(argv[1], "server") == 0)
    {
        // Adjust argc and argv to pass relevant arguments to run_server
        run_server(argc - 1, &argv[1]);
    }
    else if(strcmp(argv[1], "client") == 0)
    {
        // Adjust argc and argv to pass relevant arguments to run_client
        run_client(argc - 1, &argv[1]);
    }
    else
    {
        fprintf(stderr, "Invalid mode '%s'. Please choose 'server' or 'client'.\n", argv[1]);
        fprintf(stderr, "Usage: %s server|client [options]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
