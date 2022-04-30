Ghitan Bogdan-Elvis-Dumitru

*******************************************************************************
Opinion:

During the process of the implementation, even if mine is not nearly as safe
of efficient as the one implemented in stdio, I found it quite useful and
interesting to see how buffering works.

I'd like to mention that the homework was not fully documented, some aspects
missing. Being forced to check the forum or checking the flow of the tests to
see what certain tests require could've being avoided if the homework was more
thoroughly documented.

Also the order the tests were compiled was different from the order the tests
were checked, adding even more confusion.

*******************************************************************************
Implementation:

I started by building the structure SO_FILE and adding fields to it while
implementing other functions.

Firstly, fopen, fclose and fileno were implemented, because all tests require
these functions.

For fopen we opened the file in the mode required and following the conditions
mentioned.

For fclose we flush the write buffer and close the file.
For fileno just return the file descriptor.

Then I implemented the read operations: fgetc and fread.

Fgetc fills the buffer within the BUFSIZE and returns the current character. If
the cursor reaches the end, it resets the buffer.

Fread uses fgetc and gets every character until it reaches the required amount.

Then I implemented the write operations: fputc and fwrite.

Fputc puts a character in the buffer and flushes the content when it is full.
Fwrite writes the content from a buffer to a file using fputc.

For fseek I reset the buffer coresponding to the last operation, and then used
lseek.

For feof and ferror, I added an extra variable that activates when the conditions
are met.

For popen I checked the mode the file is open, started a pipe, used a fork,
depending on the mode (READ or WRITE) we close the other end and then execute
the command.

For pclose we close the file and wait for the child process to finish.
