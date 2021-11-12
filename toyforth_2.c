#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#define dbgprintf(...)
//#define dbgprintf printf

//#define super_bytecode_debug

// Stax

int data_stack[1024];
int rtrn_stack[1024];
int stak_ptr = 0, rtrn_ptr = 0;

void push_data(int data)    { data_stack[stak_ptr] = data; stak_ptr++; }
int  pop_data()             { stak_ptr--; return data_stack[stak_ptr]; }
int  peek_data()            { return data_stack[stak_ptr-1]; }

void push_rtrn(int data)    { rtrn_stack[rtrn_ptr] = data; rtrn_ptr++; }
int  pop_rtrn()             { rtrn_ptr--; return rtrn_stack[rtrn_ptr]; }
int  peek_rtrn()            { return rtrn_stack[rtrn_ptr-1]; }


void stack_printer()
{
    dbgprintf("@@@@@ sp: %d\t| ", stak_ptr);
    for(int i=0;i<stak_ptr;i++)
        dbgprintf("%d\t", data_stack[stak_ptr-i-1]);
    dbgprintf("\n");

    dbgprintf("@@@@@ rp: %d\t| ", rtrn_ptr);
    for(int i=0;i<rtrn_ptr;i++)
        dbgprintf("%d\t", rtrn_stack[rtrn_ptr-i-1]);
    dbgprintf("\n");
}


// Dictionary and function that runs other words
typedef struct dict_entry
{
    char* name;                 // Function name (if applic)
    int (*code_intp)(int);      // Code to run when word is being executed
    int (*code_comp)(int);      // Code to run when word is being compiled
    void* data;                 // Data for whatever
} dict_entry;

dict_entry dictionary[1024];
int dict_size = 0, top_word = 0, top_word_count = 0;

int* dict_top_bytecode()
{
    return (int*)dictionary[top_word].data;
}

int bytecode_end(int* bytecode)
{
    int offset = 0;

    while(bytecode[offset] != -1)
        offset += 2;

    return offset;
}

int add_word(dict_entry entry)
{
    dictionary[dict_size] = entry;
    dict_size++;
    return dict_size - 1;
}

void bytecode_add_inst(int inst, int param)
{
    // Add a "call token" to the top word
    int* bytecode = dict_top_bytecode();
    int last = bytecode_end(bytecode);

    bytecode[last] = inst;
    bytecode[last+1] = param;
    bytecode[last+2] = -1;
    top_word_count++;
}

int echo_word(int exe_token)
{
    // Add a "call token" to the top word
    bytecode_add_inst(1, exe_token);

    return 0;
}

int no_compile_word(int exe_token)
{
    return -1;
}

int run_word(int exe_token)
{
    // THE BYTECODE INTERPRETER IS HERE !_!_!_
    // tokens:
    // -1   Aborts execution and returns
    // 0    No operation
    // 1    Call execution token x
    // 2    Jump backwards x spaces
    // 3    Jump forwards x spaces
    // 4    Skip next instruction if top of stack is true (pops it)
    // 5    Push number x to data stack
    int* bytecode = (int*)dictionary[exe_token].data;
    int bp = 0;

    dbgprintf("Executing token %d (%s)\n", exe_token, dictionary[exe_token].name);

    while(bp < 1024) // essentially just "true until we escape the bytecode array"
    {
        int inst = bytecode[bp], param = bytecode[bp+1];
        dbgprintf("!#!# bp: %d, inst: %d %d\n", bp, inst, param);

        switch(inst)
        {
            case(-1): bp = 99999; break;
            case(0): break;
            case(1): (*dictionary[param].code_intp)(param); break;
            case(2): bp -= param; break;
            case(3): bp += param; break;
            case(4): if(pop_data() != 0) { bp += 2; } break;
            case(5): push_data(param); break;
            default: break;
        }

#ifdef super_bytecode_debug
        stack_printer();
#endif

        bp += 2;
    }

    return 0;
}

int find_word(char* name)
{
    if(strcmp("", name) == 0)
    {
        return -1;
    }

    // Search down the dictionary
    int found = dict_size-1;
    
    while(found >= 0)
    {
        if((strlen(dictionary[found].name) == strlen(name)) && (strcmp(dictionary[found].name, name) == 0))
        {
            return found;
        }

        found--;
    }

    return -1;
}


