struct Node {
    int val;
    struct Node* next;
};

struct Node* push_front(struct Node* head, int v) {
    struct Node* n = (struct Node*)malloc(sizeof(struct Node));
    n->val = v;
    n->next = head;
    return n;
}

int sum_and_free(struct Node* head) {
    int total = 0;
    struct Node* cur = head;
    while (cur != 0) {
        total = total + cur->val;
        struct Node* tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    return total;
}

int main() {
    print("sizeof(struct Node):");
    print(sizeof(struct Node));

    print("sizeof(struct Node*):");
    print(sizeof(struct Node*));

    struct Node* list = 0;
    for (int i = 1; i <= 5; i++) {
        list = push_front(list, i * 10);
    }

    print("Capul listei (ultimul inserat):");
    print(list->val);

    print("Suma si eliberare cu free():");
    print(sum_and_free(list));

    // Verificăm coalescing + reutilizare după free()
    struct Node* reused = malloc(sizeof(struct Node));
    reused->val = 999;
    reused->next = 0;
    print("Nod realocat dupa free():");
    print(reused->val);
    free(reused);

    return 0;
}