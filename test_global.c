int apeluri = 0;
int baza = 100;
float factor = 2.5;
int date[4];
char mesaj[16];

int suma_vector(int *v, int n) {
    apeluri++;
    int s = 0;
    for (int i = 0; i < n; i++) {
        s = s + v[i];
    }
    return s;
}

void test_umbrire(void) {
    int baza = 5; // Variabilă locală care umbrește globalul `baza`
    print("Local baza:");
    print(baza);
}

int main(void) {
    date[0] = 10;
    date[1] = 20;
    date[2] = 30;
    date[3] = 40;

    mesaj[0] = 'O';
    mesaj[1] = 'K';
    mesaj[2] = '!';
    mesaj[3] = 0;

    int total = suma_vector(date, 4) + baza;
    print(mesaj);
    print("Total (100 + 100):");
    print(total);

    test_umbrire();
    print("Global baza neschimbat:");
    print(baza);

    print("Apeluri functie:");
    print(apeluri);

    print("Global float:");
    print(factor);
    return 0;
}