#include "client.h"

#define PORT 9444
#define BUFFER_SIZE 1024 

char *input_dir;
char *output_dir;
int rotation_angle;
unsigned int rec_file_size;

int send_file(int socket, const char *filename) {
    // Open the file

    // Set up the request packet for the server and send it

    // Send the file data

    // Open the file


    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Unable to open file");
        return -1;
    }

    // Get the file size
    long file_size = get_file_size(filename);
    if (file_size == -1) {
        // get_file_size will handle error message
        perror("Failed to get file size");
        fclose(file);
        return -1;
    }

    // Send the file data
    char *buffer = malloc(file_size);
    if (buffer == NULL) {
        perror("Failed to allocate buffer");
        fclose(file);
        return -1;
    }

    if (fread(buffer, 1, file_size, file) != file_size) {
        perror("Failed to read the file 1");
        free(buffer);
        fclose(file);
        return -1;
    }

    //******************** extra work
    packet_t cspacket = create_packet2(IMG_OP_ROTATE, IMG_FLAG_ENCRYPTED, file_size, buffer);

    char *cs_pkt = serializePacket(&cspacket);

    if (send(socket, cs_pkt, PACKETSZ, 0) <= 0) {
        perror("Failed to send checksum data");
        free(buffer);
        fclose(file);
        return -1;
    }
    //******************** extra work

    if (send(socket, buffer, file_size, 0) <= 0) {
        perror("Failed to send file data");
        free(buffer);
        fclose(file);
        return -1;
    }


    fclose(file);  // Close the file
    free(buffer);

    return 0;      // Return success
}

int receive_file(int socket, const char *filename) {
    // Open the file

    // Receive response packet

    // Receive the file data

    // Write the data to the file

    char filepath[PATH_MAX];
    char* fn = strrchr(filename, '/')+1;

    snprintf(filepath, sizeof(filepath), "%s/%s", output_dir, fn);

    // open file to write
    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        perror("Failed to open file");
        return -1;
    }

    char *buffer = malloc(rec_file_size);
    if (buffer == NULL) {
        perror("Failed to allocate buffer");
        fclose(file);
        return -1;
    }
    //******************** extra work

    char rc_pkt[PACKETSZ];
    memset(rc_pkt,0,PACKETSZ);
    ssize_t bytes_received = recv(socket, rc_pkt, PACKETSZ, 0);
    if(bytes_received <= 0) {
        perror("recv checksum error");
        free(buffer);
        fclose(file);
        return -1;
    }

    packet_t *cspacket = deserializeData(rc_pkt);
    //******************** extra work

    // receive processed image data
    bytes_received = recv(socket, buffer, rec_file_size, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive data");
        free(buffer);
        fclose(file);
        return -1;
    }

    // Verify the checksum
    unsigned char computed_checksum[SHA256_DIGEST_LENGTH];
    compute_sha256(buffer, rec_file_size, computed_checksum);

    if (memcmp(cspacket->checksum, computed_checksum, SHA256_DIGEST_LENGTH) != 0) {
        perror("Failed to receive data");
        free(buffer);
        fclose(file);
        return -1;
    }

    // write to output file
    if (fwrite(buffer, 1, bytes_received, file) != bytes_received) {
        perror("Failed to write to file");
        free(buffer);
        fclose(file);
        return -1;
    }

    // free
    free(buffer);
    fclose(file);

    return 0;

}

int main(int argc, char* argv[]) {


    if(argc != 4) {
        fprintf(stderr, "Usage: ./client File_Path_to_images File_Path_to_output_dir Rotation_angle\n");
        return EXIT_FAILURE;
    }

    input_dir = argv[1];
    output_dir = argv[2];
    rotation_angle = atoi(argv[3]);

    // Set up socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        perror("socket error");
        return 1;
    }

    // Connect the socket
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);  // Port Number: last 4 digits
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("connect error");
        close(sock);
        return 1;
    }

    // Read the directory for all the images to rotate
    DIR *dir;
    struct dirent *entry;
    FileRotationInfo Q[MAX_QUEUE_LEN];
    int queue_size = 0;

    if((dir = opendir(input_dir)) == NULL) {
        perror("opendir");
        close(sock);
        return 1;
    }

    while((entry = readdir(dir)) != NULL && queue_size < MAX_QUEUE_LEN) {
        if (strstr(entry->d_name, ".png")) {
            char file_path[PATH_MAX];
            snprintf(file_path, sizeof(file_path), "%s/%s", input_dir, entry->d_name);
            strncpy(Q[queue_size].filename, file_path, PATH_MAX);
            Q[queue_size].rotation_angle = rotation_angle;
            queue_size++;
        }
    }
    closedir(dir);

    // Process the queue
    for(int i = 0; i < queue_size; i++) {
        // Check that the request was acknowledged
        // Create and send a rotate image packet
        packet_t rotate_packet2 = create_packet(
            IMG_OP_ROTATE,
            rotation_angle == 180 ? IMG_FLAG_ROTATE_180 : IMG_FLAG_ROTATE_270,
            get_file_size(Q[i].filename)
        );
        char *rotate_packet = serializePacket(&rotate_packet2);
        // Send the rotate packet
        if(send(sock, rotate_packet, PACKETSZ, 0) < 0) {
            perror("send rotate packet error");
            continue;
        }
        // Send the image data to the server
        if(send_file(sock, Q[i].filename) == -1) {
            perror("send_file");
            // Continue with the next file if there's an error
            continue;
        }

        // Now we expect to receive an ACK packet from the server
        char response_packet2[PACKETSZ];
        memset(response_packet2,0,PACKETSZ);
        ssize_t bytes_received = recv(sock, response_packet2, PACKETSZ, 0);
        if(bytes_received <= 0) {
            // Handle errors or closed connection
            perror("recv package error");
            continue;
        }

        packet_t *response_packet = deserializeData(response_packet2);


        // Ensure we received an ACK packet
        if(response_packet->operation != IMG_OP_ACK) {
            printf("%d", response_packet->operation);
            fprintf(stderr, "Did not receive IMG_OP_ACK from server\n");
            continue;
        }

        rec_file_size = response_packet->size;
        // Receive the processed image and save it in the output directory
        if(receive_file(sock, Q[i].filename) == -1) {
            perror("receive file error");
            // Continue with the next file if there's an error
            continue;
        }

        free(response_packet);
        free(rotate_packet);

    }


    // Terminate the connection once all images have been processed
    // Send a terminate message to the server
    packet_t exit_packet2 = create_packet(IMG_OP_EXIT, 0, 0);
    char *exit_packet = serializePacket(&exit_packet2);
    if(send(sock, exit_packet, PACKETSZ, 0) < 0) {
        perror("send terminate packet error");
    }
    free(exit_packet);
    // Close the Connection
    close(sock);

    return 0;
}
