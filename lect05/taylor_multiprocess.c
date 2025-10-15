#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#define _USE_MATH_DEFINES
#include <math.h>
#define N 4

void sinx_taylor(int num_elements, int terms, double* x, double* result)		
{
	// 파이프를 각 자식마다 1개씩 생성
	// 파이프 1개만 쓰면 데이터가 섞일 가능성?
	int fd[N][2];
	

	
        for(int i=0; i< num_elements; i++){
		pipe(fd[i]);
		int pid = fork();
		
		// 자식 프로세스
		if (pid ==0){
			close(fd[i][0]);
                	double value = x[i];
                	double numer = x[i] * x[i] * x[i];
                	double denom = 6;
                	int sign = -1;

                	for(int j=1; j <= terms ; j++){
                        	value += (double)sign * numer / denom;
                        	numer*=x[i] * x[i];
                        	denom *= (2.*(double)j + 2.)*(2.*(double)j+3.);
                        	sign *=-1;
                	}
			write(fd[i][1], &value, sizeof(value));
			exit(0);
		}
		// 부모 프로세
		else{
			close(fd[i][1]);
		}
	
		
        }
	for (int i = 0; i < num_elements; i++) {
        	wait(NULL);
        	double value;
        	read(fd[i][0], &value, sizeof(value));
        	result[i] = value;
    }


}

int main(){
	double x[N] = {0,M_PI/6., M_PI/3., 0.134};
	double res[N];

	sinx_taylor(N, 3, x, res);
	for(int i =0 ; i < N ; i++){
		printf("sin(%.2f) by Taylor series = %f\n", x[i], res[i]);
		printf("sin(%.2f) = %f\n", x[i], sin(x[i]));
	}
	return 0;
}
