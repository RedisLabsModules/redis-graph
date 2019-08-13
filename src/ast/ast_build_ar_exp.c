/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "ast_build_ar_exp.h"
#include "../util/rmalloc.h"
#include "../arithmetic/funcs.h"
#include <assert.h>

static const char *_ASTOpToString(AST_Operator op) {
	switch(op) {
	case OP_PLUS:
		return "ADD";
		break;
	case OP_MINUS:
		return "SUB";
		break;
	case OP_MULT:
		return "MUL";
		break;
	case OP_DIV:
		return "DIV";
		break;
	case OP_CONTAINS:
		return "CONTAINS";
		break;
	case OP_STARTSWITH:
		return "STARTS WITH";
		break;
	case OP_ENDSWITH:
		return "ENDS WITH";
		break;
	case OP_AND:
		return "AND";
		break;
	case OP_OR:
		return "OR";
		break;
	case OP_XOR:
		return "XOR";
		break;
	case OP_NOT:
		return "NOT";
		break;
	case OP_GT:
		return "GT";
		break;
	case OP_GE:
		return "GE";
		break;
	case OP_LT:
		return "LT";
		break;
	case OP_LE:
		return "LE";
		break;
	case OP_EQUAL:
		return "EQ";
		break;
	case OP_NEQUAL:
		return "NEQ";
		break;
	case OP_MOD:
		return "MOD";
		break;
	case OP_POW:
		return "POW";
		break;
	default:
		assert(false && "Unhandled operator was specified in query");
		return NULL;
	}
}

static AR_ExpNode *AR_EXP_NewOpNodeFromAST(AST_Operator op, uint child_count) {
	const char *func_name = _ASTOpToString(op);
	return AR_EXP_NewOpNode(func_name, child_count);
}

static AR_ExpNode *_AR_EXP_FromApplyExpression(RecordMap *record_map,
											   const cypher_astnode_t *expr) {
	// TODO handle CYPHER_AST_APPLY_ALL_OPERATOR
	const cypher_astnode_t *func_node = cypher_ast_apply_operator_get_func_name(expr);
	const char *func_name = cypher_ast_function_name_get_value(func_node);
	// TODO When implementing calls like COUNT(DISTINCT), use cypher_ast_apply_operator_get_distinct()
	unsigned int arg_count = cypher_ast_apply_operator_narguments(expr);
	AR_ExpNode *op = AR_EXP_NewOpNode((char *)func_name, arg_count);

	for(unsigned int i = 0; i < arg_count; i ++) {
		const cypher_astnode_t *arg = cypher_ast_apply_operator_get_argument(expr, i);
		// Recursively convert arguments
		op->op.children[i] = AR_EXP_FromExpression(record_map, arg);
	}
	return op;
}

static AR_ExpNode *_AR_EXP_FromIdentifierExpression(RecordMap *record_map,
													const cypher_astnode_t *expr) {
	// Identifier referencing another record_map entity
	const char *alias = cypher_ast_identifier_get_name(expr);
	return AR_EXP_NewVariableOperandNode(record_map, alias, NULL);
}

static AR_ExpNode *_AR_EXP_FromPropertyExpression(RecordMap *record_map,
												  const cypher_astnode_t *expr) {
	// Identifier and property pair
	// Extract the entity alias from the property. Currently, the embedded
	// expression should only refer to the IDENTIFIER type.
	const cypher_astnode_t *prop_expr = cypher_ast_property_operator_get_expression(expr);
	assert(cypher_astnode_type(prop_expr) == CYPHER_AST_IDENTIFIER);
	const char *alias = cypher_ast_identifier_get_name(prop_expr);

	// Extract the property name
	const cypher_astnode_t *prop_name_node = cypher_ast_property_operator_get_prop_name(expr);
	const char *prop_name = cypher_ast_prop_name_get_value(prop_name_node);

	return AR_EXP_NewVariableOperandNode(record_map, alias, prop_name);
}

static AR_ExpNode *_AR_EXP_FromIntegerExpression(RecordMap *record_map,
												 const cypher_astnode_t *expr) {
	const char *value_str = cypher_ast_integer_get_valuestr(expr);
	char *endptr = NULL;
	int64_t l = strtol(value_str, &endptr, 0);
	assert(endptr[0] == 0);
	SIValue converted = SI_LongVal(l);
	return AR_EXP_NewConstOperandNode(converted);
}

static AR_ExpNode *_AR_EXP_FromFloatExpression(RecordMap *record_map,
											   const cypher_astnode_t *expr) {
	const char *value_str = cypher_ast_float_get_valuestr(expr);
	char *endptr = NULL;
	double d = strtod(value_str, &endptr);
	assert(endptr[0] == 0);
	SIValue converted = SI_DoubleVal(d);
	return AR_EXP_NewConstOperandNode(converted);
}

