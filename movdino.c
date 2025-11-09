#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

///КОМАНДЫ ДЛЯ ТЕРМИНАЛА
///  cd C:\mingw64
///  .\movdino.exe program.txt.txt


struct Field {
    int w, h;
    char **tiles;
    int dino_x, dino_y;
    int has_dino;
    char **colors;
};
//ПОЛЕ
int init_field(struct Field *f, int w, int h) {
    f->w = w;
    f->h = h;
    f->has_dino = 0;

    f->tiles = malloc(h * sizeof(char *));
    f->colors = malloc(h * sizeof(char *)); 
    for (int y = 0; y < h; y++) {
        f->tiles[y] = malloc(w);
        f->colors[y] = malloc(w); 
        for (int x = 0; x < w; x++) {
            f->tiles[y][x] = '_';
            f->colors[y][x] = ' ';
        }
    }
    return 1;
}

//ВЫВОД ПОЛЯ
void print_field(struct Field *f) {
    if (!f->tiles) return;

    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            if (f->has_dino && x == f->dino_x && y == f->dino_y)
                putchar('#');
            else if (f->colors[y][x] != ' ')
                putchar(f->colors[y][x]);
            else
                putchar(f->tiles[y][x]);
        }
        putchar('\n');
    }
}



///ПОЗИЦИЯ ИЗНАЧАЛЬНАЯ ДЛЯ ДИНО
int place_dino(struct Field *f, int x, int y) {
    if (x < 0 || x >= f->w || y < 0 || y >= f->h) return 0;
    f->dino_x = x;
    f->dino_y = y;
    f->has_dino = 1;
    return 1;
}
///ПЕРЕХОДЫ ДИНО
void move_dino(struct Field *f, char *dir) {
    if (!f->has_dino) return;

    int nx = f->dino_x;
    int ny = f->dino_y;

    if (strcmp(dir, "UP") == 0) ny--;
    else if (strcmp(dir, "DOWN") == 0) ny++;
    else if (strcmp(dir, "LEFT") == 0) nx--;
    else if (strcmp(dir, "RIGHT") == 0) nx++;

    if (nx < 0) nx = f->w - 1;
    if (nx >= f->w) nx = 0;
    if (ny < 0) ny = f->h - 1;
    if (ny >= f->h) ny = 0;

    char cell = f->tiles[ny][nx];

    // ЯМА НИЗЯЯЯЯ
    if (cell == '%') exit(0);

    // НИИИЗЯ НА ГОРУ, ДЕРЕВО И КАМЕНЬ
    if (cell == '^' || cell == '&' || cell == '@') return;
    

    f->dino_x = nx;
    f->dino_y = ny;
}
/// ЯМААААА
/// КОПАЕМ ЯМУ
void dig_dino(struct Field *f, char *dir) {
    if (!f->has_dino) return;

    int tx = f->dino_x;
    int ty = f->dino_y;

    if (strcmp(dir, "UP") == 0) ty--;
    else if (strcmp(dir, "DOWN") == 0) ty++;
    else if (strcmp(dir, "LEFT") == 0) tx--;
    else if (strcmp(dir, "RIGHT") == 0) tx++;

    //топология
    if (tx < 0) tx = f->w - 1;
    if (tx >= f->w) tx = 0;
    if (ty < 0) ty = f->h - 1;
    if (ty >= f->h) ty = 0;

    f->tiles[ty][tx] = '%';
}

