/*
 Name:Haoyang Yuan
 Andrew ID:haoyangy
*/

/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    
    int i,j,k;
    int subi=0,subj=0;
    int tempi=0, tempj=0,tempk=0,templ=0;
    int temp = 0;
    int cross = 0;
    REQUIRES(M > 0);
    REQUIRES(N > 0);
    //standard block transposite with special process of diagonal
    if(M==32){
        for(i=0;i<N;i=i+8){
            for( j=0;j<M;j=j+8){
                for(tempi=i;tempi<i+8;tempi++){
                    for(tempj=j;tempj<j+8;tempj++){
                        //store diagonal and
                        //assign it to destination after process
                        if(tempi==tempj){
                            temp = A[tempi][tempj];
                            cross = tempi;
                            
                            continue;
                        }
                        B[tempj][tempi] = A[tempi][tempj];
                    }
                    //after process, store the diagonal data
                    if(i==j){
                        B[cross][cross] = temp;
                    }
                }
            }
        }
    }
    // a relativelt tricky one
    else if(M==64){
        for(j=0;j<N;j=j+8){
            for( i=0;i<M;i=i+8){
                //devide it into 8*8 block,
                //and for each block divide it into 4*4
                for(subj=j;subj<j+8;subj=subj+4){
                    for(subi=i;subi<i+8;subi=subi+4){
                        //for the top left subblock, do the usual transpose
                        //for top right second subblock, store data
                        //to same subblock position of its transpose block
                        if(subi==i&&subj==j){
                            for(k=subj;k<subj+4;k++){
                                B[subi][k] = A[k][subi];
                                B[subi+1][k] = A[k][subi+1];
                                B[subi+2][k] = A[k][subi+2];
                                B[subi+3][k] = A[k][subi+3];
                            
                                tempi = A[k][subi+4];
                                tempj = A[k][subi+5];
                                tempk = A[k][subi+6];
                                templ = A[k][subi+7];
                                
                                B[subi][k+4] = tempi;
                                B[subi+1][k+4] = tempj;
                                B[subi+2][k+4] = tempk;
                                B[subi+3][k+4] = templ;
                                
                            }
                        }
                        //for the bottomn right subblock
                        //do the usual transpose
                        else if(subi==i+4&&subj==j+4){
                            for(k=subj;k<subj+4;k++){
                                B[subi][k] = A[k][subi];
                                B[subi+1][k] = A[k][subi+1];
                                B[subi+2][k] = A[k][subi+2];
                                B[subi+3][k] = A[k][subi+3];
                            }
                        }
                        //for the bottom left subblock
                       
                        else if(subj==j+4&&subi==i){
                            for(k=subj;k<subj+4;k++){
                                //store the data of the top right subblock
                                //in the transpose block to variable
                                tempi = B[subi+k-subj][subj];
                                tempj = B[subi+k-subj][subj+1];
                                tempk = B[subi+k-subj][subj+2];
                                templ = B[subi+k-subj][subj+3];
                                
                                //process the bottomn left subblock
                                //transpose them to the top right subblock
                                //of the transpose block
                                B[subi+k-subj][subj] = A[subj][k-subj+subi];
                                B[subi+k-subj][subj+1] = A[subj+1][k-subj+subi];
                                B[subi+k-subj][subj+2] = A[subj+2][k-subj+subi];
                                B[subi+k-subj][subj+3] = A[subj+3][k-subj+subi];
                                
                                //assign the data in variable to the
                                //bottomn left subblock in the transpose block
                                B[subi+4+k-subj][subj-4] = tempi;
                                B[subi+4+k-subj][subj-4+1] = tempj;
                                B[subi+4+k-subj][subj-4+2] = tempk;
                                B[subi+4+k-subj][subj-4+3] = templ;
                            }
                            
                        }
                        else{
                            continue;
                        }
                    }
                }
            }
        }
    }
    //do the standard block transposite
        else{
            for(i=0;i<N;i=i+14){
                for( j=0;j<M;j=j+14){
                    for(tempi=i;tempi<i+14&&tempi<N;tempi++){
                        for(tempj=j;tempj<j+14&&tempj<M;tempj++){
                         
                            B[tempj][tempi] = A[tempi][tempj];
                        
                 
                        }
                    }
                }
            }
        }
    
    ENSURES(is_transpose(M, N, A, B));
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
   // registerTransFunction(trans, trans_desc);

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

