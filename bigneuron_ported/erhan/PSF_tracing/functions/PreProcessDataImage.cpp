/* C++ IMPLEMENTATION OF PRINCIPAL CURVE TRACING USING MEX INTERFACE
 *
 * Code developed by: Nikhila Srikanth
 * M.S. Project
 * Cognitive Systems Laboratory
 * Electrical and Computer Engineering Department
 * Northeastern University, Boston, MA
 *
 * Under the guidance of: Prof. Deniz Erdogmus and Dr. Erhan Bas
 *
 * Date Created: Jan 20, 2011
 * Date Last Updated: April 29, 2011
 * This code aims at converting the MATLAB implementation of preprocessing steps
 * of Principal Curve Tracing algorithm into C++.
 *
 *Modifications:
 *Date: 11/15/2011: Erhan Bas
 *Added threshold constrain to speed up the process
 *Changed covariance estimation step, instead of X'*W*X, now we use ((X'.*W)*X
 */

#include <math.h>
#include <cfloat>

#include "PreProcessDataImage.h"
#include "EigenDec_2D.h"
#include "EigenDec_3D.h"
#include "Create3DLookUpTable.h"
#include "FunctionsForMainCode.h"
#include "Gamma.h"
#include "MatrixMultiplication.h"
#include "Parse_Input.h"
#include <string.h>

//#define LOOKUP_TABLE_WIDTH 11

#define Z(idx) (iNum_of_Dims_of_Input_Image*(idx) + 0)
#define Y(idx) (iNum_of_Dims_of_Input_Image*(idx) + 1)
#define X(idx) (iNum_of_Dims_of_Input_Image*(idx) + 2)

// For storing final outputs generated by PreProcessDataImage code.
#define OUTPUT_WEIGHTS FALSE
#define OUTPUT_DATA FALSE
#define OUTPUT_FINAL_VECTORS FALSE
#define OUTPUT_FINAL_LAMBDA  FALSE
#define OUTPUT_FINAL_NORMP   FALSE
#define NORM_EIGENVAL FALSE

// For storing intermediate outputs generated for Anisotropic = TRUE case.
#define OUTPUT_TEMP FALSE
#define OUTPUT_TEMP_TRANSPOSE FALSE
#define OUTPUT_DIAG_OF_WEIGHTS FALSE // Warning: If enabled, it will generate a file of huge size.
#define OUTPUT_COV_I FALSE
#define OUTPUT_COV   FALSE
#define OUTPUT_EIGDEC_LAMBDA   FALSE
#define OUTPUT_EIGDEC_VECTORS  FALSE
#define OUTPUT_EIGDEC_NORMP    FALSE

typedef struct
{
    int iThreadNumber;

    double *pdWeights_InputImage;
    int *piMeshGridData_InputImage;
    int iNum_of_pixels_InputImage;
    int iNum_of_Dims_of_Input_Image;

    int *piLookUpTable;
    double *pdDistTable;
    int iMinLookupTable;
    int iMaxLookupTable;
    int iLenLookupTable;

    int iLen_UpTriang_of_Cov ;
    int *piIndexes_of_UpTriang_of_Cov ;
    double K;

    double *pdUpTriag_of_Cov_of_InputImage;
} ANISO_THREAD_ARGS;

void *Anisotropic_PreProcess (void  *pstAniso_Thread_Args);


