/*
  huffman_lab2.c
  - режимы encode / decode (выбираются в меню)
  - encode: подсчёт частот, построение дерева (min-heap), генерация кодов, запись частот + закодированных бит
  - decode: чтение частот из файла, восстановление дерева, декодирование битового потока
  - вывод статистики, проверка совпадения исходного и восстановленного файлов
*/

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define ALPH 256
#define MAGIC "HUF2"   //4 bytes magic
#define MAX_CODE_LEN 1024
#define SYMBOLS 256

//little-endian read/write helpers (portable)
static void write_u64_le(FILE *f, uint64_t v) {
    uint8_t b[8];
    for (int i = 0; i < 8; ++i) b[i] = (uint8_t)((v >> (8*i)) & 0xFF);
    fwrite(b,1,8,f);
}
static int read_u64_le(FILE *f, uint64_t *out) {
    uint8_t b[8];
    if (fread(b,1,8,f) != 8) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= ((uint64_t)b[i]) << (8*i);
    *out = v;
    return 1;
}
static void write_u16_le(FILE *f, uint16_t v) {
    uint8_t b[2]; b[0] = (uint8_t)(v & 0xFF); b[1] = (uint8_t)((v>>8)&0xFF);
    fwrite(b,1,2,f);
}
static int read_u16_le(FILE *f, uint16_t *out) {
    uint8_t b[2];
    if (fread(b,1,2,f) != 2) return 0;
    *out = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    return 1;
}

//Huffman tree node
typedef struct Node {
    unsigned char sym;
    uint64_t freq;
    struct Node *left, *right;
} Node;

//Min-heap of Node
typedef struct {
    Node **a;
    int size;
    int cap;
} MinHeap;

static Node* node_create(unsigned char s, uint64_t f) {
    Node *n = (Node*)malloc(sizeof(Node));
    if (!n) { perror("malloc"); exit(EXIT_FAILURE); }
    n->sym = s; n->freq = f; n->left = n->right = NULL;
    return n;
}
static void heap_swap(Node **x, Node **y) { Node *t = *x; *x = *y; *y = t; }
static MinHeap* heap_create(int cap) {
    MinHeap *h = (MinHeap*)malloc(sizeof(MinHeap));
    if (!h) { perror("malloc"); exit(EXIT_FAILURE); }
    h->a = (Node**)malloc(sizeof(Node*) * cap);
    if (!h->a) { perror("malloc"); exit(EXIT_FAILURE); }
    h->size = 0; h->cap = cap;
    return h;
}
static void heap_push(MinHeap *h, Node *n) {
    if (h->size >= h->cap) {
        h->cap *= 2;
        h->a = (Node**)realloc(h->a, sizeof(Node*) * h->cap);
        if (!h->a) { perror("realloc"); exit(EXIT_FAILURE); }
    }
    int i = h->size++;
    h->a[i] = n;
    while (i > 0) {
        int p = (i - 1) / 2;
        //сравниваем по частоте, при равенстве — по символу для стабильности
        if (h->a[p]->freq < h->a[i]->freq) break;
        if (h->a[p]->freq == h->a[i]->freq && h->a[p]->sym <= h->a[i]->sym) break;
        heap_swap(&h->a[p], &h->a[i]);
        i = p;
    }
}
static Node* heap_pop(MinHeap *h) {
    if (h->size == 0) return NULL;
    Node *ret = h->a[0];
    h->size--;
    h->a[0] = h->a[h->size];
    int i = 0;
    while (1) {
        int l = 2*i + 1, r = 2*i + 2, smallest = i;
        if (l < h->size && (h->a[l]->freq < h->a[smallest]->freq ||
            (h->a[l]->freq == h->a[smallest]->freq && h->a[l]->sym < h->a[smallest]->sym))) smallest = l;
        if (r < h->size && (h->a[r]->freq < h->a[smallest]->freq ||
            (h->a[r]->freq == h->a[smallest]->freq && h->a[r]->sym < h->a[smallest]->sym))) smallest = r;
        if (smallest == i) break;
        heap_swap(&h->a[i], &h->a[smallest]);
        i = smallest;
    }
    return ret;
}
static void heap_free(MinHeap *h) { if (!h) return; free(h->a); free(h); }

