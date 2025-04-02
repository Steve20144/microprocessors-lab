int sum_mod7(int hash){
	int sum = 0;
	if (hash>9){
		//creating the sum
		while (hash!=0){			//to prevent further calculations after once the hash becomes a single digit
			sum = sum + hash % 10;	//adds to the sum the last digit of the hash
			hash = hash / 10;			//removes the last digit of the hash (the one next to the last becomes last)
		}
		//modding by 7
		sum = sum % 7;
		return sum;	//0<=sum<7
	}
	else{
		return hash;	//0<=hash<=9
	}
}


//KEIL DISSASEMBLER - self notes
// #include <stdio.h>
// #include <uart.h>

// void factorial (int x){
// 	int y=0;
// 	y=x % 7;
// };
//      6:         y=x % 7; 
// //0x080007B4 9801      LDR           r0,[sp,#0x04]
// //0x080007B6 F2424193  MOVW          r1,#0x2493
// //0x080007BA F2C92149  MOVT          r1,#0x9249
// //0x080007BE FB510200  SMMLA         r2,r1,r0,r0
// //0x080007C2 1091      ASRS          r1,r2,#2
// //0x080007C4 EB0171D2  ADD           r1,r1,r2,LSR #31
// //0x080007C8 EBC101C1  RSB           r1,r1,r1,LSL #3
// //0x080007CC 1A40      SUBS          r0,r0,r1
// //0x080007CE 9000      STR           r0,[sp,#0x00]
// int main(){
// 	factorial(10);
// 	return 0;
// }
