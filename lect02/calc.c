#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]){
	double first = atof(argv[1]);
	char oper = argv[2][0];
	double second = atof(argv[3]);

	if (oper == '+') {
		printf("%f\n",first + second);
	}
	else if(oper == '-'){
		printf("%f\n",first - second);
	}
	else if(oper =='x'){
		printf("%f\n",first * second);
	}
	else if(oper =='/'){
		if(second == 0){
			printf("0으로 나눌 수 없습니다.\n");
		}
		else
			printf("%f\n",first / second);
	}
}

