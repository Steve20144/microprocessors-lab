#include <stdio.h>
#include <uart.h>

int sum_mod7(int hash){
	int sum = 0;
	if (hash>9){
		//creating the sum
		while (hash!=0){		//to prevent further calculations after once the hash becomes a single digit
			sum = sum + hash % 10;	//adds to the sum the last digit of the hash
			hash = hash / 10;	//removes the last digit of the hash (the one next to the last becomes last)
			//Assembly-related:
			// hash % 10 = hash - [10 * (hash / 10)]	//hash / 10 is an integer here, omitting any decimal points
			
			// hash / 10 = hash * [1/(2*5)] = hash * 4/5 * 1/2^2 * 1/2 = hash * 4/5 * 1/2^3
			
			// 4/5 will be a hex number [(2^32 / 10) + 1]
			// 1/2^3 is LSLS (Logical Shift Left) 3 times
		}
		//modding by 7
		sum = sum % 7;
		return sum;	//0<=sum<7
		//Assembly-related:
		// sum % 7 = sum - [7 * (hash / 7)]	//sum / 7 is an integer here, omitting any decimal points
		
		// sum / 7 = sum * 6/7 * 1/6 = sum * 6/7 * 1/3 * 1/2
		// sum / 3 = sum * 1/3 = sum * 2/3 * 1/2
		// <=> sum / 7 = sum * 6/7 * (2/3 * 1/2) * 1/2 = sum * 6/7 * 2/3 * 1/2^2
		
		// 6/7 will be a hex number [(2^32 / 7) + 1]
		// 2/3 will be a hex number [(2^32 / 3) + 1]
		// 1/2^2 is LSLS (Logical Shift Left) 2 times
	}
	else{
		return hash;	//0<=hash<=9
	}
}

int main(){
	sum_mod7(10);
	return 0;
}

//KEIL Disassembler:
//     21: int main(){ 
// 0x080007AC B580      PUSH          {r7,lr}
// 0x080007AE B082      SUB           sp,sp,#0x08
// 0x080007B0 2000      MOVS          r0,#0x00
// 0x080007B2 9000      STR           r0,[sp,#0x00]
// 0x080007B4 9001      STR           r0,[sp,#0x04]
// 0x080007B6 200A      MOVS          r0,#0x0A
//     22:         sum_mod7(10); 
// 0x080007B8 F000F804  BL.W          0x080007C4 sum_mod7
//     23:         return 0; 
// 0x080007BC 9800      LDR           r0,[sp,#0x00]
// 0x080007BE B002      ADD           sp,sp,#0x08
// 0x080007C0 BD80      POP           {r7,pc}
// 0x080007C2 0000      MOVS          r0,r0
//      4: int sum_mod7(int hash){ 
// 0x080007C4 B083      SUB           sp,sp,#0x0C
// 0x080007C6 9001      STR           r0,[sp,#0x04]
// 0x080007C8 2000      MOVS          r0,#0x00
//      5:         int sum = 0; 
// 0x080007CA 9000      STR           r0,[sp,#0x00]
//      6:         if (hash>9){ 
//      7:                 //creating the sum 
// 0x080007CC 9801      LDR           r0,[sp,#0x04]
// 0x080007CE 280A      CMP           r0,#0x0A
// 0x080007D0 DB2F      BLT           0x08000832
// 0x080007D2 E7FF      B             0x080007D4
//      8:                 while (hash!=0){                        //to prevent further calculations after once the hash becomes a single digit 
// 0x080007D4 E7FF      B             0x080007D6
// 0x080007D6 9801      LDR           r0,[sp,#0x04]
// 0x080007D8 B1D0      CBZ           r0,0x08000810
// 0x080007DA E7FF      B             0x080007DC
//      9:                         sum = sum + hash % 10;  //adds to the sum the last digit of the hash 
// 0x080007DC 9800      LDR           r0,[sp,#0x00]
// 0x080007DE 9A01      LDR           r2,[sp,#0x04]
// 0x080007E0 F2466167  MOVW          r1,#0x6667
// 0x080007E4 F2C66166  MOVT          r1,#0x6666
// 0x080007E8 FB52FC01  SMMUL         r12,r2,r1
// 0x080007EC EA4F03AC  ASR           r3,r12,#2
// 0x080007F0 EB0373DC  ADD           r3,r3,r12,LSR #31
// 0x080007F4 EB030383  ADD           r3,r3,r3,LSL #2
// 0x080007F8 EBA20243  SUB           r2,r2,r3,LSL #1
// 0x080007FC 4410      ADD           r0,r0,r2
// 0x080007FE 9000      STR           r0,[sp,#0x00]
//     10:                         hash = hash / 10;                     //removes the last digit of the hash (the one next to the last becomes last) 
//     11:                 } 
//     12:                 //modding by 7 
// 0x08000800 9801      LDR           r0,[sp,#0x04]
// 0x08000802 FB50F101  SMMUL         r1,r0,r1
// 0x08000806 1088      ASRS          r0,r1,#2
// 0x08000808 EB0070D1  ADD           r0,r0,r1,LSR #31
// 0x0800080C 9001      STR           r0,[sp,#0x04]
// 0x0800080E E7E2      B             0x080007D6
//     13:                 sum = sum % 7; 
// 0x08000810 9800      LDR           r0,[sp,#0x00]
// 0x08000812 F2424193  MOVW          r1,#0x2493
// 0x08000816 F2C92149  MOVT          r1,#0x9249
// 0x0800081A FB510200  SMMLA         r2,r1,r0,r0
// 0x0800081E 1091      ASRS          r1,r2,#2
// 0x08000820 EB0171D2  ADD           r1,r1,r2,LSR #31
// 0x08000824 EBC101C1  RSB           r1,r1,r1,LSL #3
// 0x08000828 1A40      SUBS          r0,r0,r1
// 0x0800082A 9000      STR           r0,[sp,#0x00]
//     14:                 return sum;     //0<=sum<7 
//     15:         } 
//     16:         else{ 
// 0x0800082C 9800      LDR           r0,[sp,#0x00]
// 0x0800082E 9002      STR           r0,[sp,#0x08]
// 0x08000830 E002      B             0x08000838
//     17:                 return hash;    //0<=hash<=9 
//     18:         } 
// 0x08000832 9801      LDR           r0,[sp,#0x04]
// 0x08000834 9002      STR           r0,[sp,#0x08]
// 0x08000836 E7FF      B             0x08000838
//     19: } 
// 0x08000838 9802      LDR           r0,[sp,#0x08]
// 0x0800083A B003      ADD           sp,sp,#0x0C
// 0x0800083C 4770      BX            lr
// 0x0800083E 0000      MOVS          r0,r0
