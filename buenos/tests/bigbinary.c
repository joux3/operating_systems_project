char block[40000];

int main(void)
{
    int i = 0;
    while(1) {
        block[i] = 'a';
        i = (i + 1) % 40000; 
    }
    return 0;
}
