int main(void) {
    int x = 100;
    int* p = &x;
    int** pp = &p;

    **pp = 500;
    print(x);    // 500
    print(*p);   // 500
    print(**pp); // 500

    print("------------\n");

    float f = 2.5;
    float* pf = &f;
    float** ppf = &pf;

    **ppf = **ppf + 5.0;
    print(f);      // 7.50
    print(**ppf);  // 7.50
}