#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <unistd.h>

#define _PATH_SYS_SYSTEM	"/sys/devices/system"
#define _PATH_PROC_CPUINFO	"/proc/cpuinfo"

enum {
	MODE_32BIT	= (1 << 1),
	MODE_64BIT	= (1 << 2)
};

struct cache {
    char *name;
};

struct info
{
    char *arch;
    int cpuOpMode;
    char *byteOrder;
    int  cpuNum;
    char *onlineCpuList;
    int  threadPerCore;
    int  corePerSocket;
    int  socketNum;
    int  NumaNode;
    int cacheLen;
    char *vendor;
    char *cpuFamily;
    char *model;
    char *modelName;
    char *mhz;
    char *bogoMips;
    char *hyperVender;
    char *virtualType;
    char *flag;
    char *numaNode0CPU;
    char *stepping;
};

FILE * pathOpen(const char *mode, const char *path) {
    FILE *fd;
    fd = fopen(path, mode);
    if(!fd) {
        err(EXIT_FAILURE, ("cannot open %s", path));
    }
    return fd;
}

// 파일 라인을 받아서 특정 패턴의 데이터를 value 에 넣어줌
int parseCpuInfo(char *buf, char *pattern, char **value) {
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

int initMode(void) {
	int m = 0;
    // 64 bit 만 지원하는 아키텍쳐만 모아둠
    #if defined(__alpha__) || defined(__ia64__)
        m |= MODE_64BIT;
    #endif
    // 32 bit 를 지원하는 아키텍쳐를 일단 여기에 모아둠
    #if defined(__i386__) || defined(__x86_64__) || \
        defined(__s390x__) || defined(__s390__)
        m |= MODE_32BIT;
    #endif
	return m;
}

void readAndPrintCacheInfo() {
    char cache[BUFSIZ], cachePath[BUFSIZ];
    char typeName[64], levelName[64], sizeName[64];    
    int cacheNum = 0;
    snprintf(cache, sizeof(cache), "%s%s%d", _PATH_SYS_SYSTEM, "/cpu/cpu0/cache/index", cacheNum++);
    while(access(cache, F_OK) == 0) {
        snprintf(cachePath, sizeof(cachePath), "%s%s", cache, "/type");
        FILE *f = pathOpen("r", cachePath);
        fgets(typeName, sizeof(typeName), f);
        fclose(f);
        typeName[strlen(typeName) - 1] = '\0';

        snprintf(cachePath, sizeof(cachePath), "%s%s", cache, "/level");
        f = pathOpen("r", cachePath);
        fgets(levelName, sizeof(levelName), f);
        fclose(f);

        snprintf(cachePath, sizeof(cachePath), "%s%s", cache, "/size");
        f = pathOpen("r", cachePath);
        fgets(sizeName, sizeof(sizeName), f);
        fclose(f);
        sizeName[strlen(sizeName) - 1] = '\0';

        if(strcmp(typeName, "Instruction") == 0) {
            snprintf(cachePath, sizeof(cachePath), "L%di cache:", atoi(levelName));
        } else if(strcmp(typeName, "Data") == 0) {
            snprintf(cachePath, sizeof(cachePath), "L%dd cache:", atoi(levelName));
        } else {
            snprintf(cachePath, sizeof(cachePath), "L%d cache:", atoi(levelName));
        }

        printf("%-21s%s\n", cachePath, sizeName);

        snprintf(cache, sizeof(cache), "%s%s%d", _PATH_SYS_SYSTEM, "/cpu/cpu0/cache/index", cacheNum++);
    }
}

void readDefaultInfo(struct info *info) {
    FILE *fp = pathOpen("r", _PATH_PROC_CPUINFO);
    char buf[BUFSIZ];
    struct utsname utsname;

    if(uname(&utsname) == -1) {
        err(EXIT_FAILURE, "uname fail");
    }
    info->arch = strdup(utsname.machine);

    // CPU 개수 가져오기
    char cpuNumBuf[BUFSIZ];    
    int a = 0;
    snprintf(cpuNumBuf, sizeof(cpuNumBuf), "%s%s%d", _PATH_SYS_SYSTEM, "/cpu/cpu", a);
    while(access(cpuNumBuf, F_OK) == 0) {
        snprintf(cpuNumBuf, sizeof(cpuNumBuf), "%s%s%d", _PATH_SYS_SYSTEM, "/cpu/cpu", ++a);
    }
    info->cpuNum = a;
    
    // 온라인 cpu 가져오기
    char cpuListFileName[BUFSIZ];
    snprintf(cpuListFileName, sizeof(cpuListFileName), "%s%s", _PATH_SYS_SYSTEM, "/cpu/online");
    FILE *flist = fopen(cpuListFileName, "r");

    cpuListFileName[0] = '\0';
    fgets(cpuListFileName, sizeof(cpuListFileName), flist);
    cpuListFileName[strlen(cpuListFileName) - 1] = '\0';
    info->onlineCpuList = cpuListFileName;
    fclose(flist);

    // cpuinfo 에 있는 데이터를 불러옴
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (parseCpuInfo(buf, "vendor", &info->vendor));
		else if (parseCpuInfo(buf, "vendor_id", &info->vendor));
		else if (parseCpuInfo(buf, "family", &info->cpuFamily));
		else if (parseCpuInfo(buf, "cpu family", &info->cpuFamily)) ;
		else if (parseCpuInfo(buf, "model name", &info->modelName)) ;
		else if (parseCpuInfo(buf, "model", &info->model)) ;
		else if (parseCpuInfo(buf, "stepping", &info->stepping)) ;
		else if (parseCpuInfo(buf, "cpu MHz", &info->mhz)) ;
		else if (parseCpuInfo(buf, "flags", &info->flag)) ;		/* x86 */
		else if (parseCpuInfo(buf, "features", &info->flag)) ;	/* s390 */
		else if (parseCpuInfo(buf, "bogomips", &info->bogoMips)) ;
		else
			continue;
	}

    info->cpuOpMode = initMode();
}

