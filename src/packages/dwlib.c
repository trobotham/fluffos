#include "../lpc_incl.h"
#include "../comm.h"
#include "../file_incl.h"
#include "../file.h"
#include "../backend.h"
#include "../swap.h"
#include "../compiler.h"
#include "../main.h"
#include "../eoperators.h"
#include "../efuns_main.h"
#include "../efun_protos.h"
#include "../simul_efun.h"
#include "../add_action.h"
#include "../array.h"
#include "../master.h"
#include "../port.h"


#ifdef F_QUERY_MULTIPLE_SHORT
/* Hideous mangling of C code by Taffyd. */ 
void query_multiple_short P6(svalue_t *, arg, const char *, type, int, no_dollars, int, quiet, int, dark, int, num_arg) { 
    char m[] = "$M$";
    char s[] = "_short";
    char default_function[] = "a_short";
    char separator[] = ", ";
    char andsep[] = " and ";
    int mlen = strlen(m);
    int slen = strlen(s);
    int seplen = strlen( separator );
    int andlen = strlen( andsep );

    array_t *arr = arg->u.arr;
    svalue_t *sv;
    svalue_t *v;
    int size = arr->size;
    int i;
    int len;
    int total_len;
    char *str, *res;
    object_t *ob;
    char *fun; 

    if (!size) {
        str = new_string(0, "f_query_multiple_short");
        str[0] = '\0';
        pop_n_elems(num_arg);
        push_malloced_string(str);
        return; 
    }
    
    /* 
    if (no_dollars && sizeof(args) && objectp(args[0]) && undefinedp(dark) && 
        this_player() && environment(this_player())) {
        dark = this_player()->check_dark(environment(this_player())->query_light());
        if (dark) {
        return "some objects you cannot make out";
        }
    } */ 

    if (no_dollars && arr->item->type == T_OBJECT && !dark && command_giver &&
        command_giver->super) { 
        call_origin = ORIGIN_EFUN;
        if(!apply_low("query_light", command_giver->super, 0))
            push_number(0);
        v = apply("check_dark", command_giver, 1, ORIGIN_EFUN);
        
        if (v && v->type == T_NUMBER && v->u.number) {
            pop_n_elems(num_arg);
            copy_and_push_string("some objects you cannot make out"); 
            return;
        }
    }

    /* If we don't have a type parameter, then use default_function */ 
    /* We need to free this value with FREE_MSTR() */ 

    if ( !type ) { 
        len = strlen( default_function );
        fun = new_string( len, "f_query_multiple_short");
        fun[len] = '\0';
        strncpy( fun, default_function, len );
    }
    else { 
        len = strlen( type ) + slen;
        fun = new_string( len, "f_query_multiple_short");
        fun[len] = '\0';
        strncpy( fun, type, len );
        strncpy( fun + strlen( type ), s, slen);
    }
   
    /* Check to see if there are any non-objects in the array. */ 
    for (i = 0; i < size; i++) {
        if ((arr->item + i)->type != T_OBJECT) {
            break;
        }
    }

    /* The array consists only of objects, and will use the $M$ 
       expansion code. */ 
    if (i == size && !no_dollars) {
        str = new_string(max_string_length, "f_query_multiple_short");
        str[max_string_length]= '\0';
        strncpy(str, m, mlen);
        total_len = mlen;

        for ( i = 0; i < size; i++ ) {
            sv = (arr->item + i);
            push_number(quiet);
            v = apply(fun, sv->u.ob, 1, ORIGIN_EFUN);

            if (!v || v->type != T_STRING) {
                continue;                
            }
            if(total_len + SVALUE_STRLEN(v) > max_string_length - mlen)
                continue;
            strncpy(str + total_len, v->u.string, (len = SVALUE_STRLEN(v)));
            total_len += len;
        }

        strncpy(str + total_len, m, mlen);
        total_len += mlen;

        res = new_string( total_len, "f_query_multiple_short" );
        res[ total_len ] = '\0';
        memcpy(res, str, total_len);

        /* Clean up our temporary buffer. */ 

        FREE_MSTR(str);
        FREE_MSTR(fun);

        pop_n_elems(num_arg);
        push_malloced_string(res);
        return;
    }

    /* This is a mixed array, so we don't use $M$ format.  Instead, we 
       do as much $a_short$ conversion as we can etc.  */ 

    str = new_string(max_string_length, "f_query_multiple_short");
    str[max_string_length]= '\0';
    total_len = 0;

    for ( i = 0; i < size; i++ ) {
        sv = (arr->item + i);
    
        switch(sv->type) {
            case T_STRING:
                len = SVALUE_STRLEN(sv);
                if(total_len + len < max_string_length){
                    strncpy(str + total_len, sv->u.string, len);
                    total_len += len;
                }
                break;
            case T_OBJECT:
                push_number(quiet);
                v = apply(fun, sv->u.ob, 1, ORIGIN_EFUN);

                if (!v || v->type != T_STRING) {
                    continue;                
                }

                if(total_len + SVALUE_STRLEN(v) < max_string_length){
                    strncpy(str + total_len, v->u.string, 
                            (len = SVALUE_STRLEN(v)));
                    total_len += len;
                }

                break;
            case T_ARRAY:
              /* Does anyone use this? */ 
              /* args[ i ] = "$"+ type +"_short:"+ file_name( args[ i ][ 1 ] ) +"$"; */ 
            default:    
                /* Get the next element. */ 
                continue;            
                break;
        }
        
        if ( len && size > 1 ) {
            if ( i < size - 2 ) {
                if(total_len+seplen < max_string_length){
                    strncpy( str + total_len, separator, seplen );
                    total_len += seplen;
                }
            }
            else { 
                if ( i < size - 1 ) {    
                    if(total_len+andlen < max_string_length){
                        strncpy( str + total_len, andsep, andlen );
                        total_len += andlen;
                    }
                }
            }
        }
    }

    FREE_MSTR(fun);

    res = new_string(total_len, "f_query_multiple_short");
    res[total_len] = '\0';
    memcpy(res, str, total_len);

    FREE_MSTR(str);

    /* Ok, now that we have cleaned up here we have to decide what to do
       with it. If nodollars is 0, then we need to pass it to an object
       for conversion. */ 

    if (no_dollars) { 
        if (command_giver) { 
            /* We need to call on this_player(). */ 
            push_malloced_string(res);
            v = apply("convert_message", command_giver, 1, ORIGIN_EFUN);
            
            if (v && v->type == T_STRING) { 
                pop_n_elems(num_arg);
                share_and_push_string(v->u.string);
            }
            else { 
                pop_n_elems(num_arg);
                push_undefined();
            }
            
        }
        else {
            /* We need to find /global/player. */ 
            /* Does this work? Seems not to. */ 
            ob = find_object("/global/player");
            
            if (ob) {
                push_malloced_string(res);
                v = apply("convert_message", ob, 1, ORIGIN_EFUN);
                
                /* Return the result! */ 
                if (v && v->type == T_STRING) { 
                    pop_n_elems(num_arg);
                    share_and_push_string(v->u.string);
                }
                else { 
                    pop_n_elems(num_arg);
                    push_undefined();
                }
            }
            else { 
                pop_n_elems(num_arg);
                push_undefined();
            }
        }

    }
    else { 
        pop_n_elems(num_arg);
        push_malloced_string(res);
    }
} /* query_multiple_short() */


