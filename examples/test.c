struct Point {
    int x;
    int y;
    char label;
    float weight;
    int buf[4];
};

union Data {
    int i;
    float f;
    char c;
};

struct Node {
    int value;
    struct Point pos;
    struct Node *next;
};

int global_var;
int array[10];
double pi;
int *ip;
struct Node *nodes[4];
int (*factorial_ptr)(int n);

int factorial(int n) {
    int result;
    if (n <= 1)
        result = 1;
    else
        result = n * factorial(n - 1);
    return result;
}

int main() {
    struct Point p;
    struct Node node;
    struct Node other;
    struct Node *np;
    union Data data;
    int i;
    int sum;
    int x;
    float ratio;
    double area;
    p.x = 0;
    p.y = 0;
    p.label = 65;
    p.weight = 1.5;
    p.buf[0] = 1;
    node.value = 10;
    node.pos.x = 3;
    node.pos.y = 4;
    np = &other;
    other.value = 20;
    node.next = np;
    node.next->value = 100;
    np->value = np->value + 1;
    ip = &x;
    *ip = 42;
    factorial_ptr = factorial;
    sum = factorial_ptr(4);
    nodes[0] = &other;
    nodes[0]->value = 7;
    nodes[1] = np;
    data.i = 42;
    data.f = 2.5;
    ratio = 0.5;
    area = ratio * 3.14;
    pi = 3.14159;
    sum = 0;
    for (i = 0; i < 5; i = i + 1) {
        switch (i) {
        case 0:
            sum = sum + p.x + node.pos.y + data.i + np->value
                + nodes[i]->value + *ip;
            break;
        case 1:
        case 2:
            sum = sum + p.y + p.buf[0] + i;
            break;
        default:
            sum = sum + array[i];
            break;
        }
        array[i] = i + 1;
        p.x = p.x + 1;
        p.weight = p.weight + ratio;
    }
    global_var = sum + p.x + p.y + node.value + data.c;
    return factorial(global_var);
}
