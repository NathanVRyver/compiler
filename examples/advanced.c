/* Advanced C example to test compiler features */

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int sum(int a, int b) {
    return a + b;
}

int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

int main() {
    int i = 0;
    int result = 0;
    
    // Test for loop
    for (i = 0; i < 5; i = i + 1) {
        result = result + i;
    }
    
    // Test while loop
    i = 5;
    while (i > 0) {
        result = result + i;
        i = i - 1;
    }
    
    // Test if-else
    if (result > 20) {
        result = result * 2;
    } else {
        result = result / 2;
    }
    
    // Test function calls
    int fib5 = fibonacci(5);
    int total = sum(result, fib5);
    int largest = max(result, fib5);
    
    return total + largest;
} 