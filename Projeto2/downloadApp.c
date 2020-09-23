#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "getip.h"
#include "clientTCP.h"

#define DEFAULT_SERVER_FTP_PORT 21
#define BUFFER_MAX_SIZE 255
#define STR_MAX_LEN 63
#define TRUE 1
#define FALSE 0
#define EXTENSION_MAX_SIZE 32
#define FILE_NAME_MAX_SIZE 255

//error codes
#define REPLY_ERROR -1
#define INVALID_ARG_N 1
#define INVALID_ARG 2
#define HOST_IP_NOT_FOUND 3
#define SRV_NOT_READY 4
#define LOGIN_ERROR   5
#define FILE_RETR_ERROR 6

#define max(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _a : _b;      \
    })

#define min(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })

enum ReplyType
{
    WAIT_FOR_REPLY = 1,
    //1yz Positive Preliminary reply:
    //The requested action is being initiated; expect another reply before proceeding with a new command.

    ACTION_COMPLETE = 2,
    //2yz Positive Completion reply:
    //The requested action has been successfully completed. A new request may be initiated.

    AWAITING_INFO_CMD = 3,
    //3yz Positive Intermediate reply:
    //The command has been accepted, but the requested action is being held in abeyance, pending receipt of further information.
    //The user should send another command specifying this information. This reply is used in command sequence groups.

    RETRY_CMD_SEQUENCE = 4,
    //4yz Transient Negative Completion reply:
    //The command was not accepted and the requested action did not take place, but the error condition is temporary and
    //the action may be requested again. The user should return to the beginning of the command sequence, if any.

    BAD_CMD = 5,
    //5yz Permanent Negative Completion reply:
    //The command was not accepted and the requested action did not take place. The User-process is discouraged from
    //repeating the exact request (in the same sequence).

};

void establishFTPConnection(int socketfd, char *user, char *pass);
int createReceivedFile(char *file_name);
void createFile(int socketDownload, char *filename);
void sendRETRCmd(int socketfd, int socketfdDownload, char *path);
int getNumberBeforeChar(char *string, char c);
void handlePASVReply(int socketfd, char *ip, int *port);
void readReply(int socketfd, char *replyCode, char *info, int useInfo);
void readReplyNoInfo(int socketfd, char *replyCode);
void readReplyWithInfo(int socketfd, char *replyCode, char *info);
int parseArguments(char *url, char *user, char *pass, char *host, char *path);
int getLastCharPos(char *str, char c);
void getFileNameFromPath(char *path, char *filename);
void sendFTPCmd(int sockfd, char *cmd_code, char *cmd_argument);
enum ReplyType sendCommandProcessReply(int sockfd, char *cmd_code, char *cmd_argument, int canWaitForExtraReplies, int canRetryCommand);
void replyFailureTest(enum ReplyType reply_type, int socketfd);

