 /* Copyright (C) 2003-2004 Tresys Technology, LLC
 * see file 'COPYING' for use and warranty information */

/* 
 * Author: mayerf@tresys.com 
 */

/* render.c */

/* Utility functions to render aspects of a policy into strings */

/* TODO: Need to add all rule rendering functions below, and change the
 * TCL interface (and any other) to use these rather than do their own
 * thing.
 */
 

#include "util.h"
#include "policy.h"
#include "semantic/avhash.h"
#include "semantic/avsemantics.h"
#include "render.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* global with rule names */
char *rulenames[] = {"allow", "auditallow", "auditdeny", "dontaudit", "neverallow", "type_transition", 
			"type_member", "type_change", "clone", "allow", "role_transition", "user"};
 /* classes and perm strings */
static int re_append_cls_perms(ta_item_t *list, 
				bool_t iscls,		/* 1 if list is classes, 0 if not (i.e., permissions) */
				unsigned char flags, 	/* from av_item_t object */
				char **buf,
				int *buf_sz,
				policy_t *policy)
{
	ta_item_t *ptr;
	int multiple = 0;

	if(flags & (!iscls ? AVFLAG_PERM_TILDA : AVFLAG_NONE)) {
		if(append_str(buf, buf_sz, "~") != 0) 
			return -1;
	}
	if (list != NULL && list->next != NULL) {
		multiple = 1;
		if(append_str(buf, buf_sz, "{ ") != 0) 
			return -1;
	}
	if(flags & (!iscls ? AVFLAG_PERM_STAR : AVFLAG_NONE))
		if(append_str(buf, buf_sz, "* ") != 0)
			return -1;
		
	for(ptr = list; ptr != NULL; ptr = ptr->next) {
		assert( (iscls && ptr->type == IDX_OBJ_CLASS) || (!iscls && ptr->type == IDX_PERM) );
		if(iscls) {
			if(append_str(buf, buf_sz, policy->obj_classes[ptr->idx].name) != 0)
				return -1;
		}
		else {
			if(append_str(buf, buf_sz, policy->perms[ptr->idx]) != 0)
				return -1;
		}
		if(multiple) {
			if(append_str(buf, buf_sz, " ") != 0)
				return -1;
		}
	}
	if(multiple) {
		if(append_str(buf, buf_sz, "}") != 0)
			return -1;
	} 
	if (iscls) {
		if(append_str(buf, buf_sz, " ") != 0)
			return -1;
	}
	return 0;	
}
 
static int append_type_attrib(char **buf, int *buf_sz, ta_item_t *tptr, policy_t *policy)
{
	if (append_str(buf, buf_sz, " ") != 0)  {
		free(buf);
		return -1;
	}
	if ((tptr->type & IDX_SUBTRACT)) {
		if (append_str(buf, buf_sz, "-") != 0)  {
			free(buf);
			return -1;
		}
	}
	if ((tptr->type & IDX_TYPE)) {
		if (append_str(buf, buf_sz, policy->types[tptr->idx].name) != 0)  {
			free(buf);
			return -1;
		}
	} else if(tptr->type & IDX_ATTRIB) {
		if (append_str(buf, buf_sz,  policy->attribs[tptr->idx].name) != 0)  {
			free(buf);
			return -1;
		}
	} else {
		free(buf);
		return -1;
	}
	return 0;
}
 
/* return NULL for error, mallocs memory, caller must free */
char *re_render_av_rule(bool_t 	addlineno, 	/* add policy.conf line  */
			int	idx, 		/* rule idx */
			bool_t is_au,		/* whether audit rules */
			policy_t *policy
			) 
{
	av_item_t *rule;
	ta_item_t *tptr;
	char *buf;
	int buf_sz;	
	int multiple = 0;
	char tbuf[APOL_STR_SZ+64];

	if(policy == NULL || !is_valid_av_rule_idx(idx, (is_au ? 0:1), policy)) {
		return NULL;
	}
	if(!is_au) 
		rule = &(policy->av_access[idx]);
	else
		rule = &(policy->av_audit[idx]);
	
	
	/* remember to init the buffer */
	buf = NULL;
	buf_sz = 0;
	
	if(addlineno) {
		sprintf(tbuf, "[%7lu] ", rule->lineno);
		if(append_str(&buf, &buf_sz, tbuf) != 0) {
			free(buf);
			return NULL;
		}
	}
		
	if(append_str(&buf, &buf_sz, rulenames[rule->type]) != 0) {
		return NULL;
	}
	
	/* source types */
	if(rule->flags & AVFLAG_SRC_TILDA) {
		if(append_str(&buf, &buf_sz, " ~") != 0) {
			free(buf);
			return NULL;
		}
	}
	
	if(rule->src_types != NULL && rule->src_types->next != NULL) {
		multiple = 1;
		if(append_str(&buf, &buf_sz, " {") != 0) {
			free(buf);
			return NULL;
		}
	}
	if(rule->flags & AVFLAG_SRC_STAR)
		if(append_str(&buf, &buf_sz, " *") != 0) {
			free(buf);
			return NULL;
		}
	
	for(tptr = rule->src_types; tptr != NULL; tptr = tptr->next) {
		if (append_type_attrib(&buf, &buf_sz, tptr, policy) == -1)
			return NULL;
	}
	if(multiple) {
		if(append_str(&buf, &buf_sz, " }") != 0) {
			free(buf);
			return NULL;
		}
		multiple = 0;
	}
	
	/* tgt types */
	if(rule->flags & AVFLAG_TGT_TILDA) {
		if(append_str(&buf, &buf_sz, " ~") != 0) {
			free(buf);
			return NULL;
		}
	}
	
	if(rule->tgt_types != NULL && rule->tgt_types->next != NULL) {
		multiple = 1;
		if(append_str(&buf, &buf_sz, " {") != 0) {
			free(buf);
			return NULL;
		}
	}
	if(rule->flags & AVFLAG_TGT_STAR)
		if(append_str(&buf, &buf_sz, " *") != 0) {
			free(buf);
			return NULL;
		}
				
	for(tptr = rule->tgt_types; tptr != NULL; tptr = tptr->next) {
		if (append_type_attrib(&buf, &buf_sz, tptr, policy) == -1)
			return NULL;
	}
	if(multiple) {
		if(append_str(&buf, &buf_sz, " }") != 0) {
			free(buf);
			return NULL;
		}
		multiple = 0;
	}
	if(append_str(&buf, &buf_sz, " : ") != 0) {
		free(buf);
		return NULL;
	}
	
	/* classes */
	if(re_append_cls_perms(rule->classes, 1, rule->flags, &buf, &buf_sz, policy) != 0) {
		free(buf);
		return NULL;
	}
				
	/* permissions */
	if(re_append_cls_perms(rule->perms, 0, rule->flags, &buf, &buf_sz, policy)!= 0) {
		free(buf);
		return NULL;
	}
	
	if(append_str(&buf, &buf_sz, ";") != 0) {
		free(buf);
		return NULL;
	}
		
	return buf;
}