void PreProcessDataImage(double *pdInputImage, int iNum_of_Dims_of_Input_Image, V3DLONG *piDims_InputImage, int iNum_of_pixels_InputImage,
        double *pdWeights_InputImage, double *pdEigVec_Cov_InputImage, double *pdEigVal_Cov_InputImage,
        double *pdNormP_EigVal_Cov, int *piMeshGridData_InputImage, char *outputDir, double sigma, int LookUpTableWidth)
{

    int iDim1_InputImage, iDim2_InputImage, iDim3_InputImage;
    int iIter, iRowIter, iColIter, iIdenColIter, iIdenRowIter ;
    int iMinLookupTable, iMaxLookupTable, iLenLookupTable ;
    //double INFINITY = DBL_MAX;
    double dNormP_Element, L2;
    char saveName[80];

    int iIter_Dim_1, iIter_Dim_2, iIter_Dim_3;
    int aiWindowDims[3] ;
    int *piLookUpTable;
    double *pdDistTable;

    bool bAnisotropic;
    double K, Beta;
    double *pmxaWeights_ParseInput;

    int iLen_UpTriang_of_Cov ;
    int	*piIndexes_of_UpTriang_of_Cov ;
    double *pdUpTriag_of_Cov_of_InputImage; // C
    int iNum_Elems_of_C; // to store length of C

    //pthread declarations
    pthread_t ptThreads[NUM_OF_PREPROCESS_THREADS_TO_CREATE];
    int iThreadNumber;
    int iRet_Val_Pthread_Create;
    bool bAll_Threads_Completed_Successfully = TRUE;

    double *adEigVec1x_Cov, *adEigVec1y_Cov, *adEigVec2x_Cov, *adEigVec2y_Cov, *adEigVal1_Cov, *adEigVal2_Cov; // Used for both 2d and 3d.
    double *adEigVec1z_Cov, *adEigVec2z_Cov, *adEigVec3x_Cov, *adEigVec3y_Cov, *adEigVec3z_Cov, *adEigVal3_Cov; // Used Only for 3d.

    double dGamma_of_ndims_by_2beta; // for storing: gamma(d/(2*beta))
    double dGamma_of_ndims_by_2; // for storing: gamma(d/2)
    double eps = 2.2204e-16;

    ANISO_THREAD_ARGS astAniso_Thread_Args[NUM_OF_PREPROCESS_THREADS_TO_CREATE];



    // Files generated to store test results.
    FILE *fp_data, *fp_weights, *fp_final_vectors, *fp_final_lambda, *fp_final_normP; // final outputs (for both Isotropic and Anistrpoic cases)
    FILE *fp_temp, *fp_temp_tr, *fp_diag_weights, *fp_cov_i, *fp_cov, *fp_eigdec_lambda, *fp_eigdec_vectors, *fp_eigdec_normP;


    // Populate Image Weights.
    for (iIter  = 0 ; iIter < iNum_of_pixels_InputImage ; iIter++)
    {
        pdWeights_InputImage[iIter] = pdInputImage [iIter];
    }


    #if (OUTPUT_WEIGHTS == TRUE) // Save Image Weights to a file in this form: [N x 1] even though actual dimension is [1 x N]
    strcpy(saveName,outputDir);
    strcat(saveName,"mex_weights.txt");
    fp_weights = fopen (saveName, "w");
    if (fp_weights == NULL)
    {
        printf("\nCannot create file: mex_weights.txt\n");
    }
    for (iIter = 0 ; iIter < iNum_of_pixels_InputImage ; iIter++)
    {
        fprintf (fp_weights, "%g\n", pdWeights_InputImage[iIter]);
    }
    fclose (fp_weights);
    #endif

    iDim1_InputImage = piDims_InputImage[0];
    iDim2_InputImage = piDims_InputImage[1];
    iDim3_InputImage = piDims_InputImage[2];
    printf ("\nImage is 3-dimensional. \nSize: [%d x %d x %d] \nTotal number of pixels: %d\n",
            iDim1_InputImage, iDim2_InputImage, iDim3_InputImage, iNum_of_pixels_InputImage);

    for (iIter_Dim_3 = 0 ; iIter_Dim_3 < iDim3_InputImage ; iIter_Dim_3++)
    {
        for (iIter_Dim_2 = 0 ; iIter_Dim_2 < iDim2_InputImage ; iIter_Dim_2++)
        {
            for (iIter_Dim_1 = 0 ; iIter_Dim_1 < iDim1_InputImage ; iIter_Dim_1++)
            {
                piMeshGridData_InputImage[ Z(iIter_Dim_1 + iIter_Dim_2*iDim1_InputImage + iIter_Dim_3*iDim1_InputImage*iDim2_InputImage) ] = iIter_Dim_1;
                piMeshGridData_InputImage[ Y(iIter_Dim_1 + iIter_Dim_2*iDim1_InputImage + iIter_Dim_3*iDim1_InputImage*iDim2_InputImage) ] = iIter_Dim_2;
                piMeshGridData_InputImage[ X(iIter_Dim_1 + iIter_Dim_2*iDim1_InputImage + iIter_Dim_3*iDim1_InputImage*iDim2_InputImage) ] = iIter_Dim_3;
            }
        }
    }

    aiWindowDims[0] = LookUpTableWidth;
    aiWindowDims[1] = LookUpTableWidth;
    aiWindowDims[2] = LookUpTableWidth;


    create3DLookupTable(aiWindowDims, 3, iDim1_InputImage, iDim2_InputImage, &piLookUpTable, &pdDistTable);

    iMinLookupTable = Compute_3D_Min(piLookUpTable, aiWindowDims[0], aiWindowDims[1], aiWindowDims[2]);
    iMaxLookupTable = Compute_3D_Max(piLookUpTable, aiWindowDims[0], aiWindowDims[1], aiWindowDims[2]);
    iLenLookupTable = Compute_3D_Len(piLookUpTable, aiWindowDims[0], aiWindowDims[1], aiWindowDims[2]);

    printf ("\nLookup Min: %d \nLookup Max: %d \nLookup Len: %d\n", iMinLookupTable , iMaxLookupTable , iLenLookupTable );

    parse_input (&bAnisotropic, &K, &Beta, pmxaWeights_ParseInput, iNum_of_Dims_of_Input_Image, iNum_of_pixels_InputImage, LookUpTableWidth);

    printf ("\nAnisotropic = %s \nBeta = %g \nK = %g\n", (bAnisotropic ?"TRUE" :"FALSE"), Beta, K);

    if (bAnisotropic)
    {
        // Populate uptiu.
        iLen_UpTriang_of_Cov = ((iNum_of_Dims_of_Input_Image * (iNum_of_Dims_of_Input_Image +1))/2); // dData*(dData+1)/2
        piIndexes_of_UpTriang_of_Cov = (int*) malloc (iLen_UpTriang_of_Cov * sizeof (int)); // uptiu
        if (iNum_of_Dims_of_Input_Image == 2)
        {
            piIndexes_of_UpTriang_of_Cov[0] = 0;
            piIndexes_of_UpTriang_of_Cov[1] = 2;
            piIndexes_of_UpTriang_of_Cov[2] = 3;
        }
        else if (iNum_of_Dims_of_Input_Image  == 3)
        {
            piIndexes_of_UpTriang_of_Cov[0] = 0;
            piIndexes_of_UpTriang_of_Cov[1] = 3;
            piIndexes_of_UpTriang_of_Cov[2] = 6;
            piIndexes_of_UpTriang_of_Cov[3] = 4;
            piIndexes_of_UpTriang_of_Cov[4] = 7;
            piIndexes_of_UpTriang_of_Cov[5] = 8;
        }

        // Calculate local covariance using the lookupTable
        // initialize Cov matrix
        //C = zeros(N,dData*(dData+1)/2);
        iNum_Elems_of_C = (iNum_of_pixels_InputImage * iLen_UpTriang_of_Cov); // length(C) = N*dData*(dData+1)/2
        pdUpTriag_of_Cov_of_InputImage = (double*) malloc (iNum_Elems_of_C * sizeof(double));

        for (iIter = 0; iIter < iNum_Elems_of_C; iIter++)
        {
            pdUpTriag_of_Cov_of_InputImage[iIter] = 0;
        }

        // MULTITHREADING
        for(iThreadNumber = 0; iThreadNumber < NUM_OF_PREPROCESS_THREADS_TO_CREATE; iThreadNumber++)
        {
            // Populate input args for the each thread.
            astAniso_Thread_Args[iThreadNumber].iThreadNumber = iThreadNumber; // Passes the current thread number.
            astAniso_Thread_Args[iThreadNumber].pdWeights_InputImage = pdWeights_InputImage;
            astAniso_Thread_Args[iThreadNumber].piMeshGridData_InputImage = piMeshGridData_InputImage;
            astAniso_Thread_Args[iThreadNumber].iNum_of_pixels_InputImage = iNum_of_pixels_InputImage;
            astAniso_Thread_Args[iThreadNumber].iNum_of_Dims_of_Input_Image = iNum_of_Dims_of_Input_Image;
            astAniso_Thread_Args[iThreadNumber].piLookUpTable = piLookUpTable;
            astAniso_Thread_Args[iThreadNumber].pdDistTable = pdDistTable;
            astAniso_Thread_Args[iThreadNumber].iMinLookupTable = iMinLookupTable;
            astAniso_Thread_Args[iThreadNumber].iMaxLookupTable = iMaxLookupTable;
            astAniso_Thread_Args[iThreadNumber].iLenLookupTable = iLenLookupTable;
            astAniso_Thread_Args[iThreadNumber].iLen_UpTriang_of_Cov = iLen_UpTriang_of_Cov;
            astAniso_Thread_Args[iThreadNumber].piIndexes_of_UpTriang_of_Cov = piIndexes_of_UpTriang_of_Cov;
            astAniso_Thread_Args[iThreadNumber].pdUpTriag_of_Cov_of_InputImage = pdUpTriag_of_Cov_of_InputImage;
            astAniso_Thread_Args[iThreadNumber].K = K;

            iRet_Val_Pthread_Create = pthread_create(&ptThreads[iThreadNumber], NULL, Anisotropic_PreProcess, &astAniso_Thread_Args[iThreadNumber]);
        }


        for(iThreadNumber = 0; iThreadNumber < NUM_OF_PREPROCESS_THREADS_TO_CREATE; iThreadNumber++)
        {
            pthread_join(ptThreads[iThreadNumber], NULL);
           // printf("Thread %d returns  %d\n", iThreadNumber, iRet_Val_Pthread_Create);
        }

        //pthread_exit(NULL);


        bool bAll_Threads_Completed_Successfully = TRUE;

        if (bAll_Threads_Completed_Successfully == TRUE)
        {

            // if dData==2
            //2-d and 3-d
            adEigVec1x_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
            adEigVec1y_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
            adEigVec2x_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
            adEigVec2y_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
            adEigVal1_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
            adEigVal2_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));

            //  if dData==2
            if (iNum_of_Dims_of_Input_Image == 2)
            {

                Compute_Eig_Dec_2D (iNum_of_pixels_InputImage, &pdUpTriag_of_Cov_of_InputImage[0], &pdUpTriag_of_Cov_of_InputImage[iNum_of_pixels_InputImage],
                        &pdUpTriag_of_Cov_of_InputImage[2*iNum_of_pixels_InputImage],
                        adEigVal1_Cov, adEigVal2_Cov, adEigVec1x_Cov, adEigVec1y_Cov, adEigVec2x_Cov, adEigVec2y_Cov);

                for (iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter ++)
                {
                    pdEigVal_Cov_InputImage[ROWCOL(0, iColIter , 2)] = adEigVal1_Cov[iColIter];
                    pdEigVal_Cov_InputImage[ROWCOL(1, iColIter , 2)] = adEigVal2_Cov[iColIter];
                }

                for (iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter ++)
                {
                    pdEigVec_Cov_InputImage[ROWCOL(0, iColIter , 4)] = adEigVec1x_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(1, iColIter , 4)] = adEigVec1y_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(2, iColIter , 4)] = adEigVec2x_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(3, iColIter , 4)] = adEigVec2y_Cov[iColIter];
                }
            }

            // elseif dData ==3
            else if (iNum_of_Dims_of_Input_Image == 3)
            {

                adEigVec1z_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
                adEigVec2z_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
                adEigVec3x_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
                adEigVec3y_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
                adEigVec3z_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));
                adEigVal3_Cov = (double*) malloc (iNum_of_pixels_InputImage*sizeof(double));

                Compute_Eig_Dec_3D(iNum_of_pixels_InputImage, &pdUpTriag_of_Cov_of_InputImage[0], &pdUpTriag_of_Cov_of_InputImage[iNum_of_pixels_InputImage],
                        &pdUpTriag_of_Cov_of_InputImage[2*iNum_of_pixels_InputImage], &pdUpTriag_of_Cov_of_InputImage[3*iNum_of_pixels_InputImage],
                        &pdUpTriag_of_Cov_of_InputImage[4*iNum_of_pixels_InputImage], &pdUpTriag_of_Cov_of_InputImage[5*iNum_of_pixels_InputImage],
                        adEigVec1x_Cov, adEigVec1y_Cov, adEigVec1z_Cov,
                        adEigVec2x_Cov, adEigVec2y_Cov, adEigVec2z_Cov,
                        adEigVec3x_Cov, adEigVec3y_Cov, adEigVec3z_Cov,
                        adEigVal1_Cov, adEigVal2_Cov, adEigVal3_Cov);

                for (iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter ++)
                {
                    pdEigVal_Cov_InputImage[ROWCOL(0, iColIter , 3)] = adEigVal1_Cov[iColIter];
                    pdEigVal_Cov_InputImage[ROWCOL(1, iColIter , 3)] = adEigVal2_Cov[iColIter];
                    pdEigVal_Cov_InputImage[ROWCOL(2, iColIter , 3)] = adEigVal3_Cov[iColIter];
                }

                for (iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter ++)
                {
                    pdEigVec_Cov_InputImage[ROWCOL(0, iColIter , 9)] = adEigVec1x_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(1, iColIter , 9)] = adEigVec1y_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(2, iColIter , 9)] = adEigVec1z_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(3, iColIter , 9)] = adEigVec2x_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(4, iColIter , 9)] = adEigVec2y_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(5, iColIter , 9)] = adEigVec2z_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(6, iColIter , 9)] = adEigVec3x_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(7, iColIter , 9)] = adEigVec3y_Cov[iColIter];
                    pdEigVec_Cov_InputImage[ROWCOL(8, iColIter , 9)] = adEigVec3z_Cov[iColIter];

                }
                free(adEigVec1z_Cov); free(adEigVec2z_Cov); free(adEigVec3x_Cov); free(adEigVec3y_Cov); free(adEigVec3z_Cov);
                free(adEigVal3_Cov); // Used Only for 3d.

            }
            free (adEigVec1x_Cov); free(adEigVec1y_Cov); free(adEigVec2x_Cov); free(adEigVec2y_Cov);
            free(adEigVal1_Cov); free(adEigVal2_Cov);

            for(iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter++)
            {
                dNormP_Element = 1;
                for(iRowIter = 0; iRowIter < iNum_of_Dims_of_Input_Image; iRowIter++)
                {
                    dNormP_Element = dNormP_Element * pdEigVal_Cov_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)];
                    if (pdEigVal_Cov_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)] < 0)
                    {
                        pdEigVal_Cov_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)] = 0;
                    }
                }
                dNormP_Element = 1/(pow((2*pi),(iNum_of_Dims_of_Input_Image/2))*sqrt(dNormP_Element));
                if ((dNormP_Element == INFINITY)|| (dNormP_Element < eps))
                {
                    pdNormP_EigVal_Cov[iColIter] = 0;
                }
                else
                {
                    pdNormP_EigVal_Cov[iColIter] = dNormP_Element ;
                }
            }

        } // if (bAll_Threads_Completed_Successfully == TRUE)
        else
        {
            printf ("One or more thread failed execution.\n");
        }
    } // if (bAnisotropic)
    else
    {

        for(iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter++)
        {
            for(iRowIter = 0; iRowIter < iNum_of_Dims_of_Input_Image; iRowIter++)
            {
                pdEigVal_Cov_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)] = 1;
            }
            dNormP_Element = 1/pow((2*pi),(iNum_of_Dims_of_Input_Image/2));
            pdNormP_EigVal_Cov[iColIter] = ((dNormP_Element == INFINITY) || (dNormP_Element < eps)) ? 0 : dNormP_Element ;

            //Vectors = repmat(eye(dData),1,N);
            for (iIdenColIter = 0 ; iIdenColIter < iNum_of_Dims_of_Input_Image ; iIdenColIter++)
            {
                for (iIdenRowIter = 0 ; iIdenRowIter < iNum_of_Dims_of_Input_Image ; iIdenRowIter++)
                {
                    /*
                     * (0,0)  (0,1)  (0,0+2) (0,1+2)  (0,0+4) (0,1+4) (0,0+6) (0,1+6)
                     * (1,0)  (1,1)  (1,0+2) (1,1+2)  (1,0+4) (1,1+4) (1,0+6) (1,1+6)
                     *
                     */
                    pdEigVec_Cov_InputImage[iColIter*iNum_of_Dims_of_Input_Image*iNum_of_Dims_of_Input_Image +
                            (iIdenRowIter + iIdenColIter*iNum_of_Dims_of_Input_Image)] = (iIdenRowIter == iIdenColIter) ?1 :0;
                }
            }
        }
    } // if (bAnisotropic)

    dGamma_of_ndims_by_2beta = Gamma(iNum_of_Dims_of_Input_Image/(2.0*beta)); // gamma(d/(2*beta))
    dGamma_of_ndims_by_2 = Gamma(iNum_of_Dims_of_Input_Image/2.0); // gamma(d/2)

    for(iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter++)
    {
        dNormP_Element = 1;
        #if NORM_EIGENVAL
                L2 = pdEigVal_Cov_InputImage[ROWCOL(0, iColIter, iNum_of_Dims_of_Input_Image)];
        #endif
                for(iRowIter = 0; iRowIter < iNum_of_Dims_of_Input_Image; iRowIter++)
                {
            #if NORM_EIGENVAL
                    pdEigVal_Cov_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)] /=L2;
            #endif

                    pdEigVal_Cov_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)] *= sigma*sigma;
            dNormP_Element = dNormP_Element * pdEigVal_Cov_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)];
                }
        dNormP_Element = (dGamma_of_ndims_by_2*beta)/(pow(pi, iNum_of_Dims_of_Input_Image/2.0)*dGamma_of_ndims_by_2beta *
                pow((double)2, iNum_of_Dims_of_Input_Image/(2.0*beta))*sqrt(dNormP_Element));
        pdNormP_EigVal_Cov[iColIter] = ((dNormP_Element == INFINITY) || (dNormP_Element < eps)) ? 0 : dNormP_Element ;
    }

    #if OUTPUT_FINAL_LAMBDA == TRUE // Save the final Lambda to a file in this form: [N x dData]
    strcpy(saveName,outputDir);
    strcat(saveName,"mex_final_lambda.txt");
    fp_final_lambda = fopen (saveName, "w");
    if (fp_final_lambda == NULL)
    {
        printf ("\nCannot create file: mex_final_lambda.txt\n");
    }
    for (iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter ++)
    {
        for (iRowIter = 0; iRowIter < iNum_of_Dims_of_Input_Image ; iRowIter ++)
        {
            fprintf (fp_final_lambda, "%g ", pdEigVal_Cov_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)]);
        }
        fprintf (fp_final_lambda, "\n");
    }
    fclose (fp_final_lambda);
    #endif

    #if OUTPUT_FINAL_VECTORS == TRUE // Save the final Lambda to a file in this form: [N x dData.dData]
    strcpy(saveName,outputDir);
    strcat(saveName,"mex_final_vectors.txt");
    fp_final_vectors = fopen (saveName, "w");
    if (fp_final_vectors == NULL)
    {
        printf ("\nCannot create file: mex_final_vectors.txt\n");
    }
    for (iColIter = 0; iColIter < iNum_of_pixels_InputImage ; iColIter ++)
    {
        for (iRowIter = 0; iRowIter < (iNum_of_Dims_of_Input_Image*iNum_of_Dims_of_Input_Image) ; iRowIter ++)
        {
            fprintf (fp_final_vectors, "%g ", pdEigVec_Cov_InputImage[ROWCOL(iRowIter, iColIter, (iNum_of_Dims_of_Input_Image*iNum_of_Dims_of_Input_Image))]);
        }
        fprintf (fp_final_vectors, "\n");
    }
    fclose (fp_final_vectors);
    #endif

    #if OUTPUT_FINAL_NORMP == TRUE // Save the final normP to a file in this form: [N x dData.dData]
    strcpy(saveName,outputDir);
    strcat(saveName,"mex_final_normP.txt");
    fp_final_normP = fopen (saveName, "w");
    if (fp_final_normP == NULL)
    {
        printf ("\nCannot create file: mex_final_normP.txt\n");
    }
    for (iIter = 0; iIter < iNum_of_pixels_InputImage ; iIter ++)
    {
        fprintf (fp_final_normP, "%g\n", pdNormP_EigVal_Cov[iIter]);
    }
    fclose (fp_final_normP);
    #endif
    return;

}

