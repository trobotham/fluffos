#include "std.h"
#include "lpc_incl.h"
#include "backend.h"
#include "comm.h"
#include "call_out.h"
#include "eoperators.h"

/*
 * This file implements delayed calls of functions.
 * Static functions can not be called this way.
 *
 * Allocate the structures several in one chunk, to get rid of malloc
 * overhead.
 */

#define CHUNK_SIZE	20

typedef struct pending_call_s {
    int delta;
    union string_or_func function;
    object_t *ob;
    svalue_t v;
    array_t *vs;
    struct pending_call_s *next;
#ifdef THIS_PLAYER_IN_CALL_OUT
    object_t *command_giver;
#endif
} pending_call_t;

static pending_call_t *call_list, *call_list_free;
static int num_call;

static void free_call PROT((pending_call_t *));
void remove_all_call_out PROT((object_t *));

/*
 * Free a call out structure.
 */
static void free_call P1(pending_call_t *, cop)
{
    if (cop->vs) {
	free_array(cop->vs);
    }
    free_svalue(&cop->v, "free_call");
    cop->v = const0n;
    cop->next = call_list_free;
    if (cop->ob) {
	free_string(cop->function.s);
	free_object(cop->ob, "free_call");
    } else {
	free_funp(cop->function.f);
    }
    cop->function.s = 0;
#ifdef THIS_PLAYER_IN_CALL_OUT
    if (cop->command_giver)
	free_object(cop->command_giver, "free_call");
#endif
    cop->ob = 0;
    call_list_free = cop;
}

/*
 * Setup a new call out.
 */
void new_call_out P5(object_t *, ob, svalue_t *, fun, int, delay, int, num_args, svalue_t *, arg)
{
    pending_call_t *cop, **copp;

    if (delay < 1)
	delay = 1;
    if (!call_list_free) {
	int i;

	call_list_free = CALLOCATE(CHUNK_SIZE, pending_call_t,
				   TAG_CALL_OUT, "new_call_out: call_list_free");
	for (i = 0; i < CHUNK_SIZE - 1; i++)
	    call_list_free[i].next = &call_list_free[i + 1];
	call_list_free[CHUNK_SIZE - 1].next = 0;
	num_call += CHUNK_SIZE;
    }
    cop = call_list_free;
    call_list_free = call_list_free->next;

    if (fun->type == T_STRING) {
	cop->function.s = make_shared_string(fun->u.string);
	cop->ob = ob;
	add_ref(ob, "call_out");
    } else {
	cop->function.f = fun->u.fp;
	fun->u.fp->hdr.ref++;
	cop->ob = 0;
    }
#ifdef THIS_PLAYER_IN_CALL_OUT
    cop->command_giver = command_giver;	/* save current user context */
    if (command_giver)
	add_ref(command_giver, "new_call_out");	/* Bump its ref */
#endif
    cop->v.type = T_NUMBER;
    cop->v.subtype = 0;
    cop->v.u.number = 0;
    cop->vs = NULL;
    if (arg) {
	assign_svalue(&cop->v, arg);
    }
    if (num_args > 0) {
	int j;

	cop->vs = allocate_empty_array(num_args);
	for (j = 0; j < num_args; j++) {
	    assign_svalue_no_free(&cop->vs->item[j], &arg[j + 1]);
	}
    }
    for (copp = &call_list; *copp; copp = &(*copp)->next) {
	if ((*copp)->delta >= delay) {
	    (*copp)->delta -= delay;
	    cop->delta = delay;
	    cop->next = *copp;
	    *copp = cop;
	    return;
	}
	delay -= (*copp)->delta;
    }
    *copp = cop;
    cop->delta = delay;
    cop->next = 0;
}

/*
 * See if there are any call outs to be called. Set the 'command_giver'
 * if it is a living object. Check for shadowing objects, which may also
 * be living objects.
 */
