#include "../util/so_stdio.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFSIZE 4096
#define READ 0
#define WRITE 1

/**
 * @brief Structure used to store data about the file
 * 
 */
typedef struct _so_file {
	size_t cursor;

	char bufWrite[BUFSIZE];
	char bufRead[BUFSIZE];

	int fd;
	long cursR, cursW;
	int prevOperation, feof, ferror;
	int pid, mode;
	int bytesRead, bytesWrote;
} SO_FILE;

/**
 * @brief Function to write full data from the buffer
 * Performs as many write calls as it needs to write "count" bytes
 * 
 * @param fd file descriptor
 * @param buf buffer to write from
 * @param count bytes to write
 * @return ssize_t returns bytes written
 */
ssize_t xwrite(int fd, const void *buf, size_t count)
{
	size_t bytes_written = 0;

	while (bytes_written < count) {
		ssize_t bytes_written_now = write(fd, buf + bytes_written,
										  count - bytes_written);

		if (bytes_written_now <= 0) /* I/O error */
			return -1;

		bytes_written += bytes_written_now;
	}

	return bytes_written;
}

/**
 * @brief opens a file at a certain path in the required mode
 * 
 * @param pathname the location of the file to be opened
 * @param mode the mode the file is required to be opened
 * @return SO_FILE* returns a refrence to the SO_FILE structure containing
 * the corresponding file descriptor
 */
SO_FILE *so_fopen(const char *pathname, const char *mode) {
	int retVal = 0;
	SO_FILE *stream = NULL;

	if (strcmp(mode, "r") == 0)
		retVal = open(pathname, O_RDONLY);
	else if (strcmp(mode, "r+") == 0)
		retVal = open(pathname, O_RDWR);
	else if (strcmp(mode, "w") == 0)
		retVal = open(pathname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	else if (strcmp(mode, "w+") == 0)
		retVal = open(pathname, O_CREAT | O_RDWR | O_TRUNC, 0644);
	else if (strcmp(mode, "a") == 0)
		retVal = open(pathname, O_CREAT | O_APPEND | O_WRONLY, 0644);
	else if (strcmp(mode, "a+") == 0)
		retVal = open(pathname, O_CREAT | O_APPEND | O_RDWR, 0644);
	else
		retVal = -1;

	if(retVal == -1)
		return NULL;
	
	// initializing the fields of the file to default values
	stream = calloc(1, sizeof(SO_FILE));
	stream->fd = retVal;
	stream->cursor = 0;
	stream->cursR = 0;
	stream->cursW = 0;
	stream->feof = 0;
	stream->ferror = 0;
	return stream;
}

/**
 * @brief returns the file descriptor for the SO_FILE
 * 
 * @param stream refrence to the SO_FILE
 * @return int 
 */
int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

/**
 * @brief safe closes a SO_FILE
 * if there's anything in the write buffer, it flushes it
 * 
 * @param stream refrence to the SO_FILE
 * @return int 
 */
int so_fclose(SO_FILE *stream)
{
	int retVal, res = 0;

	if (stream && stream->bufWrite[0] != 0) {
		res = so_fflush(stream);
	}

	retVal = close(stream->fd);

	if (retVal != 0 || res == SO_EOF) {
		stream->ferror = 1;
		free(stream);
		return SO_EOF;
	}
	free(stream);
	return 0;
}

/**
 * @brief performs a read call if there's nothing in the read buffer, moves the
 * cursor accordingly, resets the buffer if the cursor reached the end
 * 
 * @param stream SO_FILE
 * @return the character in the read buffer at "cursR" position
 */
int so_fgetc(SO_FILE *stream)
{
	int retVal;

	if (stream->feof == 1)
		return SO_EOF;

	if (stream->cursR == 0) {
		retVal = read(stream->fd, stream->bufRead, BUFSIZE);
		if (retVal <= 0) {
			stream->feof = 1, stream->ferror = 1;
			return SO_EOF;
		}
		stream->bytesRead = retVal;
	} else if (stream->cursR >= stream->bytesRead) {
		retVal = read(stream->fd, stream->bufRead, BUFSIZE);
		if (retVal <= 0) {
			stream->feof = 1, stream->ferror = 1;
			return SO_EOF;
		}
		stream->bytesRead = retVal;
		stream->cursR = 0;
	}

	stream->cursR++;
	stream->cursor++;

	return (int)stream->bufRead[stream->cursR - 1];
}

/**
 * @brief reads from a file to a buffer
 * stops if it reached eof
 * 
 * @param ptr the buffer to place the contents of the file
 * @param size size of the elements to read
 * @param nmemb number of elements to read
 * @param stream SO_FILE
 * @return number of the elements read
 */
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t i = 0;
	int retVal, actualSize = 0;
	char *readTo = (char *)ptr;

	stream->prevOperation = READ;
	while (i < size * nmemb) {
		retVal = so_fgetc(stream);
		if (stream->feof == 1)
			break;
		actualSize++;
		readTo[i] = (char) retVal;
		i++;
	}

	return actualSize / size;
}

