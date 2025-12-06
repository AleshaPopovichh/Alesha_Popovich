
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define ALPH 256

typedef struct Node {
    unsigned char c;
    uint64_t freq;
    struct Node *left, *right;
    struct Node *next; // для связного списка
} Node;


static Node* newNode(unsigned char c, uint64_t freq) {
    Node *n = (Node*)malloc(sizeof(Node));
    if (!n) { perror("malloc"); exit(EXIT_FAILURE); }
    n->c = c; n->freq = freq; n->left = n->right = n->next = NULL;
    return n;
}
static void freeTree(Node *r) {
    if (!r) return;
    freeTree(r->left);
    freeTree(r->right);
    free(r);
}


static void write_u64_le(FILE *f, uint64_t v) {
    uint8_t b[8];
    for (int i = 0; i < 8; ++i) b[i] = (uint8_t)((v >> (8*i)) & 0xFF);
    fwrite(b,1,8,f);
}
static uint64_t read_u64_le(FILE *f) {
    uint8_t b[8];
    if (fread(b,1,8,f) != 8) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= ((uint64_t)b[i]) << (8*i);
    return v;
}

/*Работа со списком*/
/* находит и извлекает узел с минимальной freq из списка head */
static Node* pop_min(Node **phead) {
    if (!phead || !*phead) return NULL;
    Node *head = *phead;
    Node *min = head, *min_prev = NULL;
    Node *prev = head, *cur = head->next;
    while (cur) {
        if (cur->freq < min->freq || (cur->freq == min->freq && cur->c < min->c)) {
            min_prev = prev;
            min = cur;
        }
        prev = cur; cur = cur->next;
    }
    if (!min_prev) *phead = min->next;
    else min_prev->next = min->next;
    min->next = NULL;
    return min;
}

/* Построение дерева Хаффмана из таблицы частот (без кучи) */
static Node* build_tree(uint64_t freq[ALPH]) {
    Node *head = NULL;
    int unique = 0;
    for (int i = 0; i < ALPH; ++i) if (freq[i] > 0) {
        Node *n = newNode((unsigned char)i, freq[i]);
        n->next = head; head = n;
        unique++;
    }
    if (!head) return NULL;
    if (!head->next) {
        // только один символ — создаем пустой родитель, чтобы дерево не было одиночным листом
        Node *only = pop_min(&head);
        Node *dummy = newNode(0, 0);
        Node *parent = newNode(0, only->freq);
        parent->left = only; parent->right = dummy;
        return parent;
    }
    while (head && head->next) {
        Node *a = pop_min(&head);
        Node *b = pop_min(&head);
        Node *p = newNode(0, a->freq + b->freq);
        // меньший вес — влево; при равенстве — по символу (стабильность)
        if (a->freq < b->freq || (a->freq == b->freq && a->c <= b->c)) { p->left = a; p->right = b; }
        else { p->left = b; p->right = a; }
        p->next = head; head = p;
    }
    Node *root = pop_min(&head);
    
    while (head) { Node *t = head; head = head->next; free(t); }
    return root;
}

/*  Генерация кодов (строки "0101...")  */
static void gen_codes(Node *r, char *buf, int depth, char *codes[ALPH]) {
    if (!r) return;
    if (!r->left && !r->right) {
        buf[depth] = '\0';
        codes[r->c] = strdup(buf);
        return;
    }
    if (r->left) {
        buf[depth] = '0';
        gen_codes(r->left, buf, depth+1, codes);
    }
    if (r->right) {
        buf[depth] = '1';
        gen_codes(r->right, buf, depth+1, codes);
    }
}

/*  Подсчёт частот  */
static uint64_t count_freq_file(const char *fname, uint64_t freq[ALPH]) {
    FILE *f = fopen(fname, "rb");
    if (!f) return 0;
    memset(freq, 0, sizeof(uint64_t)*ALPH);
    uint64_t total = 0;
    int c;
    while ((c = fgetc(f)) != EOF) { freq[(unsigned char)c]++; total++; }
    fclose(f);
    return total;
}

