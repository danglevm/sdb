int main() {
    /* empty loops are undefined behavior in C++ 17, so use volatile to make it safe */
    volatile int i;
    while (true) i = 42;
}