///ГОООООРЫ АЛЬПИЙСКИЕ, УССУРИЙСКИЕ, КАВКАВЗСКИЕ, ГООООООРЫЫЫЫЫЫЫЫ
void mound_dino(struct Field *f, char *dir) {
    if (!f->has_dino) return;

    int tx = f->dino_x;
    int ty = f->dino_y;

    if (strcmp(dir, "UP") == 0) ty--;
    else if (strcmp(dir, "DOWN") == 0) ty++;
    else if (strcmp(dir, "LEFT") == 0) tx--;
    else if (strcmp(dir, "RIGHT") == 0) tx++;

    //топология
    if (tx < 0) tx = f->w - 1;
    if (tx >= f->w) tx = 0;
    if (ty < 0) ty = f->h - 1;
    if (ty >= f->h) ty = 0;

    if (f->tiles[ty][tx] == '%'){
        f->tiles[ty][tx] = '_';
    } else {
        f->tiles[ty][tx] = '^';
    }
}
///ПРЫГАЕМ ОТСЮДА
void jump_dino(struct Field *f, char *dir, int jum) {
    if (!f->has_dino) return;

    int tx = f->dino_x;
    int ty = f->dino_y;
    int count = 0;

    
    if (strcmp(dir, "UP") == 0){ //движение дино вверх
        for (int i = 0; i < jum; i++){ //дино двигается до размера прыжка или до препятствия
            
            if (f->dino_y-2 >= 0 && i == (jum-2) && f->tiles[f->dino_y-2][tx] == '%'){ //низя в яму
                f->dino_y = ty;
                exit(0);
            }
            if (ty-i-1 >= 0){ //дино идет вверх до первой строчки включительно
                f->dino_y = ty-i-1;
            }
            
            if (ty-i-1 >= 0 && f->tiles[ty-i-1][tx] == '^'){ //если по пути нашлась гора, он встает перед дней
                f->dino_y = (ty-i-1)+1;
                break;
            }
            if (f->tiles[(f->h-1)][tx] == '^' && f->dino_y-1 < 0){ //если гора в конце
                break;
            }
            if (f->dino_y-1 < 0 && i == (jum-2) && f->tiles[(f->h-1)][tx] == '%'){ //если яма в конце
                f->dino_y = ty;
                exit(0);
            }


            if (ty-i-1 < 0){ //если дино выходит с противоположной стороны (то есть он идет с последней строки вверх) и по пути нету гор
                if ((((f->h)-1)-count) >= 0){
                    f->dino_y = ((f->h)-1)-count;
                    if (count < (f->h)-1 ){
                        if (f->tiles[((f->h)-1)-count-1][tx] == '^'){
                            break;
                        }
                    }
                } 
                else if ((((f->h)-1-count) < 0)){
                    count = 0;
                    f->dino_y = ((f->h)-1)-count;
                }
                count++;
            }
        }
    }
    if (strcmp(dir, "DOWN") == 0){ //движение дино вниз
        for (int i = 0; i < jum; i++){ //дино двигается до размера прыжка или до препятствия
            
            if (f->dino_y+2 < f->h && (i == (jum-2) || i == 0 || i == 1) && f->tiles[f->dino_y+2][tx] == '%'){ //низя в яму
                f->dino_y = ty;
                exit(0);
            }

            
            
            if (ty+i+1 < f->h){ //дино идет вниз до последней строчки включительно
                f->dino_y = ty+i+1;
            }
            
            if (ty+i+1 < f->h && f->tiles[ty+i+1][tx] == '^'){ //если по пути нашлась гора, он встает перед дней
                f->dino_y = (ty+i+1)-1;
                break;
            }
            if (f->tiles[0][tx] == '^' && f->dino_y+1 > f->h-1){ //если гора в начале
                break;
            }
            if (f->dino_y+1 > f->h-1 && i == (jum-2) && f->tiles[0][tx] == '%'){ //если яма в начле
                f->dino_y = ty;
                exit(0);
            }


            if (ty+i+1 > f->h-1){ //если дино выходит с противоположной стороны (то есть он идет с первой строки вниз)
                if ((count) < f->h){
                    f->dino_y = count;
                    if (count < (f->h)-1 ){
                        if (f->tiles[count+1][tx] == '^'){
                            break;
                        }
                    }
                } 
                else if (count > f->h-1){
                    count = 0;
                    f->dino_y = count;
                }
                count++;
            }
        }
    }
    if (strcmp(dir, "LEFT") == 0){ // движение дино влево
        for (int i = 0; i < jum; i++){ // двигаемся до размера прыжка или до горы

            // низя в яму
            if (f->dino_x-2 >= 0 && (i == (jum-2) || i == 0 || i == 1) && f->tiles[ty][f->dino_x-2] == '%'){
                f->dino_x = tx;
                exit(0);
            }


            // идем влево
            if (tx-i-1 >= 0){
            f->dino_x = tx-i-1;
            }

            // если по пути гора встать перед ней
            if (tx-i-1 >= 0 && f->tiles[ty][tx-i-1] == '^'){
                f->dino_x = (tx-i-1)+1;
                break;
            }

            // гора на левой границе
            if (f->tiles[ty][0] == '^' && f->dino_x-1 < 0){
                break;
            }

            // яма на левой границе
            if (f->dino_x-1 < 0 && i == (jum-2) && f->tiles[ty][(f->w)-1] == '%'){
            f->dino_x = tx;
            exit(0);
            }

            // переход через границу
            if (tx-i-1 < 0){
                if (((f->w)-1 - count) >= 0){
                    f->dino_x = (f->w)-1 - count;
                    if (count < (f->w)-1 ){
                        if (f->tiles[ty][(f->w)-1 - count - 1] == '^'){
                            break;
                        }
                    }
                }
                else {
                    count = 0;
                    f->dino_x = (f->w)-1;
                }
                count++;
            }
        }
    }
    if (strcmp(dir, "RIGHT") == 0){ // движение дино вправо
        for (int i = 0; i < jum; i++){

            // нельзя в яму
            if (f->dino_x+2 < f->w && (i == (jum-2) || i == 0 || i == 1) && f->tiles[ty][f->dino_x+2] == '%'){
                f->dino_x = tx;
                exit(0);
            }


            // идем вправо
            if (tx+i+1 < f->w){
                f->dino_x = tx+i+1;
            }

            // гора на пути
            if (tx+i+1 < f->w && f->tiles[ty][tx+i+1] == '^'){
                f->dino_x = (tx+i+1)-1;
                break;
            }

            // гора справа у края
            if (f->tiles[ty][(f->w)-1] == '^' && f->dino_x+1 > (f->w)-1){
                break;
            }

            // яма у края
            if (f->dino_x+1 > (f->w)-1 && i == (jum-2) && f->tiles[ty][0] == '%'){
                f->dino_x = tx;
                exit(0);
                }

            // переход через границу
            if (tx+i+1 > (f->w)-1){
                if ((count) < f->w){
                    f->dino_x = count;
                    if (count < (f->w)-1 ){
                        if (f->tiles[ty][count+1] == '^'){
                            break;
                        }
                    }
                }
                else {
                    count = 0;
                    f->dino_x = count;
                }
                count++;
            }
        }
    }
}


