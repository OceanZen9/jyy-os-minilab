#include <stdio.h>
#include <stdbool.h>
#include <readline/readline.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>

 // Compile a function definition and load it
bool compile_and_load_function(const char* function_def) {
    char *template = "/tmp/crepl_XXXXXX.c";
    int fd;
    if ((fd = mkstemp(template, 2)) == -1) {
        perror("mkstemp");
        return false;
    }
    FILE *fp = fdopen(fd, "w");
    if (fp == NULL) {
        perror("fdopen");
        close(fd);
        return false;
    }
    fprintf(fp, "#include <stdio.h>\n");
    fprintf(fp, "#include <stdlib.h>\n");
    fprintf(fp, "#include <stdbool.h>\n");
    fprintf(fp, "#include <string.h>\n");
    fprintf(fp, "#include <math.h>\n");
    fprintf(fp, "%s\n", function_def);

    char so_name[256];
    snprintf(so_name, sizeof(so_name), "%.*s.so", (int)strlen(template) - 2, template);

    fclose(fp);

    char compile_cmd[512];
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        unlink(template);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        unlink(template);
        return false;
    }
    if (pid == 0) {
        // 关闭管道读取段
        close(pipe_fd[0]);
        // 重定向标准输出到管道写入段
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);

        // 执行编译命令
        char *args[] = {
            "gcc",
            "-shared",
            "-fPIC",
            "-o",
            so_name,
            template,
            NULL
        };
        execvp("gcc", args);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }else {
        // 关闭管道写入端
        close(pipe_fd[1]);

        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
        }

        close(pipe_fd[0]);
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 0) {
                //编译成功
                printf("Compilation successful, Shared library created:%s\n", so_name);
                //加载共享库
                void *handle = dlopen(so_name, RTLD_NOW);
                if (!handle) {
                    fprintf(stderr, "Error loading shared library: %s\n", dlerror());
                    unlink(template);
                    return false;
                }
            }else {
                //编译失败
                fprintf(stderr, "Compilation failed with exit status %d.\n", exit_status);
                unlink(template);
                return false;
            }
        }else {
            // 子进程未正常退出
            fprintf(stderr, "Compilaer process terminated abnormally.\n");
            unlink(template);
            return false;
        }
    }
    unlink(template);
    return false;
}

// Evaluate an expression
bool evaluate_expression(const char* expression, int* result) {

    return false;
}

int main() {
    char *line;
    while (1) {
        //每次循环开始重置flag
        int flag = 0;
        line = readline("crepl>");
        if (line == NULL ) {
            printf("\n");
            break;
        }
        if (*line == '\0') {
            free(line);
            continue;
        }
        add_history(line);
        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        }
        flag = (strncmp(line, "int ", 4) == 0);
        if (flag == 1) {
            if (compile_and_load_function(line)) {
                printf("successfully loaded function\n");
            }else {
                printf("failed to load function\n");
            }
        }else {
            int result;
            if (evaluate_expression(line, &result)) {
                printf("result: %d\n", result);
            } else {
                printf("error evaluating expression\n");
            }
        }

        free(line);
    }


    return 0;
}