// Input stream
char input_buffer[1024];
int input_ptr = 0;

int input_len_until(char tok)
{
    int len = 0;

    while(input_buffer[input_ptr+len] != tok && input_buffer[input_ptr+len] != '\0')
        len++;

    return len;
}

void input_snap_to_space()
{
    if(input_buffer[input_ptr] == ' ' || input_buffer[input_ptr] == '\0')
        return;

    input_ptr += input_len_until(' ');
}

void input_snap_past_space()
{
    while(input_buffer[input_ptr] == ' ')
        input_ptr++;
}


// Some functions to define low-level words

int word_drop_intp(int exe_token)
{
    stak_ptr--;
    return 0;
}

int word_dup_intp(int exe_token)
{
    push_data(peek_data());
    return 0;
}

int compiling_state = 0;

int word_tick_intp(int exe_token)
{
    // Find length of word to attempt searching for
    int word_len = input_len_until(' ');
    dbgprintf("## Call to TICK: found word from %d to %d, '%.*s'\n", input_ptr, word_len, word_len, input_buffer+input_ptr);

    // Abort if there isn't actually a word
    if(word_len == 0)
    {
        push_data(-1);
        return 0;
    }

    // Search down the dictionary
    int found = dict_size-1;
    
    while(found >= 0)
    {
        dbgprintf("Checking %s vs %.*s\n", dictionary[found].name, word_len, input_buffer+input_ptr);

        if((strlen(dictionary[found].name) == word_len) && (strncmp(dictionary[found].name, input_buffer+input_ptr, word_len) == 0))
        {
            push_data(found);
            break;
        }

        found--;
    }

    if(found < 0)
        push_data(-1);

    input_ptr += word_len;

    return 0;
}

int word_number_exec(int exe_token)
{
    push_data((int)dictionary[exe_token].data);
    return 0;
}

int word_number_intp(int exe_token)
{
    // Find length of number to grab
    int num_len = input_len_until(' ');
    int converted_num = 0;
    int neg_mult = 1;
    dbgprintf("## Call to >NUMBER: found candidate from %d to %d, '%.*s'\n", input_ptr, num_len, num_len, input_buffer+input_ptr);

    if(input_buffer[input_ptr] == '-')
    {
        neg_mult = -1;
        num_len--;
        input_ptr++;
    }

    if(num_len < 1)
    {
        push_data(INT_MIN);
        return 0;
    }

    for(int n=0;n<num_len;n++)
    {
        char next_digit = input_buffer[input_ptr];
        input_ptr++;

        if((int)next_digit < 0x30 || (int)next_digit > 0x39)
        {
            push_data(INT_MIN);
            return 0;
        }

        converted_num *= 10;
        converted_num += (int)next_digit - 0x30;
    }
    
    converted_num *= neg_mult;

    if(compiling_state == 0)
    {
        // Put number on stack
        push_data(converted_num);
        dbgprintf("## Put %d onto data stack\n", converted_num);
    }
    else
    {
        // Store constant
        int num_tok = add_word((dict_entry){"", word_number_exec, echo_word, (void*)(converted_num)});
        echo_word(num_tok);
        dbgprintf("## Put %d into top_word: %s\n", converted_num, dictionary[top_word].name);
    }

    return 0;
}

int word_execute_intp(int exe_token)
{
    int token = pop_data();
    dbgprintf("### EXECUTEing word %d: %s\n", token, dictionary[token].name);

    if(compiling_state == 1)
    {
        // Ask the word what we should compile into the new definition
        dictionary[token].code_comp(token);
    }
    else
    {
        // Do whatever the word wants to do
        dictionary[token].code_intp(token);
    }

    return 0;
}

int interpreter_error = 0;

int word_interpret_intp(int exe_token)
{
    // Hop to next word
    input_snap_past_space();
 
    dbgprintf("# Top of INTERPRET: %d\n", input_ptr);

    // Attempt to run this word
    int input_backup = input_ptr; // should this be put on stack?
    word_tick_intp(exe_token);

    if(peek_data() != -1)
    {
        word_execute_intp(exe_token);
    }
    else
    {
        pop_data();

        // Restore input pointer and see if it's a number
        input_ptr = input_backup;
        word_number_intp(exe_token);

        if(peek_data() == INT_MIN)
        {
            pop_data();

            if(input_len_until(' ') == 0)
                interpreter_error = 0;
            else
                interpreter_error = 1;
        }
    }

    stack_printer();
    return 0;
}