/// ДЕРЕВО, БЛИН, НЕ ПРОЙДЁШЬ
void grow_dino(struct Field *f, char *dir) {
    if (!f->has_dino) return;

    int tx = f->dino_x;
    int ty = f->dino_y;

    if (strcmp(dir, "UP") == 0) ty--;
    else if (strcmp(dir, "DOWN") == 0) ty++;
    else if (strcmp(dir, "LEFT") == 0) tx--;
    else if (strcmp(dir, "RIGHT") == 0) tx++;

    if (tx < 0) tx = f->w - 1;
    if (tx >= f->w) tx = 0;
    if (ty < 0) ty = f->h - 1;
    if (ty >= f->h) ty = 0;

    if (f->tiles[ty][tx] == '_') f->tiles[ty][tx] = '&'; // выросло дерево
}

// ПОКРАСКА
void paint_cell(struct Field *f, char color) {
    if (!f->has_dino) return;
    f->colors[f->dino_y][f->dino_x] = color;
}


/// СРУБАЕМ ДЕРЕВО
void cut_dino(struct Field *f, char *dir) {
    if (!f->has_dino) return;

    int tx = f->dino_x;
    int ty = f->dino_y;

    if (strcmp(dir, "UP") == 0) ty--;
    else if (strcmp(dir, "DOWN") == 0) ty++;
    else if (strcmp(dir, "LEFT") == 0) tx--;
    else if (strcmp(dir, "RIGHT") == 0) tx++;

    if (tx < 0) tx = f->w - 1;
    if (tx >= f->w) tx = 0;
    if (ty < 0) ty = f->h - 1;
    if (ty >= f->h) ty = 0;

    if (f->tiles[ty][tx] == '&') f->tiles[ty][tx] = '_'; // срубили, клетка пустая
}

/// КАМЕНЬ
void make_dino(struct Field *f, char *dir) {
    if (!f->has_dino) return;

    int tx = f->dino_x;
    int ty = f->dino_y;

    if (strcmp(dir, "UP") == 0) ty--;
    else if (strcmp(dir, "DOWN") == 0) ty++;
    else if (strcmp(dir, "LEFT") == 0) tx--;
    else if (strcmp(dir, "RIGHT") == 0) tx++;

    if (tx < 0) tx = f->w - 1;
    if (tx >= f->w) tx = 0;
    if (ty < 0) ty = f->h - 1;
    if (ty >= f->h) ty = 0;

    if (f->tiles[ty][tx] == '_') f->tiles[ty][tx] = '@'; // вылупился камень и свалился с луны лунтику на голову
}

