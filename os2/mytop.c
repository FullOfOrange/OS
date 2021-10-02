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
#include <ncurses.h>
#include <utmp.h>
#include <math.h>

#define CLK_TCK sysconf(_SC_CLK_TCK)
#define _PATH_PROC "/proc"
#define PROCESS_MAX 65535
#define BUFFER_SIZE 1024
#define CPU_TICK_SIZE 8

typedef struct defaultInfo {
    char *nowTime;
    char *upTime;
    int activeUser;
    char *loadAvg;

    int run;
    int sleep;
    int stop;
    int zombie;

    char *cpuPercent;
    char *memKib;
    char *swapKib;

    unsigned long memTotal;
    unsigned long memFree;
    unsigned long buffer;
    unsigned long cached;
    unsigned long sreclaim;
    unsigned long swapFree;
    unsigned long swapTotal;
    unsigned long memAva;
} DefaultInfo;

typedef struct proc {
    int pid;
    int ni;
    int pr;
#include <utmp.h>
    char state;

    double cpu;
    double mem;

    unsigned long utime;
    unsigned long stime;
    unsigned long starttime;

    unsigned long time;
    char *showTime;

    char *comm;
    char *user;
    char *shortUser;

    char *res;
    long resNum;

    char *virt;
    int virtNum;

    char *shr;
    int shrNum;
} Proc;

Proc *procList[PROCESS_MAX];
int procListNum = 0;
long double prevCpuTick[CPU_TICK_SIZE] = {0.0, };
int prevUptime = 0;

int pathOpen(const char *mode, const char *path, FILE **fd) {
    *fd = fopen(path, mode);
    if(*fd == NULL) 
        return 0;

    return 1;
}

int pathGetStr(const char *mode, const char *path, char *value) {
    FILE *fd;
    if(pathOpen(mode, path, &fd) == 0) {
        return 0;
    }

    if (!fgets(value, BUFFER_SIZE, fd)){
        fclose(fd);
        return 0;
    }
    fclose(fd);

    return 1;
}