//рекурсивно освобождаем дерево
static void free_tree(Node *r) {
    if (!r) return;
    free_tree(r->left);
    free_tree(r->right);
    free(r);
}

//строим дерево хаффмана из таблицы частот (min-heap)
static Node* build_tree(uint64_t freq[ALPH]) {
    MinHeap *h = heap_create(128);
    int unique = 0;
    for (int i = 0; i < ALPH; ++i) {
        if (freq[i] > 0) { heap_push(h, node_create((unsigned char)i, freq[i])); unique++; }
    }
    if (h->size == 0) { heap_free(h); return NULL; }
    if (h->size == 1) {
        //если только один символ — сделаем родителя, чтобы код был корректен
        Node *only = heap_pop(h);
        Node *dummy = node_create(0, 0);
        Node *parent = node_create(0, only->freq);
        parent->left = only;
        parent->right = dummy;
        heap_push(h, parent);
    }
    while (h->size > 1) {
        Node *a = heap_pop(h);
        Node *b = heap_pop(h);
        Node *p = node_create(0, a->freq + b->freq);
        //меньший вес — левый; при равенстве упорядочим по символу
        if (a->freq < b->freq || (a->freq == b->freq && a->sym <= b->sym)) {
            p->left = a; p->right = b;
        } else {
            p->left = b; p->right = a;
        }
        heap_push(h, p);
    }
    Node *root = heap_pop(h);
    heap_free(h);
    return root;
}

//простая strdup (без зависимости от POSIX)
static char* my_strdup(const char *s) {
    size_t n = strlen(s);
    char *p = (char*)malloc(n + 1);
    if (!p) { perror("malloc"); exit(EXIT_FAILURE); }
    memcpy(p, s, n+1);
    return p;
}

//рекурсивно генерируем коды строками "0101..."
static void gen_codes(Node *r, char *buf, int depth, char *codes[ALPH]) {
    if (!r) return;
    if (!r->left && !r->right) {
        buf[depth] = '\0';
        codes[r->sym] = my_strdup(buf);
        return;
    }
    if (r->left) {
        buf[depth] = '0';
        gen_codes(r->left, buf, depth + 1, codes);
    }
    if (r->right) {
        buf[depth] = '1';
        gen_codes(r->right, buf, depth + 1, codes);
    }
}

//подсчёт частот и возвращение размера
static uint64_t count_freq(const char *infile, uint64_t freq[ALPH]) {
    FILE *f = fopen(infile, "rb");
    if (!f) return 0;
    memset(freq, 0, sizeof(uint64_t) * ALPH);
    uint64_t total = 0;
    int c;
    while ((c = fgetc(f)) != EOF) { freq[(unsigned char)c]++; total++; }
    fclose(f);
    return total;
}

typedef struct {
    unsigned char s;
    unsigned long long f;
} SymbolFreq;

int compareFreqDesc(const void *a, const void *b){
    SymbolFreq *x = (SymbolFreq*)a;
    SymbolFreq *y = (SymbolFreq*)b;
    if(y->f > x->f) return 1;
    else if(y->f < x->f) return -1;
    return 0;
}