/// ПИНАЕМ КАМЕНЬ, ДИНО МАГЕЕЕЕЕЕТ
void push_dino(struct Field *f, char *dir) {
    if (!f->has_dino) return;

    int sx = f->dino_x;
    int sy = f->dino_y;
    int bx = sx;
    int by = sy;

    if (strcmp(dir, "UP") == 0) by--;
    else if (strcmp(dir, "DOWN") == 0) by++;
    else if (strcmp(dir, "LEFT") == 0) bx--;
    else if (strcmp(dir, "RIGHT") == 0) bx++;

    if (bx < 0) bx = f->w - 1;
    if (bx >= f->w) bx = 0;
    if (by < 0) by = f->h - 1;
    if (by >= f->h) by = 0;

    if (f->tiles[by][bx] != '@') return; // рядом камня нет

    // куда двигаем камень? противоположно динозавру
    int nx = bx, ny = by;
    if (strcmp(dir, "UP") == 0) ny--;
    else if (strcmp(dir, "DOWN") == 0) ny++;
    else if (strcmp(dir, "LEFT") == 0) nx--;
    else if (strcmp(dir, "RIGHT") == 0) nx++;

    if (nx < 0) nx = f->w - 1;
    if (nx >= f->w) nx = 0;
    if (ny < 0) ny = f->h - 1;
    if (ny >= f->h) ny = 0;

    // камень не может в гору или дерево
    if (f->tiles[ny][nx] == '^' || f->tiles[ny][nx] == '&' || f->tiles[ny][nx] == '@') return;

    // если попал в яму
    if (f->tiles[ny][nx] == '%') f->tiles[ny][nx] = '_';
    else f->tiles[ny][nx] = '@'; 
    f->tiles[by][bx] = '_';
}

    
///КОМАНДЫЫЫЫЫЫЫЫ
void Comands_din(char *line, struct Field *f) {
    char cmd[32];
    int p = 0;

    if (sscanf(line, "%s%n", cmd, &p) != 1) return;

    if (strcmp(cmd, "SIZE") == 0) {
    int w, h;

    sscanf(line + p, "%d %d", &w, &h);
    
    if (w < 10) w = 10;
    if (w > 100) w = 100;
    if (h < 10) h = 10;
    if (h > 100) h = 100;

    init_field(f, w, h);
}

    else if (strcmp(cmd, "START") == 0) {
        int x, y;
        sscanf(line + p, "%d %d", &x, &y);
        place_dino(f, x, y);
    }
    else if (strcmp(cmd, "MOVE") == 0) {
        char dir[16];
        sscanf(line + p, "%s", dir);
        move_dino(f, dir);
    }
    else if (strcmp(cmd, "PAINT") == 0) {
        char c;
        sscanf(line + p, " %c", &c);
        paint_cell(f, c);
    }
    else if (strcmp(cmd, "DIG") == 0) {
        char dir[16];
        sscanf(line + p, "%s", dir);
        dig_dino(f, dir);
    }
    else if (strcmp(cmd, "MOUND") == 0) {
        char dir[16];
        sscanf(line + p, "%s", dir);
        mound_dino(f, dir);
    }
    else if (strcmp(cmd, "JUMP") == 0) {
        char dir[16];
        int jum;
        sscanf(line + p, "%s %d", dir, &jum);
        jump_dino(f, dir, jum);
    }
    else if (strcmp(cmd, "GROW") == 0) {
        char dir[16];
        sscanf(line + p, "%s", dir);
        grow_dino(f, dir);
    }
    else if (strcmp(cmd, "CUT") == 0) {
        char dir[16];
        sscanf(line + p, "%s", dir);
        cut_dino(f, dir);
    }
    else if (strcmp(cmd, "MAKE") == 0) {
        char dir[16];
        sscanf(line + p, "%s", dir);
        make_dino(f, dir);
    }
    else if (strcmp(cmd, "PUSH") == 0) {
        char dir[16];
        sscanf(line + p, "%s", dir);
        push_dino(f, dir);
    }
}

// ================== MAIN ==================

int main(int argn, char *args[]) {

    if (argn < 2) {
        printf("Wrong arguments!\n");
        return 0;
    }

    FILE *file = fopen(args[1], "r");
    if (!file) {
        printf("Unable to open file %s\n", args[1]);
        return 0;
    }

    struct Field field = {0};
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        Comands_din(line, &field);
        print_field(&field);
        printf("\n");
    }

    for (int y = 0; y < field.h; y++) {
        free(field.tiles[y]);
        free(field.colors[y]);
    }
    free(field.tiles);
    free(field.colors);

    fclose(file);
    return 0;
}