int este_vocala(char c) {
    if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') {
        return 1;
    }
    return 0;
}

int main() {
    // 1. Test short-circuit pe pointer NULL (nu trebuie sa faca segfault!)
    char* p_null = 0;
    if (p_null != 0 && *p_null == 'A') {
        print("EROARE short-circuit!");
    } else {
        print("Short-circuit NULL OK!");
    }

    // 2. Test !, &&, || si apeluri de functii in conditii
    char text[8];
    text[0] = 'A'; text[1] = 'B'; text[2] = 'C'; text[3] = 'E'; text[4] = 'X'; text[5] = 0;

    int vocale = 0;
    int i = 0;
    while (text[i] != 0 && !(text[i] == 'X')) {
        if (este_vocala(text[i])) {
            vocale++;
        }
        i++;
    }
    print("Vocale inainte de X (asteptat 2):");
    print(vocale);

    // 3. Test break si continue in bucle imbricate (for + while)
    int suma = 0;
    for (int k = 1; k <= 10; k++) {
        if (k % 2 == 0) {
            continue; // sare peste numerele pare (executa k++ prin for_step)
        }
        if (k > 7) {
            break;    // opreste bucla la k = 9
        }
        suma = suma + k; // 1 + 3 + 5 + 7 = 16
    }
    print("Suma impare <= 7 (asteptat 16):");
    print(suma);

    // 4. Test conditie compusa cu float
    float temp = 36.6;
    if (temp >= 36.0 && temp <= 37.5) {
        print("Temperatura normala!");
    }

    return 0;
}