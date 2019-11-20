/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "op_merge.h"
#include "../../query_ctx.h"
#include "../../schema/schema.h"
#include "../../arithmetic/arithmetic_expression.h"
#include <assert.h>

/* Forward declarations. */
static OpResult MergeInit(OpBase *opBase);
static Record MergeConsume(OpBase *opBase);
static void MergeFree(OpBase *opBase);

//------------------------------------------------------------------------------
// ON MATCH logic
//------------------------------------------------------------------------------
// Perform necessary index updates.
static void _UpdateIndices(GraphContext *gc, Node *n) {
	int label_id = Graph_GetNodeLabel(gc->g, n->entity->id);
	if(label_id == GRAPH_NO_LABEL) return; // Unlabeled node, no need to update.

	Schema *s = GraphContext_GetSchemaByID(gc, label_id, SCHEMA_NODE);
	if(!Schema_HasIndices(s)) return; // No indices, no need to update.

	Schema_AddNodeToIndices(s, n, true);

}

// Update the appropriate property on a graph entity.
static void _UpdateProperty(Record r, GraphEntity *ge, EntityUpdateEvalCtx *update_ctx) {
	SIValue new_value = SI_CloneValue(AR_EXP_Evaluate(update_ctx->exp, r));

	// Try to get current property value.
	SIValue *old_value = GraphEntity_GetProperty(ge, update_ctx->attribute_idx);

	if(old_value == PROPERTY_NOTFOUND) {
		// Add new property.
		GraphEntity_AddProperty(ge, update_ctx->attribute_idx, new_value);
	} else {
		// Update property.
		GraphEntity_SetProperty(ge, update_ctx->attribute_idx, new_value);
	}
}

// Perform all ON MATCH updates for all matched records.
static void _UpdateProperties(OpMerge *op, Record *records) {
	GraphContext *gc = QueryCtx_GetGraphCtx();
	Graph_AcquireWriteLock(gc->g); // Lock the graph.

	// Iterate over all update contexts, converting property keys to IDs.
	uint update_count = array_len(op->on_match);
	for(uint i = 0; i < update_count; i ++) {
		op->on_match[i].attribute_idx = GraphContext_FindOrAddAttribute(gc, op->on_match[i].attribute);
	}

	uint record_count = array_len(records);
	for(uint i = 0; i < record_count; i ++) {  // For each matched record
		Record r = records[i];
		for(uint j = 0; j < update_count; j ++) { // For each pending update.
			EntityUpdateEvalCtx *update_ctx = &op->on_match[j];

			// Retrieve the appropriate entry from the Record, make sure it's either a node or an edge.
			RecordEntryType t = Record_GetType(r, update_ctx->record_idx);
			assert(t == REC_TYPE_NODE || t == REC_TYPE_EDGE);
			GraphEntity *ge = Record_GetGraphEntity(r, update_ctx->record_idx);

			_UpdateProperty(r, ge, update_ctx); // Updaet the entity.
			if(t == REC_TYPE_NODE) _UpdateIndices(gc, (Node *)ge); // Update indices if necessary.
		}

		if(op->stats) op->stats->properties_set += update_count;
	}

	Graph_ReleaseLock(gc->g); // Release the lock.
}

//------------------------------------------------------------------------------
// Merge logic
//------------------------------------------------------------------------------
static inline Record _pullFromStream(OpBase *branch) {
	return OpBase_Consume(branch);
}

OpBase *NewMergeOp(const ExecutionPlan *plan, EntityUpdateEvalCtx *on_match,
				   ResultSetStatistics *stats) {
	/* Merge is an operator with two or three children.
	 * They will be created outside of here, as with other multi-stream operators (see CartesianProduct
	 * and ValueHashJoin). */
	OpMerge *op = malloc(sizeof(OpMerge));
	op->stats = stats;
	op->on_match = on_match;
	op->output_records = NULL;
	op->input_records = NULL;
	op->match_argument_tap = NULL;
	op->create_argument_tap = NULL;

	// Set our Op operations
	OpBase_Init((OpBase *)op, OPType_MERGE, "Merge", MergeInit, MergeConsume, NULL, NULL, MergeFree,
				plan);

	return (OpBase *)op;
}

static Record _handoff(OpMerge *op) {
	Record r = NULL;
	if(array_len(op->output_records)) r = array_pop(op->output_records);
	return r;
}

