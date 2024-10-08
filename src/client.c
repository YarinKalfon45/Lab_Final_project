#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define SERVER_IP "192.15.6.1"   // Server IP address
#define SERVER_PORT 8080    // Server port
#define BUFFER_SIZE 512    // Buffer size for receiving data

typedef enum {
    ERROR,
    MSG_KEEP_ALIVE,
    MSG_REQUEST_MENU,
    MSG_MENU,
    MSG_ORDER,
    MSG_ESTIMATED_TIME,
    MSG_RESTAURANT_OPTIONS,
    REST_UNAVALIABLE,
    MSG_LEAVE,
    MSG_TOKEN
} message_type_t;

typedef struct {
    message_type_t type;
    char data[BUFFER_SIZE];
    char client_token[BUFFER_SIZE];
} message_t;

char my_token[BUFFER_SIZE];

void *server_communication(void *arg);
void *keep_alive(void *arg);

int main() {
    struct sockaddr_in server_addr; // Server address
    int sock; // Socket descriptor

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Create a socket for sending and receiving data
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET; // Set the address family to IPv4
    server_addr.sin_port = htons(SERVER_PORT); // Set the port number

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) { // Convert the IP address from text to binary form
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { // Connect to the server at the specified address
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    // Create keep-alive thread
    pthread_t keep_alive_thread;
    if (pthread_create(&keep_alive_thread, NULL, keep_alive, (void *)&sock) != 0) { // Create a thread for sending keep-alive messages
        perror("Keep-alive thread creation failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    pthread_detach(keep_alive_thread); // Detach the keep-alive thread to allow it to run independently

    // Create server communication thread
    pthread_t server_comm_thread;
    if (pthread_create(&server_comm_thread, NULL, server_communication, (void *)&sock) != 0) { // Create a thread for server communication
        perror("Server communication thread creation failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    pthread_join(server_comm_thread, NULL); // Wait for the server communication thread to finish

    close(sock); // Close the socket
    return 0;
}

void *server_communication(void *arg) {
    int sock = *(int *)arg; // Socket descriptor
    message_t msg;
    memset(&msg, 0, sizeof(message_t));  // Ensure message is zeroed out

    // Receive token from server
    ssize_t bytes_received = recv(sock, &msg, sizeof(message_t), 0);
    if (bytes_received <= 0) {
        perror("recv");
        close(sock);
        pthread_exit(NULL);
    }

    if (msg.type == MSG_TOKEN) {
        strcpy(my_token, msg.data); // Correctly copy the token from msg.data to my_token
        printf("Received token from server: %s\n", my_token);
    } else {
        perror("Expected token message");
        close(sock);
        pthread_exit(NULL);
    }
    printf("i got message type %d, i wanted token\n", msg.type);
    printf("this is the data %s\n", msg.data);
    printf("this is my token now: %s\n", my_token);

    while (1) {
        // Send a message to request the list of available restaurants
        msg.type = MSG_REQUEST_MENU;
        strcpy(msg.data, "REQUEST_MENU");
        strcpy(msg.client_token, my_token); // Include the client's token in the message
        printf("Client requesting available restaurants\n");
        ssize_t bytes_sent = send(sock, &msg, sizeof(message_t), 0);
        if (bytes_sent <= 0) {
            perror("send");
            close(sock);
            pthread_exit(NULL);
        }

        // Receive restaurant options from server
        do {
            ssize_t bytes_received = recv(sock, &msg, sizeof(message_t), 0);
            if (bytes_received <= 0) {
                perror("recv");
                close(sock);
                pthread_exit(NULL);
            }
        } while (msg.type == 0);

        printf("Client received message type %d\n", msg.type);

        if (msg.type == MSG_RESTAURANT_OPTIONS) {
            printf("Restaurants:\n%s\n", msg.data); // Print the received data

            // Choose a restaurant
            printf("Enter the number of the restaurant you want to order from: "); // Prompt the user to enter a choice
            fflush(stdout); // Flush the output buffer
            int choice; // User choice
            if (scanf("%d", &choice) != 1 || choice < 1 || choice > 3) { // Read the user choice
                printf("Invalid choice.\n");
                close(sock);
                pthread_exit(NULL);
            }

            msg.type = MSG_ORDER;
            sprintf(msg.data, "%d", choice);
            strcpy(msg.client_token, my_token); // Include the client's token in the message
            bytes_sent = send(sock, &msg, sizeof(message_t), 0);
            if (bytes_sent <= 0) {
                perror("send");
                close(sock);
                pthread_exit(NULL);
            }

            // Receive response from server
            do {
                ssize_t bytes_received = recv(sock, &msg, sizeof(message_t), 0);
                if (bytes_received <= 0) {
                    perror("recv");
                    close(sock);
                    pthread_exit(NULL);
                }
            } while (msg.type == 0);

            printf("Client received message type %d\n", msg.type);

            if (msg.type == REST_UNAVALIABLE) {
                printf("%s", msg.data);
                continue;
            } else if (msg.type == MSG_MENU) {
                printf("Menu received:\n%s\n", msg.data); // Print the received menu

                // Choose a meal
                printf("Enter the number of the meal you want to order: "); // Prompt the user to enter a choice
                fflush(stdout); // Flush the output buffer
                int meal_choice; // User choice
                if (scanf("%d", &meal_choice) != 1 || meal_choice < 1 || meal_choice > 10) { // Read the user choice
                    printf("Invalid choice.\n");
                    close(sock);
                    pthread_exit(NULL);
                }

                msg.type = MSG_ORDER;
                sprintf(msg.data, "ORDER: %d", meal_choice);
                strcpy(msg.client_token, my_token); // Include the client's token in the message
                bytes_sent = send(sock, &msg, sizeof(message_t), 0);
                if (bytes_sent <= 0) {
                    perror("send");
                    close(sock);
                    pthread_exit(NULL);
                }

                // Receive time estimation from server
                do {
                    ssize_t bytes_received = recv(sock, &msg, sizeof(message_t), 0);
                    if (bytes_received <= 0) {
                        perror("recv");
                        close(sock);
                        pthread_exit(NULL);
                    }
                } while (msg.type == 0);

                if (msg.type != MSG_ESTIMATED_TIME) {
                    perror("Expected estimated time message");
                    close(sock);
                    pthread_exit(NULL);
                }
                printf("Estimated time for your order: %s\n", msg.data); // Print the time estimation
            } else {
                perror("Unexpected message type");
                close(sock);
                pthread_exit(NULL);
            }
        } else {
            perror("Expected restaurant options message");
            close(sock);
            pthread_exit(NULL);
        }
        break; // Exit the loop once an order is successfully placed and time estimation is received
    }
    return NULL; // Return from the thread
}

// Keep-alive thread function
void *keep_alive(void *arg) {
    int sock = *(int *)arg; // Socket descriptor
    message_t keep_alive_msg;
    memset(&keep_alive_msg, 0, sizeof(message_t));  // Ensure message is zeroed out
    keep_alive_msg.type = MSG_KEEP_ALIVE;
    strcpy(keep_alive_msg.data, "KEEP_ALIVE");
    strcpy(keep_alive_msg.client_token, my_token); // Include the client's token in the message

    while (1) { // Loop to send keep-alive messages
        sleep(30); // Send keep-alive message every 30 seconds
        ssize_t bytes_sent = send(sock, &keep_alive_msg, sizeof(message_t), 0);
        if (bytes_sent <= 0) {
            perror("send");
            close(sock);
            pthread_exit(NULL);
        }
        printf("\nKEEP_ALIVE sent\n"); // Print the keep-alive message
    }
    return NULL; // Return from the thread
}