//печать подробной статистики
void print_stats(uint64_t freq[ALPH], char* codes[ALPH], uint64_t originalSize, long compressedSize) {
    unsigned long long totalSymbols = 0;
    int uniqueSymbols = 0;
    for (int i = 0; i < ALPH; ++i) {
        if (freq[i] > 0) { totalSymbols += freq[i]; uniqueSymbols++; }
    }
    if (totalSymbols == 0) return;

    double totalBits = 0.0, entropy = 0.0;
    for (int i = 0; i < ALPH; ++i) {
        if (freq[i] > 0) {
            double p = (double)freq[i] / (double)totalSymbols;
            int len = codes[i] ? (int)strlen(codes[i]) : 0;
            totalBits += (double)freq[i] * len;
            if (p > 0.0) entropy -= p * log2(p);
        }
    }

    double avgCodeLength = totalBits / (double)totalSymbols;
    double compressionRatio = (originalSize > 0 && compressedSize >= 0) ? (double)compressedSize / (double)originalSize : 0.0;
    double efficiency = (avgCodeLength > 0.0) ? (entropy / avgCodeLength) * 100.0 : 0.0;

    printf("\n--- статистика ---\n");
    printf("исходный размер: %llu байт\n", (unsigned long long)originalSize);
    printf("сжатый размер: %ld байт\n", compressedSize);
    printf("коэффициент сжатия (compressed/original): %.3f\n", compressionRatio);
    printf("уникальных символов: %d\n", uniqueSymbols);
    printf("суммарные биты (по кодам): %.0f\n", totalBits);
    printf("среднее бит/символ: %.3f\n", avgCodeLength);
    printf("энтропия: %.3f бит/символ\n", entropy);
    printf("эффективность (энтропия/средн.длина): %.1f %%\n\n", efficiency);

    // топ-10 (как у тебя)
    typedef struct { unsigned char s; uint64_t f; } SF;
    SF *arr = malloc(sizeof(SF) * ALPH);
    int cnt = 0;
    for (int i = 0; i < ALPH; ++i) if (freq[i] > 0) { arr[cnt].s = (unsigned char)i; arr[cnt].f = freq[i]; cnt++; }
    qsort(arr, cnt, sizeof(SF), compareFreqDesc); // compareFreqDesc можно оставить как у тебя, но перекомпилируй его под uint64

    printf("топ-10 символов:\n");
    printf("символ   частота   длина_кода   код\n");
    for (int i = 0; i < 10 && i < cnt; ++i) {
        unsigned char s = arr[i].s;
        const char *sname;
        static char tmp[16];
        if (s == ' ') sname = "SPACE";
        else if (s == '\n') sname = "LF";
        else if (s == '\t') sname = "TAB";
        else if (s >= 32 && s <= 126) { tmp[0] = s; tmp[1] = 0; sname = tmp; }
        else { snprintf(tmp, sizeof(tmp), "0x%02X", s); sname = tmp; }
        printf("%7s  %10llu  %10zu   %s\n", sname, (unsigned long long)arr[i].f, codes[s] ? strlen(codes[s]) : 0, codes[s] ? codes[s] : "");
    }
    free(arr);
    printf("-------------------\n");
}


//compare files byte-by-byte
static int files_equal(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb");
    FILE *fb = fopen(b, "rb");
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
    int ca, cb;
    do {
        ca = fgetc(fa);
        cb = fgetc(fb);
        if (ca != cb) { fclose(fa); fclose(fb); return 0; }
    } while (ca != EOF && cb != EOF);
    int res = (ca == cb);
    fclose(fa); fclose(fb);
    return res;
}

