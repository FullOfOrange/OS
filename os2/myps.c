#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>

#define CLK_TCK sysconf(_SC_CLK_TCK)
#define _PATH_PROC "/proc"
#define PROCESS_MAX 65535
#define BUFFER_SIZE 1024

typedef struct proc {
    int pid;
    int ppid;
    int ttyNum;
    int cpu;

    unsigned long utime;
    unsigned long stime;
    unsigned long cutime;
    unsigned long cstime;

    unsigned long long starttime;

    char *time;
    char *tty;
    char *comm;
    char *command;
    char *user;
    char *shortUser;
    char *startTime;
} Proc;

Proc *procList[45781];
int procListNum = 0;

FILE * pathOpen(const char *mode, const char *path) {
    FILE *fd = NULL;
    fd = fopen(path, mode);
    if(fd == NULL) {
        err(EXIT_FAILURE, "cannot open %s", path);
    }
    return fd;
}

void pathGetStr(const char *mode, const char *path, char *value) {
    FILE *fd = NULL;
    fd = pathOpen(mode, path);
    if (!fgets(value, BUFFER_SIZE, fd))
		err(EXIT_FAILURE, ("failed to read: %s"), path);

    // fclose(fd);
    fd = NULL;
}

int pathGetNum(const char *mode, const char *path) {
    FILE *fd = pathOpen(mode, path);
    char buf[BUFFER_SIZE];

    if (!fgets(buf, sizeof(buf), fd))
		err(EXIT_FAILURE, ("failed to read: %s"), path);

    return atoi(buf);
}

int getTTYMajor(int ttyNumber) {
    return ttyNumber >> 8;
}

int getTTYMinor(int ttyNumber) {
    return ttyNumber & 0xff;
}

char* getTTYName(int ttyNumber) {
    char *buf = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    // 장치 번호에는 major, minor 번호가 존재함.
    int major = getTTYMajor(ttyNumber), 
        minor = getTTYMinor(ttyNumber), 
        device;

    if (major == 0) {
        return "?";
    }
    if (major == 4) {
        sprintf(buf, "tty%d", minor);
        return buf;
    }
    if (major == 136) {
        sprintf(buf, "pts/%d", minor);
        return buf;
    }
    return "error";
}

char* getTime(long utime, long stime, long cstime, long cutime) {
    unsigned long timeSum = (utime + stime) / CLK_TCK;
    int day, hour, min, sec;
    char *time = (char *)malloc(sizeof(char) * 50);
    min = timeSum / 60;
    sec = timeSum % 60;
    hour = min / 60;
    min = min % 60;
    day = hour / 24;
    hour = hour % 24;
    if(day > 0 ){
        sprintf(time, "[%.2d-]%.2d:%.2d:%.2d", day, hour, min,
                sec);
    } else {
        sprintf(time, "%.2d:%.2d:%.2d", hour, min, sec);
    }
    return time;
}

char* getCom(int pid) {
    char *command = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    memset(command, 0, BUFFER_SIZE);
    char pathName[BUFFER_SIZE];

    sprintf(pathName, "/proc/%d/comm", pid);
   
    pathGetStr("r", pathName, command);
    command[strlen(command) - 1] = '\0';
    return command;
}

char * getCommand(int pid) {
    char *command = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    char *returnCommand = (char *)malloc(sizeof(char) * BUFFER_SIZE);

    char pathName[BUFFER_SIZE];

    sprintf(pathName, "/proc/%d/cmdline", pid);
   
    FILE *fd = NULL;
    fd = fopen(pathName, "r");
    if (!fgets(command, BUFFER_SIZE, fd)) {
        command = getCom(pid);
        sprintf(returnCommand, "[%s]", command);
        free(command);
        return returnCommand;
    }
    fclose(fd);
    free(returnCommand);
    pathGetStr("r", pathName, command);
    return command;
}

char * getUID(int pid) {
    struct stat statbuf;
    char path[BUFFER_SIZE];
    char *user = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    sprintf(path, "/proc/%d/stat", pid);

    stat(path, &statbuf);

    struct passwd *upasswd = getpwuid(statbuf.st_uid);
    strcpy(user, upasswd->pw_name);
    return user;
}

char * getShortUID(char * user) {
    char * shortUser = (char*)malloc(sizeof(char) * BUFFER_SIZE);

    strcpy(shortUser, user);
    if(strlen(user) > 8) {
        shortUser[7] = '+';
        shortUser[8] = '\0';
    }

    return shortUser;
}

double getUptime(void) {
    char buf[BUFFER_SIZE];
    double stime, idletime;
    pathGetStr("r", "/proc/uptime", buf);
    sscanf(buf, "%lf %lf", &stime, &idletime);
    return stime;
}

char * getStime(unsigned long long starttime){
    char * buf = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    unsigned long start = time(NULL) - getUptime() + (starttime/CLK_TCK);
	struct tm *tmStart = localtime(&start);

	if((time(NULL) - start) < 24 * 60 * 60){
		strftime(buf, BUFFER_SIZE, "%H:%M", tmStart);
	} else if((time(NULL) - start) < 7 * 24 * 60 * 60){
		strftime(buf, BUFFER_SIZE, "%b%d", tmStart);
	} else {
		strftime(buf, BUFFER_SIZE, "%y", tmStart);
	}
    return buf;
}