int pathGetNum(const char *mode, const char *path) {
    FILE *fd;
    pathOpen(mode, path, &fd);
    char buf[BUFFER_SIZE];

    if (!fgets(buf, sizeof(buf), fd))
		err(EXIT_FAILURE, ("failed to read: %s"), path);

    return atoi(buf);
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

char* getCom(int pid) {
    char *command = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    memset(command, 0, BUFFER_SIZE);
    char pathName[BUFFER_SIZE];

    sprintf(pathName, "/proc/%d/comm", pid);
   
    pathGetStr("r", pathName, command);
    command[strlen(command) - 1] = '\0';
    return command;
}

double getUptime(void) {
    char buf[BUFFER_SIZE];
    double stime, idletime;
    pathGetStr("r", "/proc/uptime", buf);
    sscanf(buf, "%lf %lf", &stime, &idletime);
    return stime;
}

char* getUptimeStr(long bootTime) {
    int day, hour, min;
    char *time = (char *)malloc(sizeof(char) * 50);
    min = bootTime / 60;
    hour = min / 60;
    min = min % 60;
    day = hour / 24;
    hour = hour % 24;
    sprintf(time, "%d days, %.2d:%.2d", day, hour, min);
    return time;
}

char* getTime(long utime, long stime) {
    unsigned long timeSum = (utime + stime) / (CLK_TCK / 100);
    int day, hour, min, sec, millisec;
    char *time = (char *)malloc(sizeof(char) * 50);
    min = timeSum / 6000;
    sec = timeSum % 60;
    millisec = sec % 100;
    sprintf(time, "%3d:%.2d.%.2d", min, sec, millisec);
    return time;
}

int parseProcStatus(char *buf, char *pattern, char **value) {
    char *p, *v;
    int len = strlen(pattern);
    
    // buf 에는 개행문자까지 읽힌 데이터가 들어온다.
    if (strncmp(buf, pattern, len))
        return 0;
    
    // 공백 또는 엔터를 판단하여 다음 문자열까지 넘김
    for (p = buf + len; isspace(*p); p++);

    if(*p != ':') return 0;

    // 다음 데이터를 확인
    for(++p; isspace(*p); p++);
    // 만약 데이터 없으면 종료
    if(!*p) return 0;
    // 찾은 데이터를 기록함
    v = p;
    // 버퍼의 길이를 가져옴
    len = strlen(buf) - 1;
    //
    for(p = buf + len; isspace(*(p-1)); p--);

    *p = '\0';

    *value = strdup(v);
    return 1;
}

long getMemtotal(void){
    char buf[BUFFER_SIZE], temp[BUFFER_SIZE];
    long a;
    pathGetStr("r", "/proc/meminfo", buf);
    sscanf(buf, "%s %ld", temp, &a);
    return a;
}  

int getProcRow(Proc* procRow) {
    char procStatPath[BUFFER_SIZE];
    char procStatusPath[BUFFER_SIZE];
    char field[BUFFER_SIZE];
    char buf[BUFFER_SIZE];

    sprintf(procStatPath, "/proc/%d/stat", procRow->pid);
    if(pathGetStr("r", procStatPath, buf) == 0) 
        return 0; 

    sprintf(procStatusPath, "/proc/%d/status", procRow->pid);
    FILE *fp;
    if(pathOpen("r", procStatusPath, &fp) == 0)
        return 0;

    int idx = 1;
    int index = 0;

    for(int i = 0, j = 0; i < strlen(buf); i++) {
        if (buf[i] != ' ' && i != (strlen(buf) - 1)) {
            field[j++] = buf[i];
        } else {
            field[j] = '\0';

            if(index == 2)
                procRow->state = field[0];

            if(index == 13)
                procRow->utime = atol(field);

            if(index == 14)
                procRow->stime = atol(field);

            if(index == 17)
                procRow->pr = atoi(field);

            if(index == 18)
                procRow->ni = atoi(field);

            if(index == 21)
                procRow->starttime = atoll(field);

            index++;
            j = 0;            
        }
    }

    procRow->user = getUID(procRow->pid);
    procRow->shortUser = getShortUID(procRow->user);
    procRow->comm = getCom(procRow->pid);
    procRow->cpu = (((procRow->utime+procRow->stime)/CLK_TCK) / (getUptime() - (procRow->starttime/CLK_TCK)) * 100);

    procRow->virt = (char*)malloc(sizeof(char) * BUFFER_SIZE);
    procRow->res = (char*)malloc(sizeof(char) * BUFFER_SIZE);
    procRow->shr = (char*)malloc(sizeof(char) * BUFFER_SIZE);

    while(fgets(buf, BUFFER_SIZE, fp) != NULL) {
        if(parseProcStatus(buf, "VmSize", &procRow->virt));
        else if(parseProcStatus(buf, "VmHWM", &procRow->res)); 
        else if(parseProcStatus(buf, "RssFile", &procRow->shr));
    }
    
    if(strlen(procRow->virt) < 2) 
        strcpy(procRow->virt, "0");
    procRow->virtNum = atoi(procRow->virt);
    
    if(strlen(procRow->res) < 2) 
        strcpy(procRow->res, "0");
    procRow->resNum = atol(procRow->res);
    procRow->mem = ((double)procRow->resNum / (double)getMemtotal()) * 100;

    if(strlen(procRow->shr) < 2) 
        strcpy(procRow->shr, "0");
    procRow->shrNum = atoi(procRow->shr);

    procRow->showTime = getTime(procRow->utime, procRow->stime);

    fclose(fp);
    return 1;
}

void freeProcList() {
    for (int i = 0; i < procListNum; i++){
        free(procList[i]->comm);
        free(procList[i]->user);
        free(procList[i]->shortUser);
        free(procList[i]->showTime);
        free(procList[i]->res);
        free(procList[i]->virt);
        free(procList[i]->shr);
        free(procList[i]);
    }
    procListNum = 0;
}

void getProcList(DefaultInfo *di) {
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

        Proc *procRow = malloc(sizeof(Proc));
        procRow->pid = atoi(d->d_name);
        if(getProcRow(procRow) == 0) {
            free(procRow);
        } else {
            procList[procListNum++] = procRow;
            if(procRow->state == 'S') 
                di->sleep++;
            else if(procRow->state == 'R') di->run++;
            else if(procRow->state == 'Z') di->zombie++;
            else if(procRow->state == 'T' || procRow->state == 't') di->stop++;
        }
    }
}

