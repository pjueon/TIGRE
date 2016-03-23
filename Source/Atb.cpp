
#include "tmwtypes.h"
#include "mex.h"
#include "matrix.h"
#include "voxel_backprojection.hpp"
#include "voxel_backprojection2.hpp"
#include <string.h>
// #include <time.h>





/**
 * MEX gateway
 *
 * This function takes data from MATLAB and passes it to the MEX code.
 * It checks and casts the inputs and prepares teh outputs for MATLAB.
 *
 *
 */

void mexFunction(int  nlhs , mxArray *plhs[],
        int nrhs, mxArray const *prhs[]){
    
    //Check amount of inputs
    if (nrhs<3 ||nrhs>4) {
        mexErrMsgIdAndTxt("CBCT:MEX:Atb:InvalidInput", "Wrong number of inputs provided");
    }
    /*
     ** 4rd argument is matched or un matched.
     */
    bool krylov_proj=false; // Caled krylov, because I designed it for krylov case....
    if (nrhs==4){
        if ( mxIsChar(prhs[3]) != 1)
            mexErrMsgIdAndTxt( "CBCT:MEX:Atb:InvalidInput","4rd input shoudl be a string");
        
        /* copy the string data from prhs[0] into a C string input_ buf.    */
        char *krylov = mxArrayToString(prhs[3]);
        if (strcmp(krylov,"FDK") && strcmp(krylov,"matched"))
            mexErrMsgIdAndTxt( "CBCT:MEX:Atb:InvalidInput","4rd input shoudl be either 'FDK' or 'matched'");
        else
            if (!strcmp(krylov,"matched"))
                krylov_proj=true;
    }
    /*
     ** Third argument: angle of projection.
     */
    size_t mrows,ncols;
    
    mrows = mxGetM(prhs[2]);
    ncols = mxGetN(prhs[2]);
    
    if( !mxIsDouble(prhs[2]) || mxIsComplex(prhs[2]) ||
            !(mrows==1) ) {
        mexErrMsgIdAndTxt( "CBCT:MEX:Atb:input",
                "Input alpha must be a noncomplex array.");
    }
    size_t nalpha=ncols;
    mxArray const * const ptralphas=prhs[2];
    
    double const * const alphasM= static_cast<double const *>(mxGetData(ptralphas));
    // just copy paste the data to a float array
    float  *  alphas= (float*)malloc(nalpha*sizeof(float));
    for (int i=0;i<nalpha;i++)
        alphas[i]=(float)alphasM[i];
    
    /**
     * First input: The projections
     */
    
    // First input should be b from (Ax=b) i.e. the projections.
    mxArray const * const image = prhs[0];                 // Get pointer of the data
    mwSize const numDims = mxGetNumberOfDimensions(image); // Get numer of Dimensions of input matrix.
    // Image should be dim 3
    if (!(numDims==3 && nalpha>1) && !(numDims==2 && nalpha==1) ){
        mexErrMsgIdAndTxt("CBCT:MEX:Atb:InvalidInput",  "Projection data is not the rigth size");
    }
    // Now that input is ok, parse it to C data types.
    // NOTE: while Number of dimensions is the size of the matrix in Matlab, the data is 1D row-wise mayor.
    
    // We need a float image, and, unfortunatedly, the only way of casting it is by value
    const mwSize *size_proj= mxGetDimensions(image); //get size of image
    mrows = mxGetM(image);
    ncols = mxGetN(image);
    size_t size_proj2;
    if (nalpha==1)
        size_proj2=1;
    else
        size_proj2=size_proj[2];
    
    
    double const * const imgaux = static_cast<double const *>(mxGetData(image));
    
    
    
    float *  img = (float*)malloc(size_proj[0] *size_proj[1] *size_proj2* sizeof(float));
    
    const int size0 = size_proj[0];
	const int size1 = size_proj[1];
	const int size2 = size_proj2;
    // Permute(imgaux,[2 1 3]);

    for (int j = 0; j < size2; j++)
    {
        int jOffset = j*size0*size1;
        for (int k = 0; k < size0; k++)
        {
            int kOffset1 = k*size1;
            for (int i = 0; i < size1; i++)
            {
                int iOffset2 = i*size0;
                img[i + jOffset + kOffset1] = (float)imgaux[iOffset2 + jOffset + k];
            }
        }
    }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Second input: Geometry structure
     */
    
    
    // IMPORTANT-> Make sure Matlab creates the struct in this order.
    const char *fieldnames[11];
    fieldnames[0] = "nVoxel";
    fieldnames[1] = "sVoxel";
    fieldnames[2] = "dVoxel";
    fieldnames[3] = "nDetector";
    fieldnames[4] = "sDetector";
    fieldnames[5] = "dDetector";
    fieldnames[6] = "DSD";
    fieldnames[7] = "DSO";
    fieldnames[8] = "offOrigin";
    fieldnames[9] = "offDetector";
    fieldnames[10]= "accuracy";
    
    // Make sure input is structure
    if(!mxIsStruct(prhs[1]))
        mexErrMsgIdAndTxt( "CBCT:MEX:Atb:InvalidInput",
                "Second input must be a structure.");
    // Check number of fields
    int nfields = mxGetNumberOfFields(prhs[1]);
    if (nfields < 10 || nfields >11 )
        mexErrMsgIdAndTxt("CBCT:MEX:Atb:InvalidInput","There are missing or extra fields in the geometry");
    
    mxArray    *tmp;
    bool offsetAllOrig=false;
    bool offsetAllDetec=false;
    for(int ifield=0; ifield<nfields; ifield++) {
        tmp=mxGetField(prhs[1],0,fieldnames[ifield]);
        if(tmp==NULL){
            mexPrintf("%s number: %d %s \n", "FIELD",ifield+1, fieldnames[ifield]);
            mexErrMsgIdAndTxt( "CBCT:MEX:Atb:InvalidInput",
                    "Above field is missing. Check spelling. ");
        }
        switch(ifield){
            
            // cases where we want 3 input arrays.
            case 0:case 1:case 2:
                mrows = mxGetM(tmp);
                ncols = mxGetN(tmp);
                if (mrows!=3 || ncols!=1){
                    mexPrintf("%s %s \n", "FIELD: ", fieldnames[ifield]);
                    mexErrMsgIdAndTxt( "CBCT:MEX:Atb:InvalidInput",
                            "Above field has wrong size! Should be 3x1!");
                }
                break;
                // this ones should be 2x1
            case 3:case 4:case 5:
                mrows = mxGetM(tmp);
                ncols = mxGetN(tmp);
                if (mrows!=2 || ncols!=1){
                    mexPrintf("%s %s \n", "FIELD: ", fieldnames[ifield]);
                    mexErrMsgIdAndTxt( "CBCT:MEX:Atb:InvalidInput",
                            "Above field has wrong size! Should be 2x1!");
                }
                break;
                // this ones should be 1x1
            case 6:case 7:case 10:
                mrows = mxGetM(tmp);
                ncols = mxGetN(tmp);
                if (mrows!=1 || ncols!=1){
                    mexPrintf("%s %s \n", "FIELD: ", fieldnames[ifield]);
                    mexErrMsgIdAndTxt("CBCT:MEX:Atb:InvalidInput",
                            "Above field has wrong size! Should be 1x1!");
                }
                break;
            case 8:
                mrows = mxGetM(tmp);
                ncols = mxGetN(tmp);
                if (mrows!=3 || ( ncols!=1&& ncols!=nalpha) ){
                    mexPrintf("%s %s \n", "FIELD: ", fieldnames[ifield]);
                    mexErrMsgIdAndTxt( "CBCT:MEX:Ax:inputsize",
                            "Above field has wrong size! Should be 3x1 or 3xlength(angles)!");
                    
                }
                
                if (ncols==nalpha)
                    offsetAllOrig=true;
                
                break;
                
            case 9:
                mrows = mxGetM(tmp);
                ncols = mxGetN(tmp);
                if (mrows!=2 || ( ncols!=1&& ncols!=nalpha)){
                    mexPrintf("%s %s \n", "FIELD: ", fieldnames[ifield]);
                    mexErrMsgIdAndTxt( "CBCT:MEX:Ax:inputsize",
                            "Above field has wrong size! Should be 2x1 or 3xlength(angles)!");
                    
                }
                
                if (ncols==nalpha)
                    offsetAllDetec=true;
                
                break;
            default:
                mexErrMsgIdAndTxt( "CBCT:MEX:Atb:InvalidInput",
                        "Something wrong happened. Ensure Geometric struct has correct amount of inputs.");
        }
        
    }
    // Now we know that all the input struct is good! Parse it from mxArrays to
    // C structures that MEX can understand.
    
    double * nVoxel, *nDetec; //we need to cast these to int
    double * sVoxel, *dVoxel,*sDetec,*dDetec, *DSO, *DSD,*offOrig,*offDetec;
    double *acc;
    Geometry geo;
    int c;
    geo.unitX=1;geo.unitY=1;geo.unitZ=1;
    for(int ifield=0; ifield<nfields; ifield++) {
        tmp=mxGetField(prhs[1],0,fieldnames[ifield]);
        switch(ifield){
            case 0:
                nVoxel=(double *)mxGetData(tmp);
                // copy data to MEX memory
                geo.nVoxelX=(int)nVoxel[0];
                geo.nVoxelY=(int)nVoxel[1];
                geo.nVoxelZ=(int)nVoxel[2];
                break;
            case 1:
                sVoxel=(double *)mxGetData(tmp);
                geo.sVoxelX=(float)sVoxel[0];
                geo.sVoxelY=(float)sVoxel[1];
                geo.sVoxelZ=(float)sVoxel[2];
                break;
            case 2:
                dVoxel=(double *)mxGetData(tmp);
                geo.dVoxelX=(float)dVoxel[0];
                geo.dVoxelY=(float)dVoxel[1];
                geo.dVoxelZ=(float)dVoxel[2];
                break;
            case 3:
                nDetec=(double *)mxGetData(tmp);
                geo.nDetecU=(int)nDetec[0];
                geo.nDetecV=(int)nDetec[1];
                break;
            case 4:
                sDetec=(double *)mxGetData(tmp);
                geo.sDetecU=(float)sDetec[0];
                geo.sDetecV=(float)sDetec[1];
                break;
            case 5:
                dDetec=(double *)mxGetData(tmp);
                geo.dDetecU=(float)dDetec[0];
                geo.dDetecV=(float)dDetec[1];
                break;
            case 6:
                DSD=(double *)mxGetData(tmp);
                geo.DSD=(float)DSD[0];
                break;
            case 7:
                DSO=(double *)mxGetData(tmp);
                geo.DSO=(float)DSO[0];
            case 8:
                
                geo.offOrigX=(float*)malloc(nalpha * sizeof(float));
                geo.offOrigY=(float*)malloc(nalpha * sizeof(float));
                geo.offOrigZ=(float*)malloc(nalpha * sizeof(float));
                
                offOrig=(double *)mxGetData(tmp);
                
                for (int i=0;i<nalpha;i++){
                    if (offsetAllOrig)
                        c=i;
                    else
                        c=0;
                    geo.offOrigX[i]=(float)offOrig[0+3*c];
                    geo.offOrigY[i]=(float)offOrig[1+3*c];
                    geo.offOrigZ[i]=(float)offOrig[2+3*c];
                }
                break;
            case 9:
                geo.offDetecU=(float*)malloc(nalpha * sizeof(float));
                geo.offDetecV=(float*)malloc(nalpha * sizeof(float));
                
                offDetec=(double *)mxGetData(tmp);
                for (int i=0;i<nalpha;i++){
                    if (offsetAllDetec)
                        c=i;
                    else
                        c=0;
                    geo.offDetecU[i]=(float)offDetec[0+2*c];
                    geo.offDetecV[i]=(float)offDetec[1+2*c];
                }
                break;
            case 10:
                acc=(double*)mxGetData(tmp);
                geo.accuracy=(float)acc[0];
                break;
            default:
                mexErrMsgIdAndTxt( "CBCT:MEX:Atb:unknown","This shoudl not happen. Weird");
                break;
                
        }
    }
    if (nfields==10)
        geo.accuracy=0.5;
    
    
    /*
     * allocate memory for the output
     */
    
    float* result = (float*)malloc(geo.nVoxelX *geo.nVoxelY*geo.nVoxelZ*sizeof(float));
    
    
    /*
     * Call the CUDA kernel
     */
    if (krylov_proj){
        voxel_backprojection2(img,geo,result,alphas,nalpha);
    }
    else
        voxel_backprojection(img,geo,result,alphas,nalpha);
    /*
     * Prepare the outputs
     */
    mwSize* imgsize;
    imgsize[0]=geo.nVoxelX;
    imgsize[1]=geo.nVoxelY;
    imgsize[2]=geo.nVoxelZ;
    
    plhs[0] = mxCreateNumericArray(3,imgsize, mxDOUBLE_CLASS, mxREAL);
    double *outImage = mxGetPr(plhs[0]);
    
    
    for (int i=0; i<geo.nVoxelX*geo.nVoxelY*geo.nVoxelZ ;i++)
        outImage[i]= (double)result[i];
    
    /*
     * Free memory and out
     */
//     for (int i=0; i<geo.nVoxelX ;i++){
//         for (int j=0; j<geo.nVoxelY ;j++)
//              free (result[i][j]);
//         free(result[i]);
//     }
//     mexPrintf("%lf \n",result[0]);
//     mexCallMATLAB(0,NULL,1,&plhs[0],"disp");
    free(result);
    
    free(img);
    
    return;
}