void call_out()
{
    pending_call_t *cop;
    jmp_buf save_error_recovery_context;
    int save_rec_exists;
    object_t *save_command_giver;
    static int last_time;

    if (call_list == 0) {
	last_time = current_time;
	return;
    }
    if (last_time == 0)
	last_time = current_time;
    save_command_giver = command_giver;
    current_interactive = 0;
    call_list->delta -= current_time - last_time;
    last_time = current_time;
    memcpy((char *) save_error_recovery_context,
	   (char *) error_recovery_context, sizeof error_recovery_context);
    save_rec_exists = error_recovery_context_exists;
    error_recovery_context_exists = NORMAL_ERROR_CONTEXT;
    while (call_list && call_list->delta <= 0) {
	/*
	 * Move the first call_out out of the chain.
	 */
	cop = call_list;
	call_list = call_list->next;
	/*
	 * A special case: If a lot of time has passed, so that current call
	 * out was missed, then it will have a negative delta. This negative
	 * delta implies that the next call out in the list has to be
	 * adjusted.
	 */
	if (call_list && cop->delta < 0)
	    call_list->delta += cop->delta;
	if (!(cop->ob && cop->ob->flags & O_DESTRUCTED)) {
	    if (SETJMP(error_recovery_context)) {
		clear_state();
		debug_message("Error in call out.\n");
	    } else {
		svalue_t v;
		object_t *ob;

		ob = cop->ob;
#ifndef NO_SHADOWS
		if (ob)
		    while (ob->shadowing)
			ob = ob->shadowing;
#endif				/* NO_SHADOWS */
		command_giver = 0;
#ifdef THIS_PLAYER_IN_CALL_OUT
		if (cop->command_giver &&
		    !(cop->command_giver->flags & O_DESTRUCTED)) {
		    command_giver = cop->command_giver;
		} else if (ob && (ob->flags & O_LISTENER)) {
		    command_giver = ob;
		}
#endif
		v.type = cop->v.type;
		v.subtype = 0;
		v.u = cop->v.u;
		v.subtype = cop->v.subtype;	/* Not always used */
		if (v.type == T_OBJECT && (v.u.ob->flags & O_DESTRUCTED)) {
		    v.type = T_NUMBER;
		    v.subtype = 0;
		    v.u.number = 0;
		}
		/* current object no longer set */
		push_svalue(&v);
		if (cop->vs) {
		    int j;

		    check_for_destr(cop->vs);
		    for (j = 0; j < cop->vs->size; j++) {
			push_svalue(&cop->vs->item[j]);
		    }
		}
		if (cop->ob) {
		    (void) apply(cop->function.s, cop->ob, 
				 1 + (cop->vs ? cop->vs->size : 0),
				 ORIGIN_CALL_OUT);
		} else {
		    (void) call_function_pointer(cop->function.f, 1 + (cop->vs ? cop->vs->size : 0));
		}
	    }
	}
	free_call(cop);
    }
    memcpy((char *) error_recovery_context,
	   (char *) save_error_recovery_context,
	   sizeof error_recovery_context);
    error_recovery_context_exists = save_rec_exists;
    command_giver = save_command_giver;
}

/*
 * Throw away a call out. First call to this function is discarded.
 * The time left until execution is returned.
 * -1 is returned if no call out pending.
 */
int remove_call_out P2(object_t *, ob, char *, fun)
{
    pending_call_t **copp, *cop;
    int delay = 0;

    if (!ob) return -1;
    for (copp = &call_list; *copp; copp = &(*copp)->next) {
	delay += (*copp)->delta;
	if ((*copp)->ob == ob && strcmp((*copp)->function.s, fun) == 0) {
	    cop = *copp;
	    if (cop->next)
		cop->next->delta += cop->delta;
	    *copp = cop->next;
	    free_call(cop);
	    return delay;
	}
    }
    return -1;
}