void *Anisotropic_PreProcess(void *vptrAniso_Thread_Args)
// parfor i=1:N //
{
    ANISO_THREAD_ARGS *pstAniso_Thread_Args;

    int iThreadNumber;
    int iImagePixel_Start, iImagePixel_End;
    double *pdWeights_InputImage;
    int *piMeshGridData_InputImage;
    int iNum_of_pixels_InputImage;
    int iNum_of_Dims_of_Input_Image;

    int *piLookUpTable;
    double *pdDistTable;
    int iMinLookupTable;
    int iMaxLookupTable;
    int iLenLookupTable;

    int iPixelIter, iLookupIter;
    int iRowIter, iColIter, iIter, iUpper_Triang_Iter;
    int iNeig_Pix_Idx;

    int iLen_UpTriang_of_Cov;
    int *piIndexes_of_UpTriang_of_Cov;
    int *aiNeig_of_MeshGridData; //D
    double K;
    int *aiNeig_Midpoint;
    double *adTemp, *adTemp_Transpose;
    double *adDiag_of_Neig_Weights;
    double *adCov_Curr_Pix_Interim, *adCov_Curr_Pix;
    double *pdUpTriag_of_Cov_of_InputImage;

    double scale = 4;//double(LOOKUP_TABLE_WIDTH)/(4*sqrt(2*log(2)));

    pstAniso_Thread_Args = (ANISO_THREAD_ARGS *) vptrAniso_Thread_Args;

    iThreadNumber = pstAniso_Thread_Args->iThreadNumber;
    iNum_of_pixels_InputImage = pstAniso_Thread_Args->iNum_of_pixels_InputImage;
    pdWeights_InputImage = pstAniso_Thread_Args->pdWeights_InputImage;
    piMeshGridData_InputImage = pstAniso_Thread_Args->piMeshGridData_InputImage;
    iNum_of_Dims_of_Input_Image = pstAniso_Thread_Args->iNum_of_Dims_of_Input_Image;
    piLookUpTable = pstAniso_Thread_Args->piLookUpTable ;
    pdDistTable = pstAniso_Thread_Args->pdDistTable ;
    iMinLookupTable = pstAniso_Thread_Args->iMinLookupTable ;
    iMaxLookupTable = pstAniso_Thread_Args->iMaxLookupTable ;
    iLenLookupTable = pstAniso_Thread_Args->iLenLookupTable ;
    iLen_UpTriang_of_Cov = pstAniso_Thread_Args->iLen_UpTriang_of_Cov;
    piIndexes_of_UpTriang_of_Cov = pstAniso_Thread_Args->piIndexes_of_UpTriang_of_Cov;
    K = pstAniso_Thread_Args->K;
    pdUpTriag_of_Cov_of_InputImage = pstAniso_Thread_Args->pdUpTriag_of_Cov_of_InputImage ;

    iImagePixel_Start = iThreadNumber*(iNum_of_pixels_InputImage/NUM_OF_PREPROCESS_THREADS_TO_CREATE);
    if (iThreadNumber == (NUM_OF_PREPROCESS_THREADS_TO_CREATE-1)) // Last part of the image processed by this thread.
    {
        iImagePixel_End = iNum_of_pixels_InputImage-1 ;
    }
    else
    {
        iImagePixel_End = iImagePixel_Start + (iNum_of_pixels_InputImage/NUM_OF_PREPROCESS_THREADS_TO_CREATE) - 1;
    }
    printf ("\nPreprocess Thread %d, processing pixels %d to %d.", iThreadNumber, iImagePixel_Start, iImagePixel_End);

    aiNeig_of_MeshGridData = (int*) malloc ((iLenLookupTable*iNum_of_Dims_of_Input_Image) * sizeof(int)); // For storing D = data(:,i+LookUpTable) [iLen  dData]
    adTemp = (double*) malloc ((iNum_of_Dims_of_Input_Image*iLenLookupTable) * sizeof(double)); // For storing: temp [dData x iLen]
    adDiag_of_Neig_Weights = (double*) malloc (iLenLookupTable*iLenLookupTable*sizeof(double)); // For storing: diag(weights(i+LookUpTable)) [iLen x iLen]
    adCov_Curr_Pix_Interim = (double *) malloc (iNum_of_Dims_of_Input_Image*iLenLookupTable*sizeof(double)); // For: temp*diag(weights(i+LookUpTable)) [dData x iLen]
    adTemp_Transpose = (double *) malloc (iLenLookupTable*iNum_of_Dims_of_Input_Image*sizeof(double)); // For storing: temp' [iLen x dData]
    adCov_Curr_Pix = (double *) malloc (iNum_of_Dims_of_Input_Image*iNum_of_Dims_of_Input_Image*sizeof(double)); // For storing: cov_i = temp*diag(weights(i+LookUpTable))*temp'/(k-1) [dData x dData]

    for (iPixelIter = iImagePixel_Start; iPixelIter <= iImagePixel_End; iPixelIter++)
        //for (iPixelIter = 17342; iPixelIter < 17343; iPixelIter++)
    {
        /*
         * if i<-minL+1 | i>N-maxL
         * continue;
         * end
         */
        if((iPixelIter < (-iMinLookupTable)) ||
                (iPixelIter > (iNum_of_pixels_InputImage - iMaxLookupTable - 1))||
                (pdWeights_InputImage[iPixelIter] < W_THRESHOLD))
        {
            continue;
        }

        //D = data(:,i+LookUpTable);
        for (iLookupIter = 0 ; iLookupIter < iLenLookupTable; iLookupIter++)
        {
            iColIter = piLookUpTable[iLookupIter] + iPixelIter;
            for (iRowIter = 0; iRowIter < iNum_of_Dims_of_Input_Image ; iRowIter++)
            {
                aiNeig_of_MeshGridData[ROWCOL(iRowIter, iLookupIter,iNum_of_Dims_of_Input_Image)]  =
                        piMeshGridData_InputImage[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)];
            }
        }

        // temp = repmat(D(:,(lenL+1)/2)),1,lenL)-D;
        aiNeig_Midpoint = &aiNeig_of_MeshGridData[iNum_of_Dims_of_Input_Image*(iLenLookupTable/2)];// D(:,(lenL+1)/2))
        for (iColIter = 0 ; iColIter < iLenLookupTable; iColIter++)
        {
            for (iRowIter = 0; iRowIter < iNum_of_Dims_of_Input_Image ; iRowIter++)
            {
                adTemp [ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)] = (double)(aiNeig_Midpoint[iRowIter] -
                        aiNeig_of_MeshGridData[ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)]); // temp [dData x lenL]
            }
        }

        Double_Compute_Transpose(adTemp, adTemp_Transpose, iNum_of_Dims_of_Input_Image, iLenLookupTable); // temp' [lenL x dData]