/*  encode: записываем файл формата
   "HUF2" (4 bytes)
   uint64 original_size  (8 bytes, le)
   uint16 unique_count   (2 bytes, le)
   затем для каждого уникального символа:
     uint8 symbol
     uint64 freq (8 bytes le)
   uint8 last_bits_count (0..7) (если 0 -> последний байт заполнен полностью)
   затем последовательность байт (битовый поток)
*/
static int encode_file(const char *infile, const char *outfile) {
    uint64_t freq[ALPH];
    uint64_t original = count_freq(infile, freq);
    if (original == 0) {
        //пустой файл: запишем header с original=0 и unique=0
        FILE *out = fopen(outfile, "wb");
        if (!out) { perror("fopen"); return 0; }
        fwrite(MAGIC,1,4,out);
        write_u64_le(out, 0);
        write_u16_le(out, 0);
        uint8_t last_bits = 0;
        fwrite(&last_bits,1,1,out);
        fclose(out);
        printf("закодирован пустой файл -> %s\n", outfile);
        return 1;
    }

    Node *root = build_tree(freq);
    if (!root) { fprintf(stderr, "не получилось построить дерево\n"); return 0; }

    char *codes[ALPH];
    for (int i = 0; i < ALPH; ++i) codes[i] = NULL;
    char buf[MAX_CODE_LEN];
    gen_codes(root, buf, 0, codes);
    // если у символа код пустой (случай одного уникального символа) — назначим \"0\"
    for (int i = 0; i < ALPH; ++i) if (freq[i] && (!codes[i] || codes[i][0] == '\0')) {
        if (codes[i]) free(codes[i]);
        codes[i] = my_strdup("0");
    }

    uint16_t unique = 0; for (int i = 0; i < ALPH; ++i) if (freq[i]) unique++;

    //соберём закодированные байты в память (подходит для лабораторной задачи)
    unsigned char *outbuf = NULL;
    size_t outcap = 0, outlen = 0;
    unsigned char curbyte = 0;
    int filled = 0; //bits in curbyte (0..7)
    FILE *fin = fopen(infile, "rb");
    if (!fin) { perror("fopen input"); free_tree(root); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 0; }
    int ch;
    while ((ch = fgetc(fin)) != EOF) {
        char *code = codes[(unsigned char)ch];
        if (!code) { fprintf(stderr, "internal error: no code\n"); fclose(fin); free_tree(root); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); free(outbuf); return 0; }
        for (size_t k = 0; code[k]; ++k) {
            curbyte = (curbyte << 1) | (code[k] == '1' ? 1 : 0);
            filled++;
            if (filled == 8) {
                if (outlen + 1 > outcap) { outcap = outcap ? outcap * 2 : 4096; outbuf = (unsigned char*)realloc(outbuf, outcap); if (!outbuf) { perror("realloc"); fclose(fin); free_tree(root); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 0; } }
                outbuf[outlen++] = curbyte;
                curbyte = 0; filled = 0;
            }
        }
    }
    if (filled > 0) {
        curbyte <<= (8 - filled);
        if (outlen + 1 > outcap) { outcap = outcap ? outcap * 2 : 4096; outbuf = (unsigned char*)realloc(outbuf, outcap); if (!outbuf) { perror("realloc2"); fclose(fin); free_tree(root); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 0; } }
        outbuf[outlen++] = curbyte;
    }
    fclose(fin);
    uint8_t last_bits = (filled > 0) ? (uint8_t)filled : 0;

    //записываем файл
    FILE *out = fopen(outfile, "wb");
    if (!out) { perror("fopen out"); free(outbuf); free_tree(root); for (int i=0;i<ALPH;i++) if(codes[i]) free(codes[i]); return 0; }
    fwrite(MAGIC,1,4,out);
    write_u64_le(out, original);
    write_u16_le(out, unique);
    for (int i = 0; i < ALPH; ++i) {
        if (freq[i]) {
            uint8_t s = (uint8_t)i;
            fwrite(&s,1,1,out);
            write_u64_le(out, freq[i]);
        }
    }
    fwrite(&last_bits,1,1,out);
    if (outlen > 0) fwrite(outbuf, 1, outlen, out);
    fclose(out);

    //печатаем статистику
    long compressed_size = (long)outlen + 4 + 8 + 2 + unique * (1 + 8) + 1;
    print_stats(freq, codes, original, compressed_size);

    //освобождаем
    for (int i = 0; i < ALPH; ++i) if (codes[i]) free(codes[i]);
    free(outbuf);
    free_tree(root);

    printf("кодирование завершено -> %s\n", outfile);
    return 1;
}