int find_call_out P2(object_t *, ob, char *, fun)
{
    pending_call_t **copp;
    int delay = 0;

    if (!ob) return -1;
    for (copp = &call_list; *copp; copp = &(*copp)->next) {
	delay += (*copp)->delta;
	if ((*copp)->ob == ob && strcmp((*copp)->function.s, fun) == 0) {
	    return delay;
	}
    }
    return -1;
}

int print_call_out_usage P1(int, verbose)
{
    int i;
    pending_call_t *cop;

    for (i = 0, cop = call_list; cop; cop = cop->next)
	i++;

    if (verbose == 1) {
	add_message("Call out information:\n");
	add_message("---------------------\n");
	add_vmessage("Number of allocated call outs: %8d, %8d bytes\n",
		    num_call, num_call * sizeof(pending_call_t));
	add_vmessage("Current length: %d\n", i);
    } else {
	if (verbose != -1)
	    add_vmessage("call out:\t\t\t%8d %8d (current length %d)\n", num_call,
			num_call * sizeof(pending_call_t), i);
    }
    return (int) (num_call * sizeof(pending_call_t));
}

#ifdef DEBUGMALLOC_EXTENSIONS
void mark_call_outs()
{
    pending_call_t *cop;

    for (cop = call_list; cop; cop = cop->next) {
	mark_svalue(&cop->v);
	if (cop->ob) {
	    cop->ob->extra_ref++;
	    EXTRA_REF(BLOCK(cop->function.s))++;
	}
#ifdef THIS_PLAYER_IN_CALL_OUT
    if (cop->command_giver)
	cop->command_giver->extra_ref++;
#endif
    }
}
#endif

/*
 * Construct an array of all pending call_outs. Every item in the array
 * consists of 4 items (but only if the object not is destructed):
 * 0:	The object.
 * 1:	The function (string).
 * 2:	The delay.
 * 3:	The argument.
 */
array_t *get_all_call_outs()
{
    int i, next_time;
    pending_call_t *cop;
    array_t *v;

    for (i = 0, cop = call_list; cop; i++, cop = cop->next)
	;
    v = allocate_empty_array(i);
    next_time = 0;

    for (i = 0, cop = call_list; cop; i++, cop = cop->next) {
	array_t *vv;

	next_time += cop->delta;
	if (cop->ob && cop->ob->flags & O_DESTRUCTED)
	    continue;  /* This should be smarter.  -Beek */
	vv = allocate_empty_array(4);
	if (cop->ob) {
	    vv->item[0].type = T_OBJECT;
	    vv->item[0].u.ob = cop->ob;
	    add_ref(cop->ob, "get_all_call_outs");
	    vv->item[1].type = T_STRING;
	    vv->item[1].subtype = STRING_SHARED;
	    vv->item[1].u.string = make_shared_string(cop->function.s);
	} else {
	    vv->item[0].type = T_OBJECT;
	    vv->item[0].u.ob = cop->function.f->hdr.owner;
	    add_ref(cop->function.f->hdr.owner, "get_all_call_outs");
	    vv->item[1].type = T_STRING;
	    vv->item[1].subtype = STRING_SHARED;
	    vv->item[1].u.string = make_shared_string("<function>");
	}
	vv->item[2].u.number = next_time;
	vv->item[2].type = T_NUMBER;
	assign_svalue_no_free(&vv->item[3], &cop->v);

	v->item[i].type = T_ARRAY;
	v->item[i].u.arr = vv;	/* Ref count is already 1 */
    }
    return v;
}

void
remove_all_call_out P1(object_t *, obj)
{
    pending_call_t **copp, *cop;

    copp = &call_list;
    while (*copp) {
	if (((*copp)->ob == obj) || ((*copp)->ob->flags & O_DESTRUCTED)) {
	    cop = *copp;
	    if (cop->next)
		cop->next->delta += cop->delta;
	    *copp = cop->next;
	    free_call(cop);
	} else
	    copp = &(*copp)->next;
    }
}