#if 0
for (iColIter = 0 ; iColIter < iLenLookupTable; iColIter++)
{
    iNeig_Pix_Idx = piLookUpTable[iColIter] + iPixelIter;
    for (iRowIter = 0; iRowIter < iNum_of_Dims_of_Input_Image ; iRowIter++)
    {
        adTemp [ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)] =
                adTemp [ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)]*pdWeights_InputImage[iNeig_Pix_Idx]*pdWeights_InputImage[iNeig_Pix_Idx]/
                exp(-pow((pdDistTable[iColIter]/(scale)),0))/(K-1); // temp [dData x lenL]
    }
}
#else
for (iColIter = 0 ; iColIter < iLenLookupTable; iColIter++)
{
    iNeig_Pix_Idx = piLookUpTable[iColIter] + iPixelIter;
    for (iRowIter = 0; iRowIter < iNum_of_Dims_of_Input_Image ; iRowIter++)
    {
        adTemp [ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)] =
                adTemp [ROWCOL(iRowIter, iColIter, iNum_of_Dims_of_Input_Image)]*pdWeights_InputImage[iNeig_Pix_Idx]/(K-1); // temp [dData x lenL]
    }
}
#endif

// cov_i = temp*diag(weights(i+LookUpTable))*temp'/(k-1);
// 		Double_Mat_Multiply(adTemp, adDiag_of_Neig_Weights, iNum_of_Dims_of_Input_Image, iLenLookupTable,
// 										iLenLookupTable, iLenLookupTable, adCov_Curr_Pix_Interim);