int main(void) {
    struct info _info, *info = &_info;
    memset(info, 0, sizeof(info));

    readDefaultInfo(info);

    if(info->arch) 
        printf("Archtecture:         %s\n", info->arch);

    if(info->cpuOpMode) {
        char buf[64], *p = buf;
		if (info->cpuOpMode & MODE_32BIT) {
			strcpy(p, "32-bit, ");
			p += 8;
		}
		if (info->cpuOpMode & MODE_64BIT) {
			strcpy(p, "64-bit, ");
			p += 8;
		}
		*(p - 2) = '\0';
		printf("CPU op-mode(s):      %s\n", buf);
    }

    // 컴파일러에서 빅인디안 여부를 파악할 수 있음
    #if defined(WORDS_BIGENDIAN)
        printf("Byte Order:          Big Endian\n");
    #else
        printf("Byte Order:          Little Endian\n");
    #endif

    if(info->cpuNum)
        printf("CPU(s):              %d\n", info->cpuNum);
    if(info->onlineCpuList)
        printf("On-line CPU(s) list: %s\n", info->onlineCpuList);
    if(info->vendor) 
        printf("Vendor ID:           %s\n", info->vendor);
    if(info->cpuFamily)
        printf("CPU family:          %s\n", info->cpuFamily);
    if(info->model)
        printf("Model:               %s\n", info->model);
    if(info->modelName) 
        printf("Model name:          %s\n", info-> modelName);
    if(info->stepping)
        printf("Stepping:            %s\n", info->stepping);
    if(info->mhz)
        printf("CPU MHz:             %s\n", info->mhz);
    if(info->bogoMips)
        printf("BogoMIPS:            %s\n", info->bogoMips);
    readAndPrintCacheInfo();
    if(info->flag)
        printf("Flags:               %s\n", info->flag);

    return 0;
}
