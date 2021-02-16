/********************************
Author: Sravanthi Kota Venkata
********************************/

#include <stdio.h>
#include <stdlib.h>
#include "svm.h"


void symbol_script(alphaRet* alpha,int N,  F2D* trn1,  F2D* trn2,int iter, F2D *a_result, F2D *b_result, F2D *Yoffset,F2D *Xtst,F2D *Ytst,
		   F2D *tst1,F2D *tst2,int Ntst,F2D** result_ptr,F2D *s,int dim)
{
    printf("test 1\n");
    int i,j,n;
    //*result_ptr = malloc(sizeof(F2D));
    alpha = getAlphaFromTrainSet(N, trn1, trn2, iter);
    a_result = alpha->a_result;
    b_result = alpha->b_result;
    Yoffset = fSetArray(iter, N, 0);
printf("test 2\n");
    Xtst = usps_read_partial(tst1, tst2, -1, 1, Ntst/iter, iter);
    Ytst = usps_read_partial(tst1, tst2, -1, 0, Ntst/iter, iter);
printf("test 3\n");
    for(i=0; i<iter; i++)
    {
        F2D *temp;
        temp = usps_read_partial(trn1, trn2, i, 0, N/iter, iter);
        for(j=0; j<N; j++)
            subsref(Yoffset,i,j) = asubsref(temp,j);
        fFreeHandle(temp);
    }
printf("test 4\n");

//F2D* result = fSetArray(Ntst,1,0);
 *result_ptr = fSetArray(Ntst,1,0); 
    for( n=0; n<Ntst; n++)
    {printf("test 5\n");
        float maxs=0;
        s=fSetArray(iter,1,0);
        for( i=0; i<iter; i++)
        {
            for (j=0; j<N; j++)
            {printf("test 6\n");
                if (subsref(a_result,i,j) > 0)
                {
                    F2D *Xtemp, *XtstTemp, *X;
                    X = alpha->X;
                    Xtemp = fDeepCopyRange(X,j,1,0,X->width);
                    XtstTemp = fDeepCopyRange(Xtst, n,1,0,Xtst->width);
                    asubsref(s,i) = asubsref(s,i) + subsref(a_result,i,j) * subsref(Yoffset,i,j) * polynomial(3,Xtemp,XtstTemp, dim);
                    fFreeHandle(Xtemp); 
                    fFreeHandle(XtstTemp); 
                }
            }printf("test 7\n");
            asubsref(s,i) = asubsref(s,i) - asubsref(b_result,i);
            if( asubsref(s,i) > maxs)
                maxs = asubsref(s,i);
        }
        printf("test 8\n");
        fFreeHandle(s);
        asubsref((*result_ptr),n) = maxs;
    }
    printf("test 9\n");
    //result_ptr = &result;
     fFreeHandle(alpha->a_result);
        fFreeHandle(alpha->b_result);
     fFreeHandle(alpha->X);
     //return result;
}

int main(int argc, char* argv[])
{
    int iter, N, Ntst, k;
    F2D* trn1, *tst1, *trn2, *tst2, *Yoffset;
    alphaRet* alpha;
    F2D *a_result, *result;
    F2D** result_ptr;
    F2D *s;
    F2D *b_result;
    F2D *Xtst, *Ytst;
    unsigned int* start, *stop, *elapsed;
    char im1[256];
    int dim = 256;
    
    N = 100;
    Ntst = 100;
    iter = 10;

    #ifdef test
    N = 4;
    Ntst = 4;
    iter = 2;
    #endif
    
    #ifdef sim_fast
    N = 20;
    Ntst = 20;
    iter = 2;
    #endif

    #ifdef sim
    N = 16;
    Ntst = 16;
    iter = 8;
    #endif
    
    #ifdef sqcif
    N = 60;
    Ntst = 60;
    iter = 6;
    #endif
    
    #ifdef qcif
    N = 72;
    Ntst = 72;
    iter = 8;
    #endif
    
    #ifdef vga
    N = 450;
    Ntst = 450;
    iter = 15;
    #endif
    
    #ifdef wuxga
    N = 1000;
    Ntst = 1000;
    iter = 20;
    #endif
    printf("Input size\t\t- (%dx%dx%d)\n", N, Ntst, iter);

    if(argc < 2) 
    {
        printf("We need input image path\n");
        return -1;
    }

    sprintf(im1, "%s/d16trn_1.txt", argv[1]);
    trn1 = readFile(im1);   

    sprintf(im1, "%s/d16trn_2.txt", argv[1]);
    trn2 = readFile(im1);   

    sprintf(im1, "%s/d16tst_1.txt", argv[1]);
    tst1 = readFile(im1);   

    sprintf(im1, "%s/d16tst_2.txt", argv[1]);
    tst2 = readFile(im1);       
    printf("before time measuring\n");
    /** Start timing **/
    start = photonStartTiming();

    printf("before symbol_script\n");

    /*symbol we want to put breakpoint at*/
    symbol_script(alpha,N,trn1,trn2,iter,a_result,b_result,Yoffset,Xtst,Ytst,tst1,tst2,Ntst,result_ptr,s,dim);
    printf("test 10\n");
    /** Timing utils */
    stop = photonEndTiming();
    printf("test 11\n");
    result = *result_ptr;
#ifdef CHECK   
    /** Self checking - use expected.txt from data directory  **/
    {
        int ret=0;
        float tol = 0.5;
	 printf("test 12\n");
#ifdef GENERATE_OUTPUT
	 fWriteMatrix(result, argv[1]);
#endif
	ret = fSelfCheck(result, argv[1], tol);
	printf("test 13\n");
        //if (ret == -1)
	//  printf("Error in SVM\n");
    }
    /** Self checking done **/
#endif
    printf("test 14\n");    
    fFreeHandle(trn1);
     printf("test 15\n");
    fFreeHandle(tst1);
     printf("test 16\n");
    fFreeHandle(trn2);
    fFreeHandle(tst2);
    fFreeHandle(Yoffset);
    fFreeHandle(result);
     printf("test 17\n");
     // fFreeHandle(alpha->a_result);
     printf("test 18\n");
     //fFreeHandle(alpha->b_result);
     //fFreeHandle(alpha->X);
        printf("test 19\n");
    free(alpha);
    fFreeHandle(Xtst);
    fFreeHandle(Ytst);
    elapsed = photonReportTiming(start, stop);
    photonPrintTiming(elapsed);
    free(start);
    free(stop);
    free(elapsed);

    return 0;
}