//decode: чтение той же структуры (freqs) и восстановление
static int decode_file(const char *infile, const char *outfile) {
    FILE *in = fopen(infile, "rb");
    if (!in) { fprintf(stderr, "ошибка открытия %s: %s\n", infile, strerror(errno)); return 0; }

    char magic[5] = {0};
    if (fread(magic,1,4,in) != 4) { fprintf(stderr, "файл слишком короткий или неверный формат\n"); fclose(in); return 0; }
    if (memcmp(magic, MAGIC, 4) != 0) { fprintf(stderr, "неверный формат файла (magic)\n"); fclose(in); return 0; }

    uint64_t original = 0;
    if (!read_u64_le(in, &original)) { fprintf(stderr, "ошибка чтения original\n"); fclose(in); return 0; }

    uint16_t unique = 0;
    if (!read_u16_le(in, &unique)) { fprintf(stderr, "ошибка чтения unique\n"); fclose(in); return 0; }

    uint64_t freq[ALPH];
    memset(freq, 0, sizeof(freq));
    for (uint16_t i = 0; i < unique; ++i) {
        uint8_t s; uint64_t f;
        if (fread(&s,1,1,in) != 1) { fprintf(stderr, "ошибка чтения symbol\n"); fclose(in); return 0; }
        if (!read_u64_le(in, &f)) { fprintf(stderr, "ошибка чтения freq\n"); fclose(in); return 0; }
        freq[s] = f;
    }

    uint8_t last_bits = 0;
    if (fread(&last_bits,1,1,in) != 1) { fprintf(stderr, "ошибка чтения last_bits\n"); fclose(in); return 0; }

    //определить количество оставшихся байт
    long data_pos = ftell(in);
    if (data_pos < 0) { perror("ftell"); fclose(in); return 0; }
    if (fseek(in, 0, SEEK_END) != 0) { perror("fseek"); fclose(in); return 0; }
    long endpos = ftell(in);
    if (endpos < 0) { perror("ftell end"); fclose(in); return 0; }
    long rem = endpos - data_pos;
    if (rem < 0) rem = 0;
    if (fseek(in, data_pos, SEEK_SET) != 0) { perror("fseek back"); fclose(in); return 0; }

    unsigned char *data = NULL;
    if (rem > 0) {
        data = (unsigned char*)malloc((size_t)rem);
        if (!data) { perror("malloc data"); fclose(in); return 0; }
        if (fread(data,1,rem,in) != (size_t)rem) { perror("fread data"); free(data); fclose(in); return 0; }
    }
    fclose(in);

    //пустой исходный файл
    if (original == 0) {
        FILE *out = fopen(outfile, "wb");
        if (!out) { perror("fopen out"); free(data); return 0; }
        fclose(out);
        free(data);
        printf("восстановлен пустой файл -> %s\n", outfile);
        return 1;
    }

    //если один уникальный символ — просто запишем его original раз
    int uniq_count = 0; int only_sym = -1;
    for (int i = 0; i < ALPH; ++i) if (freq[i]) { uniq_count++; only_sym = i; }
    if (uniq_count == 1) {
        FILE *out = fopen(outfile, "wb");
        if (!out) { perror("fopen out"); free(data); return 0; }
        for (uint64_t i = 0; i < original; ++i) fputc((unsigned char)only_sym, out);
        fclose(out);
        free(data);
        printf("восстановлен файл (один символ) -> %s\n", outfile);
        return 1;
    }

    Node *root = build_tree(freq);
    if (!root) { fprintf(stderr, "не удалось построить дерево при декодировании\n"); free(data); return 0; }

    FILE *out = fopen(outfile, "wb");
    if (!out) { perror("fopen out"); free(data); free_tree(root); return 0; }

    Node *cur = root;
    uint64_t written = 0;
    for (long i = 0; i < rem && written < original; ++i) {
        unsigned char b = data[i];
        int bits = 8;
        if (i == rem - 1 && last_bits > 0) bits = last_bits;
        for (int k = 0; k < bits && written < original; ++k) {
            int bit = (b & (1 << (7 - k))) ? 1 : 0;
            cur = bit ? cur->right : cur->left;
            if (!cur) { fprintf(stderr, "повреждённое дерево/данные при декодировании\n"); fclose(out); free(data); free_tree(root); return 0; }
            if (!cur->left && !cur->right) {
                fputc(cur->sym, out);
                written++;
                cur = root;
            }
        }
    }

    fclose(out);
    free(data);
    free_tree(root);

    printf("декодирование завершено -> %s (восстановлено байт: %llu из %llu)\n", outfile, (unsigned long long)written, (unsigned long long)original);
    return 1;
}

