int factorial(int n);
int fibonacci(int n);

int main() {
    int n = 6;
    print("Factorial(6):");
    print(factorial(n));

    print("Fibonacci(8):");
    print(fibonacci(8));
    return 0;
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    int prev = factorial(n - 1);
    return n * prev;
}

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}