int getCpuUsage(unsigned long utime, unsigned long stime, unsigned long long starttime) {
    unsigned long long totalTime = utime + stime;
    int cpu = 0;
    cpu = ((totalTime / CLK_TCK) / (long double)(getUptime() - (starttime / CLK_TCK))) * 100;
    return cpu;
}

void getProcRow(Proc* procRow) {
    char procStatPath[BUFFER_SIZE];
    char field[BUFFER_SIZE];
    char buf[BUFFER_SIZE];

    sprintf(procStatPath, "/proc/%d/stat", procRow->pid);
    pathGetStr("r", procStatPath, buf);
    int idx = 1;
    int index = 0;

    for(int i = 0, j = 0; i < strlen(buf); i++) {
        if(buf[i] != ' ' && i != (strlen(buf) - 1)) {
            field[j++] = buf[i];
        } else {
            field[j] = '\0';
            if(index == 3)
                procRow->ppid = atoi(field);
            
            if(index == 6) 
                procRow->ttyNum = atoi(field);

            if(index == 13)
                procRow->utime = atol(field);

            if(index == 14)
                procRow->stime = atol(field);

            if(index == 15)
                procRow->cutime = atol(field);

            if(index == 16)
                procRow->cstime = atol(field);

            if(index == 21)
                procRow->starttime = atoll(field);

            index++;
            j = 0;            
        }
    }
    procRow->user = getUID(procRow->pid);
    procRow->shortUser = getShortUID(procRow->user);
    procRow->time = getTime(procRow->utime, procRow->stime, procRow->cstime, procRow->cutime);
    procRow->tty = getTTYName(procRow->ttyNum);
    procRow->comm = getCom(procRow->pid);
    procRow->command = getCommand(procRow->pid);
    procRow->startTime = getStime(procRow->starttime);
    procRow->cpu = getCpuUsage(procRow->utime, procRow->stime, procRow->starttime);
}

void getProcList() {
    DIR *dp = NULL;    
    struct dirent *d = NULL;

    if((dp = opendir(_PATH_PROC)) == NULL) {
        err(EXIT_FAILURE, "opendir error");
    }

    while((d = readdir(dp)) != NULL) {
        int isProcess = 1;
        for(int i = 0; i < strlen(d->d_name); i++) {
            if(!isdigit(d->d_name[i])) {
                isProcess = 0;
                break;
            }
        }
        if(isProcess == 0) continue;
        
        // Proc 파일 파싱 하는 부분
        Proc *procRow = malloc(sizeof(Proc));
        procRow->pid = atoi(d->d_name);
        
        getProcRow(procRow);

        procList[procListNum++] = procRow;
    }
}

void optionParse(char *option, int *e, int *f) {
    for(int i = 1; i < strlen(option); i++) {
        if(option[i] == 'e') {
            *e = 1;
        }

        if(option[i] == 'f') {
            *f = 1;
        }
    }
}

void printCommand(int len, char * command) {
    for(int i = 0; i < len; i++) {
        printf("%c", command[i]);
    }
    printf("\n");
}

void printRow(Proc * procRow, int f, int w) {
    int len, width;
    if(f == 0){
        len = printf("%6d %-5.5s %11s ", procRow->pid, procRow->tty,procRow->time);

        width = w-len-2;
        if(w < len) width = w - (w % len) - 2; 
        printCommand(width, procRow->comm);
    } else {
        len = printf("%-8s %6d %6d %2d %5s %-5.5s %11s ",procRow->shortUser, procRow->pid, procRow->ppid, procRow->cpu, procRow->startTime, procRow->tty, procRow->time);
        
        width = w-len-2;
        if(w < len) width = w - (w % len) - 2; 
        printCommand(width, procRow->command);
    }
}

void printMenu(int f) {
    if(f == 0) {
        printf("%6s %-5s %11s %3s\n", "PID", "TTY", "TIME", "CMD");
    } else {
        printf("%-8s %6s %6s %2s %5s %-5s %11s %3s\n", 
        "UID", "PID", "PPID", "C", "STIME", "TTY", "TIME", "CMD");
    }
}

int main(int argc, char **argv) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // 윈도우 사이즈에 따라 출력길이 조절

    getProcList();

    int e = 0, f = 0;
    if(argc >= 2) {
        optionParse(argv[1], &e, &f);
    }

    printMenu(f);

    FILE *selfFd;
    if(!(selfFd = open("/proc/self/fd/0", O_RDONLY))) {
        err(EXIT_FAILURE, "open err");
    }

    char selfTTY[BUFFER_SIZE], path[BUFFER_SIZE];
    char *tty;
    sprintf(selfTTY, "%s", ttyname(selfFd));

    for(int i = 0; i < procListNum; i++) {
        if(e == 0) {
            sprintf(path, "/proc/%d/fd/0", procList[i]->pid);
            FILE *fd = open(path, O_RDONLY);

            tty = ttyname(fd);
            if(tty && strcmp(tty, selfTTY) == 0) {
                printRow(procList[i], f, w.ws_col);
            } 
            close(fd);
        } else {
            printRow(procList[i], f, w.ws_col);
        }
    }  
}