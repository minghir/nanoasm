int adunare(int a, int b);

int main() {
    int x = 100;
    int y = 50;
    
    int rezultat = adunare(x, y);
    print(rezultat);
    
    return 0;
}

int adunare(int a, int b) {
    return a + b;
}