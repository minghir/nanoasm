int main() {
    int x = 42;
    int y = x + 18;
    
    print("Testam logica compilatorului...\n");
    
    if (y >= 60) {
        print("MIRACOL! Matematica a functionat corect!\n");
        print(y);
    }else{
		print("Y nu e 60!\n");
	}
    
    if (y <= 100) {
		print("Sub 100\n");
		if(y == 61){
			print("Exact 60\n");
		}else{
			print("nu e Exact 60\n");
		}
    }else{
		print("Nu e 100!\n");
	}
}