/* return NULL for error, mallocs memory, caller must free */
char *re_render_tt_rule(bool_t addlineno, int idx, policy_t *policy) 
{
	tt_item_t *rule;
	ta_item_t *tptr;
	char *buf;
	int buf_sz;	
	int multiple = 0;
	char tbuf[APOL_STR_SZ+64];
	
	if(policy == NULL || !is_valid_tt_rule_idx(idx,  policy)) {
		return NULL;
	}
	
	/* remember to init the buffer */
	buf = NULL;
	buf_sz = 0;
	rule = &(policy->te_trans[idx]);

	if(addlineno) {
		sprintf(tbuf, "[%7lu] ", rule->lineno);
		if(append_str(&buf, &buf_sz, tbuf) != 0) {
			free(buf);
			return NULL;
		}
	}

	if(append_str(&buf, &buf_sz, rulenames[rule->type]) != 0) {
		free(buf);
		return NULL;
	}

	/* source types */
	if(rule->flags & AVFLAG_SRC_TILDA)  {
		if(append_str(&buf, &buf_sz, " ~") != 0) {
			free(buf);
			return NULL;
		}
	}
			
	if(rule->src_types != NULL && rule->src_types->next != NULL) {
		multiple = 1;
		if(append_str(&buf, &buf_sz, " {") != 0) {
			free(buf);
			return NULL;		
		}
	}
	if(rule->flags & AVFLAG_SRC_STAR)
		if(append_str(&buf, &buf_sz, " *") != 0) {
			free(buf);
			return NULL;
		}
	
	for(tptr = rule->src_types; tptr != NULL; tptr = tptr->next) {
		if (append_type_attrib(&buf, &buf_sz, tptr, policy) == -1)
			return NULL;
	}
	if(multiple) {
		if(append_str(&buf, &buf_sz, " }") != 0) {
			free(buf);
			return NULL;
		}
		multiple = 0;
	}

	/* tgt types */
	if(rule->flags & AVFLAG_TGT_TILDA) {
		if(append_str(&buf, &buf_sz, " ~") != 0) {
			free(buf);
			return NULL;
		}
	}
	
	if(rule->tgt_types != NULL && rule->tgt_types->next != NULL) {
		multiple = 1;
		if(append_str(&buf, &buf_sz, " {") != 0) {
			free(buf);
			return NULL;
		}
	}
	if(rule->flags & AVFLAG_TGT_STAR)
		if(append_str(&buf, &buf_sz, " *") != 0) {
			free(buf);
			return NULL;
		}
	
	for(tptr = rule->tgt_types; tptr != NULL; tptr = tptr->next) {
		if (append_type_attrib(&buf, &buf_sz, tptr, policy) == -1)
			return NULL;
	}
	if(multiple) {
		if(append_str(&buf, &buf_sz, " }") != 0) {
			free(buf);
			return NULL;
		}
		multiple = 0;
	}
	if(append_str(&buf, &buf_sz, " : ") != 0) {
		free(buf);
		return NULL;
	}
			
	/* classes */
	if(re_append_cls_perms(rule->classes, 1, rule->flags, &buf, &buf_sz, policy) != 0) {
		free(buf);
		return NULL;
	}
		
	/* default type */
	if(rule->dflt_type.type == IDX_TYPE) {
		sprintf(tbuf, "%s", policy->types[rule->dflt_type.idx].name);
	}
	else if(rule->dflt_type.type == IDX_ATTRIB) {
		sprintf(tbuf, "%s", policy->attribs[rule->dflt_type.idx].name);
	}			
	else {
		fprintf(stderr, "Invalid index type: %d\n", rule->dflt_type.type);
		free(buf);
		return NULL;
	}	
	if(append_str(&buf, &buf_sz, tbuf) != 0) {
		free(buf);
		return NULL;
	}
	
	if(append_str(&buf, &buf_sz, ";") != 0) {
		free(buf);
		return NULL;
	}
		

	return buf;	

}

char *re_render_mls_level(ap_mls_level_t *level, policy_t *policy)
{
	char *rt = NULL;
	int sz = 0, i, cur;

	if (!level || !policy)
		return NULL;

	append_str(&rt, &sz, policy->sensitivities[level->sensitivity].name);
	if (!level->categories)
		return rt; /* no categories, simply return the sensitivity name */

	append_str(&rt, &sz, ":");
	append_str(&rt, &sz, policy->categories[level->categories[0]].name);
	if (level->num_categories == 1)
		return rt; /* only one category so done */

	cur = 0; /* current value to compare with cat[i] */
	for (i = 1; i < level->num_categories; i++) {
		if (level->categories[i] == level->categories[cur] + 1) {
			if (i + 1 == level->num_categories || level->categories[i+1] != level->categories[cur] + 2) {
				append_str(&rt, &sz, ".");
				append_str(&rt, &sz, policy->categories[level->categories[i]].name);
				cur = i;
			} else {
				cur++;
			}
		} else {
			append_str(&rt, &sz, ", ");
			append_str(&rt, &sz, policy->categories[level->categories[i]].name);
			cur = i;
		}
	}
	return rt;
}

char *re_render_mls_range(ap_mls_range_t *range, policy_t *policy)
{
	char *rt = NULL;
	char *sub_str = NULL;
	int sz = 0;

	if (!range || !policy)
		return NULL;

	sub_str = re_render_mls_level(range->low, policy);
	append_str(&rt, &sz, sub_str);
	free(sub_str);
	if (range->high != range->low) {
		append_str(&rt, &sz, " - ");
		sub_str = re_render_mls_level(range->high, policy);
		append_str(&rt, &sz, sub_str);
		free(sub_str);
	}
	return rt;
}

