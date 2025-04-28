#include <stdio.h>
#include <stdbool.h>
#include <readline/readline.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    char *name;
    void *func_ptr;
    void *library_handle;
} LoadedFunction;
LoadedFunction loaded_functions[100];
int loaded_function_count = 0;

 // Compile a function definition and load it
bool compile_and_load_function(const char* function_def) {
    char *template = "/tmp/crepl_XXXXXX.c";
    int fd;
    if ((fd = mkstemp(template)) == -1) {
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

    for (int i = 0; i < loaded_function_count; i++) {
        fprintf(fp, "int %s(int, ...);\n", loaded_functions[i].name);
    }
    
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
            "-Wno-implicit-function-declaration",  
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
                void *handle = dlopen(so_name, RTLD_NOW | RTLD_GLOBAL);
                if (!handle) {
                    fprintf(stderr, "Error loading shared library: %s\n", dlerror());
                    unlink(template);
                    unlink(so_name);
                    return false;
                }
                //提取函数名
                const char *start = function_def;
                while (*start == ' ' || *start == '\t') start++;
                if (strncmp(start, "int", 3) == 0) start += 3;
                while (*start == ' ' || *start == '\t') start++;
                const char *end = strchr(start, '(');
                if (end == NULL) {
                    fprintf(stderr, "Invalid function definition.\n");
                    dlclose(handle);
                    unlink(template);
                    unlink(so_name);
                    return false;
                }
                const char *name_end = end - 1;
                while (name_end > start && (*name_end == ' ' || *name_end == '\t')) name_end--;
                name_end++;
                size_t func_name_length = name_end - start;
                char *function_name = malloc(func_name_length + 1);
                if (function_name == NULL) {
                    fprintf(stderr, "Memory allocation failed.\n");
                    dlclose(handle);
                    unlink(template);
                    unlink(so_name);
                    return false;
                }
                strncpy(function_name, start, func_name_length);
                function_name[func_name_length] = '\0';
                //获取函数结构体
                loaded_functions[loaded_function_count].func_ptr = dlsym(handle, function_name);
                if (!loaded_functions[loaded_function_count].func_ptr) {
                    fprintf(stderr, "Error finding function: %s\n", dlerror());
                    dlclose(handle);
                    unlink(template);
                    unlink(so_name);
                    free(function_name);
                    return false;
                }
                loaded_functions[loaded_function_count].name = function_name;
                loaded_functions[loaded_function_count].library_handle = handle;
                loaded_function_count++;

                return true;
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
            unlink(so_name);
            return false;
        }
    }
    unlink(template);
    return false;
}

// Evaluate an expression
bool evaluate_expression(const char* expression, int* result) {
    char *template = "/tmp/crepl_XXXXXX.c";
    int fd;
    if ((fd = mkstemp(template)) == -1) {
        perror("mkstemp");
        return false;
    }

    FILE *fp = fdopen(fd, "w");
    if (fp == NULL) {
        perror("fdopen");
        close(fd);
        unlink(template);
        return false;
    }
    fprintf(fp, "#include <stdio.h>\n");
    fprintf(fp, "#include <stdlib.h>\n");
    fprintf(fp, "#include <stdbool.h>\n");
    fprintf(fp, "#include <string.h>\n");
    fprintf(fp, "#include <math.h>\n");

    // 添加已定义函数的声明
    for (int i = 0; i < loaded_function_count; i++) {
        fprintf(fp, "int %s(int, ...);\n", loaded_functions[i].name);
    }

    fprintf(fp, "int evaluate_user_expression() {\n");
    fprintf(fp, "    return %s;\n", expression);
    fprintf(fp, "}\n");

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
            "-Wno-implicit-function-declaration",  
            "-o",
            so_name,
            template,
            NULL
        };
        execvp("gcc", args);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }else {
        // 关闭写入端
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
                void *handle = dlopen(so_name, RTLD_NOW | RTLD_GLOBAL);
                if (!handle) {
                    fprintf(stderr, "Error loading shared library: %s\n", dlerror());
                    unlink(template);
                    return false;
                }
                //获取函数指针
                int (*evaluate_func)() = dlsym(handle, "evaluate_user_expression");
                if (!evaluate_func) {
                    fprintf(stderr, "Error finding function: %s\n", dlerror());
                    dlclose(handle);
                    unlink(template);
                    return false;
                }
                //调用函数
                *result = evaluate_func(); //传入参数
                dlclose(handle);
                unlink(so_name);
                unlink(template);
                return true;
            }else {
                //编译失败
                fprintf(stderr, "Compilation failed with exit status %d.\n", exit_status);
                unlink(template);
                return false;
            }
        }
    }
    
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
                printf("OK.\n");
            }else {
                printf("failed to load function\n");
            }
        }else {
            int result;
            if (evaluate_expression(line, &result)) {
                printf("= %d.\n", result);
            } else {
                printf("error evaluating expression\n");
            }
        }

        free(line);
    }
    // 释放已加载的函数和库
    for (int i = 0; i < loaded_function_count; i++) {
        dlclose(loaded_functions[i].library_handle);
        free(loaded_functions[i].name);
    }
    return 0;
}
