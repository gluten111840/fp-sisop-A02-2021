#include <stdio.h>
#include <string.h> //strlen
#include <stdlib.h> //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <pthread.h>   //for threading , link with lpthread
#include <wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <setjmp.h>

void *connection_handler(void *);

#define str_size 1024

struct userStruct
{
    char username[128];
    char password[128];
    char databases[100][256];
};

static jmp_buf context;
int dbIndex = 0, userIndex = 0;
char currentDB[str_size], currentUser[str_size];
char queryGlobal[str_size];
char databases[100][256];
struct userStruct userList[100];
char cwd[str_size];

static void sig_handler(int signo)
{
    longjmp(context, 1);
}

static void catch_segv(int catch)
{
    struct sigaction sa;

    if (catch)
    {
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = sig_handler;
        sa.sa_flags = SA_RESTART;
        sigaction(SIGSEGV, &sa, NULL);
    }
    else
    {
        sigemptyset(&sa);
        sigaddset(&sa, SIGSEGV);
        sigprocmask(SIG_UNBLOCK, &sa, NULL);
    }
}

typedef void (*func_t)(void);

static int safe_run(func_t func)
{
    catch_segv(1);

    if (setjmp(context))
    {
        catch_segv(0);
        return 0;
    }

    func();
    catch_segv(0);
    return 1;
}

void mkfile(char *filename)
{
    if (access(filename, F_OK))
    {
        FILE *fp = fopen(filename, "w+");
        fclose(fp);
    }
}

void checkFile()
{
    if (access("databases", F_OK))
    {
        FILE *fp = fopen("databases", "w+");
        fclose(fp);
    }
    if (access("users", F_OK))
    {
        FILE *fp = fopen("users", "w+");
        fclose(fp);
    }
}

char *strupr(char *str)
{
    if (str != NULL)
    {
        char *start = str;
        // printf("strupr:\t%s\n\t", str);
        while (*str)
        {
            if (*str >= 'a' && *str <= 'z')
                *str = *str - 32;
            // printf("%c", *str);
            str++;
        }
        // printf("\n");
        return start;
    }
    else
    {
        return NULL;
    }
}

char *strlwr(char *str)
{
    if (str != NULL)
    {
        char *start = str;
        // printf("strlwr:\t%s\n\t", str);
        while (*str)
        {
            if (*str >= 'A' && *str <= 'Z')
                *str = *str + 32;
            // printf("%c", *str);
            str++;
        }
        // printf("\n");
        return start;
    }
    else
    {
        return NULL;
    }
}

char *trimString(char *str)
{
    if (str != NULL)
    {
        char *end;

        while (isspace((unsigned char)*str))
            str++;

        if (*str == 0)
            return str;

        end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
            end--;

        end[1] = '\0';

        return str;
    }
    else
    {
        return NULL;
    }
}

char *getBetween(char *str, char *leftStr, char *rightStr)
{
    if (leftStr == NULL)
    {
        char tmp[str_size] = {0};
        sprintf(tmp, "LEFTBORDER%s", str);

        return getBetween(tmp, "LEFTBORDER", rightStr);
    }
    else if (rightStr == NULL)
    {
        char tmp[str_size] = {0};
        sprintf(tmp, "%sRIGHTBORDER", str);
        return getBetween(tmp, leftStr, "RIGHTBORDER");
    }

    char *res = NULL, *start, *end;

    start = strstr(str, leftStr);
    if (start)
    {
        start += strlen(leftStr);
        end = strstr(start, rightStr);
        if (end)
        {
            res = (char *)malloc(end - start + 1);
            memcpy(res, start, end - start);
            res[end - start] = '\0';
        }
    }
    // printf(" => {%s}\n", res);
    if (res == NULL)
    {
        return NULL;
    }
    else
    {
        return res;
    }
}

int checkDatabase(char *databaseName)
{
    int i = dbIndex;
    while (i--)
    {
        // printf("{%s} index %d with {%s}\n", databases[i], i, databaseName);
        if (!strcmp(databases[i], databaseName))
        {
            return 1;
        };
    }
    return 0;
}