//Меню и основная логика
static void strip_newline(char *s) {
    size_t n = strlen(s);
    if (n > 0 && s[n-1] == '\n') s[n-1] = '\0';
}

// удобный ввод строки с prompt, если пусто — возвращает default_name
static void ask_filename(const char *prompt, char *buf, size_t bufsize, const char *default_name) {
    printf("%s (по умолчанию \"%s\"): ", prompt, default_name);
    if (!fgets(buf, (int)bufsize, stdin)) { strncpy(buf, default_name, bufsize-1); buf[bufsize-1] = '\0'; return; }
    strip_newline(buf);
    if (buf[0] == '\0') { strncpy(buf, default_name, bufsize-1); buf[bufsize-1] = '\0'; }
}

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    printf("=== Хаффман (вариант 2) ===\n");
    printf("1 - кодировать (encode)\n2 - декодировать (decode)\nВыберите режим (1/2): ");

    int choice = 0;
    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "некорректный ввод\n");
        return 1;
    }
    // съедаем остаток строки после scanf
    int ch; while ((ch = getchar()) != EOF && ch != '\n') {}

    if (choice == 1) {
        char infile[512], outfile[512];
        ask_filename("введите имя входного файла", infile, sizeof(infile), "input.txt");
        ask_filename("введите имя выходного (сжатый) файла", outfile, sizeof(outfile), "compressed.huf");

        printf("анализ файла и подсчёт частот...\n");
        uint64_t freq[ALPH];
        uint64_t original = count_freq(infile, freq);
        if (original == 0) {
            printf("входной файл пуст или не найден\n");
            // всё равно попытаемся записать пустой архив
        }
        // кодируем и пишем файл
        if (!encode_file(infile, outfile)) {
            fprintf(stderr, "кодирование завершилось с ошибкой\n");
            return 1;
        }

        // спросим, нужно ли автоматически декодировать и проверить
        printf("хотите автоматически декодировать и проверить восстановление? (y/n): ");
        char ans[8];
        if (!fgets(ans, sizeof(ans), stdin)) return 0;
        if (ans[0] == 'y' || ans[0] == 'Y') {
            char restored[512];
            ask_filename("введите имя для восстановленного файла", restored, sizeof(restored), "restored.txt");
            if (!decode_file(outfile, restored)) {
                fprintf(stderr, "автоматическое декодирование завершилось с ошибкой\n");
            } else {
                if (files_equal(infile, restored)) printf("проверка: файлы совпадают (успех)\n");
                else printf("проверка: файлы НЕ совпадают (ошибка)\n");
            }
        }
    }
    else if (choice == 2) {
        char infile[512], outfile[512];
        ask_filename("введите имя входного (сжатого) файла", infile, sizeof(infile), "compressed.huf");
        ask_filename("введите имя выходного (восстановленного) файла", outfile, sizeof(outfile), "restored.txt");

        if (!decode_file(infile, outfile)) {
            fprintf(stderr, "декодирование не удалось\n");
            return 1;
        }

        printf("хотите проверить совпадение с исходным файлом? (y/n): ");
        char ans[8];
        if (!fgets(ans, sizeof(ans), stdin)) return 0;
        if (ans[0] == 'y' || ans[0] == 'Y') {
            char original_name[512];
            ask_filename("введите имя исходного файла для сравнения", original_name, sizeof(original_name), "input.txt");
            if (files_equal(original_name, outfile)) printf("проверка: файлы совпадают (всё робит)\n");
            else printf("проверка: файлы НЕ совпадают (ошибка)\n");
        }
    }
    else {
        printf("неверный выбор\n");
        return 1;
    }

    printf("=== программа завершена ===\n");
    return 0;
}
