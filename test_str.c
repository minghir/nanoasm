void afiseaza_linie(char* eticheta, int valoare);

void main(void) {
    afiseaza_linie("Scor:", 42);
    afiseaza_linie("Stop:", -1);
}

void afiseaza_linie(char* eticheta, int valoare) {
    if (valoare < 0) {
        print("Valoare invalida!");
        return;
    }
    print(eticheta);
    print(valoare);
}