#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct _HuffmanTreeNode HuffmanTreeNode, *HuffmanTree;

struct _HuffmanTreeNode                         // 哈夫曼树结点定义
{
    char ch;                                    // 结点字符
    int weight;                                 // 字符出现的次数
    HuffmanTreeNode *parent;                    // 父结点
    HuffmanTreeNode *lchild, *rchild;           // 左孩子, 右孩子
    int intree;                                 // 标志位: 结点是否已插入树中
    int virtual;                                // 标志位: 字符在字典中的结点都不是虚拟结点, 因此除了叶子结点(或者根节点)以外的结点都是虚拟结点
    char *code;                                 // 哈夫曼编码
    HuffmanTreeNode *next, *prev;               // 我们通过一个双向链表来记录字典, next指向后一个结点, prev指向前一个结点
};

typedef struct                                  // 哈夫曼编码句柄定义
{
    const char *data;                           // 待编码的字符串
    int ch_num;                                 // 统计字符串中的不同字符数量
    HuffmanTree tree;                           // 哈夫曼树根结点
    HuffmanTreeNode *head, *tail;               // 记录字典的双向链表的首节点和尾节点
} HuffmanCode;

/* 查找字符是否已在字典中
 * param hfc: 哈夫曼编码句柄
 *        ch: 待查找的字符
 * return   : 如果在字典中, 返回对应的结点; 否则返回NULL
 */
static HuffmanTreeNode *find_char_in_nodes(HuffmanCode *hfc, char ch)
{
    HuffmanTreeNode *ret = NULL;
    int i;
    if (hfc && hfc->head)
    {
        ret = hfc->head;
        while (ret)
        {
            if (ret->ch == ch)
                return ret;
            ret = ret->next;
        }
    }
    return ret;
}

/* 哈夫曼编码初始化工作, 包括统计字符出现的次数和字典
 * param           hfc: 哈夫曼编码句柄
 *       coding_string: 待编码的字符串
 */
static void huffmancode_init(HuffmanCode *hfc, const char *coding_string)
{
    int ch_num = 0, i;
    HuffmanTreeNode *node = NULL;
    hfc->data = coding_string;
    hfc->ch_num = 0;
    hfc->tree = hfc->head = hfc->tail = NULL;

    // 统计字符出现的次数和字典
    for (i = 0; i < strlen(coding_string); ++i)
    {
        node = find_char_in_nodes(hfc, coding_string[i]);
        if (node)
        {// 如果字符在字典中
            
            // 其对应结点的weight加1
            node->weight += 1;
        }
        else
        {// 如果字符不在字典中
            
            // 出现新的字符, ch_num加1
            hfc->ch_num += 1;
            // 为该字符创建新的结点
            node = (HuffmanTreeNode *)malloc(sizeof(HuffmanTreeNode));
            node->ch = coding_string[i];
            node->weight = 1;
            node->parent = node->lchild = node->rchild = node->next = node->prev = NULL;
            node->code = NULL;
            // 统计阶段, 结点未插入树中
            node->intree = 0;
            // 我们可以理解为需要输出编码的结点就不是虚拟结点
            node->virtual = 0;
            // 将结点插入双向链表
            if (hfc->tail)
            {
                hfc->tail->next = node;
                node->prev = hfc->tail;
                hfc->tail = node;
            } else
            {
                hfc->head = hfc->tail = node;
            }
        }
    }
}

/* 查找weight最小的2个结点
 * param   hfc: 哈夫曼编码句柄
 *       node1: 用于返回weight最小的结点
 *       node2: 用于返回weight次小的结点
 */
static void find_minimum_two_nodes(HuffmanCode *hfc, HuffmanTreeNode **node1, HuffmanTreeNode **node2)
{
    HuffmanTreeNode *node;
    // min用于记录最小的weight, mor用于记录次小的weight
    int min = INT_MAX, mor = INT_MAX;
    *node1 = *node2 = NULL;
    if (hfc == NULL || hfc->head == NULL)
        return;

    node = hfc->head;
    // 遍历双向链表
    while (node)
    {
        // 如果intree为1, 说明结点已被插入树中, 那么就无需进行比较
        if(!node->intree)
        {
            if (node->weight < min)
            {// 如果当前结点的weight小于min
                
                // 说明当前结点的weight是当前的最小的weight,
                // 而当前的min记录的则是次小weight
                mor = min;
                *node2 = *node1;
                min = node->weight;
                *node1 = node;
            }
            else if (node->weight < mor)
            {// 如果当前结点的weight不小于min且小于mor, 即处于[min, mor)
        
                // 说明当前结点的weight是次小weight
                mor = node->weight;
                *node2 = node;
            }
        }
        node = node->next;
    }
}

/* 构造哈夫曼树
 * param hfc: 哈夫曼编码句柄
 */
