int main(int argc, char **argv) {
    int illegal;
    argc = argc;
    argv = argv;
    illegal = *(int*)0xcafebabe;
    illegal = illegal;
    return 5;
}