void appendToFile(char *filename, char *string)
{
    FILE *fp = fopen(filename, "a+");
    printf("Append {%s} to {%s}\n", string, filename);
    fprintf(fp, "%s\n", string);
    fclose(fp);
}

void readDatabaseList()
{
    FILE *fp = fopen("databases", "r");
    ssize_t read;
    size_t len = 0;
    char *line = NULL;

    while ((read = getline(&line, &len, fp)) != -1)
    {
        line[read - 1] = '\0';
        strcpy(databases[dbIndex], line);
        dbIndex++;
    }

    fclose(fp);
    if (line)
        free(line);
}

int insertUser(char *username)
{
    int i = dbIndex;
    while (i--)
    {
        // printf("{%s} index %d with {%s}\n", databases[i], i, username);
        if (!strcmp(userList[i].username, username))
        {
            return 1;
        };
    }
    return 0;
}

void readUsernameList()
{
    FILE *fp = fopen("users", "r");
    ssize_t read;
    size_t len = 0;
    char *line = NULL;

    while ((read = getline(&line, &len, fp)) != -1)
    {
        line[read - 1] = '\0';
        strcpy(databases[dbIndex], line);
        dbIndex++;
    }

    fclose(fp);
    if (line)
        free(line);
}

char *removeQuotes(char *str)
{
    if (str[0] == '\'')
        str++;
    if (str[strlen(str) - 1] == '\'')
        str[strlen(str) - 1] = '\0';
    return str;
}