/*  Печать статистики  */
static void print_stats(uint64_t freq[ALPH], char *codes[ALPH], uint64_t original, long compressed) {
    uint64_t total = 0; int unique = 0;
    for (int i = 0; i < ALPH; ++i) if (freq[i]) { total += freq[i]; unique++; }
    if (total == 0) return;
    double entropy = 0.0, totalBits = 0.0;
    for (int i = 0; i < ALPH; ++i) {
        if (freq[i]) {
            double p = (double)freq[i] / (double)total;
            entropy -= p * log2(p);
            if (codes[i]) totalBits += freq[i] * strlen(codes[i]);
        }
    }
    double avg = total ? totalBits / total : 0.0;
    double eff = avg ? (entropy / avg) * 100.0 : 0.0;
    double ratio = original ? (double)compressed / (double)original : 0.0;
    printf("\n--- статистика ---\n");
    printf("исходный размер: %llu байт\n", (unsigned long long)original);
    printf("сжатый размер: %ld байт\n", compressed);
    printf("коэффициент сжатия (compressed/original): %.3f\n", ratio);
    printf("уникальных символов: %d\n", unique);
    printf("среднее бит/символ: %.3f\n", avg);
    printf("энтропия: %.3f бит/символ\n", entropy);
    printf("эффективность (энтропия/средн.длина): %.1f %%\n", eff);
    printf("-------------------\n\n");
}

/*  Кодирование файла  */
static int encode_file(const char *infile, const char *outfile) {
    uint64_t freq[ALPH];
    uint64_t original = count_freq_file(infile, freq);
    // откроем выход и запишем заголовок даже для пустого файла
    FILE *out = fopen(outfile, "wb");
    if (!out) { perror("fopen out"); return 0; }
    write_u64_le(out, original);
    // Записываем 256 частот (простая реализация — немного громоздко, но надёжно)
    for (int i = 0; i < ALPH; ++i) write_u64_le(out, freq[i]);

    if (original == 0) {
        fclose(out);
        printf("входной файл пуст — записан пустой архив %s\n", outfile);
        return 1;
    }

    Node *root = build_tree(freq);
    if (!root) { fprintf(stderr, "ошибка: не получилось построить дерево\n"); fclose(out); return 0; }

    char *codes[ALPH]; for (int i=0;i<ALPH;i++) codes[i]=NULL;
    char buf[1024];
    gen_codes(root, buf, 0, codes);

    // случай одного уникального символа: назначим код "0"
    int uniq = 0, only = -1;
    for (int i=0;i<ALPH;i++) if (freq[i]) { uniq++; only = i; }
    if (uniq == 1) {
        if (codes[only]) free(codes[only]);
        codes[only] = strdup("0");
    }

    // откроем вход и выпишем битовый поток
    FILE *in = fopen(infile, "rb");
    if (!in) { perror("fopen in"); freeTree(root); fclose(out); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 0; }

    unsigned char cur = 0; int filled = 0; long writtenBytes = 8 + ALPH*8; // учтём header bytes (approx)
    int ch;
    while ((ch = fgetc(in)) != EOF) {
        char *code = codes[(unsigned char)ch];
        if (!code) { fprintf(stderr, "internal error: no code for %d\n", ch); fclose(in); freeTree(root); fclose(out); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 0; }
        for (size_t k = 0; code[k]; ++k) {
            cur = (cur << 1) | (code[k] == '1');
            filled++;
            if (filled == 8) {
                fputc(cur, out); writtenBytes++;
                cur = 0; filled = 0;
            }
        }
    }
    if (filled > 0) {
        cur <<= (8 - filled);
        fputc(cur, out); writtenBytes++;
    }
    fclose(in);
    fclose(out);

    print_stats(freq, codes, original, (long)writtenBytes);

    // Автоматическая проверка: декодируем прямо из файла и сравним
    // (вместо отдельного временного файла — прочитанное сравнение в памяти)
    // Считаем декодированные байты и сравним с исходным файлом
    FILE *fenc = fopen(outfile, "rb");
    if (!fenc) { fprintf(stderr, "не удалось открыть сжатый файл для проверки\n"); freeTree(root); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 1; }
    uint64_t orig_check = read_u64_le(fenc);
    uint64_t freq_check[ALPH];
    for (int i=0;i<ALPH;i++) freq_check[i] = read_u64_le(fenc);
    Node *root_check = build_tree(freq_check);
    if (!root_check) { fclose(fenc); freeTree(root); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 1; }

    FILE *fin_orig = fopen(infile, "rb");
    if (!fin_orig) { fclose(fenc); freeTree(root_check); freeTree(root); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 1; }

    int ok = 1;
    Node *curNode = root_check;
    uint64_t decoded = 0;
    int byte_v;
    while (decoded < orig_check && (byte_v = fgetc(fenc)) != EOF) {
        unsigned char b = (unsigned char)byte_v;
        for (int k = 7; k >= 0 && decoded < orig_check; --k) {
            int bit = (b >> k) & 1;
            curNode = bit ? curNode->right : curNode->left;
            if (!curNode) { ok = 0; break; }
            if (!curNode->left && !curNode->right) {
                int orig_ch = fgetc(fin_orig);
                if (orig_ch == EOF || (unsigned char)orig_ch != curNode->c) { ok = 0; break; }
                decoded++;
                curNode = root_check;
            }
        }
        if (!ok) break;
    }
    if (decoded != orig_check) ok = 0;
    if (ok) printf("проверка восстановления: файлы совпадают (успех)\n");
    else printf("проверка восстановления: НЕ совпадают\n");

    fclose(fenc); fclose(fin_orig);
    freeTree(root); freeTree(root_check);
    for (int i=0;i<ALPH;i++) if (codes[i]) free(codes[i]);
    return 1;
}

