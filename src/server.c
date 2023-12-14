#include "server.h"
//#include <openssl/md5.h>


#define PORT 9444
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024 

int loop = 1;

void sighand(int signo){
    loop = 0;
}

int process_image(void *socket, char* image_data, unsigned int image_size, int rotation_angle, pthread_t threadID){

    //char *tempfile = "temp.png";
    int sock = *(int*)socket;
    char tempfile[100];
    snprintf(tempfile, sizeof(tempfile), "%lu.png", (unsigned long) threadID);
    FILE *file = fopen(tempfile, "wb");
    if (file == NULL) {
        perror("Failed to open file");
        return -1;
    }

    if (fwrite(image_data, 1, image_size, file) != image_size) {
        perror("Failed to write image data");
        fclose(file);
        return -1;
    }

    fclose(file);
    
    /*
    Stbi_load takes:
            A file name, int pointer for width, height, and bpp

    */
    // Variables to hold the image dimensions and color channels
    int width, height, bpp;

    // Load the image using the stb_image library
    // Replace 'curImage->file_path' with the actual path variable from your request_item_t
    uint8_t *image_result = stbi_load(tempfile, &width, &height, &bpp, CHANNEL_NUM);
    if (image_result == NULL) {
        fprintf(stderr, "Failed to load the image from file name: %s\n",tempfile);
        // Handle the error: image loading failed
        return -1;
    }        

    uint8_t **result_matrix = (uint8_t **)malloc(sizeof(uint8_t*) * width);
    uint8_t** img_matrix = (uint8_t **)malloc(sizeof(uint8_t*) * width);
    for(int i = 0; i < width; i++){
        result_matrix[i] = (uint8_t *)malloc(sizeof(uint8_t) * height);
        img_matrix[i] = (uint8_t *)malloc(sizeof(uint8_t) * height);
    }


    /*
    linear_to_image takes: 
        The image_result matrix from stbi_load
        An image matrix
        Width and height that were passed into stbi_load
    
    */
    linear_to_image(image_result, img_matrix, width, height);        

    ////TODO: you should be ready to call flip_left_to_right or flip_upside_down depends on the angle(Should just be 180 or 270)
    //both take image matrix from linear_to_image, and result_matrix to store data, and width and height.
    //Hint figure out which function you will call. 
    //flip_left_to_right(img_matrix, result_matrix, width, height); or flip_upside_down(img_matrix, result_matrix ,width, height);
    // Now decide which flip function to call based on the rotation angle
    if (rotation_angle == 180) {
        // Assuming 'flip_left_to_right' is defined and alters 'img_matrix' in place
        flip_left_to_right(img_matrix, result_matrix, width, height);
    } else if (rotation_angle == 270) {
        // Assuming 'flip_upside_down' is defined and alters 'img_matrix' in place
        flip_upside_down(img_matrix, result_matrix, width, height);
    } else {
        // Handle error: invalid rotation angle
        fprintf(stderr, "Invalid rotation angle: %d\n", rotation_angle);
    }

    
    ///Hint malloc using sizeof(uint8_t) * width * height
    uint8_t* img_array = malloc(sizeof(uint8_t) * width * height);
    if (img_array == NULL) {
        // Handle error: memory allocation failed for the image array
        fprintf(stderr, "Memory allocation failed for the image array.\n");
        // Free the image matrix
        for (int i = 0; i < height; ++i) {
            free(img_matrix[i]);
        }
        free(img_matrix);
        stbi_image_free(image_result);
        return -1;
    }

    ///TODO: you should be ready to call flatten_mat function, using result_matrix
    //img_arry and width and height; 
    flatten_mat(result_matrix, img_array, width, height);



    ///TODO: You should be ready to call stbi_write_png using:
    //New path to where you wanna save the file,
    //Width
    //height
    //img_array
    //width*CHANNEL_NUM
    char tempfile2[100];
    snprintf(tempfile2, sizeof(tempfile2), "%lu-2.png", (unsigned long) threadID);

    stbi_write_png(tempfile2, width, height, CHANNEL_NUM, img_array, width*CHANNEL_NUM);


    // free all before unlock
    for(int i = 0; i < width; ++i) {
        free(result_matrix[i]);
        free(img_matrix[i]);
    }
    free(result_matrix);
    free(img_matrix);
    stbi_image_free(image_result); // Free the memory allocated by stbi_load

    free(img_array);

    FILE *file2 = fopen(tempfile2, "rb");
    if (!file2) {
        perror("Unable to open file");
        return -1;
    }

    // Get the file size
    unsigned int file_size = get_file_size(tempfile2);

    if (file_size == -1) {
        // get_file_size will handle error message
        perror("Failed to get file size");
        fclose(file2);
        return -1;
    }

    // Send the file data
    

    char* buffer = malloc(file_size);
    if (buffer == NULL){
        perror("Failed to reallocate mem for new image data");
        fclose(file2);
        free(buffer);
        return -1;
    }

    if (fread(buffer, 1, file_size, file2) != file_size) {
        perror("Failed to read the file 2");
        free(buffer);
        fclose(file2);
        return -1;
    }

    fclose(file2);  // Close the file

    if (remove(tempfile) != 0){
    perror("error delete temp1");
        return -1;
    }

    if (remove(tempfile2) != 0){
    perror("error delete temp2");
        return -1;
    }

    //Send back package
    packet_t ak_packet2;
    ak_packet2.operation = IMG_OP_ACK;
    ak_packet2.flags = rotation_angle;
    ak_packet2.size = file_size;
    char *ak_packet = serializePacket(&ak_packet2);
    int send_ak = send(sock, ak_packet, PACKETSZ, 0);
    if(send_ak == -1){
        free(socket);
        free(image_data);
        free(ak_packet);
        perror("send ACK error");
        return -1;
    }
    //******************** extra work
    packet_t cspacket = create_packet2(IMG_OP_ROTATE, IMG_FLAG_ENCRYPTED, file_size, buffer);

    char *cs_pkt = serializePacket(&cspacket);


    if (send(sock, cs_pkt, PACKETSZ, 0) <= 0) {
        perror("Failed to send checksum data");
        free(socket);
        free(image_data);
        free(ak_packet);
        return -1;
    }
    //******************** extra work

    // Send back the processed image
    int send_proImage = send(sock, buffer, file_size, 0); // Simplified: ensure to handle partial sends
    if(send_proImage == -1){
        free(socket);
        free(image_data);
        free(ak_packet);
        perror("send processed image error");
        return -1;
    }

    free(buffer);
    free(ak_packet);


    return 0;


}

