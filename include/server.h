#ifndef IMAGE_SERVER_ROTATION_H_
#define IMAGE_SERVER_ROTATION_H_

#define _XOPEN_SOURCE

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <stdint.h>
#include "utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define CHANNEL_NUM 1

#include "hash.h"
#include "stb_image.h"
#include "stb_image_write.h"

#include <string.h>
#include <openssl/evp.h>
#include <openssl/sha.h>  


/********************* [ Helpful Macro Definitions ] **********************/
#define BUFF_SIZE 1024 
#define LOG_FILE_NAME "request_log"               //Standardized log file name
#define INVALID -1                                  //Reusable int for marking things as invalid or incorrect 

// Operations
#define IMG_OP_ACK      (1 << 0)
#define IMG_OP_NAK      (1 << 1)
#define IMG_OP_ROTATE   (1 << 2)
#define IMG_OP_EXIT     (1 << 3)

// Flags
#define IMG_FLAG_ROTATE_180     (1 << 0)
#define IMG_FLAG_ROTATE_270     (1 << 1)
#define IMG_FLAG_ENCRYPTED      (1 << 2)
#define IMG_FLAG_CHECKSUM       (1 << 3)

/********************* [ Helpful Typedefs        ] ************************/

typedef struct packet {
    unsigned char operation : 4;
    unsigned char flags : 4;
    unsigned int size;
    unsigned char checksum[SHA256_DIGEST_LENGTH];
} packet_t;

// Helper function to create the packet with the required flags
packet_t create_packet(int operation, int flags, int size) {
    packet_t packet;
    packet.operation = operation;
    packet.flags = flags;
    packet.size = htonl(size);  // Ensure size is in network byte order
    // checksum here if needed
    return packet;
}

//******************** extra work

// Function to create a packet with checksum
packet_t create_packet2(int operation, int flags, int size, char *data) {
    packet_t packet;
    EVP_MD_CTX *ctx;
    unsigned char hash_output[SHA256_DIGEST_LENGTH];
    unsigned int length;

    packet.operation = operation;
    packet.flags = flags;
    packet.size = htonl(size);

    // Using OpenSSL's EVP functions
    ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, size);
    EVP_DigestFinal_ex(ctx, hash_output, &length);
    EVP_MD_CTX_free(ctx);

    memcpy(packet.checksum, hash_output, SHA256_DIGEST_LENGTH);

    return packet;
}

void compute_sha256(char *data, size_t len, unsigned char *output) {
    EVP_MD_CTX *ctx;

    // Create a new digest context
    ctx = EVP_MD_CTX_new();
    if (!ctx) {
        // Handle error: failed to allocate context
        return;
    }

    // Initialize the digest context for SHA-256
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        // Handle error: failed to initialize
        EVP_MD_CTX_free(ctx);
        return;
    }

    // Hash the data
    if (EVP_DigestUpdate(ctx, data, len) != 1) {
        // Handle error: failed to hash data
        EVP_MD_CTX_free(ctx);
        return;
    }

    // Finalize the digest and store the output in the provided buffer
    unsigned int lengthOfHash;
    if (EVP_DigestFinal_ex(ctx, output, &lengthOfHash) != 1) {
        // Handle error: failed to finalize
        EVP_MD_CTX_free(ctx);
        return;
    }

    // Clean up the context
    EVP_MD_CTX_free(ctx);
}

//******************** extra work

unsigned int get_file_size(const char *filename) {
    FILE *file = fopen(filename, "rb");  // Open the file in binary mode
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {  // Seek to the end of the file
        perror("Error seeking to end of file");
        fclose(file);
        return -1;
    }

    unsigned int size = ftell(file);  // Get the size of the file
    if (size == -1) {
        perror("Error getting file size");
        fclose(file);
        return -1;
    }

    fclose(file);  // Close the file
    return size;  // Return the size
}

typedef struct thread_args {
    int* sock;
    int threadID;
} thread_args_t;

const int PACKETSZ = sizeof(packet_t);

// serialize packet
char *serializePacket(packet_t *packet){
    char *serializedData = (char *)malloc(sizeof(char) * PACKETSZ);
    memset(serializedData, 0, PACKETSZ);
    memcpy(serializedData, packet, PACKETSZ);
    return serializedData;
}

// deserialize data
packet_t *deserializeData(char *serializedData){
    packet_t *packet = (packet_t *)malloc(sizeof(packet_t));
    memset(packet, 0, PACKETSZ);
    memcpy(packet, serializedData, PACKETSZ);
    return packet;
}

#endif