/* security contexts */
char *re_render_security_context(const security_con_t *context, policy_t *policy)
{
	char *buf = NULL, *name = NULL, *range = NULL;
	int buf_sz;

	if(policy == NULL )
		return NULL;
	
	if(context != NULL && (!is_valid_type_idx(context->type, policy) || !is_valid_role_idx(context->role, policy) || 
			!is_valid_user_idx(context->user, policy)) )
		return NULL;

	/* initialize the buffer */
	buf = NULL;
	buf_sz = 0;

	/* handle case where initial SID does not have a context */
	if(context == NULL) {
		if(append_str(&buf, &buf_sz, "<no context>") != 0) 
			goto err_return;
		return buf;
	}

	/* render context */
	if(get_user_name2(context->user, &name, policy) != 0)
		goto err_return;
	if(append_str(&buf, &buf_sz, name) != 0) 
		goto err_return;
	free(name); 
	name = NULL;
	if(append_str(&buf, &buf_sz, ":") != 0) 
		goto err_return;
	if(get_role_name(context->role, &name, policy) != 0) 
		goto err_return;
	if(append_str(&buf, &buf_sz, name) != 0) 
		goto err_return;
	free(name);
	name = NULL;
	if(append_str(&buf, &buf_sz, ":") != 0) 
		goto err_return;
	if(get_type_name(context->type, &name, policy) != 0) 
		goto err_return;
	if(append_str(&buf, &buf_sz, name) != 0) 
		goto err_return;
	free(name);
	name = NULL;
	
	/* render range */
	if (context->range != NULL) {
		if (append_str(&buf, &buf_sz, ":") != 0)
			goto err_return;
		range = re_render_mls_range(context->range, policy);
		if (append_str(&buf, &buf_sz, range) != 0)
			goto err_return;
		free(range);
		range = NULL;
	}
	return buf;

err_return:
	if (buf != NULL) 
		free(buf);
	if (range != NULL)
		free(range);
	if (name != NULL)
		free(name);
	return NULL;	
}


char * re_render_initial_sid_security_context(int idx, policy_t *policy)
{
	if(policy == NULL || !is_valid_initial_sid_idx(idx, policy) ) {
		return NULL;
	}
	return(re_render_security_context(policy->initial_sids[idx].scontext, policy));
}


char *re_render_avh_rule_enabled_state(avh_node_t *node, policy_t *p)
{
	char *t = NULL;
	int sz, rt;
	if(avh_is_enabled(node, p))
		rt = append_str(&t, &sz, "E: ");
	else
		rt = append_str(&t, &sz, "D: ");
	if(rt < 0) {
		if(t != NULL)
			free(t);
		return NULL;	
	}
	/* else */
	return t;
}

char *re_render_cond_expr(int idx,policy_t *p)
{
	cond_expr_t *cond;
	char *rt = NULL;
	int sz;
	char tbuf[BUF_SZ];

	append_str(&rt,&sz," [ ");
	for(cond = p->cond_exprs[idx].expr; cond != NULL; cond  = cond->next) {
		switch (cond->expr_type) {
		case COND_BOOL:
			snprintf(tbuf, sizeof(tbuf)-1, "%s ", p->cond_bools[cond->bool].name); 
			append_str(&rt,&sz,tbuf);
			break;
		case COND_NOT:
			snprintf(tbuf, sizeof(tbuf)-1, "! "); 
			append_str(&rt,&sz,tbuf);
			break;
		case COND_OR:
			snprintf(tbuf, sizeof(tbuf)-1, "|| "); 
			append_str(&rt,&sz,tbuf);
			break;
		case COND_AND:
			snprintf(tbuf, sizeof(tbuf)-1, "&& "); 
			append_str(&rt,&sz,tbuf);
			break;
		case COND_XOR:
			snprintf(tbuf, sizeof(tbuf)-1, "^ "); 
			append_str(&rt,&sz,tbuf);
			break;
		case COND_EQ:
			append_str(&rt,&sz,tbuf);
			snprintf(tbuf, sizeof(tbuf)-1, "== "); 
			break;
		case COND_NEQ:
			append_str(&rt,&sz,tbuf);
			snprintf(tbuf, sizeof(tbuf)-1, "!= ");
			break;
		default:
			break;
		}
		
	}
	append_str(&rt,&sz," ] ");			
	return rt;
}

/* print the cond expr in rpn for a rule */
char *re_render_avh_rule_cond_expr(avh_node_t *node, policy_t *p)
{
	char *rt = NULL;
	if (node->flags & AVH_FLAG_COND) {
		rt = re_render_cond_expr(node->cond_expr,p);
	}
	return rt;
}

/* render a AV/Type rule from av hash table; caller must free memory.
 * Return NULL on error. */

/* conditional states */
char *re_render_avh_rule_cond_state(avh_node_t *node, policy_t *p)
{
	char *t = NULL;
	int sz = 0, rt;
	
	if(node == NULL || p == NULL)
		return NULL;
	if(node->flags & AVH_FLAG_COND) {
		if(node->cond_list) {
			rt = append_str(&t, &sz, "T ");
		}
		else {
			rt = append_str(&t, &sz, "F ");
		}
	}
	else {
		rt = append_str(&t, &sz, "  ");
	}
	if(rt < 0)
		goto err_return;

	return t;
err_return:
	if(t != NULL)
		free(t);
	return NULL;	
}

/* source line numbers if not binary */
char *re_render_avh_rule_linenos(avh_node_t *node, policy_t *p)
{
	char *t = NULL;
	int rt, sz;
	avh_rule_t *r;
	static char buf[128];
	bool_t is_av;
	void *rlist;
	int rlist_num;
	unsigned long lineno;
	
	if(node == NULL || p == NULL)
		return NULL;
	
	if(is_binary_policy(p))
		return NULL;  /* no linenos to render */
	
	if(is_av_access_rule_type(node->key.rule_type)) {
		is_av = TRUE;
		rlist = p->av_access;
		rlist_num = p->num_av_access;
	}
	else if(is_av_audit_rule_type(node->key.rule_type)) {
		is_av = TRUE;
		rlist = p->av_audit;
		rlist_num = p->num_av_audit;
	}
	else if(is_type_rule_type(node->key.rule_type)) {
		is_av = FALSE;
		rlist = p->te_trans;
		rlist_num = p->num_te_trans;
	}
	else {
		assert(0);
		return NULL;
	}

	for(r = node->rules; r != NULL; r = r->next) {
		assert(r->rule < rlist_num);
		if(is_av) 
			lineno = ((av_item_t *)rlist)[r->rule].lineno;
		else
			lineno = ((tt_item_t *)rlist)[r->rule].lineno;
		sprintf(buf, "%ld", lineno);
		rt = append_str(&t, &sz, buf);
		if(rt < 0)
			goto err_return;
		if(r->next != NULL) {
			rt = append_str(&t, &sz, " ");
			if(rt < 0)
				goto err_return;
		}
	}

	return t;
err_return:
	if(t != NULL)
		free(t);
	return NULL;	
}