/*  Декодирование файла  */
static int decode_file(const char *infile, const char *outfile) {
    FILE *in = fopen(infile, "rb");
    if (!in) { perror("fopen in"); return 0; }
    uint64_t original = read_u64_le(in);
    uint64_t freq[ALPH];
    for (int i = 0; i < ALPH; ++i) freq[i] = read_u64_le(in);

    if (original == 0) {
        // пустой исходный файл — создаём пустой выход
        FILE *out = fopen(outfile, "wb");
        if (!out) { perror("fopen out"); fclose(in); return 0; }
        fclose(out); fclose(in);
        printf("восстановлен пустой файл -> %s\n", outfile);
        return 1;
    }

    int uniq = 0, only = -1;
    for (int i=0;i<ALPH;i++) if (freq[i]) { uniq++; only = i; }
    if (uniq == 1) {
        FILE *out = fopen(outfile, "wb");
        if (!out) { perror("fopen out"); fclose(in); return 0; }
        for (uint64_t i=0;i<original;i++) fputc((unsigned char)only, out);
        fclose(out); fclose(in);
        printf("восстановлен файл (один символ) -> %s\n", outfile);
        return 1;
    }

    Node *root = build_tree(freq);
    if (!root) { fprintf(stderr, "не удалось построить дерево\n"); fclose(in); return 0; }

    FILE *out = fopen(outfile, "wb");
    if (!out) { perror("fopen out"); fclose(in); freeTree(root); return 0; }

    Node *cur = root;
    uint64_t written = 0;
    int byte_v;
    while (written < original && (byte_v = fgetc(in)) != EOF) {
        unsigned char b = (unsigned char)byte_v;
        for (int k = 7; k >= 0 && written < original; --k) {
            int bit = (b >> k) & 1;
            cur = bit ? cur->right : cur->left;
            if (!cur) { fprintf(stderr, "повреждённые данные/ошибка дерева\n"); fclose(in); fclose(out); freeTree(root); return 0; }
            if (!cur->left && !cur->right) {
                fputc(cur->c, out);
                written++;
                cur = root;
            }
        }
    }

    fclose(in); fclose(out); freeTree(root);
    printf("декодирование завершено -> %s (восстановлено байт: %llu из %llu)\n", outfile, (unsigned long long)written, (unsigned long long)original);
    return 1;
}

/*  Вспомогательные функции ввода */
static void strip_nl(char *s) {
    size_t n = strlen(s); if (n && s[n-1] == '\n') s[n-1] = '\0';
}
static void ask_fn(const char *prompt, char *buf, size_t sz, const char *def) {
    printf("%s (по умолчанию \"%s\"): ", prompt, def);
    if (!fgets(buf, (int)sz, stdin)) { strncpy(buf, def, sz-1); buf[sz-1] = '\0'; return; }
    strip_nl(buf);
    if (buf[0] == '\0') { strncpy(buf, def, sz-1); buf[sz-1] = '\0'; }
}


