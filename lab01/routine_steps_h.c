int fibonacci(int n)	//n<=9
{
	if (n == 0)
	// CMP r0, #0
	// BEQ exit1
		return 0;
	else if (n == 1)
	// CMP r0, #1
	// BEQ exit2
		return 1;
	else
		return fibonacci(n-1) + fibonacci(n-2);
	// ...

	// exit1: MOV r0, #0
	// BX lr
	// exit2: MOV r0, #1
	// BX lr
}