/**
 * @brief places an element in the write buffer, flushes the buffer if it's full
 * moves the cursor for write accordingly
 * 
 * @param c the element to place in the buffer
 * @param stream SO_FILE
 * @return the element placed or SO_EOF if failed
 */
int so_fputc(int c, SO_FILE *stream)
{
	int res = 0;

	if (stream->cursW == BUFSIZE) {
		res = xwrite(stream->fd, stream->bufWrite, stream->cursW);
		stream->cursW = 0;
		memset(stream->bufWrite, 0, BUFSIZE);
		if (res <= 0) {
			stream->ferror = 1, stream->feof = 1;
			return SO_EOF;
		}
	}

	stream->bufWrite[stream->cursW] = (char) c;

	stream->cursW++;
	stream->cursor++;
	
	return (int) stream->bufWrite[stream->cursW - 1];
}

/**
 * @brief writes the content from a buffer to a file
 * 
 * @param ptr the buffer it writes from
 * @param size the size of the elements written
 * @param nmemb the number of the elements written
 * @param stream SO_FILE
 * @return number of elements written
 */
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t i = 0;
	char *str = (char *) ptr;

	// stops when reaching eof
	stream->prevOperation = WRITE;
	while (i < nmemb * size) {
		if (stream->feof == 1)
			break;
		so_fputc(str[i], stream);
		i++;
	}

	return i / size;
}

/** 
 * @param stream SO_FILE
 * @return 1 if the cursor reached eof, 0 otherwise
 */
int so_feof(SO_FILE *stream)
{
	return stream->feof == 1 ? 1 : 0;
};

/**
 * @brief returns the current position of the cursor in file
 * 
 * @param stream SO_FILE
 * @return long 
 */
long so_ftell(SO_FILE *stream)
{
	return stream->cursor;
};

/**
 * @brief returns 1 if the file encountered an error, 0 otherwise
 * 
 * @param stream SO_FILE
 * @return int 
 */
int so_ferror(SO_FILE *stream) {
	return stream->ferror;
}

/**
 * @brief places the cursor of the file at a certain position
 * 
 * @param stream SO_FILE
 * @param offset offset-ul
 * @param whence refrence point
 * @return int 
 */
int so_fseek(SO_FILE *stream, long offset, int whence)
{
	// resets the buffers
	if (stream->prevOperation == READ) {
		memset(stream->bufRead, 0, BUFSIZE);
		stream->cursR = 0;
	} else if (stream->prevOperation == WRITE) {
		so_fflush(stream);
		stream->cursW = 0;
	}
	stream->cursor = lseek(stream->fd, offset, whence);
	return 0;
};

/**
 * @brief flushes the write buffer
 * 
 * @param stream SO_FILE
 * @return int 
 */
int so_fflush(SO_FILE *stream)
{
	int res = 0;

	if (stream->cursW != 0) {
		res = xwrite(stream->fd, stream->bufWrite, stream->cursW);
		stream->cursW = 0;
		memset(stream->bufWrite, 0, BUFSIZE);
		if (res <= 0) {
			stream->ferror = 1;
			return SO_EOF;
		}
	}

	return 0;
};

/**
 * @brief 
 * 
 * @param command 
 * @param type 
 * @return SO_FILE* 
 */
SO_FILE *so_popen(const char *command, const char *type)
{
	int res;
	int fds[2];
	SO_FILE *stream;

	stream = calloc(1, sizeof(SO_FILE));

	if (stream == NULL)
		return NULL;

	stream->mode = READ;
	stream->cursor = 0;
	stream->cursR = 0;
	stream->cursW = 0;
	stream->feof = 0;
	stream->ferror = 0;

	stream->mode = strcmp(type, "r") == 0 ? READ : WRITE;

	res = pipe(fds);
	if (res != 0) {
		free(stream);
		return NULL;
	}

	stream->fd = stream->mode == READ ? fds[READ] : fds[WRITE];

	int pid = fork();
	if (pid < 0) {
		close(fds[READ]), close(fds[WRITE]);
		free(stream);
		return NULL;
	} else if (pid == 0) {
		if (stream->mode == READ) {
			close(fds[READ]);
			dup2(fds[WRITE], STDOUT_FILENO);
		} else {
			close(fds[WRITE]);
			dup2(fds[READ], STDIN_FILENO);
		}

		res = execl("/bin/sh", "sh", "-c", command, NULL);
	} else {
		stream->pid = pid;
		close(stream->mode == READ ? fds[WRITE] : fds[READ]);
	}

	return stream;
};

int so_pclose(SO_FILE *stream)
{
	int status;
	int pid = stream->pid;

	so_fflush(stream);
	close(stream->fd);
	free(stream);

	return waitpid(pid, &status, 0) < 0 ? SO_EOF : 0;
}