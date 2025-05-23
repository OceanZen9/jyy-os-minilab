#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

// Feel free to change the data structures generated by AI.

#define MAX_SYSCALLS 1024
#define TOP_N 5

typedef struct {
    char name[64];
    double time;
} syscall_stat;

typedef struct {
    syscall_stat stats[MAX_SYSCALLS];
    int count;
    double total_time;
} syscall_stats;

int parse_strace_line(char *line, char *syscall_name, double *time) {
    char *paren = strchr(line, '(');
    if (!paren) return 0;
    
    int name_len = paren - line;
    if (name_len <= 0 || name_len >= 64) return 0;
    
    strncpy(syscall_name, line, name_len);
    syscall_name[name_len] = '\0';
    
    // 修剪空白字符
    int start = 0, end = name_len - 1;
    while (start < name_len && isspace(syscall_name[start])) start++;
    while (end > start && isspace(syscall_name[end])) end--;
    
    if (start > 0 || end < name_len - 1) {
        memmove(syscall_name, syscall_name + start, end - start + 1);
        syscall_name[end - start + 1] = '\0';
    }
    
    char *time_start = strstr(line, "<");
    char *time_end = strstr(line, ">");
    
    if (time_start && time_end && time_end > time_start) {
        time_start++;  // 跳过'<'
        *time = atof(time_start);
        return 1;
    }
    
    return 0;
}

void add_syscall(syscall_stats *stats, const char *name, double time) {
    for (int i = 0; i < stats->count; i++) {
        if (strcmp(stats->stats[i].name, name) == 0) {
            stats->stats[i].time += time;
            stats->total_time += time;
            return;
        }
    }
    if (stats->count >= MAX_SYSCALLS) {
        return;
    }
    stats->stats[stats->count].time = time;
    strncpy(stats->stats[stats->count].name, name, sizeof(stats->stats[stats->count].name) - 1);
    stats->stats[stats->count].name[sizeof(stats->stats[stats->count].name) - 1] = '\0';
    stats->total_time += time;
    stats->count++;
}

// 比较函数，用于qsort函数排序系统调用（按时间降序）
int compare_syscalls(const void *a, const void *b) {
    const syscall_stat *sa = (const syscall_stat *)a;
    const syscall_stat *sb = (const syscall_stat *)b;
    
    // 降序排列（时间多的排在前面）
    if (sa->time > sb->time) return -1;
    if (sa->time < sb->time) return 1;
    return 0;
}

void print_top_syscalls(syscall_stats *stats, int n) {
    // 按时间排序
    qsort(stats->stats, stats->count, sizeof(syscall_stat), compare_syscalls);
    
    // 打印top N
    int actual_n = (stats->count < n) ? stats->count : n;
    for (int i = 0; i < actual_n; i++) {
        int percentage = (int)((stats->stats[i].time / stats->total_time) * 100);
        printf("%s (%d%%)\n", stats->stats[i].name, percentage);
    }
    
    // 分隔符和刷新
    char nulls[81] = {0};
    fwrite(nulls, 1, 80, stdout);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        return 0;
    }
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    //多进程处理
    if (pid == 0) {
        //子进程处理
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        //执行starce命令
        //获取starce命令行参数
        char **starce_argv = malloc(sizeof(char *) * (argc + 2));
        if (!starce_argv) {
            perror("malloc");
            return EXIT_FAILURE;
        }

        starce_argv[0] = "strace";
        starce_argv[1] = "-T";

        for (int i = 1; i < argc; i++) {
            starce_argv[i + 1] = argv[i];
        }
        starce_argv[argc + 1] = NULL;//参数数组必须以NULL结尾

        //获取环境变量
        extern char **environ;

        //执行strace命令
        execve ("/usr/bin/strace", starce_argv, environ);
        perror("execve");
        free(starce_argv);
        return EXIT_FAILURE;
    }else {
        //父进程处理
        close(pipefd[1]);

        //读取管道数据
        //初始化数据结构
        syscall_stats stats = {0};//初始化系统调用结构体

        //用于读取数据的缓冲区
        char buffer[4096];
        char line[4096];
        int line_pos = 0;

        //用于计时和控制输出频率
        struct timeval last_print,current;
        gettimeofday(&last_print, NULL);
        
        //循环读取和处理数据
        ssize_t n;
        while((n = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            buffer[n] = '\0';

            //处理缓冲区的每个字符
            for (int i = 0;i < n; i++){
                if (buffer[i] == '\n') {
                    //处理完整行
                    line[line_pos] = '\0';

                    char syscall_name[64];
                    double time;
                    if (parse_strace_line(line, syscall_name, &time)) {
                        add_syscall(&stats, syscall_name, time);
                    }

                    line_pos = 0;
                } else {
                    //积累字符到行缓冲区
                    if (line_pos < sizeof(line) - 1) {
                        line[line_pos++] = buffer[i];
                    }
                }
            }
            //检查是否需要输出
            gettimeofday(&current, NULL);
            //计算时间差
            long diff = (current.tv_sec - last_print.tv_sec) * 1000 +
                        (current.tv_usec - last_print.tv_usec) / 1000;
            
            if (diff >= 1000) {
                print_top_syscalls(&stats, TOP_N);
                last_print = current;
            }
        }
        //读取结束，最后一次输出结果
        print_top_syscalls(&stats, TOP_N);
        //关闭管道
        close(pipefd[0]);

        //等待子进程结束
        int status;
        waitpid(pid, &status, 0);

        //处理子进程的退出状态
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return EXIT_FAILURE;
        }
    }
}