void
f_query_multiple_short PROT((void))
{
    svalue_t *sv = sp - st_num_arg + 1;
    const char *type = NULL;
    int no_dollars = 0, quiet = 0, dark = 0;
    
    if ( st_num_arg > 4)  {
        if (sv[4].type == T_NUMBER) {
            dark = sv[4].u.number;
        }
    }

    if ( st_num_arg > 3)  {
        if (sv[3].type == T_NUMBER) {
            quiet = sv[3].u.number;
        }
    }

    if ( st_num_arg > 2)  {
        if (sv[2].type == T_NUMBER) {
            no_dollars = sv[2].u.number;
        }
    }

    if (st_num_arg >= 2) {
        if (sv[1].type == T_STRING) { 
            type = sv[1].u.string;
        }
    }


    query_multiple_short(sv, type, no_dollars, quiet, dark, st_num_arg);
}

#endif

#ifdef F_REFERENCE_ALLOWED
/* Hideous mangling of C code by Taffyd. */ 

/* varargs int reference_allowed(object referree, mixed referrer) */ 

#define PLAYTESTER_HANDLER "/obj/handlers/playtesters"
#define PLAYER_HANDLER "/obj/handlers/player_handler"

int _in_reference_allowed = 0;

int reference_allowed P3(object_t *, referee, object_t *, referrer_obj, const char *, referrer_name) 
{
    int invis = 0;
    int referee_creator = 0;
    svalue_t *v;
    svalue_t *item;
    array_t *vec;
    const char *referee_name = NULL;
    object_t *playtester_handler = NULL;
    object_t *player_handler = NULL;
    int referrer_playtester = 0;
    int referrer_match = 0; 
    int playtester_match = 0;
    int i;
    int size;
    int ret = 0; 
   
    /* Check to see whether we're invisible */ 
    v = apply("query_invis", referee, 0, ORIGIN_EFUN);

    if (v && v->type == T_NUMBER) {
        invis = v->u.number;
    }
    /* And if we're a creator. */ 
    v = apply("query_creator", referee, 0, ORIGIN_EFUN);

    if ( v && v->type == T_NUMBER) {
        referee_creator = v->u.number;
    }
    
    /* If we're not invisible, or not a creator, or currently in a 
       reference allowed call, then we can see them. */ 
    if (!invis || !referee_creator || _in_reference_allowed) {
        return 1;
    }

    _in_reference_allowed = 1;
    
    if (referrer_obj && referee == referrer_obj) {
        _in_reference_allowed = 0;
        return 1;
    }
    
    /* Determine the names of these guys.
     * We might need to make copies of these strings? apply()
     * has a few oddities. 
     */ 

    v = apply("query_name", referee, 0, ORIGIN_EFUN);

    if (v && v->type == T_STRING) {
        referee_name = v->u.string;
    }
    
    if (referrer_obj && !referrer_name) { 
        v = apply("query_name", referrer_obj, 0, ORIGIN_EFUN);

        if (v && v->type == T_STRING) { 
            referrer_name = v->u.string;    
        }
    }
    
    if (!referee_name || !referrer_name) {
        _in_reference_allowed = 0;
        return 1;
    }

    playtester_handler = find_object(PLAYTESTER_HANDLER);
    
    if ( playtester_handler ) { 
        copy_and_push_string(referrer_name);
        v = apply("query_playtester", playtester_handler, 1, ORIGIN_EFUN);
        
        if (v && v->type == T_NUMBER) { 
            referrer_playtester =  v->u.number;
        }
    }

    v = apply( "query_allowed", referee, 0, ORIGIN_EFUN);

    if (v && v->type == T_ARRAY) {
        vec = v->u.arr;
        size = vec->size;
        /* Iterate through the allowed array. */ 
        for (i = 0; i < size; i++) {
            item = vec->item + i;

            if (strcmp(referrer_name, item->u.string) == 0) { 
                referrer_match = 1;
                
                /* If they're not a playtester, then bail as soon as 
                 * we make this match. */ 
                if (!referrer_playtester) {
                    break;
                }
            }

            if (referrer_playtester && 
                strcmp("playtesters", item->u.string) == 0) { 
                playtester_match = 1;
                
                /* If we've already made a referrer match, then time
                 * to bail now. */ 
                if (referrer_match) {
                    break;
                }
            }
        }
    }
    
    /* If we found a match, then they are allowed so bail. */ 
    if ( referrer_match || playtester_match ) {
        _in_reference_allowed = 0;
        return 1;
    }

    switch(invis) { 
        case 3:
            /* Check for High Lord Invis. */ 

            copy_and_push_string(referrer_name);
            v = apply("high_programmer", master_ob, 1, ORIGIN_EFUN);
            
            if (v && v->type == T_NUMBER) {
                ret = v->u.number;
            }
        break;
        
        case 2:
            /* Check for Lord Invis */

                      copy_and_push_string(referrer_name);
            v = apply("query_lord", master_ob, 1, ORIGIN_EFUN);
            
            if (v && v->type == T_NUMBER) {
                ret = v->u.number;
            }
        break;

        case 1: { 
            /* Creator Invis */ 
            if (referrer_obj) {
                v = apply("query_creator", referrer_obj, 0, ORIGIN_EFUN);

                if (v && v->type == T_NUMBER) {
                    ret = v->u.number;
                }
            }
            else {
                player_handler = find_object(PLAYER_HANDLER);
                copy_and_push_string(referrer_name);
                v = apply("test_creator", player_handler, 1, ORIGIN_EFUN);

                if (v && v->type == T_NUMBER) {
                    ret = v->u.number;
                }
            }
        }
        break;

        default:
            /* A normal player.
             * Shouldn't we do this somewhere else? */
            
            ret = 1;
        break;
    }

    _in_reference_allowed = 0;
    return ret;
}