void freeDefaultInfo(DefaultInfo *di) {
    free(di->nowTime);
    free(di->upTime);
    free(di->loadAvg);
    free(di->cpuPercent);
    free(di->memKib);
    free(di->swapKib);
    di->run = 0;
    di->sleep = 0;
    di->stop = 0;
    di->zombie = 0;
}

void getDefaultInfo(DefaultInfo *di) {
    char buf[BUFFER_SIZE], temp[BUFFER_SIZE];

    // get now time
    di->nowTime = (char*)malloc(sizeof(char) * BUFFER_SIZE);
    time_t now = time(NULL);
	struct tm *tmNow = localtime(&now);
	sprintf(di->nowTime, "%02d:%02d:%02d ", tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec);

    // get uptime
    di->upTime = getUptimeStr(getUptime());

    // active users
    struct utmp *ut;
    setutent();
    di->activeUser = 0;
    while((ut = getutent()) != NULL)  // /var/usr/utmp에서 utent 읽어들이기
        if(ut->ut_type == USER_PROCESS) // /ut_type이 USER일 경우에만 count
            di->activeUser++;
    
    // loadavg
    pathGetStr("r", "/proc/loadavg", buf);
    double a, b, c;
    sscanf(buf, "%lf %lf %lf", &a, &b, &c);
    di->loadAvg = (char*)malloc(sizeof(char) * BUFFER_SIZE);
    sprintf(di->loadAvg, "%4.2f, %4.2f, %4.2f", a, b, c);

    pathGetStr("r", "/proc/stat", buf);
    long double cpuTick[CPU_TICK_SIZE] = {0.0,};
    long double result[CPU_TICK_SIZE] = {0.0,};

	sscanf(buf, "%s %Lf %Lf %Lf %Lf %Lf %Lf %Lf %Lf", 
    temp, &cpuTick[0], &cpuTick[1], &cpuTick[2], &cpuTick[3], &cpuTick[4], &cpuTick[5], &cpuTick[6], &cpuTick[7]);	//ticks read

    // cpu usage info
    double uptime = getUptime();
    unsigned long cpuTickCount = 0;
	if(prevUptime == 0){
		 cpuTickCount = uptime * CLK_TCK;				//부팅 후 현재까지 일어난 context switching 횟수
		 for(int i = 0; i < CPU_TICK_SIZE; i++) {		//읽은 ticks 그대로 출력
			result[i] = (cpuTick[i]  / cpuTickCount) * 100 / 4;
            if(isnan(result[i]) || isinf(result[i]))	//예외 처리
			    result[i] = 0;
         }
	} else {
		cpuTickCount = (uptime - prevUptime) * CLK_TCK;	//부팅 후 현재까지 일어난 context switching 횟수
		for(int i = 0; i < CPU_TICK_SIZE; i++) {
			result[i] = ((cpuTick[i] - prevCpuTick[i]) / cpuTickCount) * 100 / 4;
            if(isnan(result[i]) || isinf(result[i]))	//예외 처리
			    result[i] = 0;
        }
	}

    di->cpuPercent = (char*)malloc(sizeof(char) * BUFFER_SIZE);
    sprintf(di->cpuPercent, "%%Cpu(s) : %4.1Lf us, %4.1Lf sy, %4.1Lf ni, %4.1Lf id, %4.1Lf wa, %4.1Lf hi, %4.1Lf si, %4.1Lf st",
    result[0],result[2],result[1],result[3],result[4],result[5],result[6],result[7]);

    prevUptime = uptime;
    for(int i = 0; i < CPU_TICK_SIZE; i++)
        prevCpuTick[i] = cpuTick[i];
    
    // memory info
    FILE * meminfoFp;
    pathOpen("r", "/proc/meminfo", &meminfoFp);

    char key[BUFFER_SIZE], value[BUFFER_SIZE];
    while(fgets(buf, BUFFER_SIZE, meminfoFp)) {
        sscanf(buf, "%s %s", key, value);
        if(strcmp(key, "MemTotal:") == 0)
            di->memTotal = atol(value);
        else if(strcmp(key, "MemFree:") == 0)
            di->memFree = atol(value);
        else if(strcmp(key, "Buffers:") == 0)
            di->buffer = atol(value);
        else if(strcmp(key, "Cached:") == 0)
            di->cached = atol(value);
        else if(strcmp(key, "SReclaimable:") == 0)
            di->sreclaim = atol(value);
        else if(strcmp(key, "SwapTotal:") == 0)
            di->swapTotal = atol(value);
        else if(strcmp(key, "SwapFree:") == 0)
            di->swapFree = atol(value);
        else if(strcmp(key, "MemAvailable:") == 0)
            di->memAva = atol(value);
    }

    di->memKib = (char*)malloc(sizeof(char) * BUFFER_SIZE);
    di->swapKib = (char*)malloc(sizeof(char) * BUFFER_SIZE);

    sprintf(di->memKib, "Kib Mem : %8ld total, %8ld free, %8ld used, %8ld buff/cache", 
    di->memTotal, 
    di->memFree, 
    di->memTotal - di->memFree - di->buffer - di->cached - di-> sreclaim,
    di->buffer + di->cached + di->sreclaim);

    sprintf(di->swapKib, "Kib Swap: %8ld total, %8ld free, %8ld used. %8ld avail Mem", 
    di->swapTotal,
    di->swapFree, 
    di->swapTotal - di->swapFree,
    di->memAva);
}