/* rule itself as it is in the hash */
char *re_render_avh_rule(avh_node_t *node, policy_t *p)
{
	char *t = NULL, *name;
	int sz = 0, rt, i;
	
	/* rule identifier */
	assert(node->key.rule_type >= RULE_TE_ALLOW && node->key.rule_type <= RULE_TE_CHANGE);
	if(append_str(&t, &sz, rulenames[node->key.rule_type]) != 0) 
		goto err_return;
	rt = append_str(&t, &sz, " ");
	if(rt < 0)
		goto err_return;
	
	/* source */
	assert(is_valid_type(p, node->key.src, FALSE));
	rt = get_type_name(node->key.src, &name, p);
	if(rt < 0)
		goto err_return;
	rt = append_str(&t, &sz, name);
	free(name);
	if(rt < 0)
		goto err_return;
	rt = append_str(&t, &sz, " ");
	if(rt < 0)
		goto err_return;
	
	/* target */
	assert(is_valid_type(p, node->key.tgt, FALSE));
	rt = get_type_name(node->key.tgt, &name, p);
	if(rt < 0)
		goto err_return;
	rt = append_str(&t, &sz, name);
	free(name);
	if(rt < 0)
		goto err_return;
	rt = append_str(&t, &sz, " : ");
	if(rt < 0)
		goto err_return;
	
	/* class */
	assert(is_valid_obj_class(p, node->key.cls));
	rt = get_obj_class_name(node->key.cls, &name, p);
	if(rt < 0)
		goto err_return;
	rt = append_str(&t, &sz, name);
	free(name);
	if(rt < 0)
		goto err_return;

	/* permissions (AV rules) or default type (Type rules) */
	if(node->key.rule_type <= RULE_MAX_AV) {
		/* permissions */
	/* TODO: skip this assertion for now; there is a bug in binpol */
	/*	assert(node->num_data > 0); */
		rt = append_str(&t, &sz, " { ");
		if(rt < 0)
			goto err_return;
		for(i = 0; i < node->num_data; i++) {
			rt = get_perm_name(node->data[i], &name, p);
			if(rt < 0)
				goto err_return;
			rt = append_str(&t, &sz, name);
			free(name);
			if(rt < 0)
				goto err_return;
			rt = append_str(&t, &sz, " ");
			if(rt < 0)
				goto err_return;
		}
		rt = append_str(&t, &sz, "};");
		if(rt < 0)
			goto err_return;
	}
	else {
		/* default type */
		assert(node->num_data == 1);
		rt = append_str(&t, &sz, " ");
		if(rt < 0)
			goto err_return;
		rt = get_type_name(node->data[0], &name, p);
		if(rt < 0)
			goto err_return;
		rt = append_str(&t, &sz, name);
		free(name);	
		rt = append_str(&t, &sz, " ;");
		if(rt < 0)
			goto err_return;
	}
				
	return t;
err_return:
	if(t != NULL)
		free(t);
	return NULL;
}

char *re_render_fs_use(ap_fs_use_t *fsuse, policy_t *policy)
{
	char *context_str = NULL;
	char *line = NULL;
	char *behavior_str = NULL;

	switch (fsuse->behavior) {
	case AP_FS_USE_PSID:
		behavior_str = strdup("fs_use_psid");
		break;
	case AP_FS_USE_XATTR:
		behavior_str = strdup("fs_use_xattr");
		break;
	case AP_FS_USE_TASK:
		behavior_str = strdup("fs_use_task");
		break;
	case AP_FS_USE_TRANS:
		behavior_str = strdup("fs_use_trans");
		break;
	default:
		break;
	}
	if (!behavior_str)
		return NULL;

	context_str = re_render_security_context(fsuse->scontext, policy);
	if (!context_str) {
		free(behavior_str);
		return NULL;
	}

	line = (char *)calloc(strlen(behavior_str) + strlen(context_str) + strlen(fsuse->fstype) + 4, sizeof(char));
	if (!line) {
		free(context_str);
		free(behavior_str);
		return NULL;
	}
	strcat(line, behavior_str);
	strcat(line, " ");
	strcat(line, fsuse->fstype);
	strcat(line, " ");
	strcat(line, context_str);
	strcat(line, ";");

	free(behavior_str);
	free(context_str);
	return line;
}

char *re_render_portcon(ap_portcon_t *portcon, policy_t *policy)
{
	char *line = NULL;
	char *buff = NULL;
	char *proto_str = NULL;
	char *context_str = NULL;

	const int bufflen = 50; /* arbitrary size big enough to hold port no. */

	if (!portcon || !policy)
		return NULL;

	buff = (char*)calloc(bufflen + 1, sizeof(char));
	if (!buff)
		goto exit_err;

	switch (portcon->protocol) {
	case AP_TCP_PROTO:
		proto_str = strdup("tcp");
		break;
	case AP_UDP_PROTO:
		proto_str = strdup("udp");
		break;
	case AP_ESP_PROTO:
		proto_str = strdup("esp");
		break;
	default:
		break;
	}
	if (!proto_str)
		goto exit_err;

	if (portcon->lowport == portcon->highport)
		snprintf(buff, bufflen, "%d", portcon->lowport);
	else
		snprintf(buff, bufflen, "%d-%d", portcon->lowport, portcon->highport);

	context_str = re_render_security_context(portcon->scontext, policy);
	if (!context_str) 
		goto exit_err;

	line = (char *)calloc(4 + strlen("portcon") + strlen(proto_str) + strlen(buff) + strlen(context_str), sizeof(char));

	strcat(line, "portcon");
	strcat(line, " ");
	strcat(line, proto_str);
	strcat(line, " ");
	strcat(line, buff);
	strcat(line, " ");
	strcat(line, context_str);

	free(buff);
	free(proto_str);
	free(context_str);

	return line;

exit_err:
	free(line);
	free(buff);
	free(proto_str);
	free(context_str);

	return NULL;
}