void
f_reference_allowed PROT((void))
  {
    svalue_t *sv = sp - st_num_arg + 1;
    svalue_t *v;
    object_t *referee = NULL;
    object_t *referrer_obj = command_giver; /* Default to this_player(). */
    const char *referrer_name = NULL;
    int result = 0;
    int num_arg = st_num_arg;

    /* Maybe I could learn how to use this :p 
    CHECK_TYPES(sp-1, T_NUMBER, 1, F_MEMBER_ARRAY); */

    if (sv->type == T_OBJECT && sv->u.ob) {
        referee = sv->u.ob;
    }

    if (st_num_arg > 1) {
        if (sv[1].type == T_STRING && sv[1].u.string) {
            /* We've been passed in a string, now we need to call 
             * find_player() */ 
#ifdef F_FIND_PLAYER
            /* If we have a find_player() efun, then we need to sue 
             * the following method.  This hasn't been tested!
             */ 
             referrer = find_living_object(sv[1].u.string, 1);
#else
            if (simul_efun_ob) {
                push_svalue(&sv[1]);
                v = apply("find_player", simul_efun_ob, 1, ORIGIN_EFUN);

                if (v && v->type == T_OBJECT) {
                    referrer_obj = v->u.ob;
                    referrer_name = sv[1].u.string;
                }
                else {
                    referrer_obj = NULL;
                    referrer_name = sv[1].u.string;
                }
            }
#endif
        }
        if (sv[1].type == T_OBJECT && sv[1].u.ob) { 
            referrer_obj = sv[1].u.ob;
            referrer_name = NULL;
        }
    }

    if (referee && (referrer_obj || referrer_name)) {
        result = reference_allowed(referee, referrer_obj, referrer_name);

        pop_n_elems(num_arg);
        push_number(result);
    } else { 
        pop_n_elems(num_arg);
        push_undefined();
    }
}

