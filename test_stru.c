struct Node {
    int val;
    struct Node* next;
};

int sum_list(struct Node* head) {
    int total = 0;
    struct Node* cur = head;
    while (cur != 0) {
        total = total + cur->val;
        cur = cur->next;
    }
    return total;
}

void scale_array(struct Node arr[], int count, int factor) {
    for (int i = 0; i < count; i++) {
        arr[i].val = arr[i].val * factor;
    }
}

int main() {
    struct Node nodes[4];

    for (int i = 0; i < 4; i++) {
        nodes[i].val = (i + 1) * 10;
        if (i < 3) {
            nodes[i].next = &nodes[i + 1];
        } else {
            nodes[i].next = 0;
        }
    }

    scale_array(nodes, 4, 2);

    print("Elemente dupa scale_array(nodes, 4, 2):");
    print(nodes[0].val);
    print(nodes[1].val);
    print(nodes[2].val);
    print(nodes[3].val);

    print("Parcurgere prin pointeri (nodes[0].next->next->val):");
    print(nodes[0].next->next->val);

    print("Suma parcursa prin lista inlantuita:");
    print(sum_list(&nodes[0]));

    return 0;
}