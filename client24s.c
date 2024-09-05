#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>  
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <tar.h>
#include <ctype.h>

//Defining global variables
#define PORT 16457
#define BUFSIZE 1024
#define SPDF_PORT 16458
#define STEXT_PORT 16459
#define SERVER_ADDR "127.0.0.1"

//function declaration
void handle_local_c_file(int sockfd, char *filepath);
void handle_remote_file(int sockfd, char *filepath, int remote_port, const char *replacement);
void handle_remote_tar(int sockfd, char *filetype, int remote_port, const char *replacement);
void handle_local_tar_file(int sockfd, char *filepath);


void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Helper function to trim leading, trailing spaces, and newline characters
char *trimming_whitespace(char *string) {
    char *end;
    // Trim leading space
    while(isspace((unsigned char)*string)) string++;
    if(*string == 0)  // All spaces or newline?
        return string;
    end = string + strlen(string) - 1;
    while(end > string && (isspace((unsigned char)*end) || *end == '\n')) end--;
    end[1] = '\0';
    return string;
}



// Function to connect to Spdf or Stext server
int connect_Server_to_Server(int port) {
    int sockfd;  // Declare a variable for the socket file descriptor
    struct sockaddr_in serv_addr;  // Declare a structure to hold the server's address information
    // Create a socket for TCP/IP communication (AF_INET for IPv4, SOCK_STREAM for TCP)
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("Socket opening failed"); 
    serv_addr.sin_family = AF_INET;  // Set the address family to IPv4
    serv_addr.sin_port = htons(port);  // Set the server's port number, converting to network byte order

    // Convert the IP address from text to binary form and store it in serv_addr.sin_addr
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        error("Invalid IP address"); 
    }

    // Attempt to connect to the server using the provided address and port
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("Connection to server failed");  
    return sockfd;  // Return the socket file descriptor for the connection
}

//---------------------------------------------------Forward File to the Server----------------------------------------
//Created a function for forwarding file to the respective server
void forward_file_to_server(int server_sockfd, char *filename, char *destination) {
    char buffer[BUFSIZE];
    FILE *fp;
    int n;
    // Send the command to the server (e.g., ufile filename destination)
    snprintf(buffer, sizeof(buffer), "ufile %s %s", filename, destination);
    n = write(server_sockfd, buffer, strlen(buffer));
    if (n < 0) error("Failed to write to server socket");
    // Open the file to read its data in read binary mode
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        return;
    }
    // Sending file data to the server
    while ((n = fread(buffer, sizeof(char), BUFSIZE, fp)) > 0) {
        if (write(server_sockfd, buffer, n) < 0) {
            error("Error writing file data to server");
        }
    }
    fclose(fp);
    // Shutdown the writing side of the socket to signal the end of data transmission
    shutdown(server_sockfd, SHUT_WR);
    // Receive acknowledgment from the respective server
    bzero(buffer, BUFSIZE);
    n = read(server_sockfd, buffer, BUFSIZE - 1);
    if (n < 0) error("Error reading from server socket");
    printf("Server response: %s\n", buffer);
}


//----------------------------------- receive file from Spdf or Stext Server------------------------------------------------
//Created a function for receiving file from the Spdf or Stext Server 
void receive_file_from_server(int sockfd, int client_sockfd) 
{
    //Array for storing buffer
    char buffer[BUFSIZE];
    int n;
    while ((n = read(sockfd, buffer, BUFSIZE)) > 0) 
    {
        if (write(client_sockfd, buffer, n) < 0) error("Failed to write to client socket");
    }

    close(sockfd);
}

//------------------------------------------------------------send a command to Spdf or Stext-----------------------------------------------
//Created a function for sending the command to the Spdf or Stext Server 
//So that they can handle their respective files at their own server
void send_command_to_server(int port, const char *command, const char *argument) {
    int sockfd = connect_Server_to_Server(port);
    write(sockfd, command, strlen(command));
    write(sockfd, argument, strlen(argument));
    close(sockfd);
}