int word_quit_intp(int exe_token)
{
    while(input_buffer[input_ptr] != '\0' && input_ptr < 1024)
    {
        word_interpret_intp(exe_token);

        if(interpreter_error != 0)
        {
            printf("INTERPRET reported an error\n");
            break;
        }
    }

    printf(" ok \n");
    return 0;
}

int word_plus_intp(int exe_token)
{
    int a = pop_data(), b = pop_data();
    push_data(a+b);
    return 0;
}

int word_minus_intp(int exe_token)
{
    int a = pop_data(), b = pop_data();
    push_data(b-a);
    return 0;
}


int word_dot_intp(int exe_token)
{
    printf(" %d", pop_data());
    return 0;
}

int word_emit_intp(int exe_token)
{
    printf("%c", pop_data());
    return 0;
}

int word_cr_intp(int exe_token)
{
    printf("\n");
    return 0;
}

int word_printstr_intp(int exe_token)
{
    // This runs when ." is executed on the REPL. It prints a string from the current input position to the next quote.
    
    // Advance to the first non-space character
    while(input_buffer[input_ptr] == ' ') 
        input_ptr++;

    // Start printing
    while(input_buffer[input_ptr] != '"')
    {
        printf("%c", input_buffer[input_ptr]);
        input_ptr++;
    }
   
    // Gobble the quote
    input_ptr++;

    return 0;
}

int word_printstr_exec(int exe_token)
{
    // This runs when ." is executed from a colon definition. It prints the contents of the data field as a string.
    printf("%s", (char*)dictionary[exe_token].data);
    return 0;
}

int word_printstr_comp(int exe_token)
{
    // This runs when ." is being compiled into a function definition. It makes a new word in the dictionary
    // containing the contents of the input buffer to the next quote, and with the interpreter code to print it.
    
    // Advance to the first non-space character
    int curr_input = input_ptr;
    while(input_buffer[curr_input] == ' ') 
        curr_input++;

    // Figure out how big the string is
    int curr_len = 0;
    while(input_buffer[curr_input+curr_len] != '"')
        curr_len++;

    // Make a new string and copy this string into it
    char* new_str = (char*)malloc(sizeof(char) + curr_len + 2);
    strncpy(new_str, input_buffer+curr_input, curr_len*sizeof(char));
    new_str[curr_len] = '\0';

    // Advance real input pointer to that quotemark
    input_ptr = curr_input + curr_len;

    // Add a word to the dictionary for this string (no name bc we aren't allowed to call it)
    int new_str_word = add_word((dict_entry){"", word_printstr_exec, echo_word, (void*)new_str}); 

    // And return that word
    return new_str_word;
}

int word_colon_intp(int exe_token)
{
    // Set up a new entry
    top_word = add_word((dict_entry){"", run_word, echo_word, (void*)0});
    dictionary[top_word].data = (void*)malloc(sizeof(int)*1024);
    ((int*)(dictionary[top_word].data))[0] = -1;

    // Get a name for the next word
    input_snap_past_space();
    int name_len = input_len_until(' ');
    dictionary[top_word].name = (char*)malloc(sizeof(int)*(name_len+1));
    strncpy(dictionary[top_word].name, input_buffer+input_ptr, name_len);
    dictionary[top_word].name[name_len] = '\0';
    input_ptr += name_len;

    // Set compile state
    compiling_state = 1;

    return 0;
}

int word_semicolon_intp(int exe_token)
{
    // Turn off compilation and bypass EXECUTE fucking us over
    compiling_state = 0;

    return -1;
}

int word_pushrtrn_intp(int exe_token)
{
    push_rtrn(pop_data());
    return 0;
}

int word_poprtrn_intp(int exe_token)
{
    push_data(pop_rtrn());
    return 0;
}

int word_twodup_intp(int exe_token)
{
    int a = pop_data();
    int b = pop_data();

    push_data(b);
    push_data(a);
    push_data(b);
    push_data(a);

    return 0;
}

int word_equality_intp(int exe_token)
{
    int a = pop_data(), b = pop_data();
    
    if(a == b)
        push_data(-1);
    else
        push_data(0);

    return 0;
}