int main(int argc, char *argv[]) {
    #ifdef _WIN32
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
    #endif

    if (argc == 4) {
        if (strcmp(argv[1], "encode") == 0) {
            encode_file(argv[2], argv[3]);
            return 0;
        } else if (strcmp(argv[1], "decode") == 0) {
            decode_file(argv[2], argv[3]);
            return 0;
        } else {
            printf("Неверный режим. Используйте 'encode' или 'decode'.\n");
            return 1;
        }
    }

    // интерактивное меню
    printf("Хаффман\n");
    printf("1 - кодировать (encode)\n2 - декодировать (decode)\nВыберите режим (1/2): ");
    int choice = 0;
    if (scanf("%d", &choice) != 1) { fprintf(stderr, "некорректный ввод\n"); return 1; }
    int ch; while ((ch = getchar()) != EOF && ch != '\n') {} // очистка ввода

    if (choice == 1) {
        char infile[512], outfile[512];
        ask_fn("введите имя входного файла", infile, sizeof(infile), "input.txt");
        ask_fn("введите имя выходного (сжатый) файла", outfile, sizeof(outfile), "compressed.huf");
        if (!encode_file(infile, outfile)) fprintf(stderr, "кодирование завершилось с ошибкой\n");
        else {
            // автоматическая декод-проверка по желанию (предложение)
            printf("хотите автоматически декодировать и проверить восстановление? (y/n): ");
            char ans[8];
            if (fgets(ans, sizeof(ans), stdin)) {
                if (ans[0] == 'y' || ans[0] == 'Y') {
                    char restored[512];
                    ask_fn("введите имя для восстановленного файла", restored, sizeof(restored), "restored.txt");
                    if (!decode_file(outfile, restored)) fprintf(stderr, "автоматическое декодирование завершилось с ошибкой\n");
                    else {
                        // сравнение файлов
                        FILE *f1 = fopen(infile, "rb"), *f2 = fopen(restored, "rb");
                        if (!f1 || !f2) { if (f1) fclose(f1); if (f2) fclose(f2); printf("не удалось открыть файлы для сравнения\n"); }
                        else {
                            int ok = 1;
                            int a,b;
                            do { a = fgetc(f1); b = fgetc(f2); if (a != b) { ok = 0; break; } } while (a != EOF && b != EOF);
                            fclose(f1); fclose(f2);
                            if (ok) printf("проверка: файлы совпадают (успех)\n"); else printf("проверка: файлы НЕ совпадают\n");
                        }
                    }
                }
            }
        }
    } else if (choice == 2) {
        char infile[512], outfile[512];
        ask_fn("введите имя входного (сжатого) файла", infile, sizeof(infile), "compressed.huf");
        ask_fn("введите имя выходного (восстановленного) файла", outfile, sizeof(outfile), "restored.txt");
        if (!decode_file(infile, outfile)) fprintf(stderr, "декодирование не удалось\n");
        else {
            printf("хотите проверить совпадение с исходным файлом? (y/n): ");
            char ans[8];
            if (fgets(ans, sizeof(ans), stdin)) {
                if (ans[0] == 'y' || ans[0] == 'Y') {
                    char original_name[512];
                    ask_fn("введите имя исходного файла для сравнения", original_name, sizeof(original_name), "input.txt");
                    FILE *f1 = fopen(original_name, "rb"), *f2 = fopen(outfile, "rb");
                    if (!f1 || !f2) { if (f1) fclose(f1); if (f2) fclose(f2); printf("не удалось открыть файлы для сравнения\n"); }
                    else {
                        int ok = 1; int a,b;
                        do { a = fgetc(f1); b = fgetc(f2); if (a != b) { ok = 0; break; } } while (a != EOF && b != EOF);
                        fclose(f1); fclose(f2);
                        if (ok) printf("проверка: файлы совпадают (успех)\n"); else printf("проверка: файлы НЕ совпадают\n");
                    }
                }
            }
        }
    } else {
        printf("неверный выбор\n");
        return 1;
    }

    printf("=== программа завершена ===\n");
    return 0;
}