#endif

#ifdef F_ELEMENT_OF
void f_element_of PROT((void)){
  array_t *arr = sp->u.arr;
  if(!arr->size){
    error("Can't take element from empty array.\n");
  }
  assign_svalue_no_free(sp, &arr->item[random_number(arr->size)]);
  free_array(arr);
}
#endif
#ifdef F_SHUFFLE

/* shuffle efun, based on LPC shuffle simul efun.
 * conversion by Taffyd.
 */

void shuffle P1(array_t *, args) {
    int i, j;
    svalue_t temp;

    /* Hrm, if we have less than two elements, then the order isn't 
     * going to change! Let's just leave the old array on the stack. 
     */
    if ( args->size < 2 ) {
        return;
    }
    
    for ( i = 0; i < args->size; i++ ) {
        j = random_number( i + 1 );
        
        if ( i == j ) {
            continue;
        }
        
        temp = args->item[i];
        args->item[i] = args->item[j];
        args->item[j] = temp;
    }
    
    /* Well, that's it. We don't need to push or anything. */ 
}

void
f_shuffle PROT((void))
{
    svalue_t *sv = sp - st_num_arg + 1;

    if (sv->type == T_ARRAY && sv->u.arr) {
        shuffle(sv->u.arr);
    }
    else {
        push_refed_array(&the_null_array);
    }
}
#endif

