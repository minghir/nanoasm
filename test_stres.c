// test_stress.c - Test de stres pentru cc.c:
// QuickSort recursiv, cautare binara (iterativa + recursiva), pointeri si int**

void swap(int* a, int* b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int partition(int* arr, int low, int high) {
    int pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j] <= pivot) {
            i++;
            int tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
        }
    }

    int next_i = i + 1;
    int tmp2 = arr[next_i];
    arr[next_i] = arr[high];
    arr[high] = tmp2;
    return next_i;
}

void quicksort(int* arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }
}

int is_sorted(int* arr, int size) {
    for (int i = 0; i < size - 1; i++) {
        if (arr[i] > arr[i + 1]) {
            return 0;
        }
    }
    return 1;
}

int binary_search(int* arr, int size, int target) {
    int low = 0;
    int high = size - 1;

    while (low <= high) {
        int mid = (low + high) / 2;
        int val = arr[mid];

        if (val == target) {
            return mid;
        } else if (val < target) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

int binary_search_rec(int* arr, int low, int high, int target) {
    if (low > high) {
        return -1;
    }

    int mid = (low + high) / 2;
    int val = arr[mid];

    if (val == target) {
        return mid;
    }
    if (val > target) {
        return binary_search_rec(arr, low, mid - 1, target);
    }
    return binary_search_rec(arr, mid + 1, high, target);
}

void print_array(int* arr, int size) {
    for (int i = 0; i < size; i++) {
        print(arr[i]);
    }
}

int main() {
    int failures = 0;

    // 1. Test swap prin pointeri si dereferentiere dubla (int**)
    int x = 100;
    int y = -50;
    int* px = &x;
    int** ppx = &px;

    if (**ppx != 100) {
        print("EROARE: dereferentiere int** esuata!");
        failures++;
    }

    swap(&x, &y);
	//print(x);
    //print(y);
	
    if (x != -50 || y != 100) {
        print("EROARE: swap(&x, &y) esuat!");
        failures++;
    } else {
        print("1. Swap prin pointeri & int** OK!");
    }

    // 2. Initializare array nesortat (negative, duplicate, zero)
    int n = 12;
    int data[12];
    data[0] = 42;
    data[1] = -7;
    data[2] = 19;
    data[3] = 0;
    data[4] = 88;
    data[5] = -7;
    data[6] = 3;
    data[7] = 100;
    data[8] = 15;
    data[9] = -25;
    data[10] = 42;
    data[11] = 8;

    if (is_sorted(data, n) != 0) {
        print("EROARE: array-ul initial nu trebuia sa fie sortat!");
        failures++;
    }

    // 3. QuickSort recursiv
    quicksort(data, 0, n - 1);

    if (is_sorted(data, n) == 1) {
        print("2. QuickSort recursiv OK! Array sortat:");
        print_array(data, n);
    } else {
        print("EROARE: QuickSort nu a sortat corect!");
        failures++;
    }

    // 4. Acces prin pointer si aritmetica de adrese (pe octeti, 8 octeti/int)
    int* p_start = data;
    int* p_idx5 = data + 5 * 8;
    if (*p_start != -25 || *p_idx5 != 8) {
        print("EROARE: acces pointer pe array sortat!");
        failures++;
    } else {
        print("3. Acces pointer pe array (*p_start == -25, *(data+5*8) == 8) OK!");
    }

    // 5. Cautare binara iterativa (capete, mijloc si valori absente)
    int idx_min = binary_search(data, n, -25);
    int idx_max = binary_search(data, n, 100);
    int idx_mid = binary_search(data, n, 15);
    int idx_miss_low = binary_search(data, n, -100);
    int idx_miss_mid = binary_search(data, n, 50);
    int idx_miss_high = binary_search(data, n, 999);

    if (idx_min != 0 || idx_max != 11 || idx_mid != 6) {
        print("EROARE: binary_search pentru elemente existente!");
        failures++;
    }
    if (idx_miss_low != -1 || idx_miss_mid != -1 || idx_miss_high != -1) {
        print("EROARE: binary_search pentru elemente inexistente!");
        failures++;
    }
    if (failures == 0) {
        print("4. Binary search iterativ OK!");
    }

    // 6. Cautare binara recursiva (4 parametri: rdi, rsi, rdx, rcx)
    int rec1 = binary_search_rec(data, 0, n - 1, 88);
    int rec2 = binary_search_rec(data, 0, n - 1, 0);
    int rec_miss = binary_search_rec(data, 0, n - 1, 77);

    if (rec1 != 10 || rec2 != 3 || rec_miss != -1) {
        print("EROARE: binary_search_rec esuat!");
        failures++;
    } else {
        print("5. Binary search recursiv (4 parametri) OK!");
    }

    // Verdict final
    print("Numar total erori:");
    print(failures);
    if (failures == 0) {
        print("TOATE TESTELE DE STRES AU TRECUT!");
    }

    return 0;
}