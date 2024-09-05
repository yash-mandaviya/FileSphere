# FileSphere: A Distributed File System Project

## Overview

This project implements a distributed file system using socket programming in C. The system is designed to handle multiple clients and includes three servers: Smain, Spdf, and Stext. Clients interact exclusively with the Smain server, which handles file storage and distribution transparently by delegating certain files to the Spdf and Stext servers.

## Components

1. **Smain (Main Server)**: 
   - Stores `.c` files locally.
   - Transfers `.pdf` files to the Spdf server.
   - Transfers `.txt` files to the Stext server.

2. **Spdf (PDF Server)**: 
   - Stores `.pdf` files.

3. **Stext (Text Server)**:
   - Stores `.txt` files.

4. **client24s (Client)**:
   - Provides a command-line interface for interacting with the Smain server.

## File Structure

- **Smain.c**: Contains the implementation for the Smain server.
- **Spdf.c**: Contains the implementation for the Spdf server.
- **Stext.c**: Contains the implementation for the Stext server.
- **client24s.c**: Contains the implementation for the client program.

## Server Details

- **Smain**:
  - Listens for client connections.
  - Manages file storage and redirection of `.pdf` and `.txt` files.
  - Forks a child process for each client connection to handle requests.

- **Spdf**:
  - Handles requests from Smain for storing and retrieving `.pdf` files.

- **Stext**:
  - Handles requests from Smain for storing and retrieving `.txt` files.

## Client Commands

The client supports the following commands:

1. **`ufile filename destination_path`**:
   - Uploads a file to Smain.
   - Smain handles redirection to Spdf or Stext if necessary.

2. **`dfile filename`**:
   - Downloads a file from Smain.
   - Smain retrieves the file from the appropriate server if needed.

3. **`rmfile filename`**:
   - Removes a file from Smain.
   - Smain requests deletion from Spdf or Stext if necessary.

4. **`dtar filetype`**:
   - Creates a tar file of the specified type (`.c`, `.pdf`, or `.txt`) and downloads it.

5. **`display pathname`**:
   - Lists all files of type `.c`, `.pdf`, and `.txt` in the specified directory.

## Installation and Usage

1. **Compile the Code**:
   ```sh
   gcc -o Smain Smain.c
   gcc -o Spdf Spdf.c
   gcc -o Stext Stext.c
   gcc -o client24s client24s.c
   ```

2. **Run the Servers**:
   - Start Smain on one terminal:
     ```sh
     ./Smain
     ```
   - Start Spdf on another terminal:
     ```sh
     ./Spdf
     ```
   - Start Stext on a third terminal:
     ```sh
     ./Stext
     ```

3. **Run the Client**:
   ```sh
   ./client24s
   ```

4. **Example Commands**:
   - Upload a file:
     ```sh
     client24s$ ufile sample.c ~smain/folder1/folder2
     ```
   - Download a file:
     ```sh
     client24s$ dfile ~smain/folder1/folder2/sample.c
     ```
   - Remove a file:
     ```sh
     client24s$ rmfile ~smain/folder1/folder2/sample.c
     ```
   - Create a tar file:
     ```sh
     client24s$ dtar .c
     ```
   - Display files:
     ```sh
     client24s$ display ~smain/folder1
     ```

## Notes

- Ensure that all servers and the client are running on different machines/terminals.
- Paths are case-sensitive, and directory structures must be created as needed.
- Proper error handling is implemented for command validation and network issues.
