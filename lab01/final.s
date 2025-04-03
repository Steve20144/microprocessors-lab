.global hashthestring
.global addandmod
.global fibonacci
.global xor
.p2align 1
.type hashthestring, %function
.type addandmod, %function
.type fibonacci, %function
.type xor, %function
.data
.global hashresult	 		//Make the symbol visible to the linker
.global xor_checksum
hashresult: .word 0			//Allocate 4 bytes initialized to 0
xor_checksum: .word 0 
values:
	.word 5, 12, 7, 6, 4, 11, 6, 3, 10, 23		//Table values
		
.text
hashthestring:
	.fnstart
	push {r4, r5, r6, r7, r8, lr}
	mov r1, #0				//Initialize length counter
	mov r3, #0				//Capital letters counter
	mov r4, #0				//Lowercase letters counter
	mov r5, r0				//Copy the pointer with the string to r5
	mov r7, #0				//Number accumulator
	
loop:
	ldrb r2, [r5], #1		//Load the string to r2 and increment pointer
	cmp r2, #0				//Check for null terminator
	beq end					//In case null terminator is reached, exit loop
	add r1, r1, #1			//Increment length counter
	
	//Check uppercase
	cmp r2, #'A'			//Compare the current character with 'A'
	blt check_numbers		//If lower than 'A' branch to check_numbers
	cmp r2, #'Z'			//Compare the current character with 'Z'
	bgt lower_case			//If greater than 'Z' branch to lower_case
	add r3, r3, r2, lsl #1 	//r3 += 2 * ASCII
	b loop
	
lower_case:
	cmp r2, #'a'			//Compare current character with 'a'
	blt check_numbers		//If lower than 'a' proceed to check_numbers
	cmp r2, #'z'			//Compare current character with 'z'
	bgt check_numbers		//If gretaer than 'z' proceed to check_numbers
	rsb r6, r2, #97			//r6 = 97 - ASCII
	mul r6, r6, r6			//Square the result
	add r4, r4, r6			//Add to lowercase additive (tbn accumulator)
	b loop
	
check_numbers:
	cmp r2, #'0'			//Compare current character with '0'
	blt loop				//If lower than '0' branch to loop
	cmp r2, #'9'			//Compare current character with '9'
	bgt loop				//If greater than '9' branch to loop
	sub r8, r2, #'0'		//Convert ASCII to digit
	ldr r6, =values			//Load base address of the array
	ldr r8, [r6, r8, lsl #2]	//Load values[r2] into r8. lsl because one word is 4 bytes so we need to offset by that.
	add r7, r7, r8			//Add the number to the numbers accumulator
	b loop

end:
	add r1, r1, r3			//Add the capital letters counter
	add r1, r1, r4			//Add the lowercase letters counter
	add r1, r1, r7			//Add the numbers accumulator
	ldr r3, =hashresult		//Load the hashresult into r3
	str r1, [r3]			//Store the hash into hashresult
	pop {r4, r5, r6, r7, r8, lr}
	bx lr
	.fnend
	
addandmod:
    .fnstart
    push {r4, r5, r6, r7, r8, lr}
    mov r6, #0                  // Initialize digit sum accumulator
    ldr r0, =hashresult         // Load address of hashresult
    ldr r0, [r0]                // Load value of hashresult into r0
	cmp r0, #9					// Compare loaded value with 9
	blt exit9					// If loaded value is less than 9 go to exit9
    mov r4, #10                 // Preserve divisor (10) in r4
    mov r8, #0                  // Temporary register for remainder

extract_digits:
    udiv r5, r0, r4             // r5 = r0 / 10 (quotient)
    mls r8, r4, r5, r0         // r8 = r0 % 10 (remainder)
    add r6, r6, r8             // Add digit to sum (r6 += remainder)
    mov r0, r5                 // Update r0 with quotient for next iteration
    cmp r0, #0                 // Check if quotient is 0
    bne extract_digits          // Loop until all digits processed

exit:
    mov r7, #7                 // Divisor for mod operation
    udiv r8, r6, r7            // r8 = sum / 7
    mls r0, r8, r7, r6         // r0 = sum % 7
    pop {r4, r5, r6, r7, r8, lr}
    bx lr
	
exit9:
	pop {r4, r5, r6, r7, r8, lr} //Restore registers
    bx lr						 // Branch back to caller with r0
    .fnend
	
	
fibonacci:
	.fnstart
	   push {lr}
	   // Base Case
	   // If n<=1, return
	   cmp r0, #1			//Compare r0 with 1
	   ble end_fib			//If it is equal or less than one exit via end_fib
	   
	   //Recursive Case
	   // fibonacci(n-1) + fibonacci(n-2)
	   push {r0}			//Push r0 in the stack
	   sub r0, r0, #1		
	   bl fibonacci
	   pop {r1}
	   push {r0}
	   sub r0, r1, #2
	   bl fibonacci
	   
	   pop {r1}
	   add r0, r0, r1
   
end_fib:
		pop {pc}
	.fnend

xor:
	.fnstart
	push {r4, r5, lr}		// Save register state
	mov r4, r0				// Place the string in r4
	mov r5, #0				// Initialize the XOR accumulator
	
xor_loop:
	ldrb r1, [r4], #1		// Load first string character and increment pointer
	cmp r1, #0				// Check for null terminator
	beq store_and_exit		// If null, exit
	eor r5, r5, r1			// Calculate XOR with character
	b xor_loop				// Continue for next character
	
store_and_exit:
	ldr r2, =xor_checksum	// Load memory address into r2
	str r5, [r2]			// Store result into r2
	pop {r4, r5, lr}		// Restore registers to previous state
	bx lr
	
	
	.fnend
	