//---------------------------------------------------------------replace smain with stext or spdf----------------------------------------------
void replace_smain_with_stext_or_spdf(char *destination, const char *replacement) {
    char *pos = strstr(destination, "smain");
    if (pos != NULL) {
        char temp[BUFSIZE];
        // Copy everything before "smain"
        strncpy(temp, destination, pos - destination);
        temp[pos - destination] = '\0';
        // Append the replacement (stext or spdf)
        strcat(temp, replacement);
        // Append everything after "smain"
        strcat(temp, pos + strlen("smain"));
        // Copy back to destination
        strcpy(destination, temp);
    }
}

//----------------------------------------------------------Upload File Function-------------------------------------------- 
//Created a upload file function for performing the ufile command 
void ufile_Command_Function(int sockfd, char *filename, char *destination) {
    char buffer[BUFSIZE];
    FILE *fp;
    int n;

    // Remove any trailing newline from destination path
    destination[strcspn(destination, "\n")] = 0;

// storing the file extension
    char *file_extension = strrchr(filename, '.');
    char fullpath[BUFSIZE];

    // making changes for respective files 
    //Found .pdf extension from the filename
    if (file_extension && strcmp(file_extension, ".pdf") == 0) {
        // Replacing "smain" with "spdf" in the destination path
        replace_smain_with_stext_or_spdf(destination, "spdf");
        // Forward the request to Spdf server
        // Connecting to spdf server using SPDF Port
        int spdf_sockfd = connect_Server_to_Server(SPDF_PORT);
        if (spdf_sockfd < 0) {
            perror("Error connecting to PDF server");
            return;
        }
        forward_file_to_server(spdf_sockfd, filename, destination);
        close(spdf_sockfd);
        return;
    } 
    // Found .txt extension from the filename
    else if (file_extension && strcmp(file_extension, ".txt") == 0) {
        // Replace "smain" with "stext" in the destination path
        replace_smain_with_stext_or_spdf(destination, "stext");
        // Forward the request to Stxt server
        int stxt_sockfd = connect_Server_to_Server(STEXT_PORT);
        if (stxt_sockfd < 0) {
            perror("Error connecting to Text server");
            return;
        }
        forward_file_to_server(stxt_sockfd, filename, destination);
        close(stxt_sockfd);
        return;
    } 
    // Found .c extension in the filename
    else if (file_extension && strcmp(file_extension, ".c") == 0) {
        // Perform task related to file .c on Smain server only
        printf("Processing .c file on main server.\n");
        snprintf(fullpath, sizeof(fullpath), "%s/%s", destination, filename);
    }

    // Check whether the directory exists or not, if not create a new
    struct stat st = {0};
    if (stat(destination, &st) == -1) {
        printf("Directory not found. Creating: %s\n", destination);
        // Create directories recursively
        char temp_storage[BUFSIZE];
        snprintf(temp_storage, sizeof(temp_storage), "%s", destination);
        for (char *p = temp_storage + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(temp_storage, 0700);
                *p = '/';
            }
        }
        mkdir(destination, 0700);  // Create final directory
    }

    // Handle .c and other supported file types locally
    fp = fopen(fullpath, "wb");
    if (fp == NULL) {
        perror("Error opening file");
        return;
    }

    // Write file data
    int total_bytes_received = 0;
    while ((n = read(sockfd, buffer, BUFSIZE)) > 0) {
        printf("Received %d bytes of data.\n", n); 
        if (n < 0) {
            perror("Error reading from socket");
            break;
        }
        fwrite(buffer, sizeof(char), n, fp);
        total_bytes_received += n;
    }
    printf("Total bytes received: %d\n", total_bytes_received); 
    fclose(fp);
    printf("File upload complete: %s\n", fullpath);

    // Send acknowledgment message to the client
    n = write(sockfd, "File upload successful.\n", 25);
    if (n < 0) error("Error writing to socket");
    printf("Acknowledgment sent to client.\n"); 
}