int main(int argc, char **argv)
{

    if (argc != 2)
    {
        printf("Usage:\tdownload url\n\tex: download ftp://anonymous:1@speedtest.tele2.net/1KB.zip\n");
        exit(INVALID_ARG_N);
    }

    //parse argument

    char user[STR_MAX_LEN];
    memset(user, 0, STR_MAX_LEN);

    char pass[STR_MAX_LEN];
    memset(pass, 0, STR_MAX_LEN);

    char host[STR_MAX_LEN];
    memset(host, 0, STR_MAX_LEN);

    char path[STR_MAX_LEN];
    memset(path, 0, STR_MAX_LEN);

    if (parseArguments(argv[1], user, pass, host, path))
    {
        exit(INVALID_ARG);
    }

    char host_ip[STR_MAX_LEN];
    memset(host_ip, 0, STR_MAX_LEN);

    if (getIPByName(host, host_ip))
        exit(HOST_IP_NOT_FOUND);

    printf(" - Username:  %s\n", user);
    printf(" - Password:  %s\n", pass);
    printf(" - Host:      %s\n", host);
    printf(" - HOST IP:   %s\n", host_ip);
    printf(" - Path:      %s\n", path);

    //abrir socket user - server

    int socketfd = initSocket(DEFAULT_SERVER_FTP_PORT, host_ip);

    //receive connection greetings
    char greetingsCode[4];
    memset(greetingsCode, 0, 4);

    readReplyNoInfo(socketfd, greetingsCode);

    if (strcmp(greetingsCode, "220"))
    {
        close(socketfd);
        if (greetingsCode[0] == '1')
            printf("server expecting delay, try later");
        else
            printf("server not ready");
        exit(4);
    }

    printf("Greetings code - %s\n", greetingsCode);

    printf("Establishing connection\n");
    // establish connection
    
    establishFTPConnection(socketfd, user, pass);

    //entrada em modo passivo - pasv
    printf("Server entering passive mode\n");
    // reply_type = sendCommandProcessReply(socketfd, "pasv", "", FALSE, TRUE);
    // replyFailureTest(reply_type, socketfd);
    sendFTPCmd(socketfd, "pasv", "");

    int download_port = 0;
    char download_ip[STR_MAX_LEN];
    memset(download_ip, 0, STR_MAX_LEN);

    handlePASVReply(socketfd, download_ip, &download_port);

    printf(" - Download Port: %d\n", download_port);
    printf(" - Download IP:   %s\n", download_ip);
    

    // abrir socket server user

    int socketfdDownload = initSocket(download_port, download_ip);

    // retrieve file - retr
    sendRETRCmd(socketfd, socketfdDownload, path);


    close(socketfdDownload);

    
    //close connection

    enum ReplyType reply_type = sendCommandProcessReply(socketfd, "quit", "", FALSE, TRUE);
    replyFailureTest(reply_type, socketfd);

    close(socketfd);
    printf("Connection ended successfuly\n");

    return 0;
}

void establishFTPConnection(int socketfd, char *user, char *pass){


    enum ReplyType reply_type;

    do
    {

        // username - user
        printf("Sending username\n");
        reply_type = sendCommandProcessReply(socketfd, "user", user, FALSE, TRUE);
        if (reply_type == AWAITING_INFO_CMD)
        {

            // password - pass

            printf("Sending password\n");
            reply_type = sendCommandProcessReply(socketfd, "pass", pass, FALSE, FALSE);
            if (reply_type != RETRY_CMD_SEQUENCE)
                replyFailureTest(reply_type, socketfd);
        }
        else
        {
            replyFailureTest(reply_type, socketfd);
        }
    } while (reply_type != ACTION_COMPLETE);
}

int createReceivedFile(char *file_name){

  int file;
  int i = 1;
  char extension[EXTENSION_MAX_SIZE];
  char file_name_no_extension[FILE_NAME_MAX_SIZE]; 

  int last_dot_pos = getLastCharPos(file_name, '.');//position of last '.'
  if (last_dot_pos > 0)//if it has an extension
  {
    strcpy(extension, file_name + last_dot_pos); //get extension
    memcpy(file_name_no_extension, file_name, last_dot_pos); //get file name without extension
    file_name_no_extension[last_dot_pos] = '\0';
  }
  else strcpy(file_name_no_extension, file_name);

  while ((file = open(file_name, O_WRONLY | O_CREAT | O_EXCL | O_APPEND, 0777)) < 0)
  {
    if (errno == EEXIST)
    {
      if (last_dot_pos > 0) //if it has extension
        sprintf(file_name, "%s(%d)%s", file_name_no_extension, i, extension);
      else
        sprintf(file_name, "%s(%d)", file_name_no_extension, i);
      if (strlen(file_name) < FILE_NAME_MAX_SIZE){
        //if the size does not exceed the maximum file
        i++;
      }
      else {
        perror("file name too big\n");
        exit(-1);
      }
    }
    else
    {
      perror("error on creating file\n");
      exit(-1);
    }
  }
  return file;
}

void createFile(int socketDownload, char *filename){

    int fd = createReceivedFile(filename);

    char buffer[BUFFER_MAX_SIZE];

    int bytes;

    while ( (bytes = read(socketDownload, buffer, BUFFER_MAX_SIZE) ) > 0){
        write(fd, buffer, bytes);
    }

    printf("download has ended successfuly\n");
    close(fd);
}

