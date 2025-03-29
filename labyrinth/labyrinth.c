#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <testkit.h>
#include "labyrinth.h"
void printUsage(void);

int main(int argc, char *argv[]) {
    // TODO: Implement this function
    if(argc < 2 ){
        printUsage();
        return 1;
    }
    const char *mapFILE = NULL;
    char playerID = '\0';
    const char *moveDirection = NULL;
    for(int i = 1;i < argc;i++){
        if(strcmp(argv[i],"--version") == 0){
            if(argc != 2){
                return 1;
            }
            printf("Success\n");
            printf("%s\n",VERSION_INFO);
            return 0;
        }
        else if(strcmp(argv[i],"-m") == 0 || strcmp(argv[i],"--map") == 0){
            if(i + 1 < argc){
                mapFILE = argv[++i];
                }
            else{
                return 1;
            }
        }
        else if(strcmp(argv[i],"-p") == 0 || strcmp(argv[i],"--player") == 0){
            if(i + 1 < argc){
                playerID = argv[++i][0];
            }
            else{
                return 1;
            }
        }
        else if( strcmp(argv[i],"--move") == 0){
            if(i + 1 < argc){
                moveDirection = argv[++i];
            }
            else{
                return 1;
            }
        }
        else{
            return 1;
        }
    }
    if(!mapFILE){
        fprintf(stderr,"Error:Map file is required\n");
        return 1;
    }

    if(!playerID){
        fprintf(stderr,"Error:Player ID is required\n");
        return 1;
    }
    if(!isValidPlayer(playerID)){
        fprintf(stderr,"Error:Invalid player ID\n");
        return 1;
    }
    Labyrinth labyrinth;
    if(!loadMap(&labyrinth,mapFILE)){
        fprintf(stderr,"Error:Failed to load map\n");
        return 1;
    }
    Position pos = findPlayer(&labyrinth,playerID);
    if(pos.row == -1 && pos.col == -1){
        pos = findFirstEmptySpace(&labyrinth);
    }
    if(pos.row != -1 && pos.col != -1){
        labyrinth.map[pos.row][pos.col] = playerID;
    }
    else{
        fprintf(stderr,"Error:Player not found\n");
        return 1;
    }

    if(moveDirection){
        if(strcmp(moveDirection,"up") != 0 && strcmp(moveDirection,"down") != 0
    && strcmp(moveDirection,"left") != 0 && strcmp(moveDirection,"right") != 0){
            fprintf(stderr,"Error:Invalid move direction\n");
            return 1;
        }
        if(!movePlayer(&labyrinth,playerID,moveDirection)){
            fprintf(stderr,"Error:Invalid move\n");
            return 1;        
        }
        if(!saveMap(&labyrinth,mapFILE)){
            fprintf(stderr,"Error:Failed to save map\n");
            return 1;
        }
    }
    for(int i = 0;i < labyrinth.rows;i++){
        printf("%s\n",labyrinth.map[i]);
    }
    return 0;
}

void printUsage() {
    printf("Usage:\n");
    printf("  labyrinth --map map.txt --player id\n");
    printf("  labyrinth -m map.txt -p id\n");
    printf("  labyrinth --map map.txt --player id --move direction\n");
    printf("  labyrinth --version\n");
}

bool isValidPlayer(char playerId) {
    // TODO: Implement this function
    if(playerId >= '0' && playerId <= '9'){
        return true;
    }
    return false;
}

bool loadMap(Labyrinth *labyrinth, const char *filename) {
    // TODO: Implement this function
    FILE *file = fopen(filename, "r");
    if(file == NULL){
        return false;
    }
    labyrinth->cols = 0;
    labyrinth->rows = 0;
    char line[MAX_COLS +1];
    while(fgets(line, sizeof(line), file) != NULL && labyrinth->rows < MAX_ROWS){
        size_t len = strlen(line);
        if(len > 0 && (line[len -1] == '\n' || line[len - 1] == '\r')){
            line[--len] = '\0';
        }
        if(len > 0 && (line[len -1] == '\n' || line[len - 1] == '\r')){
            line[--len] = '\0';
        }
        if(len > labyrinth->cols){
            labyrinth->cols = len;
        }
        strcpy(labyrinth->map[labyrinth->rows],line);

        labyrinth->rows++;
        
    }
    fclose(file);
    return (labyrinth->rows>0);
}

