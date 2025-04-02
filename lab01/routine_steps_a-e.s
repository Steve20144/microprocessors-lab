.global hashthestring
.p2align 1
.type hashthestring, %function
	
.data
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
	rsb r6, r6, #97			//r6 = 97 - ASCII
	mul r6, r6, r6			//Square the result
	add r4, r4, r6			//Add to lowercase additive (tbn accumulator)
	b loop
	
check_numbers:
	cmp r2, #'0'			//Compare current character with '0'
	blt loop				//If lower than '0' branch to loop
	cmp r2, #'9'			//Compare current character with '9'
	bgt loop				//If greater than '9' branch to loop
	ldr r8, =values			//Load base address of the array
	ldr r8, [r8, r2, lsl #2]	//Load values[r2] into r8. lsl because one word is 4 bytes so we need to offset by that.
	add r7, r7, r8			//Add the number to the numbers accumulator
	b loop
	

	
	.fnend

	