//--------------------------------------------------------Download function-------------------------------------------------
//Created a function for downloading files provided by the client from the server 
void dfile_Command_Function(int sockfd, char *filepath) {
    char buffer[BUFSIZE];
    int n;

    printf("Processing file in directory: %s\n", filepath);

    // Identifying the file extension
    char *file_extension = strrchr(filepath, '.');
    if (file_extension == NULL) {
        printf("File extension not found in the provided filepath: %s\n", filepath);
        write(sockfd, "Error: No file extension found\n", 31);
        return;
    }

    // Trimming down any whitespace if found in the file extension
    file_extension = trimming_whitespace(file_extension);
    printf("File type detected: '%s'\n", file_extension);

    if (strcmp(file_extension, ".c") == 0) 
    {
        // perform .c files locally
        handle_local_c_file(sockfd, filepath);
    } 
    else if (strcmp(file_extension, ".pdf") == 0) 
    {
        // Process .pdf files
        printf("Handling .pdf file type.\n");
        handle_remote_file(sockfd, filepath, SPDF_PORT, "spdf");

    } 
    else if (strcmp(file_extension, ".txt") == 0) 
    {
        // Process .txt files
        printf("Handling .txt file type.\n");
        handle_remote_file(sockfd, filepath, STEXT_PORT, "stext");
    } 
    else if (strcmp(file_extension, ".tar") == 0) 
    {
        // handling for .tar file
        handle_local_tar_file(sockfd, filepath);
    } 
    else 
    {
        printf("Requested file type is not supported: %s\n", filepath);
        write(sockfd, "Unsupported file type\n", 21);
        shutdown(sockfd, SHUT_WR);
    }

    printf("File processing complete.\n");
}

// creating a funcion for handling .c files locally on the Smain server 
void handle_local_c_file(int sockfd, char *filepath) {
    // An array to store buffer
    char buffer[BUFSIZE];
    FILE *fp;
    int n;

    printf("Accessing local .c file: %s\n", filepath);
    // Opening file in read bytes mode
    fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        //Write error to the client using socket fd
        write(sockfd, "Error: File not found\n", 22);
        shutdown(sockfd, SHUT_WR);
        return;
    }
    printf("Sending file content to client\n");
    while ((n = fread(buffer, sizeof(char), BUFSIZE, fp)) > 0) {
        if (write(sockfd, buffer, n) < 0) {
            perror("Error writing to client socket");
            break;
        }
    }
    //close file
    fclose(fp);
    printf("File transfer complete: %s\n", filepath);
    // Close the connection to signal the end of the transmission
    shutdown(sockfd, SHUT_WR);
}

//----------------------------Handling Remote File---------------------------------------------
void handle_remote_file(int sockfd, char *filepath, int remote_port, const char *replacement) {
    char buffer[BUFSIZE];
    int n;

    // Replace "smain" with the appropriate server 
    replace_smain_with_stext_or_spdf(filepath, replacement);

    int remote_sockfd = connect_Server_to_Server(remote_port);
    if (remote_sockfd < 0) {
        perror("Error connecting to server");
        //writing Error 
        write(sockfd, "Error: Connection to server failed\n", 35);
        shutdown(sockfd, SHUT_WR);
        return;
    }
    snprintf(buffer, sizeof(buffer), "dfile %s", filepath);
    n = write(remote_sockfd, buffer, strlen(buffer));
    if (n < 0) {
        perror("Error writing to server socket");
        close(remote_sockfd);
        return;
    }

    printf("Retrieving file content\n");

    // Forward the file content from the remote server to the client
    while ((n = read(remote_sockfd, buffer, BUFSIZE)) > 0) {
        if (write(sockfd, buffer, n) < 0) {
            perror("Error writing to client socket");
            break;
        }
    }
    // Closing the remote socket file descriptor
    close(remote_sockfd);
    shutdown(sockfd, SHUT_WR);
}

//-------------------------------------------------Remove file function-------------------------
// Remove function with optimization
void remove_file(int sockfd, char *filepath) {
    char *ext = strrchr(filepath, '.');

    printf("Processing file: %s\n", filepath);  // Debug print
    if (ext) {
        // Trim any whitespace or newline characters from the extension
        ext = trimming_whitespace(ext);
        printf("Detected file extension: '%s'\n", ext);  // Debug print
    } else {
        printf("File extension missing for: %s\n", filepath);
        write(sockfd, "No file extension found\n", 25);
        return;
    }
    if (strcmp(ext, ".c") == 0) {
        // Handle .c files locally
        if (remove(filepath) == 0) {
            printf("Successfully deleted file: %s\n", filepath);
            write(sockfd, "File deleted successfully.\n", 28);
        } else {
            perror("Error deleting file");
            write(sockfd, "File deletion failed.\n", 23);
        }
    } 
    else if (strcmp(ext, ".pdf") == 0) {
        // Process .pdf file deletion
        send_command_to_server(SPDF_PORT, "rmfile ", filepath);
        write(sockfd, "File deletion request processed.\n", 34);
    } 
    else if (strcmp(ext, ".txt") == 0) {
        // Process .txt file deletion
        send_command_to_server(STEXT_PORT, "rmfile ", filepath);
        write(sockfd, "File deletion request processed.\n", 34);
    } 
    else {
        printf("File type not supported: %s\n", filepath);
        write(sockfd, "Unsupported file type\n", 21);
    }

    shutdown(sockfd, SHUT_WR);
}