int word_do_intp(int exe_token)
{
    // Trigger an error cuz this shit is illegal
    interpreter_error = 1;
    return -1;
}

int word_do_comp(int exe_token)
{
    // Push the word counter onto the return stack for later retreival
    push_rtrn(top_word_count);

    // Save the index and limit to the return stack (when run)
    bytecode_add_inst(1, find_word(">r"));
    bytecode_add_inst(1, find_word(">r"));

    return 0;
}

int word_loop_intp(int exe_token)
{
    // Trigger an error cuz this shit is illegal
    interpreter_error = 1;
    return -1;
}

int word_loop_comp(int exe_token)
{
    // Calculate how far back to jump
    int last_do = pop_rtrn();
    int delta = (top_word_count+8) - last_do;

    // Run some instructions to check equality
    bytecode_add_inst(1, find_word("r>"));      // move limit back to data stack
    bytecode_add_inst(1, find_word("r>"));      // move index back to data stack
    bytecode_add_inst(5, 1);                    // put 1 on stack
    bytecode_add_inst(1, find_word("+"));       // increment index
    bytecode_add_inst(1, find_word("2dup"));    // duplicate params
    bytecode_add_inst(1, find_word("="));       // see if they were equal

    // Jump back if not done   
    bytecode_add_inst(4, 0);                    // skip next if index equals limit
    bytecode_add_inst(2, delta*2);              // jump backwards

    // Cleanup
    bytecode_add_inst(1, find_word("drop"));    // erase
    bytecode_add_inst(1, find_word("drop"));    // erase
}

int word_i_intp(int exe_token)
{
    push_data(rtrn_stack[rtrn_ptr-2]);
    return 0;
}


// Main

int main(int argc, char** argv)
{
    // Make a new word
    add_word((dict_entry){".\"", word_printstr_intp, word_printstr_comp, (void*)0});
    add_word((dict_entry){"cr", word_cr_intp, echo_word, (void*)0});
    add_word((dict_entry){".", word_dot_intp, echo_word, (void*)0});
    add_word((dict_entry){"emit", word_emit_intp, echo_word, (void*)0});
    add_word((dict_entry){"quit", word_quit_intp, echo_word, (void*)0});
    add_word((dict_entry){"interpret", word_interpret_intp, echo_word, (void*)0});
    add_word((dict_entry){"number", word_number_intp, echo_word, (void*)0});
    add_word((dict_entry){"'", word_tick_intp, echo_word, (void*)0});
    add_word((dict_entry){"execute", word_execute_intp, echo_word, (void*)0});
    add_word((dict_entry){"dup", word_dup_intp, echo_word, (void*)0});
    add_word((dict_entry){"drop", word_drop_intp, echo_word, (void*)0});
    add_word((dict_entry){"+", word_plus_intp, echo_word, (void*)0});
    add_word((dict_entry){"-", word_minus_intp, echo_word, (void*)0});
    add_word((dict_entry){":", word_colon_intp, echo_word, (void*)0});
    add_word((dict_entry){";", word_semicolon_intp, word_semicolon_intp, (void*)0});
    add_word((dict_entry){">r", word_pushrtrn_intp, echo_word, (void*)0});
    add_word((dict_entry){"r>", word_poprtrn_intp, echo_word, (void*)0});
    add_word((dict_entry){"2dup", word_twodup_intp, echo_word, (void*)0});
    add_word((dict_entry){"=", word_equality_intp, echo_word, (void*)0});
    add_word((dict_entry){"i", word_i_intp, echo_word, (void*)0});
    add_word((dict_entry){"do", word_do_intp, word_do_comp, (void*)0});
    add_word((dict_entry){"loop", word_loop_intp, word_loop_comp, (void*)0});
    
    //strcpy(input_buffer, ": ding 10 0 do 42 emit loop ; ding");
    //strcpy(input_buffer, ": ding 5 0 do 10 0 do 42 emit loop cr loop ; ding");
    //strcpy(input_buffer, ": ding 2 2 + . ; ding");

    strcpy(input_buffer, ": star 42 emit ; : stars 0 do star loop ; : square dup 0 do dup stars cr loop drop ; : triangle 1 + 1 do i stars cr loop ; : tower dup 1 - triangle square ; 6 tower");

    input_ptr = 0;
    word_quit_intp(0);

    return 0;
}
