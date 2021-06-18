#include <stdio.h>
#include <string.h> //strlen
#include <stdlib.h> //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <ftw.h>
#include <unistd.h>  //write
#include <pthread.h> //for threading , link with lpthread
#include <wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <setjmp.h>

void *connection_handler(void *);

#define _XOPEN_SOURCE 500
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

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

int rmrf(char *path)
{
    return nftw(path, unlink_cb, 64, 8 | 1);
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

char *removeQuotes(char *str)
{
    if (str[0] == '\'')
        str++;
    if (str[strlen(str) - 1] == '\'')
        str[strlen(str) - 1] = '\0';
    return str;
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
        sprintf(databases[dbIndex], "%s", line);
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
        sprintf(databases[dbIndex], "%s", line);
        dbIndex++;
    }

    fclose(fp);
    if (line)
        free(line);
}

int findColIndex(char *header, char *columnName)
{
    char curCol[100][str_size];
    char tmpHead[str_size];
    sprintf(tmpHead, "%s", header);
    int index = 0;
    char *p, *tmp = trimString(strtok_r(tmpHead, ":::", &p));
    sprintf(curCol[index], "%s", tmp);
    index++;

    while (1)
    {
        tmp = trimString(strtok_r(NULL, ":::", &p));
        if (tmp == NULL)
            break;

        sprintf(curCol[index], "%s", tmp);
        index++;
    }

    char *colname, *type;
    while (index--)
    {
        colname = trimString(strtok_r(curCol[index], "|", &p));
        type = trimString(strtok_r(NULL, "|", &p));

        if (!strcmp(colname, columnName))
            return index;
    }

    return -1;
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
            char col[100][str_size], *colname, *type, *tmpsave;

            sprintf(filepath, "%s/%s/%s", cwd, currentDB, tableName);
            memset(header, 0, sizeof(header));

            if (access(filepath, F_OK))
            {
                printf("\t   --  Filepath: %s\n", filepath);
                printf("\t      --  Columns: %s\n", columns);
                mkfile(filepath);

                int colIndex = 0;
                tmpsave = trimString(strtok_r(columns, ",", &p));
                // printf("\t         --  %s\n", tmpsave);
                sprintf(col[colIndex], "%s", tmpsave);
                colIndex++;

                while (tmpsave != NULL)
                {
                    tmpsave = trimString(strtok_r(NULL, ",", &p));

                    if (tmpsave != NULL)
                        sprintf(col[colIndex], "%s", tmpsave);

                    colIndex++;
                }
                colIndex--;

                for (int i = 0; i < colIndex - 1; i++)
                {
                    printf("\t         --  %s\n", col[i]);
                    colname = trimString(strtok_r(col[i], " ", &p));
                    type = trimString(strtok_r(NULL, " ", &p));
                    sprintf(header, "%s%s|%s:::", header, colname, type);
                }
                colname = trimString(strtok_r(col[colIndex - 1], " ", &p));
                type = trimString(strtok_r(NULL, " ", &p));
                sprintf(header, "%s%s|%s", header, colname, type);
                // printf("%s", header);
                appendToFile(filepath, header);
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
                char prevName[str_size], tmpName[str_size], tmpData[str_size];
                tableName = strlwr(getBetween(query, "FROM ", ";"));
                printf("\t   --  Table Name: {%s}\n", tableName);

                sprintf(prevName, "%s/%s/%s", cwd, currentDB, tableName);
                if (access(prevName, F_OK))
                {
                    printf("TABLE DOES NOT EXIST!");
                }
                else
                {
                    sprintf(tmpName, "%s/%s/[[tmp]]", cwd, currentDB);
                    printf("%s\n", prevName);
                    FILE *fp = fopen(prevName, "r");
                    FILE *fp2 = fopen(tmpName, "w");
                    fprintf(fp2, "%s", fgets(tmpData, sizeof(tmpData), fp));
                    fclose(fp);
                    fclose(fp2);
                    remove(prevName);
                    rename(tmpName, prevName);
                }
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
                    char tablePath[str_size], header[str_size];
                    sprintf(tablePath, "%s/%s/%s", cwd, currentDB, tableName);
                    if (access(tablePath, F_OK))
                    {
                        printf("TABLE DOES NOT EXIST!");
                    }
                    else
                    {
                        printf("\t      --  Filepath: {%s}\n", tablePath);

                        FILE *fp = fopen(tablePath, "r");
                        fgets(header, sizeof(header), fp);
                        printf("\t      --  Header: {%s}\n", header);

                        char *condition = getBetween(query, "WHERE ", ";");
                        char *col = getBetween(condition, NULL, "=");
                        char *val = removeQuotes(getBetween(condition, "=", NULL));
                        printf("\t   [%s]\n", subcmd);
                        printf("\t      --  Column: {%s}\n", col);
                        printf("\t      --  Value: {%s}\n", val);

                        int colNum = findColIndex(header, col);
                        char *p;
                        printf("[[%d]]\n", colNum);
                        char curLine[str_size];
                        if (colNum != -1)
                        {
                            char tmpPath[str_size];
                            sprintf(tmpPath, "%s/%s/tmp", cwd, currentDB);
                            printf("\t      --  filepath: {%s}\n", tmpPath);
                            FILE *nfile = fopen(tmpPath, "w");
                            fprintf(nfile, "%s", header);
                            char tmpLine[str_size];
                            while (fgets(curLine, sizeof(curLine), fp) != NULL)
                            {
                                sprintf(tmpLine, "%s", curLine);
                                printf("\t      --  Line: {%s}\n", curLine);
                                char *colVal;
                                colVal = trimString(strtok_r(curLine, ":::", &p));
                                int tmpIdx = colNum;

                                while (tmpIdx--)
                                {
                                    colVal = trimString(strtok_r(NULL, ":::", &p));
                                }

                                if (strcmp(colVal, val))
                                {
                                    fprintf(nfile, "%s", tmpLine);
                                }
                            }
                            fclose(nfile);
                            remove(tablePath);
                            rename(tmpPath, tablePath);
                        }

                        fclose(fp);
                    }
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
            char tmpPath[str_size];
            char *databaseName = strlwr(getBetween(query, "DATABASE ", ";"));
            printf("\t   --  Database Name: {%s}\n", databaseName);

            sprintf(tmpPath, "%s/%s", cwd, databaseName);
            printf("\t   --  Filepath: {%s}\n", tmpPath);
            rmrf(tmpPath);
        }
        //Dropping Table
        else if (!strcmp(subcmd, "TABLE"))
        {
            char filepath[str_size];
            char *tableName = strlwr(getBetween(query, "TABLE ", ";"));
            printf("\t   --  Table Name: {%s}\n", tableName);

            sprintf(filepath, "%s/%s/%s", cwd, currentDB, tableName);
            remove(filepath);
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
                char tablePath[str_size], header[str_size];
                sprintf(tablePath, "%s/%s/%s", cwd, currentDB, tableName);
                if (access(tablePath, F_OK))
                {
                    printf("TABLE DOES NOT EXIST!");
                }
                else
                {
                    printf("\t      --  Filepath: {%s}\n", tablePath);

                    FILE *fp = fopen(tablePath, "r");
                    fgets(header, sizeof(header), fp);
                    printf("\t      --  Header: {%s}\n", header);

                    int colNum = findColIndex(header, columnName);
                    char *p;
                    printf("[[%d]]\n", colNum);
                    char curLine[str_size];
                    if (colNum != -1)
                    {
                        char tmpPath[str_size];
                        sprintf(tmpPath, "%s/%s/tmp", cwd, currentDB);
                        printf("\t      --  filepath: {%s}\n", tmpPath);
                        FILE *nfile = fopen(tmpPath, "w");

                        char tmpHeader[str_size];
                        memset(tmpHeader, 0, sizeof(tmpHeader));
                        char *colVal, colVal2[str_size];

                        int curNum = 0;
                        colVal = trimString(strtok_r(header, ":::", &p));
                        while (1)
                        {
                            printf("\t         --  ColVal: {%s}\n", colVal);
                            sprintf(colVal2, "%s", colVal);
                            colVal = trimString(strtok_r(NULL, ":::", &p));
                            if (curNum != colNum)
                            {
                                if (colVal != NULL)
                                    sprintf(tmpHeader, "%s%s:::", tmpHeader, colVal2);
                                else
                                {
                                    sprintf(tmpHeader, "%s%s", tmpHeader, colVal2);
                                    break;
                                }
                            }
                            curNum++;
                        }
                        fprintf(nfile, "%s\n", tmpHeader);

                        char tmpLine[str_size];
                        while (fgets(curLine, sizeof(curLine), fp) != NULL)
                        {
                            memset(tmpLine, 0, sizeof(tmpLine));
                            printf("\t      --  Line: {%s}\n", curLine);

                            int curNum = 0;
                            colVal = trimString(strtok_r(curLine, ":::", &p));
                            while (1)
                            {
                                printf("\t         --  ColVal: {%s}\n", colVal);
                                sprintf(colVal2, "%s", colVal);
                                colVal = trimString(strtok_r(NULL, ":::", &p));
                                if (curNum != colNum)
                                {
                                    if (colVal != NULL)
                                        sprintf(tmpLine, "%s%s:::", tmpLine, colVal2);
                                    else
                                    {
                                        sprintf(tmpLine, "%s%s", tmpLine, colVal2);
                                        break;
                                    }
                                }
                                curNum++;
                            }

                            fprintf(nfile, "%s\n", tmpLine);
                        }
                        fclose(nfile);
                        remove(tablePath);
                        rename(tmpPath, tablePath);
                    }

                    fclose(fp);
                }
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

            char tablePath[str_size];
            sprintf(tablePath, "%s/%s/%s", cwd, currentDB, tableName);
            if (access(tablePath, F_OK))
            {
                printf("TABLE DOES NOT EXIST!");
            }
            else
            {
                char *values = strlwr(getBetween(query, tmp, ");"));

                char *p, val2[str_size], line[str_size];
                memset(line, 0, sizeof(line));
                char *val = trimString(strtok_r(values, ",", &p));

                while (1)
                {
                    val = removeQuotes(val);
                    printf("\t      --  {%s}\n", val);
                    sprintf(val2, "%s", val);
                    val = trimString(strtok_r(NULL, ",", &p));
                    if (val != NULL)
                        sprintf(line, "%s%s:::", line, val2);
                    else
                    {
                        sprintf(line, "%s%s", line, val2);
                        break;
                    }
                }
                appendToFile(tablePath, line);
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

            char tablePath[str_size], header[str_size];
            sprintf(tablePath, "%s/%s/%s", cwd, currentDB, tableName);
            if (access(tablePath, F_OK))
            {
                printf("TABLE DOES NOT EXIST!");
            }
            else
            {
                printf("\t      --  Filepath: {%s}\n", tablePath);

                FILE *fp = fopen(tablePath, "r");
                fgets(header, sizeof(header), fp);
                char *cur, *p, cur2[str_size];
                char curLine[str_size];
                char toPrint[str_size];
                memset(toPrint, 0, sizeof(toPrint));

                cur = trimString(strtok_r(header, ":::", &p));
                while (1)
                {
                    sprintf(cur2, "%s", cur);
                    cur = trimString(strtok_r(NULL, ":::", &p));

                    if (cur != NULL)
                        sprintf(toPrint, "%s%s\t", toPrint, cur2);
                    else
                    {
                        sprintf(toPrint, "%s%s\n", toPrint, cur2);
                        break;
                    }
                }
                printf("\e[32m%s\e[0m", toPrint);

                // printf("\t      --  Columns: {%s}\n", columns);
                if (!strcmp(columns, "*"))
                {
                    while (fgets(curLine, sizeof(curLine), fp) != NULL)
                    {
                        memset(toPrint, 0, sizeof(toPrint));

                        // printf("\t         --  CurLine: {%s}\n", curLine);
                        cur = trimString(strtok_r(curLine, ":::", &p));
                        while (1)
                        {
                            // printf("\t         --  ColVal: {%s}\n", cur);
                            sprintf(cur2, "%s", cur);
                            // printf("\t         --  ColVal: {%s}\n", cur2);
                            cur = trimString(strtok_r(NULL, ":::", &p));
                            if (cur != NULL)
                                sprintf(toPrint, "%s%s\t\t", toPrint, cur2);
                            else
                            {
                                sprintf(toPrint, "%s%s\n", toPrint, cur2);
                                break;
                            }
                        }
                        printf("\e[32m%s\e[0m", toPrint);
                    }
                }

                // int colNum = findColIndex(header, columnName);
                // char *p;
                // printf("[[%d]]\n", colNum);
                // char curLine[str_size];
                // if (colNum != -1)
                // {
                //     char tmpPath[str_size];
                //     sprintf(tmpPath, "%s/%s/tmp", cwd, currentDB);
                //     printf("\t      --  filepath: {%s}\n", tmpPath);
                //     FILE *nfile = fopen(tmpPath, "w");

                //     char tmpHeader[str_size];
                //     memset(tmpHeader, 0, sizeof(tmpHeader));
                //     char *colVal, colVal2[str_size];

                //     int curNum = 0;
                //     colVal = trimString(strtok_r(header, ":::", &p));
                //     while (1)
                //     {
                //         printf("\t         --  ColVal: {%s}\n", colVal);
                //         sprintf(colVal2, "%s", colVal);
                //         colVal = trimString(strtok_r(NULL, ":::", &p));

                //         if (colVal != NULL)
                //             sprintf(tmpHeader, "%s%s:::", tmpHeader, colVal2);
                //         else
                //         {
                //             sprintf(tmpHeader, "%s%s", tmpHeader, colVal2);
                //             break;
                //         }

                //         curNum++;
                //     }
                //     fprintf(nfile, "%s\n", tmpHeader);

                //     char tmpLine[str_size];
                //     while (fgets(curLine, sizeof(curLine), fp) != NULL)
                //     {
                //         memset(tmpLine, 0, sizeof(tmpLine));
                //         printf("\t      --  Line: {%s}\n", curLine);

                //         int curNum = 0;
                //         colVal = trimString(strtok_r(curLine, ":::", &p));
                //         while (1)
                //         {
                //             printf("\t         --  ColVal: {%s}\n", colVal);
                //             sprintf(colVal2, "%s", colVal);
                //             colVal = trimString(strtok_r(NULL, ":::", &p));
                //             if (curNum == colNum)
                //             {
                //                 sprintf(colVal2, "%s", val);
                //             }
                //             if (colVal != NULL)
                //                 sprintf(tmpLine, "%s%s:::", tmpLine, colVal2);
                //             else
                //             {
                //                 sprintf(tmpLine, "%s%s", tmpLine, colVal2);
                //                 break;
                //             }

                //             curNum++;
                //         }

                //         fprintf(nfile, "%s\n", tmpLine);
                //     }
                //     fclose(nfile);
                //     remove(tablePath);
                //     rename(tmpPath, tablePath);
                // }

                // fclose(fp);
            }
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
            char *toChange = getBetween(query, "SET ", " WHERE");

            //No Where Query
            if (toChange == NULL)
            {
                toChange = strlwr(getBetween(query, "SET ", ";"));
                char *columnName = getBetween(toChange, NULL, "=");
                char *val = removeQuotes(getBetween(toChange, "=", NULL));

                printf("\t   --  Column: {%s}\n", columnName);
                printf("\t   --  Value: {%s}\n", val);

                sprintf(tmp, "%s ", columnName);
                free(subcmd);
                subcmd = strupr(getBetween(query, tmp, " "));

                printf("\t   --  Table Name: {%s}\n", tableName);
                char tablePath[str_size], header[str_size];
                sprintf(tablePath, "%s/%s/%s", cwd, currentDB, tableName);
                if (access(tablePath, F_OK))
                {
                    printf("TABLE DOES NOT EXIST!");
                }
                else
                {
                    printf("\t      --  Filepath: {%s}\n", tablePath);

                    FILE *fp = fopen(tablePath, "r");
                    fgets(header, sizeof(header), fp);
                    printf("\t      --  Header: {%s}\n", header);

                    int colNum = findColIndex(header, columnName);
                    char *p;
                    printf("[[%d]]\n", colNum);
                    char curLine[str_size];
                    if (colNum != -1)
                    {
                        char tmpPath[str_size];
                        sprintf(tmpPath, "%s/%s/tmp", cwd, currentDB);
                        printf("\t      --  filepath: {%s}\n", tmpPath);
                        FILE *nfile = fopen(tmpPath, "w");

                        char tmpHeader[str_size];
                        memset(tmpHeader, 0, sizeof(tmpHeader));
                        char *colVal, colVal2[str_size];

                        int curNum = 0;
                        colVal = trimString(strtok_r(header, ":::", &p));
                        while (1)
                        {
                            printf("\t         --  ColVal: {%s}\n", colVal);
                            sprintf(colVal2, "%s", colVal);
                            colVal = trimString(strtok_r(NULL, ":::", &p));

                            if (colVal != NULL)
                                sprintf(tmpHeader, "%s%s:::", tmpHeader, colVal2);
                            else
                            {
                                sprintf(tmpHeader, "%s%s", tmpHeader, colVal2);
                                break;
                            }

                            curNum++;
                        }
                        fprintf(nfile, "%s\n", tmpHeader);

                        char tmpLine[str_size];
                        while (fgets(curLine, sizeof(curLine), fp) != NULL)
                        {
                            memset(tmpLine, 0, sizeof(tmpLine));
                            printf("\t      --  Line: {%s}\n", curLine);

                            int curNum = 0;
                            colVal = trimString(strtok_r(curLine, ":::", &p));
                            while (1)
                            {
                                printf("\t         --  ColVal: {%s}\n", colVal);
                                sprintf(colVal2, "%s", colVal);
                                colVal = trimString(strtok_r(NULL, ":::", &p));
                                if (curNum == colNum)
                                {
                                    sprintf(colVal2, "%s", val);
                                }
                                if (colVal != NULL)
                                    sprintf(tmpLine, "%s%s:::", tmpLine, colVal2);
                                else
                                {
                                    sprintf(tmpLine, "%s%s", tmpLine, colVal2);
                                    break;
                                }

                                curNum++;
                            }

                            fprintf(nfile, "%s\n", tmpLine);
                        }
                        fclose(nfile);
                        remove(tablePath);
                        rename(tmpPath, tablePath);
                    }

                    fclose(fp);
                }
            }
            //Where Query Exists
            else
            {
                sprintf(tmp, "%s ", toChange);
                printf("TMP %s\n", tmp);
                char *columnName = getBetween(toChange, NULL, "=");
                char *val1 = removeQuotes(getBetween(toChange, "=", NULL));

                printf("\t   --  Column: {%s}\n", columnName);
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

                    sprintf(tmp, "%s ", columnName);
                    free(subcmd);
                    subcmd = strupr(getBetween(query, tmp, " "));

                    printf("\t   --  Table Name: {%s}\n", tableName);
                    char tablePath[str_size], header[str_size];
                    sprintf(tablePath, "%s/%s/%s", cwd, currentDB, tableName);
                    if (access(tablePath, F_OK))
                    {
                        printf("TABLE DOES NOT EXIST!");
                    }
                    else
                    {
                        printf("\t      --  Filepath: {%s}\n", tablePath);

                        FILE *fp = fopen(tablePath, "r");
                        fgets(header, sizeof(header), fp);
                        printf("\t      --  Header: {%s}\n", header);

                        int colNum = findColIndex(header, columnName);
                        char *p;
                        printf("[[%d]]\n", colNum);
                        char curLine[str_size];
                        if (colNum != -1)
                        {
                            char tmpPath[str_size];
                            sprintf(tmpPath, "%s/%s/tmp", cwd, currentDB);
                            printf("\t      --  filepath: {%s}\n", tmpPath);
                            FILE *nfile = fopen(tmpPath, "w");

                            char tmpHeader[str_size];
                            memset(tmpHeader, 0, sizeof(tmpHeader));
                            char *colVal, colVal2[str_size];

                            int curNum = 0;
                            colVal = trimString(strtok_r(header, ":::", &p));
                            while (1)
                            {
                                printf("\t         --  ColVal: {%s}\n", colVal);
                                sprintf(colVal2, "%s", colVal);
                                colVal = trimString(strtok_r(NULL, ":::", &p));

                                if (colVal != NULL)
                                    sprintf(tmpHeader, "%s%s:::", tmpHeader, colVal2);
                                else
                                {
                                    sprintf(tmpHeader, "%s%s", tmpHeader, colVal2);
                                    break;
                                }

                                curNum++;
                            }
                            fprintf(nfile, "%s\n", tmpHeader);

                            char tmpLine[str_size], curLineCopy[str_size];
                            int found;
                            while (fgets(curLine, sizeof(curLine), fp) != NULL)
                            {
                                found = 0;
                                memset(tmpLine, 0, sizeof(tmpLine));
                                sprintf(curLineCopy, "%s", curLine);
                                printf("\t      --  Line: {%s}\n", curLine);

                                int curNum = 0;
                                colVal = trimString(strtok_r(curLineCopy, ":::", &p));
                                while (1)
                                {
                                    printf("\t         --  ColVal: {%s}\n", colVal);
                                    sprintf(colVal2, "%s", colVal);
                                    colVal = trimString(strtok_r(NULL, ":::", &p));
                                    if (!strcmp(colVal2, val2))
                                    {
                                        found = 1;
                                        printf("FOUND!\n");
                                        break;
                                    }
                                    if (colVal == NULL)
                                        break;

                                    curNum++;
                                }

                                curNum = 0;
                                colVal = trimString(strtok_r(curLine, ":::", &p));
                                while (1)
                                {
                                    printf("\t         --  ColVal: {%s}\n", colVal);
                                    sprintf(colVal2, "%s", colVal);
                                    colVal = trimString(strtok_r(NULL, ":::", &p));
                                    if (curNum == colNum && found)
                                    {
                                        sprintf(colVal2, "%s", val1);
                                    }
                                    if (colVal != NULL)
                                        sprintf(tmpLine, "%s%s:::", tmpLine, colVal2);
                                    else
                                    {
                                        sprintf(tmpLine, "%s%s", tmpLine, colVal2);
                                        break;
                                    }

                                    curNum++;
                                }

                                fprintf(nfile, "%s\n", tmpLine);
                            }
                            fclose(nfile);
                            remove(tablePath);
                            rename(tmpPath, tablePath);
                        }

                        fclose(fp);
                    }
                }
            }
        }

        free(subcmd);
    }
}

int main(int argc, char *argv[])
{
    checkFile();
    getcwd(cwd, str_size);
    sprintf(currentDB, "uwukan");
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
            //
        }
        else
        {
            printf("Query Error!\n");
            send(sock, "Query Error!", 1024, 0);
        }
        printf("\n\n");
        write(sock, client_message, strlen(client_message));
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