void printLine(char * buf, int width, int enter) {
    for(int i = 0; i < width; i++) {
        if(buf[i] == '\0') break;
        printf("%c", buf[i]);
    }
    if(enter == 1)
        printf("\n");
}

int compare(const void *a, const void *b) {
    Proc **pa = (Proc**)a;
    Proc **pb = (Proc**)b;

    double p = (*pb)->cpu - (*pa)->cpu;
    if(p == 0) 
        return (*pa)->pid - (*pb)->pid;
    return p;
}

void printEntire(DefaultInfo *di,int width, int height, int after) {
    char buf[BUFFER_SIZE],
        tasks[BUFFER_SIZE], 
        first[BUFFER_SIZE],
        menu[BUFFER_SIZE];

    sprintf(first, "top - %sup %s, %2d user,  load average: %s",
    di->nowTime, di->upTime, di->activeUser, di->loadAvg);
    sprintf(tasks, "Tasks: %3d total, %3d running, %3d sleeping, %3d stopped, %3d zombie",
    procListNum, di->run, di->sleep, di->stop, di->zombie);
    sprintf(menu, "%6s %-8s %4s %3s %8s %7s %6s %1s %4s %4s %9s %s",
    "PID", "USER", "PR", "NI", "VIRT", "RES", "SHR", "S", "%CPU", "%MEM", "TIME+", "COMMAND");

    qsort(procList, procListNum, sizeof(Proc *), compare);

    printLine(first, width, 1);
    printLine(tasks, width, 1);
    printLine(di->cpuPercent, width, 1);
    printLine(di->memKib, width,1);
    printLine(di->swapKib, width,1);
    printf("\n");
    printLine(menu, width,1);
    for(int i = after; i < height - 8; i++) {
        sprintf(buf, "%6d %-8s %4d %3d %8d %7ld %6d %c %4.1f %4.1f %9s %s", 
        procList[i]->pid, procList[i]->shortUser, procList[i]->pr, procList[i]->ni,
        procList[i]->virtNum, procList[i]->resNum, procList[i]->shrNum, procList[i]->state,
        procList[i]->cpu, procList[i]->mem, procList[i]->showTime, procList[i]->comm);
        if(i == height - 7)
            printLine(buf, width, 0);
        else
            printLine(buf, width, 1);
    }
}

int main(int argc, char **argv) {
    initscr();
    endwin();

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    struct winsize w;
    DefaultInfo *di = (DefaultInfo *)malloc(sizeof(DefaultInfo));
    time_t before = time(NULL), now;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // 윈도우
    getProcList(di);    
    getDefaultInfo(di);
    printEntire(di, w.ws_col, w.ws_row, 0);

    do {
        // printf("input %d", ch);
        now = time(NULL);
        if(now - before >= 3) {
            erase();

            setvbuf(stdout, NULL, _IOLBF, 0);
            setvbuf(stderr, NULL, _IONBF, 0);

            freeProcList();
            freeDefaultInfo(di);

            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // 윈도우
            getProcList(di);    
            getDefaultInfo(di);
            printEntire(di, w.ws_col, w.ws_row, 0);
            before = now;

            refresh();
            endwin();
        }
    } while(true);

    endwin();
}