void sendRETRCmd(int socketfd, int socketfdDownload, char *path){

    enum ReplyType reply_type = sendCommandProcessReply(socketfd, "retr", path, FALSE, TRUE);
    if (reply_type == WAIT_FOR_REPLY)
    {
        char filename[STR_MAX_LEN];
        memset(filename, 0, STR_MAX_LEN);

        getFileNameFromPath(path, filename);

        printf(" - File Name: %s\n", filename);

        createFile(socketfdDownload, filename);

        char replyCode[4];
        memset(replyCode, 0, 4);

        readReplyNoInfo(socketfd, replyCode);
        printf("%s - Download Reply Code\n", replyCode);
    }
    else
    {
        close(socketfdDownload);
        close(socketfd);
        printf("error on retrieving file\n");
        exit(FILE_RETR_ERROR);
    }
}

int getNumberBeforeChar(char *string, char c){

    unsigned str_len = strlen(string);
    int number = 0;

    for (unsigned i = 0; i < str_len; i++){
        char curr_char = string[i];
        if (curr_char == c)
            return number;
        else if (isdigit(curr_char)){
            number = number * 10 + (curr_char - '0');
        }
        else break;
    }
    return -1;
}

void handlePASVReply(int socketfd, char *ip, int *port){

    char buffer[BUFFER_MAX_SIZE];
    memset(buffer, 0, BUFFER_MAX_SIZE);

    char replyCode[4];
    memset(replyCode, 0, 4);

    readReplyWithInfo(socketfd, replyCode, buffer);
    printf("Code - %s PASV reply - %s\n", replyCode, buffer);

    enum ReplyType reply_type = replyCode[0] - '0';
    replyFailureTest(reply_type, socketfd);

    unsigned i = 0;
    while (buffer[i++] != '(');

    unsigned numbers_processed_counter = 0;
    unsigned index = 0;
    while (numbers_processed_counter < 4)
    {
        char curr_char = buffer[i++];
        if (curr_char == ',')
        {
            numbers_processed_counter++;
            if (numbers_processed_counter < 4)
            {
                ip[index] = '.';
            }
        }
        else
            ip[index] = curr_char;
        index++;
    }

    int first_byte = getNumberBeforeChar(buffer + i, ',');
    // printf("%d\n", first_byte);
    while (buffer[i++] != ',');
    int second_byte = getNumberBeforeChar(buffer + i, ')');
    // printf("%d\n", second_byte);
    *port = first_byte * 256 + second_byte;
    // printf("%d\n", *port);
    
}

void replyFailureTest(enum ReplyType reply_type, int socketfd){
    if (reply_type != ACTION_COMPLETE){
        printf("Login error - Reply Code = %d\n", reply_type);
        close(socketfd);
        exit(LOGIN_ERROR);
    }
}

void sendFTPCmd(int sockfd, char *cmd_code, char *cmd_argument)
{

    write(sockfd, cmd_code, strlen(cmd_code));
    unsigned int arg_length = strlen(cmd_argument);
    if (arg_length > 0)
    {
        write(sockfd, " ", 1);
        write(sockfd, cmd_argument, arg_length);
    }
    write(sockfd, "\n", 1);
}

enum ReplyType sendCommandProcessReply(int sockfd, char *cmd_code, char *cmd_argument, int canWaitForExtraReplies, int canRetryCommand)
{

    sendFTPCmd(sockfd, cmd_code, cmd_argument);

    char reply[4];
    memset(reply, 0, 4);
    enum ReplyType reply_type;

    int stop = FALSE;

    while (!stop)
    {
        readReplyNoInfo(sockfd, reply);
        reply_type = reply[0] - '0';
        switch (reply_type)
        {
        case WAIT_FOR_REPLY:
            if (!canWaitForExtraReplies)
                stop = TRUE;
            break;

        case ACTION_COMPLETE:
        case AWAITING_INFO_CMD:
        case BAD_CMD:
            stop = TRUE;
            break;

        case RETRY_CMD_SEQUENCE:
            if (canRetryCommand)
                sendFTPCmd(sockfd, cmd_code, cmd_argument);
            else
                stop = TRUE;
            break;
        }
    }

