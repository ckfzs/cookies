#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define max(a, b) ((a)>=(b)?(a):(b))
#define min(a, b) ((a)<=(b)?(a):(b))

typedef struct _PLC PLC;

struct _PLC                  // 我们使用(p, len, c)的格式来表示每个编码
{
    int p;                   // p   :  最长匹配时, 字典中匹配子串开始时的位置(相对cursor的偏移量)
    int len;                 // len :  最长匹配时, 缓冲区匹配子串的长度
    char c;                  // c   :  最长匹配时, 缓冲区匹配子串的下一个字符; 当不匹配时, 就是缓冲区的第一个字符(紧邻cursor)
    struct _PLC *next;       // next:  指向下一个编码
};

typedef struct               // 我们使用单链表的形式来记录整个压输结果
{
    PLC *head;               // 单链表的首节点
    PLC *tail;               // 为了方便在链表后追加编码, 我们记录单链表的尾节点
} EncodedLink;

typedef struct               // LZ77编码句柄: [>>>字典<<<] |(cursor) [>>>lookahead缓冲区<<<]
{
    EncodedLink link;        // 记录编码结果
    const char *data;        // 指向待编码的字符串
    int dic_size;            // 字典大小
    int la_buffer_size;      // lookahead缓冲区大小
    int cursor;              // cursor的当前位置
} LZ77;

/* 打印编码后的数据
 */
static void print_lz77_compressed_data(LZ77 *lz77)
{
    EncodedLink *link;
    PLC *current;
    if (lz77 == NULL)
        return;
    link = &(lz77->link);
    current = link->head;
    if (current == NULL)
        return;
    printf("LZ77 COMPRESSED RESULT:\r\n");
    while (current)
    {
        printf("(%d,%d,%c) ", current->p, current->len, current->c);
        current = current->next;
    }
    printf("\n");
}

/* 比较子串sub1和sub2是否相等
 * sub1为lookahead缓冲区中的子串, 而sub2为字典中的子串, 这两者长度可以不等
 */
static int substring_equal(const char *str, int sub1_begin, int sub1_end, int sub2_begin, int sub2_end)
{
    int sub1_len = sub1_end - sub1_begin + 1;
    int sub2_len = sub2_end - sub2_begin + 1;
    int repetition, last, i, ret;
    char *new_sub2, *cur;
    if (sub1_len <= 0 || sub2_len <= 0)
        return 0;

    /* 例子1: 字典"a(字典结束)",  缓冲区"(缓冲区起始)aaaa" 
     * 例子2: 字典aa, 缓冲区a
     * 例子3: 字典aa, 缓冲区aa
     * 注: 以上3个例子, 缓冲区的子串都是可以匹配字典子串的
     */
    repetition = sub1_len / sub2_len;
    last = sub1_len % sub2_len;
    new_sub2 = malloc(repetition * sub2_len + last);
    cur = new_sub2;
    for (i = 0; i < repetition; ++i)
    {
        memcpy(cur, str + sub2_begin, sub2_len);
        cur += sub2_len;
    }
    memcpy(cur, str + sub2_begin, last);
    ret = (strncmp(str + sub1_begin, new_sub2, sub1_len) == 0);
    free(new_sub2);
    return ret;
}

/* 求取当前cursor下的最大匹配
 */
static int longest_match(LZ77 *lz77)
{
    int cursor = lz77->cursor;
    int length = strlen(lz77->data);
    int p = -1, l = -1;
    char c = '\0';
    int i, j;
    PLC *plc;
    int dic_start = max(0, cursor - lz77->dic_size + 1);
    int end_buffer = min(cursor + lz77->la_buffer_size, length - 1);
    
    for (i = cursor + 1; i <= end_buffer; ++i)
    {   
        // substring: data[cursor + 1, i]
        // find substring in dictionary
        for (j = dic_start; j <= cursor; ++j)
        {
            if (substring_equal(lz77->data, cursor + 1, i, j, cursor) && i - cursor > l) 
            {
                p = cursor - j + 1;
                l = i - cursor;
                if (i + 1 < length)
                    c = lz77->data[i + 1];
                else
                    c = '\0';
            }
        }
    }
    if (p == -1 && l == -1)
    {
        p = 0;
        l = 0;
        if (cursor + 1 < length)
            c = lz77->data[cursor + 1];
    }
    plc = malloc(sizeof(PLC));
    plc->p = p;
    plc->len = l;
    plc->c = c;
    plc->next = NULL;
    if (lz77->link.head == NULL)
    {
        lz77->link.head = plc;
        lz77->link.tail = plc;
    } else
    {
        lz77->link.tail->next = plc;
        lz77->link.tail = plc;
    }

    return l;
}

/* 释放压缩结果在堆区占用的内存
 */
static void lz77_free(LZ77 *lz77)
{
    PLC *cur, *next;
    if (lz77 == NULL)
        return;
    
    cur = lz77->link.head;
    while (cur)
    {
        next = cur->next;
        free(cur);
        cur = next;
    }
}

/* LZ77压缩算法
 */
void lz77_compress(const char *str, int dic_size, int buffer_size)
{
    LZ77 lz77;
    int length, match_len;
    length = strlen(str);
    if (length <= 0)
    {
        printf("LZ77 input string cannot be empty!\r\n");
        return;
    }
    if (dic_size <= 0 || buffer_size <=0)
    {
        printf("LZ77 dictionary or buffer size cannot less than 0!\r\n");
        return;
    }
    // 初始化LZ77编码句柄
    lz77.data = str;
    lz77.dic_size = dic_size;
    lz77.la_buffer_size = buffer_size;
    lz77.cursor = -1;
    lz77.link.head = NULL;
    lz77.link.tail = NULL;
    // 当cursor没有移到待编码字符串的结尾时
    while (lz77.cursor < length - 1)
    {
        // 求取当前cursor下的最大匹配
        match_len = longest_match(&lz77);
        // 移动cursor
        lz77.cursor += match_len + 1;
    }
    // 打印最终结果
    print_lz77_compressed_data(&lz77);
    lz77_free(&lz77);
}

int main(int argc, char *argv[])
{
    int dic_size, buffer_size;
    const char *data;

    if (argc != 4)
    {
        printf("Usage: lz77 <encode-string> <dictionary-size> <lookahead-buffer-size>\r\n");
        exit(1);
    } 
    data = argv[1];
    dic_size = atoi(argv[2]);
    buffer_size = atoi(argv[3]);   
    lz77_compress(data, dic_size, buffer_size);
}