char *re_render_netifcon(ap_netifcon_t *netifcon, policy_t *policy)
{
	char *line = NULL;
	char *devcon_str = NULL;
	char *pktcon_str = NULL;

	if (!netifcon || !policy)
		return NULL;

	devcon_str = re_render_security_context(netifcon->device_context, policy);
	if (!devcon_str)
		return NULL;

	pktcon_str = re_render_security_context(netifcon->packet_context, policy);
	if (!pktcon_str) {
		free(devcon_str);
		return NULL;
	}

	line = (char *)calloc(4 + strlen(netifcon->iface) + strlen(devcon_str) + strlen(pktcon_str) + strlen("netifcon"), sizeof(char));

	strcat(line, "netifcon");
	strcat(line, " ");
	strcat(line, netifcon->iface);
	strcat(line, " ");
	strcat(line, devcon_str);
	strcat(line, " ");
	strcat(line, pktcon_str);

	free(devcon_str);
	free(pktcon_str);

	return line;
}

char *re_render_nodecon(ap_nodecon_t *nodecon, policy_t *policy)
{
	char *line = NULL;
	char *context_str = NULL;
	char *addr_str = NULL;
	char *mask_str = NULL;
	uint32_t tmp = 0;
	char *tmp_str = NULL;

	/* max length of a string for an IP is 40 characters
	 *  (8 fields * 4 char/field) + 7  * ':' + '\0' */
	const size_t ip_addr_str_len_max = 41;

	if (!nodecon || !policy)
		return NULL;

	addr_str = (char *)calloc(ip_addr_str_len_max, sizeof(char));
	mask_str = (char *)calloc(ip_addr_str_len_max, sizeof(char));

	if (!addr_str || !mask_str) {
		free(addr_str);
		free(mask_str);
		return NULL;
	}

	switch (nodecon->flag) {
	case AP_IPV4:
		tmp = nodecon->addr[3];
		/* the math below prints one byte at a time as a decimal value */
		snprintf(addr_str, ip_addr_str_len_max - 1, "%3d.%3d.%3d.%3d", (tmp/(1<<24)), (tmp/(1<<16)%(1<<8)), (tmp/(1<<8)%(1<<8)), (tmp%(1<<8)));
		tmp = nodecon->mask[3];
		snprintf(mask_str, ip_addr_str_len_max - 1, "%3d.%3d.%3d.%3d", (tmp/(1<<24)), (tmp/(1<<16)%(1<<8)), (tmp/(1<<8)%(1<<8)), (tmp%(1<<8)));
		break;
	case AP_IPV6:
		snprintf(addr_str, ip_addr_str_len_max - 1, "%s", (tmp_str = re_render_ipv6_addr(nodecon->addr)));
		snprintf(mask_str, ip_addr_str_len_max - 1, "%s", (tmp_str = re_render_ipv6_addr(nodecon->mask)));
		break;
	default:
		break;
	}

	context_str = re_render_security_context(nodecon->scontext, policy);
	if (!context_str)
		return NULL;

	line = (char*)calloc(4 + strlen("nodecon") + strlen(addr_str) + strlen(mask_str) + strlen(context_str), sizeof(char));
	if (!line) {
		free(addr_str);
		free(mask_str);
		free(context_str);
		return NULL;
	}

	strcat(line, "nodecon");
	strcat(line, " ");
	strcat(line, addr_str);
	strcat(line, " ");
	strcat(line, mask_str);
	strcat(line, " ");
	strcat(line, context_str);

	free(addr_str);
	free(mask_str);
	free(context_str);
	return line;
}

char *re_render_genfscon(ap_genfscon_t *genfscon, policy_t *policy)
{
	char **lines = NULL;
	int num_lines = 0;
	char *context_str = NULL;
	char *type_str = NULL;
	ap_genfscon_node_t *path = NULL;
	char *front_str = NULL;
	char *text = NULL;
	int i;
	size_t text_sz = 0, len = 0;

	if (!genfscon || !policy)
		return NULL;

	for (path = genfscon->paths; path; path = path->next)
		num_lines++;

	lines = (char**)calloc(num_lines, sizeof(char*));
	if (!lines) 
		return NULL;

	front_str = (char *)calloc(3 + strlen("genfscon") + strlen(genfscon->fstype), sizeof(char));
	if (!front_str) {
		free(lines);
		return NULL;
	}

	strcat(front_str, "genfscon ");
	strcat(front_str, genfscon->fstype);
	strcat(front_str, " ");

	len = strlen(front_str);

	for (path = genfscon->paths, i = 0; i < num_lines && path; path = path->next, i++) {
		context_str = re_render_security_context(path->scontext, policy);
		if (!context_str) {
			return NULL;
		}
		switch (path->filetype) {
		case FILETYPE_DIR:
			type_str = strdup(" -d ");
			break;
		case FILETYPE_CHR:
			type_str = strdup(" -c ");
			break;
		case FILETYPE_BLK:
			type_str = strdup(" -b ");
			break;
		case FILETYPE_REG:
			type_str = strdup(" -- ");
			break;
		case FILETYPE_FIFO:
			type_str = strdup(" -p ");
			break;
		case FILETYPE_LNK:
			type_str = strdup(" -l ");
			break;
		case FILETYPE_SOCK:
			type_str = strdup(" -s ");
			break;
		case FILETYPE_ANY:
			type_str = strdup("    ");
			break;
		default:
			goto exit_err;
			break;
		}
		/* front_str + path + type_str + context_str + '\0' */
		lines[i] = (char*)calloc(len + strlen(path->path) + 4 + strlen(context_str) + 1 , sizeof(char));
		if (!lines[i])
			goto exit_err;

		strcat(lines[i], front_str);
		strcat(lines[i], path->path);
		strcat(lines[i], type_str);
		strcat(lines[i], context_str);

		text_sz += (strlen(lines[i]) + 1); /* add newline */

		free(context_str);
		free(type_str);
	}

	text = (char*)calloc(text_sz + 1, sizeof(char));
	if (!text)
		goto exit_err;

	for (i = 0; i < num_lines; i++) {
		strcat(text, lines[i]);
		strcat(text, "\n");
	}

	free(front_str);
	for (i = 0; i < num_lines; i++)
		free(lines[i]);
	free(lines);

	return text;

exit_err:
	free(context_str);
	free(type_str);
	free(front_str);
	for (i = 0; i < num_lines; i++)
		free(lines[i]);
	free(lines);
	return NULL;
}