void handleCommand()
{
    char *query = trimString(queryGlobal);
    char *cmd = strupr(getBetween(query, NULL, " "));
    printf("CMD: [%s]\n", cmd);

    /*

    //// CREATE COMMAND ////
    //// CREATE COMMAND ////

    */
    if (!strcmp(cmd, "CREATE"))
    {
        char *subcmd = strupr(getBetween(query, "CREATE ", " "));
        printf("\t[%s]\n", subcmd);

        /*
        //// CREATE USER ////
        */
        if (!strcmp(subcmd, "USER"))
        {
            char *username, *password;
            char tmp[str_size];

            username = getBetween(query, "USER ", " ");
            printf("\t   --  Username: {%s}\n", username);
            sprintf(tmp, "%s ", username);

            free(subcmd);
            subcmd = strupr(getBetween(query, tmp, " "));
            printf("\t   --  [%s]\n", subcmd);

            if (!strcmp(subcmd, "IDENTIFIED"))
            {
                free(subcmd);
                subcmd = strupr(getBetween(query, "IDENTIFIED ", " "));
                printf("\t   --  [%s]\n", subcmd);

                if (!strcmp(subcmd, "BY"))
                {
                    char tmpstr[1024];
                    password = getBetween(query, "BY ", ";");
                    printf("\t   --  Password: {%s}\n", password);

                    sprintf(tmpstr, "%s:::%s:::\n", username, password);
                    appendToFile("users", tmpstr);
                }
                else
                {
                    printf("[BY] NOT FOUND\n");
                }
            }
            else
            {
                printf("[IDENTIFIED] NOT FOUND\n");
            }
        }
        /*
        //// CREATE DATABASE ////
        */
        else if (!strcmp(subcmd, "DATABASE"))
        {
            char *databaseName = strlwr(getBetween(query, "DATABASE ", ";"));
            char filepath[str_size];
            sprintf(filepath, "%s/%s", cwd, databaseName);
            printf("\t   --  Table Name: {%s}\n", databaseName);
            printf("\t   --  Filepath : {%s}\n", filepath);

            if (!checkDatabase(databaseName))
            {
                printf("\t   Making Database: %s\n", databaseName);
                mkdir(filepath, 777);
                sprintf(filepath, "%s/[[table]]", filepath);
                mkfile(filepath);
                appendToFile("databases", databaseName);
            }
            else
            {
                printf("\t   DATABASE EXISTS!", databaseName);
            }
        }
        /*
        //// CREATE TABLE ////
        */
        else if (!strcmp(subcmd, "TABLE"))
        {
            char tmp[str_size], filepath[str_size], header[str_size];
            char *tableName = strlwr(getBetween(query, "TABLE ", " "));
            printf("\t   --  Table Name: {%s}\n", tableName);

            sprintf(tmp, "%s (", tableName);
            char *columns = strlwr(getBetween(query, tmp, ");"));

            char *p = NULL;
            char col[100], *colname, *type, *tmpsave;

            sprintf(filepath, "%s/%s/%s", cwd, currentDB, tableName);
            memset(header, 0, sizeof(header));
            if (access(filepath, F_OK))
            {
                printf("\t   --  Filepath: %s\n");
                printf("\t      --  Columns: \n");
                mkfile(filepath);

                int colIndex = 0;
                while (1)
                {
                    tmpsave = trimString(strtok_r(NULL, ",", &p));
                    if (tmpsave != NULL)
                    {
                        printf("\t         --  %s\n", tmpsave);
                        strcpy(col[colIndex], tmpsave);
                    }
                    else
                        break;

                    colIndex++;
                }
                // colname = trimString(strtok_r(col, " ", &p));
                // type = trimString(strtok_r(NULL, " ", &p));
                // sprintf(header, "%s|%s", col, type);
                // appendToFile(filepath, header);
            }
            else
            {
                printf("\t      TABLE EXISTS!\n");
            }
        }

        free(subcmd);
        return 0;
    }
    /*

    //// DELETE COMMAND ////
    //// DELETE COMMAND ////

    */
    else if (!strcmp(cmd, "DELETE"))
    {
        char tmp[str_size];
        char *subcmd = strupr(getBetween(query, "DELETE ", " "));
        printf("\t[%s]\n", subcmd);

        if (!strcmp(subcmd, "FROM"))
        {
            char *tableName = strlwr(getBetween(query, "FROM ", " "));
            //No Where Query
            if (tableName == NULL)
            {
                tableName = strlwr(getBetween(query, "FROM ", ";"));
                printf("\t   --  Table Name: {%s}\n", tableName);
            }
            //Where Query Exists
            else
            {
                printf("\t   --  Table Name: {%s}\n", tableName);

                sprintf(tmp, "%s ", tableName);
                free(subcmd);
                subcmd = strupr(getBetween(query, tmp, " "));

                if (!strcmp(subcmd, "WHERE"))
                {
                    char *condition = getBetween(query, "WHERE ", ";");
                    char *col = getBetween(condition, NULL, "=");
                    char *val = removeQuotes(getBetween(condition, "=", NULL));
                    printf("\t   [%s]\n", subcmd);
                    printf("\t      --  Column: {%s}\n", col);
                    printf("\t      --  Value: {%s}\n", val);
                }
            }
        }

        free(subcmd);
    }
    /*

    //// DROP COMMAND ////
    //// DROP COMMAND ////

    */
    else if (!strcmp(cmd, "DROP"))
    {
        char tmp[str_size];
        char *subcmd = strupr(getBetween(query, "DROP ", " "));
        printf("\t[%s]\n", subcmd);

        //Dropping Database
        if (!strcmp(subcmd, "DATABASE"))
        {
            char *databaseName = strlwr(getBetween(query, "DATABASE ", ";"));
            printf("\t   --  Database Name: {%s}\n", databaseName);
        }
        //Dropping Table
        else if (!strcmp(subcmd, "TABLE"))
        {
            char *tableName = strlwr(getBetween(query, "TABLE ", ";"));
            printf("\t   --  Table Name: {%s}\n", tableName);
        }
        //Dropping Column on a Table
        else if (!strcmp(subcmd, "COLUMN"))
        {
            char *columnName = strlwr(getBetween(query, "COLUMN ", " "));
            printf("\t   --  Column Name: {%s}\n", columnName);

            sprintf(tmp, "%s ", columnName);
            free(subcmd);
            subcmd = strupr(getBetween(query, tmp, " "));

            if (!strcmp(subcmd, "FROM"))
            {
                char *tableName = strlwr(getBetween(query, "FROM ", ";"));

                printf("\t   --  Table Name: {%s}\n", tableName);
            }
        }

        free(subcmd);
    }
    /*

    //// INSERT COMMAND ////
    //// INSERT COMMAND ////

    */
    else if (!strcmp(cmd, "INSERT"))
    {
        char tmp[str_size];
        char *subcmd = strupr(getBetween(query, "INSERT ", " "));
        printf("\t[%s]\n", subcmd);

        if (!strcmp(subcmd, "INTO"))
        {
            char *tableName = strlwr(getBetween(query, "INTO ", " "));
            sprintf(tmp, "%s (", tableName);
            printf("\t   --  Table Name: {%s}\n", tableName);

            char *values = strlwr(getBetween(query, tmp, ");"));

            char *p;
            char *val = trimString(strtok_r(values, ",", &p));
            while (val != NULL)
            {
                val = removeQuotes(val);
                printf("\t      --  {%s}\n", val);
                val = trimString(strtok_r(NULL, ",", &p));
            }
        }

        free(subcmd);
    }
    /*

    //// SELECT COMMAND ////
    //// SELECT COMMAND ////

    */
    else if (!strcmp(cmd, "SELECT"))
    {
        char tmp[str_size];

        char *columns = strlwr(getBetween(query, "SELECT ", " FROM"));
        printf("\t--  Column Names: {%s}\n", columns);

        char *tableName = strlwr(getBetween(query, "FROM ", " "));
        printf("\t[FROM]\n");

        //No Where Query
        if (tableName == NULL)
        {
            tableName = strlwr(getBetween(query, "FROM ", ";"));
            printf("\t   --  Table Name: {%s}\n", tableName);
        }
        //Where Query Exists
        else
        {
            printf("\t   --  Table Name: {%s}\n", tableName);

            sprintf(tmp, "%s ", tableName);
            char *subcmd = strupr(getBetween(query, tmp, " "));

            if (!strcmp(subcmd, "WHERE"))
            {
                char *condition = getBetween(query, "WHERE ", ";");
                char *col = getBetween(condition, NULL, "=");
                char *val = removeQuotes(getBetween(condition, "=", NULL));

                printf("\t   [%s]\n", subcmd);
                printf("\t      --  Column: {%s}\n", col);
                printf("\t      --  Value: {%s}\n", val);
            }

            free(subcmd);
        }
    }
    /*

    //// UPDATE COMMAND ////
    //// UPDATE COMMAND ////

    */
    else if (!strcmp(cmd, "UPDATE"))
    {
        char tmp[str_size];
        char *tableName = strlwr(getBetween(query, "UPDATE ", " "));
        printf("\t--  Table Name: {%s}\n", tableName);

        sprintf(tmp, "%s ", tableName);
        char *subcmd = strupr(getBetween(query, tmp, " "));
        printf("\t[%s]\n", subcmd);

        if (!strcmp(subcmd, "SET"))
        {
            char *toChange = getBetween(query, "SET ", " ");

            //No Where Query
            if (toChange == NULL)
            {
                toChange = strlwr(getBetween(query, "SET ", ";"));
                char *col = getBetween(toChange, NULL, "=");
                char *val = removeQuotes(getBetween(toChange, "=", NULL));

                printf("\t   --  Column: {%s}\n", col);
                printf("\t   --  Value: {%s}\n", val);
            }
            //Where Query Exists
            else
            {
                sprintf(tmp, "%s ", toChange);
                char *col1 = getBetween(toChange, NULL, "=");
                char *val1 = removeQuotes(getBetween(toChange, "=", NULL));

                printf("\t   --  Column: {%s}\n", col1);
                printf("\t   --  Value: {%s}\n", val1);

                free(subcmd);
                subcmd = strupr(getBetween(query, tmp, " "));
                if (!strcmp(subcmd, "WHERE"))
                {
                    char *condition = getBetween(query, "WHERE ", ";");
                    char *col2 = getBetween(condition, NULL, "=");
                    char *val2 = removeQuotes(getBetween(condition, "=", NULL));

                    printf("\t   [%s]\n", subcmd);
                    printf("\t      --  Column: {%s}\n", col2);
                    printf("\t      --  Value: {%s}\n", val2);
                }
            }
        }

        free(subcmd);
    }
}

