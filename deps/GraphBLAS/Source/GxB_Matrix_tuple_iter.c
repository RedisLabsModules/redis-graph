//------------------------------------------------------------------------------
// GxB_MatrixTupleIter: Iterates over matrix none zero values
//------------------------------------------------------------------------------

#include "GB.h"

// Sets iterator as depleted.
static inline void _EmptyIterator
(
	GxB_MatrixTupleIter *iter   // Iterator to deplete.
) {
	iter->nvals = 0;
	iter->nnz_idx = 0;
}

static inline void _MatrixTupleIter_init
(
	GxB_MatrixTupleIter *iter,      // iterator to init
	GrB_Matrix A,                   // matrix to iterate over
	int sparsity_type               // sparsity type of the matrix
) {
	GrB_Index nrows;
	GrB_Matrix_nrows(&nrows, A) ;

	GrB_Matrix_nvals(&iter->nvals, A) ;
	iter->A = A ;
	iter->nnz_idx = 0 ;
	iter->row_idx = 0 ;
	iter->nrows = nrows;
	iter->p = A->p[0] ;
	iter->sparsity_type = sparsity_type;
	if(sparsity_type == GxB_HYPERSPARSE) {
		iter->sparse_row_idx = 0;
		iter->nrows_non_empty = A->nvec;
	}
}

// Create a new iterator
GrB_Info GxB_MatrixTupleIter_new
(
	GxB_MatrixTupleIter **iter,     // iterator to create
	GrB_Matrix A,                   // matrix to iterate over
	int sparsity_type               // sparsity type of the matrix
) {
	GB_WHERE(A, "GxB_MatrixTupleIter_iterate_row (A)") ;
	GB_RETURN_IF_NULL_OR_FAULTY(A) ;

	// make sure matrix is not bitmap or full
	GxB_set(A, GxB_SPARSITY_CONTROL, sparsity_type) ;

	*iter = GB_MALLOC(1, GxB_MatrixTupleIter) ;
	_MatrixTupleIter_init(*iter, A, sparsity_type);
	return (GrB_SUCCESS) ;
}

// finds the row index in Ah for HYPERSPARSE matrix,
// returns true if found else false.
static inline bool _find_row_index_in_Ah
(
	GxB_MatrixTupleIter *iter,
	GrB_Index rowIdx,
	GrB_Index *result
) {
	if(!result || !iter) return false;
	GrB_Index l = 0, h = iter->nrows_non_empty - 1, m;
	// find the index using binary search
	while(l <= h) {
		m = (l + h)/2;
		int64_t val = iter->A->h[m];
		if(val == rowIdx) {
			*result = m;
			return true;
		} else if(val < rowIdx) {
			l = m + 1;
		} else { // val > rowIdx
			h = m - 1;
		}
	}

	return false;
}

GrB_Info GxB_MatrixTupleIter_iterate_row
(
	GxB_MatrixTupleIter *iter,
	GrB_Index rowIdx
) {
	GB_WHERE1("GxB_MatrixTupleIter_iterate_row (iter, rowIdx)") ;
	GB_RETURN_IF_NULL(iter) ;

	// Deplete iterator, should caller ignore returned error.
	_EmptyIterator(iter) ;

	if(rowIdx < 0 || rowIdx >= iter->nrows) {
		GB_ERROR(GrB_INVALID_INDEX,
				 "Row index " GBu " out of range; must be < " GBu,
				 rowIdx, iter->nrows) ;
	}

	GrB_Index rowIdx_sparse;
	bool hypersparse_and_row_is_empty = false;
	if(iter->sparsity_type == GxB_SPARSE) {
		rowIdx_sparse = rowIdx;
	} else {  // GxB_HYPERSPARSE - find the row_index in the sparse vector
		if(!iter->nrows_non_empty || !_find_row_index_in_Ah(iter, rowIdx, &rowIdx_sparse)) hypersparse_and_row_is_empty = true;
	}

	// If hypersparse and row is empty setting nvals = 0 cause iterator.next should return depleted
	iter->nvals = hypersparse_and_row_is_empty ? 0 : iter->A->p[rowIdx_sparse + 1];
	iter->nnz_idx = iter->A->p[rowIdx_sparse];
	iter->row_idx = rowIdx;
	if(iter->sparsity_type == GxB_HYPERSPARSE) {
		iter->sparse_row_idx = rowIdx_sparse;
		iter->nrows_non_empty = iter->A->nvec;
	}
	iter->p = 0;
	return (GrB_SUCCESS) ;
}