Double_Mat_Multiply(adTemp, adTemp_Transpose,  iNum_of_Dims_of_Input_Image, iLenLookupTable,
        iLenLookupTable, iNum_of_Dims_of_Input_Image, adCov_Curr_Pix);

// I added this part to above loop, so we dont need this anymore
// 		for (iIter = 0 ; iIter < iNum_of_Dims_of_Input_Image*iNum_of_Dims_of_Input_Image ; iIter++)
// 		{
// 			adCov_Curr_Pix[iIter] = adCov_Curr_Pix[iIter]/(K-1);
// 		}


// C(i,:) = cov_i(uptiu);
/*
 * | cov_1(1) cov_1(3) cov_1(4) |
 *
 *
 * | cov_N(1) cov_N(3) cov_N(4) | //N rows * 3 Cols*/
for (iUpper_Triang_Iter = 0 ; iUpper_Triang_Iter < iLen_UpTriang_of_Cov ; iUpper_Triang_Iter ++)
{
    pdUpTriag_of_Cov_of_InputImage[ROWCOL(iPixelIter, iUpper_Triang_Iter, iNum_of_pixels_InputImage)] = adCov_Curr_Pix[piIndexes_of_UpTriang_of_Cov[iUpper_Triang_Iter]];
}

#if OUTPUT_COV == TRUE // Output Cov to a file in this form: [N x dData*(dData+1)/2]
for (iUpper_Triang_Iter = 0 ; iUpper_Triang_Iter < iLen_UpTriang_of_Cov ; iUpper_Triang_Iter ++)
{
    fprintf (fp_cov, "%g ", pdUpTriag_of_Cov_of_InputImage[ROWCOL(iPixelIter, iUpper_Triang_Iter, iNum_of_pixels_InputImage)]);
}
fprintf (fp_cov, "\n");
#endif
    } // end of for loop.

    free (adCov_Curr_Pix_Interim); free (adTemp_Transpose); // Since they are no longer needed for any further processing.
    free (adCov_Curr_Pix); free (aiNeig_of_MeshGridData); free (adTemp); free (adDiag_of_Neig_Weights );

    // If files were created for saving temp, temp', diag_weights, cov_i or cov, close them.
    #if OUTPUT_TEMP == TRUE
            fclose (fp_temp);
    #endif
            #if OUTPUT_TEMP_TRANSPOSE == TRUE
            fclose (fp_temp_tr);
    #endif
            #if OUTPUT_DIAG_OF_WEIGHTS == TRUE
            fclose (fp_diag_weights);
    #endif
            #if OUTPUT_COV_I == TRUE
            fclose (fp_cov_i);
    #endif

}