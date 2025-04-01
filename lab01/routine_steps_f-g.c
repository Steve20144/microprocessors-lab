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