GrB_Info GxB_MatrixTupleIter_jump_to_row
(
	GxB_MatrixTupleIter *iter,
	GrB_Index rowIdx
) {
	GB_WHERE1("GxB_MatrixTupleIter_jump_to_row (iter, rowIdx)") ;
	GB_RETURN_IF_NULL(iter) ;

	// Deplete iterator, should caller ignore returned error.
	_EmptyIterator(iter) ;

	if(rowIdx < 0 || rowIdx >= iter->nrows) {
		GB_ERROR(GrB_INVALID_INDEX,
				 "Row index " GBu " out of range; must be < " GBu,
				 rowIdx, iter->nrows) ;
	}

	GrB_Index _rowIdx = rowIdx;
	if(iter->sparsity_type == GxB_HYPERSPARSE) {
		GrB_Index rowIdx_sparse;
		if(!_find_row_index_in_Ah(iter, rowIdx, &rowIdx_sparse)) {
			GB_ERROR (GrB_INVALID_INDEX,
				"Row index " GBu " doesn't exist in the hypersparse matrix, row might be empty",
				rowIdx ) ;
		}
		_rowIdx = rowIdx_sparse;
		iter->sparse_row_idx = rowIdx_sparse;
		iter->nrows_non_empty = iter->A->nvec;
	}
	GrB_Matrix_nvals(&(iter->nvals), iter->A) ;

	iter->p        =  0                   ;
	iter->nnz_idx  =  iter->A->p[_rowIdx]  ;
	iter->row_idx  =  rowIdx              ;

	return (GrB_SUCCESS) ;
}

// finds the start row index in Ah for HYPERSPARSE matrix, 
// returns true if found else false.
static inline bool _find_start_row_index_in_Ah
(
	GxB_MatrixTupleIter *iter,
	GrB_Index rowIdx,
	GrB_Index *result
) {
	if(!result || !iter) return false;
	GrB_Index l = 0, h = iter->nrows_non_empty - 1, m;
	// find the index using binary search
	while(l < h) {
		m = (l + h)/2;
		int64_t val = iter->A->h[m];
		if(val == rowIdx) {
			*result = m;
			return true;
		} else if(val < rowIdx) {
			l = m + 1;
		} else { // val > rowIdx
			h = m;
		}
	}

	if(iter->A->h[m] < rowIdx) return false;
	*result = l;
	return true;
}

// finds the end row index in Ah for HYPERSPARSE matrix,
// returns true if found else false.
static inline bool _find_end_row_index_in_Ah
(
	GxB_MatrixTupleIter *iter,
	GrB_Index rowIdx,
	GrB_Index *result
) {
	if(!result || !iter) return false;
	GrB_Index l = 0, h = iter->nrows_non_empty - 1, m;
	// find the index using binary search
	while(l < h) {
		m = (l + h + 1)/2;  // Ceiling the result cause we are assign l = m in case val < rowIdx
		int64_t val = iter->A->h[m];
		if(val == rowIdx) {
			*result = m;
			return true;
		} else if(val < rowIdx) {
			l = m;
		} else { // val > rowIdx
			h = m - 1;
		}
	}

	if(iter->A->h[m] > rowIdx) return false;
	*result = h;
	return true;
}

GrB_Info GxB_MatrixTupleIter_iterate_range
(
	GxB_MatrixTupleIter *iter,  // iterator to use
	GrB_Index startRowIdx,      // row index to start with
	GrB_Index endRowIdx         // row index to finish with
) {
	GB_WHERE1("GxB_MatrixTupleIter_iterate_range (iter, startRowIdx, endRowIdx)") ;
	GB_RETURN_IF_NULL(iter) ;

	// Deplete iterator, should caller ignore returned error.
	_EmptyIterator(iter) ;

	if(startRowIdx < 0 || startRowIdx >= iter->nrows) {
		GB_ERROR(GrB_INVALID_INDEX,
				 "row index " GBu " out of range; must be < " GBu,
				 startRowIdx, iter->nrows) ;
	}

	if(startRowIdx > endRowIdx) {
		GB_ERROR(GrB_INVALID_INDEX,
				 "row index " GBu " must be > " GBu,
				 startRowIdx, endRowIdx) ;
	}

	GrB_Index startRowIdx_sparse = startRowIdx;
	GrB_Index endRowIdx_sparse = endRowIdx;
	bool hypersparse_no_more_rows = false;

	if(iter->sparsity_type == GxB_HYPERSPARSE) {
		iter->nrows_non_empty = iter->A->nvec;
		if(!iter->nrows_non_empty || !_find_start_row_index_in_Ah(iter, startRowIdx, &startRowIdx_sparse)
		|| !_find_end_row_index_in_Ah(iter, endRowIdx, &endRowIdx_sparse))
			hypersparse_no_more_rows = true;
		iter->sparse_row_idx = startRowIdx_sparse;
	}
	iter->nnz_idx = iter->A->p[startRowIdx_sparse] ;
	iter->row_idx = startRowIdx ;
	if(hypersparse_no_more_rows) iter->nvals = 0; // simulate depletion of the iterator
	else if(endRowIdx_sparse < iter->nrows) iter->nvals = iter->A->p[endRowIdx_sparse + 1] ;
	else GrB_Matrix_nvals(&(iter->nvals), iter->A) ;

	return (GrB_SUCCESS) ;
}

