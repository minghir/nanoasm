int main() {
    char *s = "Mimi";
    print(s[0]); 

    s[0] = 'N';  
    print(s);    

    for(int i=0; i<5; ++i) { 
        print(s[i]); 
    }
    
    return 0;
}