int main(int argc, char *argv[])
{
    // DAEMON
    /* Our process ID and Session ID */
    // pid_t pid, sid;

    // /* Fork off the parent process */
    // pid = fork();
    // if (pid < 0) {
    //     exit(EXIT_FAILURE);
    // }
    // /* If we got a good PID, then
    //    we can exit the parent process. */
    // if (pid > 0) {
    //     exit(EXIT_SUCCESS);
    // }

    // /* Open any logs here */

    // /* Create a new SID for the child process */
    // sid = setsid();
    // if (sid < 0) {
    //     /* Log the failure */
    //     exit(EXIT_FAILURE);
    // }

    // /* Change the current working directory */
    // chdir("home/bayuekap/Documents/modul4/fp/database/");

    // /* Change the file mode mask */
    // umask(0);

    // /* Close out the standard file descriptors */
    // close(STDIN_FILENO);
    // close(STDOUT_FILENO);
    // close(STDERR_FILENO);

    // /* Daemon-specific initialization goes here */

    // /* The Big Loop */
    // while (1) {
        checkFile();
        getcwd(cwd, str_size);
        strcpy(currentDB, "uwukan");
        int socket_desc, client_sock, c, *new_sock;
        struct sockaddr_in server, client;

        //Create socket
        socket_desc = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_desc == -1)
        {
            printf("Could not create socket");
        }
        puts("Socket created");

        //Prepare the sockaddr_in structure
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(7000);

        //Bind
        if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            //print the error message
            perror("Bind failed. Error");
            return 1;
        }
        puts("Bind done");

        //Listen
        listen(socket_desc, 3);

        //Accept and incoming connection
        puts("Waiting for incoming connections...");
        c = sizeof(struct sockaddr_in);
        while ((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c)))
        {
            puts("Connection accepted");

            pthread_t sniffer_thread;
            new_sock = malloc(1);
            *new_sock = client_sock;

            if (pthread_create(&sniffer_thread, NULL, connection_handler, (void *)new_sock) < 0)
            {
                perror("Could not create thread");
                return 1;
            }

            //Now join the thread , so that we dont terminate before the thread
            pthread_join(sniffer_thread, NULL);
            puts("Handler assigned");
        }

        if (client_sock < 0)
        {
            perror("accept failed");
            return 1;
        }
    //     sleep(30); /* wait 30 seconds */
    //     break;
    // }
    // exit(EXIT_SUCCESS);

    return 0;
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
    //Get the socket descriptor
    int sock = *(int *)socket_desc;
    int read_size;
    char *message, client_message[1024];

    bzero(client_message, 1024 * sizeof(client_message[0]));
    //Receive a message from client
    while ((read_size = recv(sock, client_message, 1024, 0)) > 0)
    {
        printf("%s\n", client_message);
        sprintf(queryGlobal, "%s", client_message);
        // handleCommand();
        if (safe_run(handleCommand))
        {
            printf("Query Success!\n");
            send(sock, "Query Success!", 1024, 0);
        }
        else
        {
            printf("Query Error!\n");
            send(sock, "Query Error!", 1024, 0);
        }
        printf("\n\n");
        // write(sock, client_message, strlen(client_message));
        bzero(client_message, 1024 * sizeof(client_message[0]));
        bzero(message, 1024 * sizeof(message[0]));
    }

    if (read_size == 0)
    {
        puts("Client Disconnected\n");
    }
    else if (read_size == -1)
    {
        perror("Recv Failed\n");
    }

    //Free the socket pointer
    close(sock);
    free(socket_desc);

    return 0;
}