// Advance iterator
GrB_Info GxB_MatrixTupleIter_next
(
	GxB_MatrixTupleIter *iter,      // iterator to consume
	GrB_Index *row,                 // optional output row index
	GrB_Index *col,                 // optional output column index
	bool *depleted                  // indicate if iterator depleted
) {
	GB_WHERE1("GxB_MatrixTupleIter_next (iter, row, col, depleted)") ;
	GB_RETURN_IF_NULL(iter) ;
	GB_RETURN_IF_NULL(depleted) ;
	GrB_Index nnz_idx = iter->nnz_idx ;

	if(nnz_idx >= iter->nvals) {
		*depleted = true ;
		return (GrB_SUCCESS) ;
	}

	GrB_Matrix A = iter->A ;

	//--------------------------------------------------------------------------
	// extract the column indices
	//--------------------------------------------------------------------------

	if(col)
		*col = A->i[nnz_idx] ;

	//--------------------------------------------------------------------------
	// extract the row indices
	//--------------------------------------------------------------------------

	const int64_t *Ap = A->p;
	int64_t i;
	GrB_Index nrows;
	if(iter->sparsity_type == GxB_SPARSE) {
		i = iter->row_idx;
		nrows = iter->nrows;
	} else { // GxB_HYPERSPARSE
		i = iter->sparse_row_idx;
		nrows = iter->nrows_non_empty;
	}

	for(; i < nrows; i++) {
		int64_t p = iter->p + Ap[i];
		if(p < Ap[i + 1]) {
			iter->p++ ;
			if(row)
				*row = (iter->sparsity_type == GxB_SPARSE) ? i : A->h[i];
			break;
		}
		iter->p = 0 ;
	}

	if(iter->sparsity_type == GxB_SPARSE) {
		iter->row_idx = i;
	} else { // GxB_HYPERSPARSE
		iter->row_idx = A->h[i];
		iter->sparse_row_idx = i;
	}

	iter->nnz_idx++ ;

	*depleted = false ;
	return (GrB_SUCCESS) ;
}

// Reset iterator
GrB_Info GxB_MatrixTupleIter_reset
(
	GxB_MatrixTupleIter *iter       // iterator to reset
) {
	GB_WHERE1("GxB_MatrixTupleIter_reset (iter)") ;
	GB_RETURN_IF_NULL(iter) ;
	_MatrixTupleIter_init(iter, iter->A, iter->sparsity_type);
	return (GrB_SUCCESS) ;
}

// Update iterator to scan given matrix
GrB_Info GxB_MatrixTupleIter_reuse
(
	GxB_MatrixTupleIter *iter,      // iterator to update
	GrB_Matrix A,                   // matrix to scan
	int sparsity_type               // sparsity type of the matrix
) {
	GB_WHERE(A, "GxB_MatrixTupleIter_reuse (iter, A)") ;
	GB_RETURN_IF_NULL(iter) ;
	GB_RETURN_IF_NULL_OR_FAULTY(A) ;

	// make sure matrix is not bitmap or full
	GxB_set(A, GxB_SPARSITY_CONTROL, GxB_SPARSE) ;

	_MatrixTupleIter_init(iter, A, sparsity_type);
	return (GrB_SUCCESS) ;
}

// Release iterator
GrB_Info GxB_MatrixTupleIter_free
(
	GxB_MatrixTupleIter *iter       // iterator to free
) {
	GB_WHERE1("GxB_MatrixTupleIter_free (iter)") ;
	GB_RETURN_IF_NULL(iter) ;
	GB_FREE(iter) ;
	return (GrB_SUCCESS) ;
}

