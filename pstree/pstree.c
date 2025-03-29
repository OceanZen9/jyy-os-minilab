#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

typedef struct proc_info{
    pid_t pid;
    char name[256];
    pid_t ppid;
    struct proc_info **children;
    int child_count;
    int child_capacity;
}proc_info_t;

typedef struct proc_list{
    proc_info_t **procs;
    int count;
    int capacity;
}proc_list_t;

void print_process(proc_info_t *proc, int depth, bool show_pids, bool is_last_child, char **prefix_lines);
void print_process_tree(proc_info_t *root, bool show_pids, bool numeric_sort);
void recursive_sort(proc_info_t *proc);
proc_info_t *read_proc_info(const char *pid_str);
int is_numeric(const char *str);
proc_list_t *get_all_processes(void);
void build_process_tree(proc_list_t *list);
proc_info_t *find_process_by_pid(proc_list_t *list,pid_t pid);
void free_proc_info(proc_info_t *proc);
void free_proc_list(proc_list_t *list);


int main(int argc ,char *argv[]) {
    bool show_pids = false;
    bool numeric = false;
    for(int i = 1;i < argc;i++){
        if(strcmp(argv[i],"-V") == 0 || strcmp(argv[i],"--version") == 0){
            printf("pstree version 0.1\n");
            return 0;
        }else if(strcmp(argv[i],"-p") == 0 || strcmp(argv[i],"--show-pids") == 0){
            show_pids = true;
        }else if(strcmp(argv[i],"-n") == 0 || strcmp(argv[i],"--numeric-sort") == 0){
            numeric = true;
        }
    }
    proc_list_t *list = get_all_processes();
    if(!list){
        fprintf(stderr,"Failed to get process list\n");
        return 1;
    }
    build_process_tree(list);
    proc_info_t *root = find_process_by_pid(list,1);
    if(!root){
        fprintf(stderr,"Failed to get the root process\n");
        free(list);
        return 1;
    }
    print_process_tree(root,show_pids,numeric);
    free_proc_list(list);
    return 0;
}

//从/proc/[pid]/status读取单个信息
proc_info_t *read_proc_info(const char *pid_str){
    char line[256];
    char path[256];

    proc_info_t *proc = malloc(sizeof(proc_info_t));
    if(proc == NULL) {
        perror("memory allocation failure");
        return NULL;
    }
    proc->pid = -1;
    proc->ppid = -1;
    proc->name[0] = '\0';
    proc->children = NULL;
    proc->child_count = 0;
    proc->child_capacity = 0;

    snprintf(path,sizeof(path),"/proc/%s/status",pid_str);
    FILE *fp = fopen(path,"r");
    if(fp == NULL){
        perror("can't open the thread's status");
        return NULL;
    }
    while(fgets(line,sizeof(line),fp) != NULL){
        if(strncmp(line,"Name:",5) == 0){
            sscanf(line,"Name: %s",proc->name);
        }else if(strncmp(line,"Pid: ",4) == 0){
            sscanf(line,"Pid: %d",&proc->pid);
        }else if(strncmp(line,"PPid: ",5) == 0){
            sscanf(line,"PPid: %d",&proc->ppid);
        }
    }
    fclose(fp);
    if(proc->pid == -1 || proc->ppid == -1 || proc->name[0] == '\0'){
        fprintf(stderr,"Failed to read complete process info for %s\n",pid_str);
        free(proc);
        return NULL;
    }
    return proc;
}

//判断pid
int is_numeric(const char *str){
    while(*str){
        if(!isdigit(*str)) return 0;
        str ++;
    }
    return 1;
}

//扫描/proc目录，获取所有进程信息
proc_list_t *get_all_processes(void){
    proc_list_t *proc_list = malloc(sizeof(proc_list_t));
    proc_list ->count = 0;
    proc_list ->capacity = 10;
    proc_list ->procs = malloc(sizeof(proc_info_t*) * proc_list->capacity);
    DIR *dir = opendir("/proc");
    if(dir == NULL){
        perror("can't open the dir");
        free(proc_list->procs);
        free(proc_list);
        return NULL;
    }
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL){
        if(is_numeric(entry -> d_name)){
            proc_info_t *proc = read_proc_info(entry->d_name);
            if(proc != NULL){
                if(proc_list->count >= proc_list->capacity){
                    proc_list->capacity *= 2;
                    proc_list->procs = realloc(proc_list->procs,sizeof(proc_info_t*) * proc_list->capacity);
                }

                proc_list->procs[proc_list->count++] = proc;
            }
        }
    }
    closedir(dir);
    return proc_list;
}