char *re_render_cexpr(ap_constraint_expr_t *expr, policy_t *policy)
{
	char *rt = NULL, *tmp_name = NULL;
	char tmp[BUF_SZ];
	int sz = 0, retv;
	ap_constraint_expr_t *cexpr = NULL;
	ta_item_t *name = NULL;

	append_str(&rt, &sz, "( ");
	for (cexpr = expr; cexpr; cexpr = cexpr->next) {
		switch (cexpr->expr_type) {
		case AP_CEXPR_NOT:
			snprintf(tmp, sizeof(tmp)-1, "! ");
			append_str(&rt, &sz, tmp);
			break;
		case AP_CEXPR_AND:
			snprintf(tmp, sizeof(tmp)-1, "&& ");
			append_str(&rt, &sz, tmp);
			break;
		case AP_CEXPR_OR:
			snprintf(tmp, sizeof(tmp)-1, "|| ");
			append_str(&rt, &sz, tmp);
			break;
		case AP_CEXPR_ATTR:
			if (cexpr->attr == AP_CEXPR_USER) {
				snprintf(tmp, sizeof(tmp)-1, "(u1 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == AP_CEXPR_ROLE) {
				snprintf(tmp, sizeof(tmp)-1, "(r1 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == AP_CEXPR_TYPE) {
				snprintf(tmp, sizeof(tmp)-1, "(t1 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr & (AP_CEXPR_MLS_LOW1_LOW2 | AP_CEXPR_MLS_LOW1_HIGH2 | AP_CEXPR_MLS_LOW1_HIGH1)) {
				snprintf(tmp, sizeof(tmp)-1, "(l1 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == AP_CEXPR_MLS_LOW2_HIGH2) {
				snprintf(tmp, sizeof(tmp)-1, "(l2 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr & (AP_CEXPR_MLS_HIGH1_LOW2 | AP_CEXPR_MLS_HIGH1_HIGH2)) {
				snprintf(tmp, sizeof(tmp)-1, "(h1 ");
				append_str(&rt, &sz, tmp);
			}

			if (cexpr->op == AP_CEXPR_EQ) {
				snprintf(tmp, sizeof(tmp)-1, "== ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->op == AP_CEXPR_NEQ) {
				snprintf(tmp, sizeof(tmp)-1, "!= ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->op == AP_CEXPR_DOM) {
				snprintf(tmp, sizeof(tmp)-1, "dom ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->op == AP_CEXPR_DOMBY) {
				snprintf(tmp, sizeof(tmp)-1, "domby ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->op == AP_CEXPR_INCOMP) {
				snprintf(tmp, sizeof(tmp)-1, "incomp ");
				append_str(&rt, &sz, tmp);
			}

			if (cexpr->attr == AP_CEXPR_USER) {
				snprintf(tmp, sizeof(tmp)-1, "u2) ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == AP_CEXPR_ROLE) {
				snprintf(tmp, sizeof(tmp)-1, "r2) ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == AP_CEXPR_TYPE) {
				snprintf(tmp, sizeof(tmp)-1, "t2) ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr & (AP_CEXPR_MLS_LOW1_LOW2 | AP_CEXPR_MLS_HIGH1_LOW2)) {
				snprintf(tmp, sizeof(tmp)-1, "l2) ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr & (AP_CEXPR_MLS_LOW1_HIGH2 | AP_CEXPR_MLS_HIGH1_HIGH2 | AP_CEXPR_MLS_LOW2_HIGH2)) {
				snprintf(tmp, sizeof(tmp)-1, "h2) ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == AP_CEXPR_MLS_LOW1_HIGH1) {
				snprintf(tmp, sizeof(tmp)-1, "h1) ");
				append_str(&rt, &sz, tmp);
			}
			break;
		case AP_CEXPR_NAMES:
			if (cexpr->attr == (AP_CEXPR_USER)) {
				snprintf(tmp, sizeof(tmp)-1, "(u1 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == (AP_CEXPR_USER|AP_CEXPR_TARGET)) {
				snprintf(tmp, sizeof(tmp)-1, "(u2 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == (AP_CEXPR_USER|AP_CEXPR_XTARGET)) {
				snprintf(tmp, sizeof(tmp)-1, "(u3 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == (AP_CEXPR_ROLE)) {
				snprintf(tmp, sizeof(tmp)-1, "(r1 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == (AP_CEXPR_ROLE|AP_CEXPR_TARGET)) {
				snprintf(tmp, sizeof(tmp)-1, "(r2 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == (AP_CEXPR_ROLE|AP_CEXPR_XTARGET)) {
				snprintf(tmp, sizeof(tmp)-1, "(r3 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == (AP_CEXPR_TYPE)) {
				snprintf(tmp, sizeof(tmp)-1, "(t1 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == (AP_CEXPR_TYPE|AP_CEXPR_TARGET)) {
				snprintf(tmp, sizeof(tmp)-1, "(t2 ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->attr == (AP_CEXPR_TYPE|AP_CEXPR_XTARGET)) {
				snprintf(tmp, sizeof(tmp)-1, "(t3 ");
				append_str(&rt, &sz, tmp);
			}

			if (cexpr->op == AP_CEXPR_EQ) {
				snprintf(tmp, sizeof(tmp)-1, "== ");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->op == AP_CEXPR_NEQ) {
				snprintf(tmp, sizeof(tmp)-1, "!= ");
				append_str(&rt, &sz, tmp);
			}

			if (cexpr->name_flags == AP_CEXPR_STAR) {
				snprintf(tmp, sizeof(tmp)-1, "*");
				append_str(&rt, &sz, tmp);
			} else if (cexpr->name_flags == AP_CEXPR_TILDA) {
				snprintf(tmp, sizeof(tmp)-1, "~");
				append_str(&rt, &sz, tmp);
			}

			if (cexpr->names && cexpr->names->next) {
				snprintf(tmp, sizeof(tmp)-1, "{");
				append_str(&rt, &sz, tmp);
			}

			for (name = cexpr->names; name; name = name->next) {
				retv = get_ta_item_name(name, &tmp_name, policy);
				if (retv) {
					free(rt);
					return NULL;
				}
				if (name->type & IDX_SUBTRACT)
					snprintf(tmp, sizeof(tmp)-1, "-%s", tmp_name);
				else
					snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
				append_str(&rt, &sz, tmp);
				free(tmp_name);
				tmp_name = NULL;
				if (name->next) {
					snprintf(tmp, sizeof(tmp)-1, " ");
					append_str(&rt, &sz, tmp);
				}
			}

			if (cexpr->names && cexpr->names->next) {
				snprintf(tmp, sizeof(tmp)-1, "} ");
				append_str(&rt, &sz, tmp);
			}

			append_str(&rt, &sz, ") ");

			break;
		}
	}
	append_str(&rt, &sz, ")");

	return rt;
}

char *re_render_constraint(bool_t addlineno, ap_constraint_t *constraint, policy_t *policy)
{
	char *rt = NULL, *tmp_name = NULL, *expr_str = NULL;
	char tmp[BUF_SZ];
	int sz = 0, retv;
	ta_item_t *name = NULL;

	if (!(constraint && constraint->classes) || !policy)
		return NULL;

	if (addlineno) {
		snprintf(tmp, sizeof(tmp)-1, "[%7lu] ", constraint->lineno);
		append_str(&rt, &sz, tmp);
	}

	if (constraint->is_mls)
		append_str(&rt, &sz, "mls");

	if (constraint->perms) {
		snprintf(tmp, sizeof(tmp)-1, "constrain ");
		append_str(&rt, &sz, tmp);
	} else {
		snprintf(tmp, sizeof(tmp)-1, "validatetrans ");
		append_str(&rt, &sz, tmp);
	}

	if (constraint->classes->next)
		append_str(&rt, &sz, "{");

	for (name = constraint->classes; name; name = name->next) {
		retv = get_ta_item_name(name, &tmp_name, policy);
		if (retv) {
			free(rt);
			return NULL;
		}
		snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
		append_str(&rt, &sz, tmp);
		free(tmp_name);
		tmp_name = NULL;
		if (name->next)
			append_str(&rt, &sz, " ");
	}

	if (constraint->classes->next)
		append_str(&rt, &sz, "} ");

	if (constraint->perms) {
		if (constraint->perms->next)
			append_str(&rt, &sz, "{");

		for (name = constraint->perms; name; name = name->next) {
			retv = get_ta_item_name(name, &tmp_name, policy);
			if (retv) {
				free(rt);
				return NULL;
			}
			snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
			append_str(&rt, &sz, tmp);
			free(tmp_name);
			tmp_name = NULL;
			if (name->next)
				append_str(&rt, &sz, " ");
		}

		if (constraint->perms->next)
			append_str(&rt, &sz, "} ");
	}

	expr_str = re_render_cexpr(constraint->expr, policy);
	if (!expr_str) {
		free(rt);
		return NULL;
	}

	append_str(&rt, &sz, "\n\t");
	append_str(&rt, &sz, expr_str);
	append_str(&rt, &sz, ";");

	return rt;
}

char *re_render_rangetrans(bool_t addlineno, int idx, policy_t *policy)
{
	char *rt = NULL, *tmp_name = NULL, *sub_str = NULL;
	char tmp[BUF_SZ];
	int sz = 0, retv;
	ta_item_t *name = NULL;

	if (!policy || idx < 0 || idx >= policy->num_rangetrans)
		return NULL;

	if (addlineno) {
		snprintf(tmp, sizeof(tmp)-1, "[%7lu] ", policy->rangetrans[idx].lineno);
		append_str(&rt, &sz, tmp);
	}

	append_str(&rt, &sz, "range_transition ");

	/* render source(s) */
	if (policy->rangetrans[idx].flags & AVFLAG_SRC_STAR) {
		append_str(&rt, &sz, "*");
	} else if (policy->rangetrans[idx].flags & AVFLAG_SRC_TILDA) {
		append_str(&rt, &sz, "~");
	}
	if (policy->rangetrans[idx].src_types->next) {
		append_str(&rt, &sz, "{");
	}

	for (name = policy->rangetrans[idx].src_types; name; name = name->next) {
		retv = get_ta_item_name(name, &tmp_name, policy);
		if (retv) {
			free(rt);
			return NULL;
		}
		if (name->type & IDX_SUBTRACT) {
			snprintf(tmp, sizeof(tmp)-1, "-%s", tmp_name);
		} else {
			snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
		}
		append_str(&rt, &sz, tmp);
		free(tmp_name);
		tmp_name = NULL;
		if (name->next) {
			append_str(&rt, &sz, " ");
		}
	}

	if (policy->rangetrans[idx].src_types->next) {
		append_str(&rt, &sz, "}");
	}
	append_str(&rt, &sz, " ");

	/* render target(s) */
	if (policy->rangetrans[idx].flags & AVFLAG_TGT_STAR) {
		append_str(&rt, &sz, "*");
	} else if (policy->rangetrans[idx].flags & AVFLAG_TGT_TILDA) {
		append_str(&rt, &sz, "~");
	}

	if (policy->rangetrans[idx].tgt_types->next) {
		append_str(&rt, &sz, "{");
	}

	for (name = policy->rangetrans[idx].tgt_types; name; name = name->next) {
		retv = get_ta_item_name(name, &tmp_name, policy);
		if (retv) {
			free(rt);
			return NULL;
		}
		if (name->type & IDX_SUBTRACT) {
			snprintf(tmp, sizeof(tmp)-1, "-%s", tmp_name);
		} else {
			snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
		}
		append_str(&rt, &sz, tmp);
		free(tmp_name);
		tmp_name = NULL;
		if (name->next) {
			append_str(&rt, &sz, " ");
		}
	}

	if (policy->rangetrans[idx].tgt_types->next) {
		append_str(&rt, &sz, "}");
	}

	append_str(&rt, &sz, " ");

	/* render range */
	sub_str = re_render_mls_range(policy->rangetrans[idx].range, policy);
	append_str(&rt, &sz, sub_str);
	free(sub_str);

	append_str(&rt, &sz, ";");

	return rt;
}

char *re_render_role_trans(bool_t addlineno, int idx, policy_t *policy)
{
	char *rt = NULL, *tmp_name = NULL;
	char tmp[BUF_SZ];
	int sz = 0, retv;
	ta_item_t *name;

	if (!policy || idx < 0 || idx >= policy->num_role_trans)
		return NULL;

	if (addlineno) {
		snprintf(tmp, sizeof(tmp)-1, "[%7lu] ", policy->role_trans[idx].lineno);
		append_str(&rt, &sz, tmp);
	}

	append_str(&rt, &sz, "role_transition ");

	/* render source role(s) */
	if (policy->role_trans[idx].flags & AVFLAG_SRC_STAR) {
		append_str(&rt, &sz, "*");
	} else if (policy->role_trans[idx].flags & AVFLAG_SRC_TILDA) {
		append_str(&rt, &sz, "~");
	}
	if (policy->role_trans[idx].src_roles->next) {
		append_str(&rt, &sz, "{");
	}

	for (name = policy->role_trans[idx].src_roles; name; name = name->next) {
		retv = get_ta_item_name(name, &tmp_name, policy);
		if (retv) {
			free(rt);
			return NULL;
		}
		snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
		append_str(&rt, &sz, tmp);
		free(tmp_name);
		tmp_name = NULL;
		if (name->next) {
			append_str(&rt, &sz, " ");
		}
	}

	if (policy->role_trans[idx].src_roles->next) {
		append_str(&rt, &sz, "}");
	}
	append_str(&rt, &sz, " ");

	/* render target type(s) */
	if (policy->role_trans[idx].flags & AVFLAG_TGT_STAR) {
		append_str(&rt, &sz, "*");
	} else if (policy->role_trans[idx].flags & AVFLAG_TGT_TILDA) {
		append_str(&rt, &sz, "~");
	}

	if (policy->role_trans[idx].tgt_types->next) {
		append_str(&rt, &sz, "{");
	}

	for (name = policy->role_trans[idx].tgt_types; name; name = name->next) {
		retv = get_ta_item_name(name, &tmp_name, policy);
		if (retv) {
			free(rt);
			return NULL;
		}
		snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
		append_str(&rt, &sz, tmp);
		free(tmp_name);
		tmp_name = NULL;
		if (name->next) {
			append_str(&rt, &sz, " ");
		}
	}

	if (policy->role_trans[idx].tgt_types->next) {
		append_str(&rt, &sz, "}");
	}

	append_str(&rt, &sz, " ");

	/* render transition role */
	append_str(&rt, &sz, policy->roles[policy->role_trans[idx].trans_role.idx].name);

	append_str(&rt, &sz, ";");

	return rt;
}

char *re_render_role_allow(bool_t addlineno, int idx, policy_t *policy)
{
	char *rt = NULL, *tmp_name = NULL;
	char tmp[BUF_SZ];
	int sz = 0, retv;
	ta_item_t *name;

	if (!policy || idx < 0 || idx >= policy->num_role_allow)
		return NULL;

	if (addlineno) {
		snprintf(tmp, sizeof(tmp)-1, "[%7lu] ", policy->role_allow[idx].lineno);
		append_str(&rt, &sz, tmp);
	}

	append_str(&rt, &sz, "allow ");

	/* render source role(s) */
	if (policy->role_allow[idx].flags & AVFLAG_SRC_STAR) {
		append_str(&rt, &sz, "*");
	} else if (policy->role_allow[idx].flags & AVFLAG_SRC_TILDA) {
		append_str(&rt, &sz, "~");
	}
	if (policy->role_allow[idx].src_roles->next) {
		append_str(&rt, &sz, "{");
	}

	for (name = policy->role_allow[idx].src_roles; name; name = name->next) {
		retv = get_ta_item_name(name, &tmp_name, policy);
		if (retv) {
			free(rt);
			return NULL;
		}
		snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
		append_str(&rt, &sz, tmp);
		free(tmp_name);
		tmp_name = NULL;
		if (name->next) {
			append_str(&rt, &sz, " ");
		}
	}

	if (policy->role_allow[idx].src_roles->next) {
		append_str(&rt, &sz, "}");
	}
	append_str(&rt, &sz, " ");

	/* render target role(s) */
	if (policy->role_allow[idx].flags & AVFLAG_SRC_STAR) {
		append_str(&rt, &sz, "*");
	} else if (policy->role_allow[idx].flags & AVFLAG_SRC_TILDA) {
		append_str(&rt, &sz, "~");
	}
	if (policy->role_allow[idx].tgt_roles->next) {
		append_str(&rt, &sz, "{");
	}

	for (name = policy->role_allow[idx].tgt_roles; name; name = name->next) {
		retv = get_ta_item_name(name, &tmp_name, policy);
		if (retv) {
			free(rt);
			return NULL;
		}
		snprintf(tmp, sizeof(tmp)-1, "%s", tmp_name);
		append_str(&rt, &sz, tmp);
		free(tmp_name);
		tmp_name = NULL;
		if (name->next) {
			append_str(&rt, &sz, " ");
		}
	}

	if (policy->role_allow[idx].tgt_roles->next) {
		append_str(&rt, &sz, "}");
	}

	append_str(&rt, &sz, ";");

	return rt;
}

/*	case AP_IPV6:
		tmp_arr = nodecon->addr;
		/ * the math below prints the IPv6 fields in hexidecimal 2 bytes at a time * /
		snprintf(addr_str, ip_addr_str_len_max - 1, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
			tmp_arr[0]/(1<<16), tmp_arr[0]%(1<<16), tmp_arr[1]/(1<<16), tmp_arr[1]%(1<<16),
			tmp_arr[2]/(1<<16), tmp_arr[2]%(1<<16), tmp_arr[3]/(1<<16), tmp_arr[3]%(1<<16) );
		tmp_arr = nodecon->mask;
		snprintf(mask_str, ip_addr_str_len_max - 1, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
			tmp_arr[0]/(1<<16), tmp_arr[0]%(1<<16), tmp_arr[1]/(1<<16), tmp_arr[1]%(1<<16),
			tmp_arr[2]/(1<<16), tmp_arr[2]%(1<<16), tmp_arr[3]/(1<<16), tmp_arr[3]%(1<<16) );
		break;
*/
char *re_render_ipv6_addr(uint32_t addr[4])
{
	char *str = NULL;
	uint16_t tmp[8] = {0,0,0,0,0,0,0,0};
	int i, sz = 0, retv;
	char buff[40]; /* 8 * 4 hex digits + 7 * ':' + '\0' == max size of string */
	int contract = 0, prev_contr = 0, contr_idx_end = -1; 
	for (i = 0; i < 4; i++) {
		tmp[2*i] = addr[i]/(1<<16);
		tmp[2*i+1] = addr[i]%(1<<16);
	}

	for (i = 0; i < 8; i++) {
		if (tmp[i] == 0) {
			contract++;
			if (i == 7 && contr_idx_end == -1)
				contr_idx_end = 8;
		} else {
			if (contract > prev_contr) {
				contr_idx_end = i;
			}
			prev_contr = contract;
			contract = 0;
		}
	}

	if (prev_contr > contract)
		contract = prev_contr;

	for (i = 0; i < 8; i++) {
		if (i == contr_idx_end - contract) {
			retv = snprintf(buff + sz, 40 - sz, i?":":"::");
			sz += retv;
		} else if (i > contr_idx_end - contract && i < contr_idx_end) {
			continue;
		} else {
			retv = snprintf(buff + sz, 40 - sz, i==7?"%04x":"%04x:", tmp[i]);
			sz += retv;
		}
	}

	buff[sz] = '\0';

	str = strdup(buff);

	return str;
}