//---------------------------------------------------------------------------
//Creating a tar function for creating tar file for the extension provided
void create_Tarfile(int sockfd, char *filetype) {
    char tarname[BUFSIZE];
    // Ensure there is no leading whitespace and remove the newline character
    filetype = trimming_whitespace(filetype);
    // Create a proper tar file name based on the file type
    snprintf(tarname, sizeof(tarname), "%sfiles.tar", filetype + 1); // +1 for skipping the '.' in the filetype
    // Ensure tarname does not have any trailing spaces or newlines
    tarname[strcspn(tarname, "\n")] = 0;
    tarname[strcspn(tarname, "\r")] = 0; 
    if (strcmp(filetype, ".c") == 0) 
    {
        char command[BUFSIZE];
        snprintf(command, sizeof(command), "tar -cvf %s $(find ~/smain -type f -name '*%s')", tarname, filetype);
        printf("Creating tar file with command: %s\n", command);
        system(command);
        //Calling dfile_Command_Function for downloading the tar file
        dfile_Command_Function(sockfd, tarname);
        remove(tarname);
    } 
    else if (strcmp(filetype, ".pdf") == 0) 
    {
        // Process tar creation for .pdf file type
        send_command_to_server(SPDF_PORT, "dtar ", filetype);
    } 
    else if (strcmp(filetype, ".txt") == 0) 
    {
        // Process tar creation for .txt file type
        send_command_to_server(STEXT_PORT, "dtar ", filetype);
    } 
    else 
    {
        // Notify if file type is not supported
        write(sockfd, "Provided file type is not supported\n", 21);
    }
}


//-----------------------------------------------------------------------------------------------
//Created a handle_local_tar_file function for handling tar file for .c filetype
void handle_local_tar_file(int sockfd, char *filepath) {
    char buffer[BUFSIZE];
    FILE *fp;
    int n;
    // Opening file for reading the content in read bytes mode
    fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("Error opening tar file");
        write(sockfd, "Error: Tar file not found\n", 26);
        shutdown(sockfd, SHUT_WR);
        return;
    }

    printf("Sending tar file to client\n");

    while ((n = fread(buffer, sizeof(char), BUFSIZE, fp)) > 0) {
        if (write(sockfd, buffer, n) < 0) {
            perror("Error writing to client socket");
            break;
        }
    }

    fclose(fp);
    printf("Tar file transfer complete: %s\n", filepath);

    // Close the connection to signal the end of the transmission
    shutdown(sockfd, SHUT_WR);
}



//---------------------------------------------------------------------------------------------------------------

//Created a display files function for displaying all the file name present in the directory
void display_files(int sockfd, char *directory) {
    char buffer[BUFSIZE];
    int n;

    // Get the list of .c files from the local directory
    DIR *d;
    struct dirent *dir;
    char file_list[BUFSIZE * 10] = ""; 

    // Opening the given directory provided from the client
    d = opendir(directory);
    if (d) {
        // Reading the directory
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG && strstr(dir->d_name, ".c")) {
                strcat(file_list, dir->d_name);
                strcat(file_list, "\n");
            }
        }
        closedir(d);
    } else {
        perror("Error opening directory");
    }
    
    // Request the list of .pdf files
    int spdf_sockfd = connect_Server_to_Server(SPDF_PORT);
    if (spdf_sockfd >= 0) {
        snprintf(buffer, BUFSIZE, "display %s", directory);
        write(spdf_sockfd, buffer, strlen(buffer));
        // Reading the buffer content
        while ((n = read(spdf_sockfd, buffer, BUFSIZE - 1)) > 0) {
            buffer[n] = '\0';  
            // Concatenate the files and store it in file_list
            strcat(file_list, buffer);
        }
        close(spdf_sockfd);
    } else {
        printf("Connection to server failed.\n");
    }

    // Requesting the list of .txt files
    int stext_sockfd = connect_Server_to_Server(STEXT_PORT);
    if (stext_sockfd >= 0) {
        snprintf(buffer, BUFSIZE, "display %s", directory);
        write(stext_sockfd, buffer, strlen(buffer));
        
        while ((n = read(stext_sockfd, buffer, BUFSIZE - 1)) > 0) {
            buffer[n] = '\0'; 
            strcat(file_list, buffer);
        }
        close(stext_sockfd);
    } else {
        printf("Connection to server failed.\n");
    }

    // Sending the combined file_list found from all the servers and writing to the client fd
    n = write(sockfd, file_list, strlen(file_list));
    if (n < 0) perror("Error writing to socket");

    shutdown(sockfd, SHUT_WR);
}



