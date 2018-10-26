/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Apache License, Version 2.0,
* modified with the Commons Clause restriction.
*/

#ifndef __OP_CREATE_H
#define __OP_CREATE_H

#include "op.h"
#include "../../graph/entities/node.h"
#include "../../graph/entities/edge.h"
#include "../../parser/ast.h"
#include "../../resultset/resultset.h"
/* Creates new entities according to the CREATE clause. */

typedef struct {
    Edge *original_edge;
    Edge **original_edge_ref;
    char *src_node_alias;
    char *dest_node_alias;
} EdgeCreateCtx;

typedef struct {
    Node *original_node;
    Node **original_node_ref;
} NodeCreateCtx;

typedef struct {
    OpBase op;
    GraphContext *gc;
    AST_Query *ast;
    QueryGraph *qg;

    NodeCreateCtx *nodes_to_create;
    size_t node_count;

    EdgeCreateCtx *edges_to_create;
    size_t edge_count;

    Vector *created_nodes;
    Vector *created_edges;
    ResultSet *result_set;
} OpCreate;

OpBase* NewCreateOp(RedisModuleCtx *ctx, GraphContext *gc, AST_Query *ast, QueryGraph *qg, ResultSet *result_set);

OpResult OpCreateConsume(OpBase *opBase, Record *r);
OpResult OpCreateReset(OpBase *ctx);
void OpCreateFree(OpBase *ctx);

#endif