static AR_ExpNode *_AR_EXP_FromStringExpression(RecordMap *record_map,
												const cypher_astnode_t *expr) {
	const char *value_str = cypher_ast_string_get_value(expr);
	SIValue converted = SI_ConstStringVal((char *)value_str);
	return AR_EXP_NewConstOperandNode(converted);
}

static AR_ExpNode *_AR_EXP_FromTruExpression(RecordMap *record_map, const cypher_astnode_t *expr) {
	SIValue converted = SI_BoolVal(true);
	return AR_EXP_NewConstOperandNode(converted);
}

static AR_ExpNode *_AR_EXP_FromFalseExpression(RecordMap *record_map,
											   const cypher_astnode_t *expr) {
	SIValue converted = SI_BoolVal(false);
	return AR_EXP_NewConstOperandNode(converted);
}

static AR_ExpNode *_AR_EXP_FromNullExpression(RecordMap *record_map, const cypher_astnode_t *expr) {
	SIValue converted = SI_NullVal();
	return AR_EXP_NewConstOperandNode(converted);
}

static AR_ExpNode *_AR_EXP_FromUnaryOpExpression(RecordMap *record_map,
												 const cypher_astnode_t *expr) {
	const cypher_astnode_t *arg = cypher_ast_unary_operator_get_argument(expr); // CYPHER_AST_EXPRESSION
	const cypher_operator_t *operator = cypher_ast_unary_operator_get_operator(expr);
	if(operator == CYPHER_OP_UNARY_MINUS) {
		// This expression can be something like -3 or -a.val
		// TODO In the former case, we can construct a much simpler tree than this.
		AR_ExpNode *ar_exp = AR_EXP_FromExpression(record_map, arg);
		AR_ExpNode *op = AR_EXP_NewOpNodeFromAST(OP_MULT, 2);
		op->op.children[0] = AR_EXP_NewConstOperandNode(SI_LongVal(-1));
		op->op.children[1] = AR_EXP_FromExpression(record_map, arg);
		return op;
	} else if(operator == CYPHER_OP_UNARY_PLUS) {
		// This expression is something like +3 or +a.val.
		// I think the + can always be safely ignored.
		return AR_EXP_FromExpression(record_map, arg);
	} else if(operator == CYPHER_OP_NOT) {
		AR_ExpNode *op = AR_EXP_NewOpNodeFromAST(OP_NOT, 1);
		op->op.children[0] = AR_EXP_FromExpression(record_map, arg);
		return op;
	}
	assert(false);
	return NULL;
}

static AR_ExpNode *_AR_EXP_FromBinaryOpExpression(RecordMap *record_map,
												  const cypher_astnode_t *expr) {
	const cypher_operator_t *operator = cypher_ast_binary_operator_get_operator(expr);
	AST_Operator operator_enum = AST_ConvertOperatorNode(operator);
	// Arguments are of type CYPHER_AST_EXPRESSION
	AR_ExpNode *op = AR_EXP_NewOpNodeFromAST(operator_enum, 2);
	const cypher_astnode_t *lhs_node = cypher_ast_binary_operator_get_argument1(expr);
	op->op.children[0] = AR_EXP_FromExpression(record_map, lhs_node);
	const cypher_astnode_t *rhs_node = cypher_ast_binary_operator_get_argument2(expr);
	op->op.children[1] = AR_EXP_FromExpression(record_map, rhs_node);
	return op;
}

static AR_ExpNode *_AR_EXP_FromComparisonExpression(RecordMap *record_map,
													const cypher_astnode_t *expr) {
	// 1 < 2 = 2 <= 4
	AR_ExpNode *op;
	uint length = cypher_ast_comparison_get_length(expr);
	if(length > 1) {
		op = AR_EXP_NewOpNodeFromAST(OP_AND, length);
		for(uint i = 0; i < length; i++) {
			const cypher_operator_t *operator = cypher_ast_comparison_get_operator(expr, i);
			AST_Operator operator_enum = AST_ConvertOperatorNode(operator);
			const cypher_astnode_t *lhs_node = cypher_ast_comparison_get_argument(expr, i);
			const cypher_astnode_t *rhs_node = cypher_ast_comparison_get_argument(expr, i + 1);

			AR_ExpNode *inner_op = AR_EXP_NewOpNodeFromAST(operator_enum, 2);
			inner_op->op.children[0] = AR_EXP_FromExpression(record_map, lhs_node);
			inner_op->op.children[1] = AR_EXP_FromExpression(record_map, rhs_node);
			op->op.children[i] = inner_op;
		}
	} else {
		const cypher_operator_t *operator = cypher_ast_comparison_get_operator(expr, 0);
		AST_Operator operator_enum = AST_ConvertOperatorNode(operator);
		op = AR_EXP_NewOpNodeFromAST(operator_enum, 2);
		const cypher_astnode_t *lhs_node = cypher_ast_comparison_get_argument(expr, 0);
		const cypher_astnode_t *rhs_node = cypher_ast_comparison_get_argument(expr, 1);
		op->op.children[0] = AR_EXP_FromExpression(record_map, lhs_node);
		op->op.children[1] = AR_EXP_FromExpression(record_map, rhs_node);
	}
	return op;
}

