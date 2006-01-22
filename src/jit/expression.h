#ifndef __EXPRESSION_H
#define __EXPRESSION_H

#include <jvm_types.h>

enum operator {
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_REM,
};

enum expression_type {
	EXPR_VALUE,
	EXPR_FVALUE,
	EXPR_LOCAL,
	EXPR_TEMPORARY,
	EXPR_ARRAY_DEREF,
	EXPR_BINOP,
};

struct expression {
	enum expression_type type;
	unsigned long refcount;
	enum jvm_type jvm_type;
	union {
		/* EXPR_VALUE */
		unsigned long long value;

		/* EXPR_FVALUE */
		double fvalue;

		/* EXPR_LOCAL */
		unsigned long local_index;

		/* EXPR_TEMPORARY */
		unsigned long temporary;

		/* EXPR_ARRAY_DEREF */
		struct {
			struct expression *arrayref;
			struct expression *array_index;
		};

		/* EXPR_BINOP */
		struct {
			enum operator operator;
			struct expression *left;
			struct expression *right;
		};
	};
};

struct expression *alloc_expression(enum expression_type, enum jvm_type);
void free_expression(struct expression *);

void expr_get(struct expression *);
void expr_put(struct expression *);

struct expression *value_expr(enum jvm_type, unsigned long long);
struct expression *fvalue_expr(enum jvm_type, double);
struct expression *local_expr(enum jvm_type, unsigned long);
struct expression *temporary_expr(enum jvm_type, unsigned long);
struct expression *array_deref_expr(enum jvm_type, struct expression *, struct expression *);
struct expression *binop_expr(enum jvm_type, enum operator, struct expression *, struct expression *);

#endif
