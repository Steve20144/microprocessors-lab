#include <stdio.h>
#include <stdint.h>

extern int hashthestring(const char *str);
extern int addandmod(const int answer);
extern uint32_t hashresult;
extern int fibonacci(const int previousresult);

int main() {
    const char *test_str = "A9b3";
    hashthestring("A9b3");
		int result2 = addandmod(hashresult);
		int result3 = fibonacci(result2);
    printf("Hash value for '%s': %d\n", test_str, hashresult);
		printf("Sum and mod for '%s': %d\n", test_str, result2);
		printf("Fibonacci result for '%s': %d\n", test_str, result3);
    return 0;
}
