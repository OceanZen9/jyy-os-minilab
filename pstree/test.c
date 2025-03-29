#include<stdio.h>
#include<stdbool.h>
#include<string.h>
int main(int agrc,char *agrv[]){
    char line[256];
    char process_name[256];
    int process_id = -1;
    int process_parent_id = -1;
    FILE *fp = fopen("/proc/100/status","r");
    while(fgets(line,sizeof(line),fp) != NULL){
        if(strncmp(line,"Name:",5) == 0){
            sscanf(line,"Name: %s",process_name);
        }else if(strncmp(line,"Pid:",4) == 0){
            sscanf(line,"Pid: %d",&process_id);
        }else if(strncmp(line,"PPid:",5) == 0){
            sscanf(line,"PPid: %d",&process_parent_id);
        }
    }
    fclose(fp);
    printf("进程名：%s\n",process_name);
    printf("Pid：%d\n",process_id);
    printf("PPid：%d\n",process_parent_id);
    return 0;
}