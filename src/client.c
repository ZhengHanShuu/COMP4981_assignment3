#include "../include/client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void           parse_arguments(int argc, char *argv[], char **address, char **port);
static void           handle_arguments(const char *binary_name, const char *address, const char *port_str, in_port_t *port);
static in_port_t      parse_in_port_t(const char *binary_name, const char *port_str);
_Noreturn static void usage(const char *program_name, int exit_code, const char *message);
static void           convert_address(const char *address, struct sockaddr_storage *addr, socklen_t *addr_len);
static int            socket_create(int domain, int type, int protocol);
static void           get_address_to_server(struct sockaddr_storage *addr, in_port_t port);
static void           socket_close(int sockfd);
static void           send_command(int sockfd, const struct sockaddr_storage *addr, socklen_t addr_len, const char *command);

#define UNKNOWN_OPTION_MESSAGE_LEN 24
#define BASE_TEN 10
#define SIXTY_FOUR 64
#define TEN 10

void send_command(int sockfd, const struct sockaddr_storage *addr, socklen_t addr_len, const char *command)
{
    if(sendto(sockfd, command, strlen(command), 0, (const struct sockaddr *)addr, addr_len) == -1)
    {
        perror("sendto command");
        exit(EXIT_FAILURE);
    }
}

_Noreturn void run_client(int argc, char *argv[])
{
    char     *address;
    char     *port_str;
    in_port_t port;
    int       sockfd;

    struct sockaddr_storage addr;
    socklen_t               addr_len;
    int                     x                  = 0;
    int                     y                  = 0;
    char                    buffer[SIXTY_FOUR] = {0};

    address  = NULL;
    port_str = NULL;

    parse_arguments(argc, argv, &address, &port_str);
    handle_arguments(argv[0], address, port_str, &port);
    convert_address(address, &addr, &addr_len);
    sockfd = socket_create(addr.ss_family, SOCK_DGRAM, 0);
    get_address_to_server(&addr, port);
    memset(buffer, 0, sizeof(buffer));

    initscr();               // Initialize ncurses
    cbreak();                // Disable line buffering
    noecho();                // Don't echo keypresses
    keypad(stdscr, TRUE);    // Enable function and arrow keys

    clear();
    mvprintw(y, x, "@");    // @ represents the character
    refresh();

    while(true)
    {
        char                   *next_number;
        int                     ch;
        struct sockaddr_storage sender_addr;
        socklen_t               sender_addr_len;
        ssize_t                 bytes_received;
        sender_addr_len = sizeof(sender_addr);

        ch = getch();

        switch(ch)
        {
            case KEY_UP:
                send_command(sockfd, &addr, addr_len, "UP");
                break;
            case KEY_DOWN:
                send_command(sockfd, &addr, addr_len, "DOWN");
                break;
            case KEY_LEFT:
                send_command(sockfd, &addr, addr_len, "LEFT");
                break;
            case KEY_RIGHT:
                send_command(sockfd, &addr, addr_len, "RIGHT");
                break;
            case 'q':    // Quit on 'q' key press
                endwin();
                socket_close(sockfd);
                exit(EXIT_SUCCESS);
            default:
                // Handle other keys or exit
                break;
        }

        bytes_received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
        if(bytes_received == -1)
        {
            perror("recvfrom");
            endwin();
            exit(EXIT_FAILURE);
        }

        buffer[bytes_received] = '\0';    // Null-terminate the buffer

        next_number = strchr(buffer, '(');
        if(next_number)
        {
            char *endptr = NULL;
            long  lx;
            long  ly;
            next_number++;
            lx = strtol(next_number, &endptr, TEN);
            if(next_number == endptr)
            {
                continue;
            }
            next_number = strchr(endptr, ',');
            if(!next_number)
            {
                continue;
            }
            next_number++;
            ly = strtol(next_number, &endptr, TEN);

            if(next_number == endptr)
            {
                continue;
            }

            if(*endptr != ')')
            {
                continue;
            }
            mvprintw(y, x, " ");    // Clear the previous character
            x = (int)lx;
            y = (int)ly;

            mvprintw(y, x, "@");    // Print the character at the new position
            refresh();              // Refresh the screen to show the update
        }
    }
}