//根据pid和ppid关系构建树结构
void build_process_tree(proc_list_t *list){
    for(int i = 0;i < list->count;i++){
        list->procs[i]->child_capacity = 4;
        list->procs[i]->children = malloc(sizeof(proc_info_t*) * list->procs[i]->child_capacity);
        list->procs[i]->child_count = 0;
    }
    for(int i = 0;i < list->count;i++){
        for(int j = 0;j < list->count;j++){
            if(list->procs[i]->pid == list->procs[j]->ppid){
                if(list->procs[i]->child_count >= list->procs[i]->child_capacity){
                    list->procs[i]->child_capacity *= 2;
                    list->procs[i]->children = realloc(list->procs[i]->children,sizeof(proc_info_t*) * list->procs[i]->child_capacity);
                }
                list->procs[i]->children[list->procs[i]->child_count++] = list->procs[j];
            }
        }
    }
}

//根据pid查找进程
proc_info_t *find_process_by_pid(proc_list_t *list,pid_t pid){
    for(int i = 0;i < list->count;i++){
        if(list->procs[i]->pid == pid) return list->procs[i];
    }
    return NULL;
}

//递归打印单个进程及其子进程
void print_process(proc_info_t *proc, int depth, bool show_pids, bool is_last_child, char **prefix_lines) {
    // 打印当前行的前缀（竖线和空格）
    for (int i = 0; i < depth; i++) {
        if (prefix_lines[i]) 
            printf("%s", prefix_lines[i]);
        else 
            printf("    ");
    }
    
    // 打印当前节点的连接线和进程名
    if (depth > 0) {
        if (is_last_child)
            printf("└── ");
        else
            printf("├── ");
    }
    
    printf("%s", proc->name);
    if (show_pids) {
        printf("(%d)", proc->pid);
    }
    printf("\n");

    char *old_prefix = NULL;
    if (depth > 0) {
        old_prefix = prefix_lines[depth-1];
    }
    // 设置新的前缀行
    if (depth > 0) {
        if (is_last_child)
            prefix_lines[depth-1] = "    "; 
        else
            prefix_lines[depth-1] = "│   "; 
    }
    
    // 递归打印所有子进程
    for (int i = 0; i < proc->child_count; i++) {
        bool is_last = (i == proc->child_count - 1);
        print_process(proc->children[i], depth + 1, show_pids, is_last, prefix_lines);
    }

    if (depth > 0 && old_prefix != NULL) {
        prefix_lines[depth-1] = old_prefix;
    }
}

void recursive_sort(proc_info_t *proc) {
    // 先对当前节点的子节点进行排序
    for (int i = 0; i < proc->child_count - 1; i++) {
        for (int j = 0; j < proc->child_count - i - 1; j++) {
            if (proc->children[j]->pid > proc->children[j + 1]->pid) {
                proc_info_t *temp = proc->children[j];
                proc->children[j] = proc->children[j + 1];
                proc->children[j + 1] = temp;
            }
        }
    }
    
    // 递归对每个子节点进行排序
    for (int i = 0; i < proc->child_count; i++) {
        recursive_sort(proc->children[i]);
    }
}

//打印整个进程树
void print_process_tree(proc_info_t *root, bool show_pids, bool numeric_sort) {
    if (numeric_sort) {
        recursive_sort(root);  // 递归排序整个树
    }
    
    char *prefix_lines[256] = {0}; // 初始化前缀行数组为空
    print_process(root, 0, show_pids, true, prefix_lines);
}

//释放资源
void free_proc_info(proc_info_t *proc){
    if(proc){
        if(proc->children) free(proc->children);
        free(proc);
    }
}

void free_proc_list(proc_list_t *list){
    if(list){
        for(int i = 0;i < list->count;i ++){
            free_proc_info(list->procs[i]);
        }
        free(list->procs);
        free(list);
    }
}