//------------------------------------------------------------------------------
// GB_mex_select: C<M> = accum(C,select(A,k)) or select(A',k)
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2019, All Rights Reserved.
// http://suitesparse.com   See GraphBLAS/Doc/License.txt for license.

//------------------------------------------------------------------------------

// Apply a select operator to a matrix

#include "GB_mex.h"

#define USAGE "C = GB_mex_select (C, M, accum, op, A, Thunk, desc, test)"

#define FREE_ALL                        \
{                                       \
    GB_VECTOR_FREE (&Thunk) ;           \
    GB_MATRIX_FREE (&C) ;               \
    GB_MATRIX_FREE (&M) ;               \
    GB_MATRIX_FREE (&A) ;               \
    GrB_free (&desc) ;                  \
    GB_mx_put_global (true, 0) ;        \
}

void mexFunction
(
    int nargout,
    mxArray *pargout [ ],
    int nargin,
    const mxArray *pargin [ ]
)
{

    bool malloc_debug = GB_mx_get_global (true) ;
    GrB_Matrix C = NULL ;
    GrB_Matrix M = NULL ;
    GrB_Matrix A = NULL ;
    GrB_Descriptor desc = NULL ;
    int64_t k = 0 ;
    GrB_Vector Thunk = NULL ;

    // check inputs
    GB_WHERE (USAGE) ;
    if (nargout > 1 || nargin < 6 || nargin > 8)
    {
        mexErrMsgTxt ("Usage: " USAGE) ;
    }

    // get C (make a deep copy)
    #define GET_DEEP_COPY \
    C = GB_mx_mxArray_to_Matrix (pargin [0], "C input", true, true) ;   \
    if (nargin > 7 && C != NULL) C->nvec_nonempty = -1 ;
    #define FREE_DEEP_COPY GB_MATRIX_FREE (&C) ;
    GET_DEEP_COPY ;
    if (C == NULL)
    {
        FREE_ALL ;
        mexErrMsgTxt ("C failed") ;
    }

    mxClassID cclass = GB_mx_Type_to_classID (C->type) ;

    // get M (shallow copy)
    M = GB_mx_mxArray_to_Matrix (pargin [1], "M", false, false) ;
    if (M == NULL && !mxIsEmpty (pargin [1]))
    {
        FREE_ALL ;
        mexErrMsgTxt ("M failed") ;
    }

    // get A (shallow copy)
    A = GB_mx_mxArray_to_Matrix (pargin [4], "A input", false, true) ;
    if (A == NULL)
    {
        FREE_ALL ;
        mexErrMsgTxt ("A failed") ;
    }

    // get accum; default: NOP, default class is class(C)
    GrB_BinaryOp accum ;
    if (!GB_mx_mxArray_to_BinaryOp (&accum, pargin [2], "accum",
        GB_NOP_opcode, cclass, C->type == Complex, A->type == Complex))
    {
        FREE_ALL ;
        mexErrMsgTxt ("accum failed") ;
    }

    // get select operator; must be present
    GxB_SelectOp op ;
    if (!GB_mx_mxArray_to_SelectOp (&op, pargin [3], "op"))
    {
        FREE_ALL ;
        mexErrMsgTxt ("SelectOp failed") ;
    }

    // get k
    k = (int64_t) mxGetScalar (pargin [5]) ;

    // get desc
    if (!GB_mx_mxArray_to_Descriptor (&desc, PARGIN (6), "desc"))
    {
        FREE_ALL ;
        mexErrMsgTxt ("desc failed") ;
    }

    // just for testing
    if (nargin > 7)
    {
        if (M != NULL) M->nvec_nonempty = -1 ;
        A->nvec_nonempty = -1 ;
        C->nvec_nonempty = -1 ;
    }

    GrB_Vector_new (&Thunk, GrB_INT64, 1) ;
    GrB_Vector_setElement (Thunk, k, 0) ;
    GrB_Index ignore ;
    GrB_Vector_nvals (&ignore, Thunk) ;
    // GxB_print (Thunk, 3) ;

    // C<M> = accum(C,op(A))
    if (C->vdim == 1 && (desc == NULL || desc->in0 == GxB_DEFAULT))
    {
        // this is just to test the Vector version
        METHOD (GxB_select ((GrB_Vector) C, (GrB_Vector) M, accum, op,
            (GrB_Vector) A, Thunk, desc)) ;
    }
    else
    {
        METHOD (GxB_select (C, M, accum, op, A, Thunk, desc)) ;
    }

    // return C to MATLAB as a struct and free the GraphBLAS C
    pargout [0] = GB_mx_Matrix_to_mxArray (&C, "C output", true) ;

    FREE_ALL ;
}