#ifdef F_SET_CHECK_LIMIT
extern int check_limit;

void f_set_check_limit PROT((void)){
  check_limit = sp->u.number;
  if(check_limit > NUM_OPCODES)
    check_limit = NUM_OPCODES;
  if(check_limit < BASE)
    check_limit = BASE;
  sp->u.number = check_limit;
}
#endif

#ifdef F_MAX

void
f_max PROT((void)) {
   svalue_t *sarr = sp - st_num_arg + 1;
   array_t *arr = sarr->u.arr;
   int find_index = 0;
   int max_index = 0;
   int i;

   if( !arr->size ) {
      error( "Can't find max of an empty array.\n" );
   }

   if( arr->item->type != T_NUMBER && arr->item->type != T_REAL &&
            arr->item->type != T_STRING ) {
      error( "Array must consist of ints, floats or strings.\n" );
   }

   for( i = 1; i < arr->size; i++ ) {
      // Check the type of this element.
      switch( arr->item[i].type ) {
         case T_NUMBER:
            switch( arr->item[max_index].type ) {
               case T_NUMBER:
                  if( arr->item[i].u.number > arr->item[max_index].u.number )
                     max_index = i;
                  break;
               case T_REAL:
                  if( arr->item[i].u.number > arr->item[max_index].u.real )
                     max_index = i;
                  break;
               default:
                  error( "Inhomogeneous array.\n" );
            }
            break;
         case T_REAL:
            switch( arr->item[max_index].type ) {
               case T_NUMBER:
                  if( arr->item[i].u.real > arr->item[max_index].u.number )
                     max_index = i;
                  break;
               case T_REAL:
                  if( arr->item[i].u.real > arr->item[max_index].u.real )
                     max_index = i;
                  break;
               default:
                  error( "Inhomogeneous array.\n" );
            }
            break;
         case T_STRING:
            if( arr->item[max_index].type != T_STRING ) {
               error( "Inhomogeneous array.\n" );
            }
            if( strcmp( arr->item[i].u.string,
                     arr->item[max_index].u.string ) > 0 )
               max_index = i;
            break;
         default:
            error( "Array must consist of ints, floats or strings.\n" );
      }
   }

   if( st_num_arg == 2 && sp->u.number != 0 )
      find_index = 1;

   pop_n_elems( st_num_arg );

   if( find_index != 0 )
      push_number( max_index );
   else {
      switch( arr->item[max_index].type ) {
         case T_STRING:
            share_and_push_string( arr->item[max_index].u.string );
            break;
         case T_NUMBER:
            push_number( arr->item[max_index].u.number );
            break;
         default:
            push_real( arr->item[max_index].u.real );
      }
   }

}

#endif

#ifdef F_MIN

