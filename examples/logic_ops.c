int main()
{
    int a = 1;
    int b = 0;
    int c = 1;
    if (a && b)
        return 1;
    if (a || b)
        c = c + 1;
    if (!b)
        return c;
    return 0;
}