void *clientHandler(void *socket) {

    // Receive packets from the client

    // Determine the packet operatation and flags

    // Receive the image data using the size

    // Process the image data based on the set of flags

    // Acknowledge the request and return the processed image data
    pthread_t threadID = pthread_self();

    int sock = *(int*)socket;
    ssize_t bytes_received;
    while(1){
         // Receive the request packet with image size, data, and rotation angle
        char packet2[PACKETSZ];
        memset(packet2,0,PACKETSZ);
        bytes_received = recv(sock, packet2, PACKETSZ, 0);
        if (bytes_received <= 0) {
            // Handle error or disconnection
            free(socket);
            perror("recv package error");
            return NULL;
        }
        packet_t *packet = deserializeData(packet2);

        // Determine the packet operatation
        if (packet->operation == IMG_OP_EXIT) {
            // Close the connection and clean up
            close(sock);
            return NULL;
        }

        // Check the flags to determine the rotation
        int rotation_angle = (packet->flags & IMG_FLAG_ROTATE_180) ? 180 : 270;

        // Receive the image file
        unsigned int image_size = ntohl(packet->size); // Assuming packet.size is in network byte order
        char *image_data = malloc(image_size);
        if (!image_data) {
            // Handle memory allocation failure
            free(socket);
            perror("image data mem allocate error");
            return NULL;
        }
        //******************** extra work

        char rc_pkt[PACKETSZ];
        memset(rc_pkt,0,PACKETSZ);
        ssize_t bytes_received = recv(sock, rc_pkt, PACKETSZ, 0);
        if(bytes_received <= 0) {
            // Handle errors or closed connection
            free(socket);
            free(image_data);
            perror("recv checksum error");
            continue;
        }

        packet_t *cspacket = deserializeData(rc_pkt);
        //******************** extra work
        

        bytes_received = recv(sock, image_data, image_size, 0);
        if (bytes_received <= 0) {
            // Handle incomplete transfer
            free(socket);
            free(image_data);
            perror("recv image data error");
            return NULL;
        }

        // Verify the checksum
        unsigned char computed_checksum[SHA256_DIGEST_LENGTH];
        compute_sha256(image_data, image_size, computed_checksum);

        if (memcmp(cspacket->checksum, computed_checksum, SHA256_DIGEST_LENGTH) != 0) {
            //Send back an error message
            packet_t nak_packet2;
            nak_packet2.operation = IMG_OP_NAK;
            char *nak_packet = serializePacket(&nak_packet2);
            int send_nack = send(sock, nak_packet, PACKETSZ, 0);
            if(send_nack == -1){
                free(socket);
                free(image_data);
                free(nak_packet);
                perror("send NACK error");
                return NULL;
            }
        }

        // Process the image (this is pseudo-code, replace with actual image processing)
        int success = process_image((void *)socket, image_data, image_size, rotation_angle, threadID);
        if (success < 0) {
            //Send back an error message
            packet_t nak_packet2;
            nak_packet2.operation = IMG_OP_NAK;
            char *nak_packet = serializePacket(&nak_packet2);
            int send_nack = send(sock, nak_packet, PACKETSZ, 0);
            if(send_nack == -1){
                free(socket);
                free(image_data);
                free(nak_packet);
                perror("send NACK error");
                return NULL;
            }
            
        }
        free(packet);
        
    }
    free(socket);
   
    return NULL;
}

int main(int argc, char* argv[]) {

    // Creating socket file descriptor

    // Bind the socket to the port

    // Listen on the socket

    // Accept connections and create the client handling threads

    // Release any resources


    int socket_desc, client_sock, c, *new_sock;
    struct sockaddr_in server, client;


    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        perror("Could not create socket");
        return 1;
    }

    // Prepare the sockaddr_in structure
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    // Bind the socket to the port
    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed");
        return 1;
    }

    // Listen on the socket
    if (listen(socket_desc, MAX_CLIENTS) < 0) {
        perror("listen failed");
        return 1;
    }

    // Accept an incoming connection
    c = sizeof(struct sockaddr_in);

    while (loop) {
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0){
            perror("accept failed");
            return 1;
        }
        // Create client handling thread
        pthread_t client_thread;
        new_sock = malloc(sizeof(int));
        *new_sock = client_sock;
        if (pthread_create(&client_thread, NULL, clientHandler, (void*)new_sock) < 0) {
            perror("could not create thread");
            free(new_sock);
            return 1;
        }
        pthread_detach(client_thread);
    }

    // Release any resources if needed
    close(socket_desc);
    
    return 0;
}
