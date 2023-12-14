Project group number: 33

Group member names and x500s:

    Yuan Jiang ( jian0655 )
    Matthew Olson ( olso9444 )
    Thomas Nicklaus ( nickl098 )

CSELabs computer: 

    csel-kh1250-35.cselabs.umn.edu

Members’ individual contributions:

    Yuan: 2.1 2.1.1 2.2 extra work
    Matt: 2.1 2.1.1 2.2 extra work
    Thomas: 2.1 2.2

Any changes you made to the Makefile or existing files that would affect grading:

    Add helper functions: get_file_size(), create_packet() in client.c and server.c

    By "make test", there are 22 (11 for original image data by recv and 11 for processed image data) ".png" temporary files will be created in the same directory. But After running, all files will be deleted by the code

    We also add "rm -f *.png" in case to delete all .png file after "make clean"

    (extra work): We add extra work helper functions in all .h files and add checksum parts in all .c files

    (extra work): The extra work part are all labeled "//******************** extra work" in the code

    (extra work): Compile the project with -lssl -lcrypto flags to link against OpenSSL in Makefile

Any assumptions that you made that weren’t outlined in section 7:
    
    (Extra work) Implementation of OpenSSL EVP interface

How you designed your program for Package sending and processing (again, high-level pseudocode would be acceptable/preferred for this part):

    In below pseudocode, assume all packets transfer have been processed by deserializing and serializing
    (Extra work) Also assume all data have been verified by checksum which will be discussed in next section

    send_data:
        // Open the file for reading
        file = open file with filename in read-binary mode
         // Get the file size
        long file_size = get_file_size(filename);
        // Read file data into buffer
        fread(buffer, 1, file_size, file);
        // Send file data over the socket
        send(socket, buffer, file_size, 0);

    process_image:
        // Generate a temporary file name based on the threadID
        tempfile = format "%lu.png" with (unsigned long) threadID
        // Open the file for writing
        file = open tempfile in write-binary mode
        // Write image data to the file
        fwrite(tempfile,data)
        close tempfile
        
        // Load the image process by 180/270 degree rotation based on tempfile
        // code from PA3
        tempfile2 = format "%lu0.png" with (unsigned long) threadID
        // save processed image data to new file tempfile2
        
        // read the processed image data to buffer
        fread(tempfile2,data)
        close tempfile2

        // Delete all temporary files
        remove(tempfile)
        remove(tempfile2)
        // send ACK or NACK
        send(sock, &ak_packet, sizeof(ak_packet), 0)
        // send processed image data back with processed size
        send(sock, buffer, sizeof(buffer), 0)

    receive_data:
        // get a output file path
        char filepath[PATH_MAX];
        char* fn = strrchr(filename, '/')+1;
        snprintf(filepath, sizeof(filepath), "%s/%s", output_dir, fn);

        // open file to write
        FILE *file = fopen(filepath, "w");
        char *buffer = malloc(rec_file_size);
        // receive processed image data
        int bytes_received = recv(socket, buffer, rec_file_size, 0);
        // write to output file
        fwrite(buffer, 1, bytes_received, file);

If your original design (intermediate submission) was different, explain how
it changed:

    1. create more threads for different queue/client request
    2. use ACK and NACK to make sure correct data transmission
    3. deserializing and serializing for packet
    4. (extra work) implement error detection/correction and the package encryption

Any code that you used AI helper tools to write with a clear justification and explanation of the code (Please see below for the AI tools acceptable use policy):

    1. Ask ChatGPT some mem pointer's questions to create helper function process_image()
    2. Ask help for extra work ideas and implement SHA-256 by google and ChatGPT

How you implement error detection/correction and the package encryption (extra credit, optional):

    The extra work part are all labeled "//******************** extra work" in the code

    The implementation involves creating and verifying checksums for data packets transmitted over a network. 
    SHA-256, a secure hashing algorithm, is used to generate checksums that help in verifying the integrity of the data received.

    Dependencies: OpenSSL: This project uses OpenSSL for SHA-256 hashing.

    Key Components:

        packet_t Structure: Defines the structure of the packets being sent and received, including the checksum field.
        create_packet2: A function that prepares a packet, including calculating its checksum using SHA-256.
        compute_sha256: A function that computes the SHA-256 hash of given data. 
        
    ***Note: compute_sha256 is implemented using the OpenSSL EVP interface. It initializes a digest context, hashes the input data, finalizes the digest, and writes the output hash.

    Sending and Receiving Data:

        When sending data, the packet header (including the checksum) is sent first, followed by the actual data.
        Upon receiving data, the checksum is recomputed and verified against the one received in the packet to check data integrity

    Checksum Verification:

        After receiving data, compute_sha256 is used to calculate the checksum of the received data, which is then compared with the checksum sent in the packet.

    Compilation:

        Compile the project with -lssl -lcrypto flags to link against OpenSSL in Makefile. 