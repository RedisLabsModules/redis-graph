/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#pragma once

#include "../redismodule.h"
#include "../util/vector.h"
#include "../ast/ast_shared.h"
#include "../../deps/rax/rax.h"
#include "../execution_plan/record.h"
#include "../arithmetic/arithmetic_expression.h"

#define FILTER_FAIL 0
#define FILTER_PASS 1

/* Nodes within the filter tree are one of two types
 * Either a predicate node or a condition node. */
typedef enum {
    FT_N_EXP,   // Expression node.
	FT_N_PRED,  // Predicate node.
	FT_N_COND,  // Conditional node.
} FT_FilterNodeType;

struct FT_FilterNode;

/* The FT_ExpressionNode represent a leaf node within the filter tree
 * it holds a single boolean arithmetic expression to evaluate 
 * e.g. `WHERE false` in which case `false` is the expression. */
typedef struct {
    AR_ExpNode *exp;    /* Boolean expression to evaluate. */
} FT_ExpressionNode;

/* The FT_PredicateNode represents a leaf node within the filter tree 
 * it holds an operator: [<. <=, =, >, >=] 
 * a left and right hand-side arithmetic expressions
 * which are evaluated and compared to one another using the operator. */
typedef struct {
	AR_ExpNode *lhs;
	AR_ExpNode *rhs;
	AST_Operator op;	/* Can validly be an operation (<, <=, =, =>, >, <>, maybe NOT). */
} FT_PredicateNode;

/* The FT_ConditionNode is a top level node in the filter tree 
 * it holds a conditional operator: [OR, AND] 
 * a left and right hand-side filter nodes which can be further extended */
typedef struct {
	struct FT_FilterNode *left;
	struct FT_FilterNode *right;
	AST_Operator op;	/* Can validly be OR, AND (and later, XOR and maybe NOT) */
} FT_ConditionNode;

/* All nodes within the filter tree are of type FT_FilterNode. */
struct FT_FilterNode {
	union {
        FT_ExpressionNode exp;
		FT_PredicateNode pred;
		FT_ConditionNode cond;
	};
	FT_FilterNodeType t;	/* Determines the actual type of this node. */
};

typedef struct FT_FilterNode FT_FilterNode;

/* Determines if node is a predicate node. */
int IsNodePredicate(const FT_FilterNode *node);

/* Determines if node is an expression node. */
int IsNodeExpression(const FT_FilterNode *node);

/* Appends a left hand-side node to root. */
FT_FilterNode *AppendLeftChild(FT_FilterNode *root, FT_FilterNode *child);

/* Appends a right hand-side node to root. */
FT_FilterNode *AppendRightChild(FT_FilterNode *root, FT_FilterNode *child);

/* Creates a new expression node. */
FT_FilterNode *FilterTree_CreateExpressionFilter(AR_ExpNode *exp);

/* Creates a new predicate node. */
FT_FilterNode *FilterTree_CreatePredicateFilter(AST_Operator op, AR_ExpNode *lhs, AR_ExpNode *rhs);

/* Creates a new condition node. */
FT_FilterNode *FilterTree_CreateConditionFilter(AST_Operator op);

/* Runs val through the filter tree. */
int FilterTree_applyFilters(const FT_FilterNode *root, const Record r);

/* Extract every modified record ID mentioned in the tree
 * without duplications. */
rax *FilterTree_CollectModified(const FT_FilterNode *root);

/* Break filter tree into sub filter trees as follows:
 * sub trees under an OR operator are returned,
 * sub trees under an AND operator are broken down to the smallest
 * components possible following the two rules above. */
Vector *FilterTree_SubTrees(const FT_FilterNode *root);

/* Remove NOT nodes by applying DeMorgan laws */
void FilterTree_DeMorgan(FT_FilterNode **root);

/* Prints tree. */
void FilterTree_Print(const FT_FilterNode *root);

/* Free tree. */
void FilterTree_Free(FT_FilterNode *root);
