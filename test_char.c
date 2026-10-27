int main(void) {
    char c = 'A';
    char* ch = &c;

    print(ch);   // Afișează adresa
    print("------------\n");
    print(*ch);  // Afișează 'A'

    *ch = 'Z';
    print(c);    // Afișează 'Z'
    print(*ch);  // Afișează 'Z'
}