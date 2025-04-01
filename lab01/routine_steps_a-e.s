.global hashthestring
.p2align 1
.type hashthestring, %function
	
.data
values:
	.word 5, 12, 7, 6, 4, 11, 6, 3, 10, 23		//Table values
	
mov r1, #0					// Initialize counter (length = 0)
mov r3, #0					// Initialize the capital letters additive
mov r4, #0					// Initialize the lower case letters additive
strlength:
	push {r0}				// So that the starting address of the string gets preseved into memory
	LDRB r2, [r0], #1		// Load R0 into R1 and then Post-increment r0 
	CMP r2, #0				// Compare with null terminator
	BEQ end					// If zero, exit loop
	ADD r1, r1, #1			// Increment counter
							// (b,c)
	CMP r2, #41				// Compare with ASCII for 'A'
	BLT strlength			// If r2 < 65 go to strlength
	CMP r2, #5A				// Compare with ASCII for 'Z'
	BGT lower			    // If r2 > 5A go to strlength
	MLA r3, r2, #2, r3		// Add the ASCII of the letter multiplied by two
	B strlength
	
lower:
	CMP r2, #61				// Compare r2 with the 'a' ascii value.
	BLT strlength			// If  r2 < 61 then go back to strlength
	CMP r2, #7A				// Compare r2 with the 'z' ascii value
	BGT strlength			// If  r2 > 7A then go back to strlength
	RSB r5, r2, #97			// Subtract the ascii code from 97 (97 - r2)
	MLA r4, r5, r5, r4		// Add into the accumulator the output of the subtraction and square.
	B strlength
	
	
	
	
	
	
	
	
	
	
end:
	pop {r0}
	BX lr
	

hashthestring:
	.fnstart
		bl strlength
		cmp r0,
	
	.fnend
