int main() {
    int x = 5;
    int y = 10;
    
    // Ambele adevărate -> intră în if
    if (x < 10 && y > 5) {
        print("Ambele conditii sunt adevarate!\n");
    }else{
		print("Aiurea!\n");
	}
    
    // Prima falsă -> face short-circuit și intră în else
    if (x > 20 && y > 5) {
        print("Eroare!\n");
    } else {
        print("Short-circuit a functionat: prima fiind falsa, a srit direct aici!\n");
    }
}