static void build_huffman_tree(HuffmanCode *hfc)
{
    HuffmanTreeNode *node1 = NULL, *node2 = NULL, *node;
    do
    {
        // 从双向链表字典中找出weight最小且不在树中的2个结点node1和node2
        find_minimum_two_nodes(hfc, &node1, &node2);
        if (node1 && !node2)
        {// 如果node1不为空且node2为空, 注意是不会出现node1为空且node2不为空的情况的

            // 说明哈夫曼树构造完毕
            // node1即为哈夫曼树的根结点
            node1->intree = 1;
            hfc->tree = node1;
            break; 
        }
        else if(node1 && node2)
        {// 如果node1不为空且node2不为空
            
            // 将2个结点作为左右子树来构造一棵新树
            // 为该新树创建根结点
            node = (HuffmanTreeNode *)malloc(sizeof(HuffmanTreeNode));
            // 新树的根结点的weight等于node1和node2的weight之和
            node->weight = node1->weight + node2->weight;
            node->ch = -1;
            // 新树的根结点当前未插入哈夫曼树
            node->intree = 0;
            // 新树的根结点为虚拟结点
            node->virtual = 1;
            node->parent = NULL;
            // 新树的根结点的左孩子为node1
            node->lchild = node1;
            // 新树的根结点的右孩子为node2
            node->rchild = node2;
            node->code = NULL;
            node->next = node->prev = NULL;
            
            // node1和node2的父亲都为新树的根结点
            node1->parent = node;
            node2->parent = node;
            // node1和node2都已被插入树中
            node1->intree = 1;
            node2->intree = 1;
            // 将新树的根结点插入双向链表
            hfc->tail->next = node;
            node->prev = hfc->tail;
            hfc->tail = node;
        }
    } while (node1 && node2);
}

/* 为结点填充哈夫曼编码
 * param node: 待填充编码的结点
 */
static void fill_huffmancode(HuffmanTreeNode *node)
{
    char *code;
    int len;
    if (node == NULL)
        return;

    if (node->parent == NULL)
    {// node的父结点为空, 说明node为哈夫曼树的根结点

        // 其编码为空
        node->code = NULL;
    }
    else
    {// node的父结点不为空

        // len用于记录父结点编码的长度
        len = 0;
        if (node->parent->code)
            len = strlen(node->parent->code);
        // node的编码为其父结点编码+0/1, 还有一个结束符, 说明需要为node的编码申请len+2字节的缓冲区
        code = malloc(len + 2);
        if (len)
            // 如果len不为0, 那么就将父结点的编码拷贝至node的编码缓冲区
            memcpy(code, node->parent->code, len);
        // 如果node为其父的左孩子, 那么编码缓冲区后追加0, 反之追加1
        if (node == node->parent->lchild)
            code[len] = '0';
        else
            code[len] = '1';
        // 最后一个字节存放结束符
        code[len + 1] = '\0';
        // 记录node的编码
        node->code = code;
    }
    // 为node的左孩子和右孩子分别填充哈夫曼编码
    fill_huffmancode(node->lchild);
    fill_huffmancode(node->rchild);
}

/* 打印哈夫曼编码结果
 * param hfc: 哈夫曼编码句柄
 */
static void print_huffman_code(HuffmanCode *hfc)
{
    HuffmanTreeNode *current;
    if (hfc == NULL || hfc->head == NULL)
        return;
    printf("Huffman Code Result:\r\n");
    current = hfc->head;
    // 因为虚拟结点是在构造哈夫曼树的过程中插入双向链表的
    // 因此当遇到第一个虚拟结点的时候, 就表明字符的编码均已打印完毕
    while (current && !current->virtual)
    {
        printf("\t%c->weight:%d, code:%s\r\n", current->ch, current->weight, current->code);
        current = current->next;
    }
}

/* 释放哈夫曼编码过程中申请的内存空间
 * param hfc: 哈夫曼编码句柄
 */
static void huffman_free(HuffmanCode *hfc)
{
    HuffmanTreeNode *current, *next;
    if (hfc == NULL || hfc->head == NULL)
        return;

    current = hfc->head;
    while (current)
    {
        if (current->code)
            free(current->code);
        next = current->next;
        free(current);
        current = next;
    }   
}

/* 哈夫曼编码函数
 * param coding_string: 待编码的字符串
 */
void huffman_coding(const char *coding_string)
{
    HuffmanCode coder;
    huffmancode_init(&coder, coding_string);
    build_huffman_tree(&coder);
    fill_huffmancode(coder.tree);
    print_huffman_code(&coder);
    huffman_free(&coder);
}

int main(int argc, char *argv[])
{
    const char *input;

    if (argc != 2)
    {
        printf("Usage: huffman-coder <coding-string>\r\n");
        exit(1);
    }

    input = argv[1];
    huffman_coding(input);
}