static void parse_arguments(int argc, char *argv[], char **address, char **port)
{
    int opt;

    opterr = 0;

    while((opt = getopt(argc, argv, "h")) != -1)
    {
        switch(opt)
        {
            case 'h':
            {
                usage(argv[0], EXIT_SUCCESS, NULL);
            }
            case '?':
            {
                char message[UNKNOWN_OPTION_MESSAGE_LEN];

                snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
                usage(argv[0], EXIT_FAILURE, message);
            }
            default:
            {
                usage(argv[0], EXIT_FAILURE, NULL);
            }
        }
    }

    if(optind + 1 >= argc)
    {
        // If there are not enough arguments for address and port
        usage(argv[0], EXIT_FAILURE, "An IP address and port are required.");
    }

    *address = argv[optind];
    *port    = argv[optind + 1];
}

static void handle_arguments(const char *binary_name, const char *address, const char *port_str, in_port_t *port)
{
    if(address == NULL)
    {
        usage(binary_name, EXIT_FAILURE, "The address is required.");
    }

    if(port_str == NULL)
    {
        usage(binary_name, EXIT_FAILURE, "The port is required.");
    }

    *port = parse_in_port_t(binary_name, port_str);
}

in_port_t parse_in_port_t(const char *binary_name, const char *str)
{
    char     *endptr;
    uintmax_t parsed_value;

    errno        = 0;
    parsed_value = strtoumax(str, &endptr, BASE_TEN);

    if(errno != 0)
    {
        perror("Error parsing in_port_t");
        exit(EXIT_FAILURE);
    }

    // Check if there are any non-numeric characters in the input string
    if(*endptr != '\0')
    {
        usage(binary_name, EXIT_FAILURE, "Invalid characters in input.");
    }

    // Check if the parsed value is within the valid range for in_port_t
    if(parsed_value > UINT16_MAX)
    {
        usage(binary_name, EXIT_FAILURE, "in_port_t value out of range.");
    }

    return (in_port_t)parsed_value;
}

_Noreturn static void usage(const char *program_name, int exit_code, const char *message)
{
    if(message)
    {
        fprintf(stderr, "%s\n", message);
    }

    fprintf(stderr, "Usage: %s [-h] <address> <port>\n", program_name);
    fputs("Options:\n", stderr);
    fputs("  -h  Display this help message\n", stderr);
    exit(exit_code);
}

static void convert_address(const char *address, struct sockaddr_storage *addr, socklen_t *addr_len)
{
    memset(addr, 0, sizeof(*addr));

    if(inet_pton(AF_INET, address, &(((struct sockaddr_in *)addr)->sin_addr)) == 1)
    {
        addr->ss_family = AF_INET;
        *addr_len       = sizeof(struct sockaddr_in);
    }
    else if(inet_pton(AF_INET6, address, &(((struct sockaddr_in6 *)addr)->sin6_addr)) == 1)
    {
        addr->ss_family = AF_INET6;
        *addr_len       = sizeof(struct sockaddr_in6);
    }
    else
    {
        fprintf(stderr, "%s is not an IPv4 or an IPv6 address\n", address);
        exit(EXIT_FAILURE);
    }
}

static int socket_create(int domain, int type, int protocol)
{
    int sockfd;

    sockfd = socket(domain, type, protocol);

    if(sockfd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

static void get_address_to_server(struct sockaddr_storage *addr, in_port_t port)
{
    if(addr->ss_family == AF_INET)
    {
        struct sockaddr_in *ipv4_addr;

        ipv4_addr             = (struct sockaddr_in *)addr;
        ipv4_addr->sin_family = AF_INET;
        ipv4_addr->sin_port   = htons(port);
    }
    else if(addr->ss_family == AF_INET6)
    {
        struct sockaddr_in6 *ipv6_addr;

        ipv6_addr              = (struct sockaddr_in6 *)addr;
        ipv6_addr->sin6_family = AF_INET6;
        ipv6_addr->sin6_port   = htons(port);
    }
}

static void socket_close(int sockfd)
{
    if(close(sockfd) == -1)
    {
        perror("Error closing socket");
        exit(EXIT_FAILURE);
    }
}
