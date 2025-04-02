.global fibonacci
.p2align 1
.type fibonacci, %function

.text
fibonacci:
    .fnstart
    push {lr}		//push lr of previous call
       // Base Case
       // If n<=1, return n
       cmp r0, #1            //Compare r0 with 1
       ble end_fib           //If r0<=1, then exit via end_fib

       //Recursive Case
       // fibonacci(n-1) + fibonacci(n-2)
       push {r0}            //push r0 (argument, n) in the stack
       sub r0, r0, #1		//r0=n-1
       bl fibonacci			//calls fibonacci(n-1)
       
       //if you are here, it means end_fib was called	//r0<=1	//r0=fibonacci(n-1)
       pop {r1}				//pop r0 to r1 //r1=n
       push {r0}			//push r0 (output of previous call, <=1, fibonacci(n-1))

       sub r0, r1, #2		//r0=n-2
       bl fibonacci			//calls fibonacci(n-2)

       //if you are here, it means end_fib was called	//r0<=1	//r0=fibonacci(n-2)
       pop {r1}				//pop r0 to r1	//r1=fibonacci(n-1)
       add r0, r0, r1		//r0=r0+r1		//r0= fibonacci(n-2) + fibonacci(n-1)

end_fib:
        pop {pc}
        //same as:
			// pop {lr}
			// BX lr
    .fnend