Position findPlayer(Labyrinth *labyrinth, char playerId) {
    // TODO: Implement this function
    int cols = labyrinth->cols;
    int rows = labyrinth->rows;
    for(int i = 0;i < rows;i++){
        for(int j = 0;j < cols;j++){
            if (labyrinth->map[i][j] == playerId){
                Position pos = {i,j};
                return pos;
            }
        }
    }
    Position pos = {-1, -1};
    return pos;
}

Position findFirstEmptySpace(Labyrinth *labyrinth) {
    // TODO: Implement this function
    int cols = labyrinth->cols;
    int rows = labyrinth->rows;
    for(int i = 0;i < rows;i++){
        for(int j = 0;j < cols;j++){
            if (labyrinth->map[i][j] == '.' ){  
                Position pos = {i,j};
                return pos;
            }
        }
    }
    Position pos = {-1, -1};
    return pos;
}

bool isEmptySpace(Labyrinth *labyrinth, int row, int col) {
    // TODO: Implement this function
    if(row < 0 || col < 0 || row >= labyrinth->rows || col >= labyrinth->cols){
        return false;
    }
    return labyrinth->map[row][col] == '.';
}

bool movePlayer(Labyrinth *labyrinth, char playerId, const char *direction) {
    // TODO: Implement this function
    Position pos = findPlayer(labyrinth,playerId);
    int row = pos.row;
    int col = pos.col;
    if(row == -1) return false;
    if(strcmp(direction,"up") == 0){
        if(row > 0 && isEmptySpace(labyrinth,row - 1,col)){
            labyrinth->map[row][col] = '.';
            labyrinth->map[row - 1][col] = playerId;
            return true;
        }
    }
    if(strcmp(direction,"down") == 0){
        if(row < labyrinth->rows - 1 && isEmptySpace(labyrinth,row + 1,col)){
            labyrinth->map[row][col] = '.';
            labyrinth->map[row + 1][col] = playerId;
            return true;
        }
    }
    if(strcmp(direction,"left") == 0){
        if(col > 0 && isEmptySpace(labyrinth,row,col - 1)){
            labyrinth->map[row][col] = '.';
            labyrinth->map[row][col - 1] = playerId;
            return true;
        }
    }
    if(strcmp(direction,"right") == 0){
        if(col < labyrinth->cols - 1 && isEmptySpace(labyrinth,row,col + 1)){
            labyrinth->map[row][col] = '.';
            labyrinth->map[row][col + 1] = playerId;
            return true;
        }
    }
    return false;
}

bool saveMap(Labyrinth *labyrinth, const char *filename){    
    // TODO: Implement this function
    FILE *file = fopen(filename,"w");
    if(file == NULL) return false;
    for(int i = 0;i < labyrinth->rows;i++){
        fputs(labyrinth->map[i],file);
        fputc('\n',file);
    }
    fclose(file);
    return true;
}
// Check if all empty spaces are connected using DFS
void dfs(Labyrinth *labyrinth, int row, int col, bool visited[MAX_ROWS][MAX_COLS]) {
    // TODO: Implement this function
    if(row < 0 || col < 0 || row >= labyrinth->rows||
    col >= labyrinth->cols|| !isEmptySpace(labyrinth,row,col) ||
    visited[row][col]){
        return;
    }
    visited[row][col] = true;

    dfs(labyrinth,row - 1,col,visited);
    dfs(labyrinth,row + 1,col,visited);
    dfs(labyrinth,row,col - 1,visited);
    dfs(labyrinth,row,col + 1,visited);
}

bool isConnected(Labyrinth *labyrinth) {
    // TODO: Implement this function
    bool visited[MAX_ROWS][MAX_COLS] = {false};
    Position pos = findFirstEmptySpace(labyrinth);
    if(pos.row == -1) return false;
    dfs(labyrinth,pos.row,pos.col,visited);
    for(int i = 0;i < labyrinth->rows;i ++){
        for(int j = 0 ;j < labyrinth->cols;j ++){
            if(isEmptySpace(labyrinth,i,j) && !visited[i][j]){
                return false;
            }
        }
    }
    return true;
}
