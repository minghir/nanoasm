int main() {
    int x = 25;
    int y = 3;
    
    // Prima falsă, a doua adevărată -> OR trebuie să intre în if
    if (x < 10 || y < 5) {
        print("OR a functionat: cel putin o conditie este adevarata!\n");
    } else {
        print("Eroare la OR!\n");
    }
}