#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <stdbool.h>


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


int main(int argc ,char *argv[]) {
    if(argc < 2){

    }
    const char *command = NULL;
    for(int i = 1;i < argc;i++){
        if(strcmp(argv[i],"--show-pids") == 0 ||strcmp(argv[i],"-p") == 0){
            command = argv[i++];
        }else if(strcmp(argv[i],"--numeric-sort") == 0 ||strcmp(argv[i],"-n") == 0){
            command = argv[i++];
        }else if(strcmp(argv[i],"--version") == 0 ||strcmp(argv[i],"-V") == 0){
            command = argv[i++];
        }
    } 
    return 0;
}

//从/proc/[pid]/status读取单个信息
proc_info_t *read_proc_info(const char *pid_str){
    char line[256];
    char path[256];

    proc_info_t *proc = malloc(sizeof(proc_info_t));
    if(proc == NULL){
        perror("内存分配失败");
        return NULL;
    }
    //初始化结构体
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
            if(list->procs[j]->pid == 1) continue;
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

//打印整个进程树，从根节点开始
void print_process_tree(proc_info_t *list,bool show_pids,bool numeric_sort){

}
//递归打印单个进程及其子进程
void print_process(proc_info_t *proc,int depth,bool show_pids){

}