    return reply_type;
}


void readReply(int socketfd, char *replyCode, char *info, int useInfo)
{

    enum State
    {
        CODE,
        MIDDLE,
        LASTLINE,
        END
    };

    enum State state = CODE;
    char curr_char;
    int i = 0;
    int info_index = 0;

    while (state != END)
    {
        read(socketfd, &curr_char, 1);
        switch (state)
        {
        case CODE:
            if (curr_char == ' ')
            {
                if (i != 3)
                {
                    printf("Error on receiving reply code\n");
                    exit(REPLY_ERROR);
                }
                state = LASTLINE;
            }
            else if (curr_char == '-')
            {
                if (i != 3)
                {
                    printf("Error on receiving reply code\n");
                    exit(REPLY_ERROR);
                }
                state = MIDDLE;
                i = 0;
            }
            else
            {
                if (isdigit(curr_char))
                {
                    replyCode[i] = curr_char;
                    i++;
                }
                else
                {
                    i = 0;
                }
            }
            break;

        case MIDDLE:
            if (i == 3 && curr_char == ' ')
                state = LASTLINE;
            else if (replyCode[i] == curr_char)
                i++;
            else
            {
                if (useInfo)
                {
                    info[info_index++] = curr_char;
                }
                i = 0;
            }
            break;

        case LASTLINE:
            if (curr_char == '\n')
                state = END;
            else if (useInfo)
            {
                info[info_index++] = curr_char;
            }
            break;

        default:
            break;
        }
    }
}

void readReplyWithInfo(int socketfd, char *replyCode, char *info){
    readReply(socketfd, replyCode, info, TRUE);
}


void readReplyNoInfo(int socketfd, char *replyCode)
{
    readReply(socketfd, replyCode, NULL, FALSE);
}

int parseArguments(char *url, char *user, char *pass, char *host, char *path)
{

    enum State
    {
        PREFIX,
        USERNAME,
        PASSWORD,
        HOST,
        PATH
    };

    enum State state = PREFIX;

    char prefix[] = "ftp://";
    unsigned int prefix_length = strlen(prefix);
    unsigned int url_length = strlen(url);
    unsigned int i = 0;
    unsigned int curr_field_index = 0;

    while (i < url_length)
    {

        char curr_char = url[i];

        switch (state)
        {
        case PREFIX:
            if (i == prefix_length - 1)
            {
                state = USERNAME;
            }
            if (prefix[i] != curr_char)
                return -1;
            break;
        case USERNAME:
            if (curr_char == '@')
            {
                //EMPTY USERNAME, NO PASSWORD
                state = HOST;
                curr_field_index = 0;
            }
            else if (curr_char == ':')
            {
                state = PASSWORD;
                curr_field_index = 0;
            }
            else
            {
                user[curr_field_index] = url[i];
                curr_field_index++;
            }
            break;
        case PASSWORD:
            if (curr_char == '@')
            {
                state = HOST;
                curr_field_index = 0;
            }
            else
            {
                pass[curr_field_index] = url[i];
                curr_field_index++;
            }
            break;
        case HOST:
            if (curr_char == '/')
            {
                state = PATH;
                curr_field_index = 0;
            }
            else
            {
                host[curr_field_index] = url[i];
                curr_field_index++;
            }
            break;
        case PATH:
            path[curr_field_index] = url[i];
            curr_field_index++;
            break;

        default:
            break;
        }

        i++;
    }

    return 0;
}

int getLastCharPos(char *str, char c)
{

    unsigned int str_length = strlen(str);

    for (int i = str_length - 1; i >= 0; i--)
    {
        if (str[i] == c)
            return i;
    }

    return -1;
}

void getFileNameFromPath(char *path, char *filename)
{

    int pos = getLastCharPos(path, '/');
    unsigned int path_length = strlen(path);

    unsigned int filename_length = path_length - (pos + 1);

    memcpy(filename, path + max(pos + 1, 0), filename_length);
}
