/* Simple C example for testing */

int sum(int a, int b) {
    return a + b;
}

int main() {
    int x;
    x = 5;
    int y;
    y = 10;
    int result;
    result = sum(x, y);
    
    if (result > 10) {
        result = result * 2;
    } else {
        result = result / 2;
    }
    
    return result;
} 