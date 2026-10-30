# TFTP Client-Server

A UDP-based TFTP client-server implementation. The server listens for initial requests on UDP port `20000`.

## Project Files

- `client.c` — interactive TFTP client
- `server.c` — TFTP server
- `udp.c`, `udp.h` — UDP socket communication
- `tftp.c`, `tftp.h` — TFTP packet formation and parsing
- `file.c`, `file.h` — file operations
- `Makefile` — builds the client and server
- `ClientTestFiles/` — files used to test client uploads and downloads
- `ServerTestFiles/` — files used to test server uploads and downloads

## Build

Run these commands from the project directory:

```sh
make clean
make
```

This should create two executables: `client` and `server`.

To build only one program:

```sh
make client
make server
```

To remove the executables:

```sh
make clean
```

## Run

Open two Terminal windows, both in the project directory.

**Terminal 1 — start the server:**

```sh
./server
```

Leave it running.

**Terminal 2 — start the client:**

```sh
./client
```

At the client prompt, connect to the local server:

```text
connect 127.0.0.1
```

Use `help` inside the client to display the available commands and exact syntax. Common commands are:

```text
get <filename>
put <filename>
mode octet
mode netascii
bye
```

## Test GET and PUT

TFTP filenames are resolved by the running programs' file-handling logic and working directory. Before testing, place each test file in the directory that the server or client expects. The test directories are for organizing test files; a directory prefix such as `ClientTestFiles/` or `ServerTestFiles/` works only if your implementation supports paths in filenames.

### PUT — client to server

1. Place `upload.txt` in the client's accessible file directory.
2. Ensure the server can create the uploaded file in its configured/current destination directory.
3. At the client prompt, run:

   ```text
   put upload.txt
   ```

4. Compare the original and uploaded copies, adjusting the paths to their actual locations:

   ```sh
   cmp ClientTestFiles/upload.txt ServerTestFiles/upload.txt
   ```

### GET — server to client

1. Place `download.txt` in the server's accessible file directory.
2. Ensure the client can write the downloaded file in its configured/current destination directory.
3. At the client prompt, run:

   ```text
   get download.txt
   ```

4. Compare the original and downloaded copies, adjusting the paths to their actual locations:

   ```sh
   cmp ServerTestFiles/download.txt ClientTestFiles/download.txt
   ```

## Boundary-Size Tests

Use files of exactly 511, 512, and 513 bytes to check transfers around the 512-byte TFTP data-block limit.

After transferring each file, compare the source and destination copies. For example:

```sh
cmp ClientTestFiles/test_511.bin ServerTestFiles/test_511.bin
cmp ClientTestFiles/test_512.bin ServerTestFiles/test_512.bin
cmp ClientTestFiles/test_513.bin ServerTestFiles/test_513.bin
```

For downloads, reverse the source and destination paths as appropriate.

## Checking Results

`cmp` produces no output and returns exit status `0` when the two files are identical. If it reports a difference or a missing file, check the paths and repeat the test.

## Notes

- Run the client and server from the directories expected by your file-handling implementation.
- Use `help` in the client to confirm command syntax.
- Keep test files separate from source files.