static AR_ExpNode *_AR_EXP_FromCaseExpression(RecordMap *record_map, const cypher_astnode_t *expr) {
	//Determin number of child expressions:
	unsigned int arg_count;
	const cypher_astnode_t *expression = cypher_ast_case_get_expression(expr);
	unsigned int alternatives = cypher_ast_case_nalternatives(expr);

	/* Simple form: 2 * alternatives + default
	 * Generic form: 2 * alternatives + default */
	if(expression) arg_count = 1 + 2 * alternatives + 1;
	else arg_count = 2 * alternatives + 1;

	// Create Expression and child expressions
	AR_ExpNode *op = AR_EXP_NewOpNode("case", arg_count);

	// Value to compare against
	int offset = 0;
	if(expression != NULL) {
		op->op.children[offset++] = AR_EXP_FromExpression(record_map, expression);
	}

	// Alternatives
	for(uint i = 0; i < alternatives; i++) {
		const cypher_astnode_t *predicate = cypher_ast_case_get_predicate(expr, i);
		op->op.children[offset++] = AR_EXP_FromExpression(record_map, predicate);
		const cypher_astnode_t *value = cypher_ast_case_get_value(expr, i);
		op->op.children[offset++] = AR_EXP_FromExpression(record_map, value);
	}

	// Default value.
	const cypher_astnode_t *deflt = cypher_ast_case_get_default(expr);
	if(deflt == NULL) {
		// Default not specified, use NULL.
		op->op.children[offset] = AR_EXP_NewConstOperandNode(SI_NullVal());
	} else {
		op->op.children[offset] = AR_EXP_FromExpression(record_map, deflt);
	}

	return op;
}

AR_ExpNode *AR_EXP_FromExpression(RecordMap *record_map, const cypher_astnode_t *expr) {
	const cypher_astnode_type_t type = cypher_astnode_type(expr);

	/* Function invocations */
	if(type == CYPHER_AST_APPLY_OPERATOR || type == CYPHER_AST_APPLY_ALL_OPERATOR) {
		return _AR_EXP_FromApplyExpression(record_map, expr);
		/* Variables (full nodes and edges, UNWIND artifacts */
	} else if(type == CYPHER_AST_IDENTIFIER) {
		return _AR_EXP_FromIdentifierExpression(record_map, expr);
		/* Entity-property pair */
	} else if(type == CYPHER_AST_PROPERTY_OPERATOR) {
		return _AR_EXP_FromPropertyExpression(record_map, expr);
		/* SIValue constant types */
	} else if(type == CYPHER_AST_INTEGER) {
		return _AR_EXP_FromIntegerExpression(record_map, expr);
	} else if(type == CYPHER_AST_FLOAT) {
		return _AR_EXP_FromFloatExpression(record_map, expr);
	} else if(type == CYPHER_AST_STRING) {
		return _AR_EXP_FromStringExpression(record_map, expr);
	} else if(type == CYPHER_AST_TRUE) {
		return _AR_EXP_FromTruExpression(record_map, expr);
	} else if(type == CYPHER_AST_FALSE) {
		return _AR_EXP_FromFalseExpression(record_map, expr);
	} else if(type == CYPHER_AST_NULL) {
		return _AR_EXP_FromNullExpression(record_map, expr);
		/* Handling for unary operators (-5, +a.val) */
	} else if(type == CYPHER_AST_UNARY_OPERATOR) {
		return _AR_EXP_FromUnaryOpExpression(record_map, expr);
	} else if(type == CYPHER_AST_BINARY_OPERATOR) {
		return _AR_EXP_FromBinaryOpExpression(record_map, expr);
	} else if(type == CYPHER_AST_COMPARISON) {
		return _AR_EXP_FromComparisonExpression(record_map, expr);
	} else if(type == CYPHER_AST_CASE) {
		return _AR_EXP_FromCaseExpression(record_map, expr);
	} else {
		/*
		   Unhandled types:
		   CYPHER_AST_COLLECTION
		   CYPHER_AST_CASE
		   CYPHER_AST_LABELS_OPERATOR
		   CYPHER_AST_LIST_COMPREHENSION
		   CYPHER_AST_MAP
		   CYPHER_AST_MAP_PROJECTION
		   CYPHER_AST_PARAMETER
		   CYPHER_AST_PATTERN_COMPREHENSION
		   CYPHER_AST_SLICE_OPERATOR
		   CYPHER_AST_REDUCE
		   CYPHER_AST_SUBSCRIPT_OPERATOR
		*/
		printf("Encountered unhandled type '%s'\n", cypher_astnode_typestr(type));
		assert(false);
	}

	assert(false);
	return NULL;
}