static Record MergeConsume(OpBase *opBase) {
	OpMerge *op = (OpMerge *)opBase;

	// Return mode, all data was consumed.
	if(op->output_records) return _handoff(op);

	// Consume mode.
	op->output_records = array_new(Record, 32);

	// If we have a bound variable stream, pull from it and store records until depleted.
	if(op->bound_variable_stream) {
		Record input_record;
		while((input_record = _pullFromStream(op->bound_variable_stream))) {
			op->input_records = array_append(op->input_records, input_record);
		}
	}

	bool must_create_records = false;
	bool reading_matches = true;
	// Match mode: attempt to resolve the pattern once for every record from the bound variable
	// stream, or once if we have no bound variables.
	while(reading_matches) {
		Record lhs_record = NULL;
		if(op->input_records) {
			// If we had bound variables but have depleted our input records,
			// we're done pulling from the Match stream.
			if(array_len(op->input_records) == 0) break;

			// Pull a new input record.
			lhs_record = array_pop(op->input_records);
			// Propagate record to the top of the Match stream.
			Argument_AddRecord(op->match_argument_tap, Record_Clone(lhs_record));
		} else {
			// This loop only executes once if we don't have input records resolving bound variables.
			reading_matches = false;
		}

		bool should_create_pattern = true;
		Record rhs_record;
		// Retrieve Records from the Match stream until it's depleted.
		while((rhs_record = _pullFromStream(op->match_stream))) {
			// Pattern was successfully matched.
			should_create_pattern = false;
			op->output_records = array_append(op->output_records, rhs_record);
		}

		if(should_create_pattern) {
			// Copy the LHS record to the Create stream to build once we finish reading.
			if(lhs_record) Argument_AddRecord(op->create_argument_tap, Record_Clone(lhs_record));
			must_create_records = true;
		}
	}

	// Explicitly free the read streams in case either holds an index read lock.
	if(op->bound_variable_stream) OpBase_PropagateFree(op->bound_variable_stream);
	OpBase_PropagateFree(op->match_stream);

	// If we are setting properties with ON MATCH, execute all pending updates.
	if(op->on_match && array_len(op->output_records) > 0) _UpdateProperties(op, op->output_records);

	// True if we have at least one pattern to create.
	if(must_create_records) {
		/* We've populated the Create stream with all the Records it must read;
		 * pull from it until we've retrieved all newly-created Records. */
		Record created_record;
		while((created_record = _pullFromStream(op->create_stream))) {
			op->output_records = array_append(op->output_records, created_record);
		}
	}

	return _handoff(op);
}

static OpResult MergeInit(OpBase *opBase) {
	OpMerge *op = (OpMerge *)opBase;

	// If Merge has 2 children, there are no bound variables and thus no Arguments in the child streams.
	if(opBase->childCount == 2) {
		op->bound_variable_stream = NULL;
		op->match_stream = opBase->children[0];
		op->create_stream = opBase->children[1];
		return OP_OK;
	}

	op->input_records = array_new(Record, 1);

	// Otherwise, the first stream resolves bound variables.
	op->bound_variable_stream = opBase->children[0];
	op->match_stream = opBase->children[1];
	op->create_stream = opBase->children[2];

	// Find and store references to the Argument taps for the Match and Create streams.
	// The Match stream is populated by an Argument tap, store a reference to it.
	op->match_argument_tap = (Argument *)ExecutionPlan_LocateOp(op->match_stream, OPType_ARGUMENT);

	// If the create stream is populated by an Argument tap, store a reference to it.
	OpBase *create_stream = op->op.children[2];
	op->create_argument_tap = (Argument *)ExecutionPlan_LocateOp(op->create_stream, OPType_ARGUMENT);

	return OP_OK;
}

// Modification of ExecutionPlan_LocateOp that only follows LHS child.
// Otherwise, the assumptions of Merge_SetStreams fail in MERGE..MERGE queries.
// Match and Create streams are always guaranteed to not branch (have any ops with multiple children).
static OpBase *_LocateOp(OpBase *root, OPType type) {
	if(!root) return NULL;

	if(root->type & type) return root;

	if(root->childCount > 0) return _LocateOp(root->children[0], type);

	return NULL;
}

void Merge_SetStreams(OpBase *op) {
	/* Merge has 2 children if it is the first clause, and 3 otherwise.
	   The order of these children is critical:
	   - If there are 3 children, the first should resolve the Merge pattern's bound variables.
	   - The next (first if there are 2 children, second otherwise) should attempt to match the pattern.
	   - The last creates the pattern.
	*/
	OpBase *bound_variable_stream = NULL;
	OpBase *match_stream = NULL;
	OpBase *create_stream = NULL;
	if(op->childCount == 2) {
		// If we only have 2 streams, we simply need to determine which has a Create op.
		if(_LocateOp(op->children[0], OPType_CREATE)) {
			// If the Create op is in the first stream, swap the children.
			// Otherwise, the order is already correct.
			create_stream = op->children[0];
			op->children[0] = op->children[1];
			op->children[1] = create_stream;
		}
		return;
	}

	// Handling the three-stream case.
	for(int i = 0; i < op->childCount; i ++) {
		OpBase *child = op->children[i];
		bool child_has_argument = _LocateOp(child, OPType_ARGUMENT);
		// The bound variable stream is the only stream not populated by an Argument op.
		if(!bound_variable_stream && !child_has_argument) {
			bound_variable_stream = child;
			continue;
		}

		// The Create stream is the only stream with a Create op and Argument op.
		if(!create_stream && _LocateOp(child, OPType_CREATE) && child_has_argument) {
			create_stream = child;
			continue;
		}

		// The Match stream has an unknown set of operations, but is the only other stream
		// populated by an Argument op.
		if(!match_stream && child_has_argument) {
			match_stream = child;
			continue;
		}
	}

	assert(bound_variable_stream && match_stream && create_stream);

	// Migrate the children so that EXPLAIN calls print properly.
	op->children[0] = bound_variable_stream;
	op->children[1] = match_stream;
	op->children[2] = create_stream;
	OpMerge *op_merge = (OpMerge *)op;
	// op_merge->bound_variable_stream = op->children[0] = bound_variable_stream;
	// op_merge->match_stream = op->children[1] = match_stream;
	// op_merge->create_stream = op->children[2] = create_stream;
}

static void MergeFree(OpBase *ctx) {
}