//---------------------------------------------------prcclient function----------------------------------------------------
void prcclient(int newsockfd) 
{
    char buffer[BUFSIZE];
    int n;
    // Running infinite loop for keeping the connection connected with the client
    while (1) {
        bzero(buffer, BUFSIZE); // Clear the buffer
        n = read(newsockfd, buffer, BUFSIZE - 1); // Read the client's command
        if (n < 0) perror("Error reading from socket"); // Error handling

        // Trim leading and trailing whitespace
        char *trimmed_command = trimming_whitespace(buffer);
        
        // Command Handling
        // Handling for ufile --> upload file command
        if (strncmp("ufile", buffer, 5) == 0) {
            char *filename = strtok(buffer + 6, " "); // Extract filename
            char *destination = strtok(NULL, " "); // Extract destination path
            if (filename && destination) {
                ufile_Command_Function(newsockfd, filename, destination);
            } else {
                write(newsockfd, "Invalid ufile command syntax", 28);
            }
        } 
        // Handling for dfile --> download file command
        else if (strncmp("dfile", buffer, 5) == 0) {
            // Download file command
            printf("Executing download command\n");
            char *filepath = strtok(buffer + 6, " "); // Extract filepath
            if (filepath) {
                dfile_Command_Function(newsockfd, filepath);
            } else {
                write(newsockfd, "Invalid dfile command syntax", 29);
            }
        } 
        // Handling for rmfile --> remove file from the directory
        else if (strncmp("rmfile", buffer, 6) == 0) {
            // Remove file command
            char *filepath = strtok(buffer + 7, " "); // Extract filepath
            if (filepath) {
                remove_file(newsockfd, filepath);
            } else {
                write(newsockfd, "Invalid rmfile command syntax", 30);
            }
        } 
        // Handling for dtar --> create tar file of the given extension
        else if (strncmp("dtar", trimmed_command, 4) == 0) {
            // Handle dtar command
            char *filetype = strtok(trimmed_command + 5, " "); // Extract file type
            if (filetype) {
                create_Tarfile(newsockfd, filetype);
            } else {
                write(newsockfd, "Invalid dtar command syntax", 28);
            }
        } 
        // Handling for display --> display all the files present in the pathname
        else if (strncmp("display", buffer, 7) == 0) {
            // Display directory contents command
            char *directory = strtok(buffer + 8, " "); // Extract directory path
            if (directory) {
                display_files(newsockfd, directory);
            } else {
                write(newsockfd, "Invalid display command syntax", 31);
            }
        } else {
            // Invalid command
            n = write(newsockfd, "Invalid command", 15);
            if (n < 0) perror("Error writing to socket"); // Error handling
        }
    }
}


int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int pid;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) perror("Error opening socket");

    // Initialize socket structure
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = PORT;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Bind the host address
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        perror("Error binding socket");

    // Listen for client connection
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (1) {
        // Accepting the connection request from the client
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) perror("Error accepting connection");

        // Forking
        // Create a child process for client
        pid = fork();
        if (pid < 0) perror("Error during fork");
        if (pid == 0) {
            close(sockfd);
            prcclient(newsockfd);
            exit(0);
        } else close(newsockfd);
    }

    close(sockfd);
    return 0;
}