void
f_min PROT((void)) {
   svalue_t *sarr = sp - st_num_arg + 1;
   array_t *arr = sarr->u.arr;
   int find_index = 0;
   int min_index = 0;
   int i;

   if( !arr->size ) {
      error( "Can't find min of an empty array.\n" );
   }

   if( arr->item->type != T_NUMBER && arr->item->type != T_REAL &&
            arr->item->type != T_STRING ) {
      error( "Array must consist of ints, floats or strings.\n" );
   }

   for( i = 1; i < arr->size; i++ ) {
      // Check the type of this element.
      switch( arr->item[i].type ) {
         case T_NUMBER:
            switch( arr->item[min_index].type ) {
               case T_NUMBER:
                  if( arr->item[i].u.number < arr->item[min_index].u.number )
                     min_index = i;
                  break;
               case T_REAL:
                  if( arr->item[i].u.number < arr->item[min_index].u.real )
                     min_index = i;
                  break;
               default:
                  error( "Inhomogeneous array.\n" );
            }
            break;
         case T_REAL:
            switch( arr->item[min_index].type ) {
               case T_NUMBER:
                  if( arr->item[i].u.real < arr->item[min_index].u.number )
                     min_index = i;
                  break;
               case T_REAL:
                  if( arr->item[i].u.real < arr->item[min_index].u.real )
                     min_index = i;
                  break;
               default:
                  error( "Inhomogeneous array.\n" );
            }
            break;
         case T_STRING:
            if( arr->item[min_index].type != T_STRING ) {
               error( "Inhomogeneous array.\n" );
            }
            if( strcmp( arr->item[i].u.string,
                     arr->item[min_index].u.string ) < 0 )
               min_index = i;
            break;
         default:
            error( "Array must consist of ints, floats or strings.\n" );
      }
   }

   if( st_num_arg == 2 && sp->u.number != 0 )
      find_index = 1;

   pop_n_elems( st_num_arg );

   if( find_index != 0 )
      push_number( min_index );
   else {
      switch( arr->item[min_index].type ) {
         case T_STRING:
            share_and_push_string( arr->item[min_index].u.string );
            break;
         case T_NUMBER:
            push_number( arr->item[min_index].u.number );
            break;
         default:
            push_real( arr->item[min_index].u.real );
      }
   }

}
#endif

#ifdef F_ROLL_MDN

void
f_roll_MdN PROT((void)) {
   int roll = 0;

   if ( (sp - 1)->u.number > 0 && sp->u.number > 0 ) {
      while( (sp - 1)->u.number-- )
         roll += 1 + random_number( sp->u.number );
   }

   pop_stack();            // Pop one...
   sp->u.number = roll;    // And change the other!
}

#endif

#ifdef F_ADD_A

void
f_add_a PROT((void)) {
   const char *str = sp->u.string;
   char *ret;
   char *p;
   int len;

   while( *str == ' ' )
      str++;

   // If *str is 0, it was only spaces.  Return "a ".
   if( *str == 0 ) {
      pop_stack();
      copy_and_push_string( "a " );
      return;
   }

   len = strlen( str );
   // Don't add anything if it already begins with a or an.
   if( strncmp( str, "a ", 2 ) == 0 || strncmp( str, "an ", 3 ) == 0 ) {
      return;
   }

   switch( *str ) {
      case 'a':
      case 'e':
      case 'i':
      case 'o':
      case 'u':
      case 'A':
      case 'E':
      case 'I':
      case 'O':
      case 'U':
         // Add an.
         if( len + 3 > max_string_length ) {
            free_string_svalue( sp );
            error( "add_a() exceeded max string length.\n" );
         }
         ret = new_string( len + 3, "f_add_a" );
         memcpy( ret, "an ", 3 );
         p = ret + 3;
         break;
      default:
         // Add a.
         if( len + 2 > max_string_length ) {
            free_string_svalue( sp );
            error( "add_a() exceeded max string length.\n" );
         }
         ret = new_string( len + 2, "f_add_a" );
         memcpy( ret, "a ", 2 );
         p = ret + 2;
         break;
   }

   // Add the rest of the string.
   memcpy( p, str, len + 1 );    // + 1: get the \0.
   free_string_svalue( sp );
   sp->type = T_STRING;
   sp->subtype = STRING_MALLOC;
   sp->u.string = ret;
}

#endif
// This along with add_a() is the only sfun in /secure/simul_efun/add_a.c
#ifdef F_VOWEL
void
f_vowel PROT((void)) {
   char v = (char)sp->u.number;

   if( v == 'a' || v == 'e' || v == 'i' || v == 'o' || v == 'u' ||
          v == 'A' || v == 'E' || v == 'I' || v == 'O' || v == 'U' )
      sp->u.number = 1;
   else
      sp->u.number = 0;
}
#endif

#ifdef F_NUM_CLASSES

void
f_num_classes PROT((void)) {
   int i = sp->u.ob->prog->num_classes;
   pop_stack();
   push_number( i );
}

#endif