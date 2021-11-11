#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

// Dictionary and function that runs other words
typedef struct dict_entry
{
    char* name;                 // Function name (if applic)
    int (*code_intp)(int);      // Code to run when word is being executed
    int (*code_comp)(int);      // Code to run when word is being compiled
    void* data;                 // Data for whatever
} dict_entry;

dict_entry dictionary[1024];
int dict_size = 0;

int add_word(dict_entry entry)
{
    dictionary[dict_size] = entry;
    dict_size++;
    return dict_size - 1;
}

int echo_word(int exe_token)
{
    return exe_token;
}

int no_compile_word(int exe_token)
{
    return -1;
}

int run_word(int exe_token)
{
    int* wordlist = (int*)dictionary[exe_token].data;
    int count = 0;

    for(int w=0;w<wordlist[0];w++)
        (*dictionary[w].code_intp)(w);

    return 0;
}


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

// Input stream
char input_buffer[1024];
int input_ptr = 0;

int input_len_until(char tok)
{
    int len = 0;

    while(input_buffer[input_ptr+len] != tok)
        len++;

    return len;
}

void input_snap_to_space()
{
    if(input_buffer[input_ptr] == ' ')
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

int word_tick_intp(int exe_token)
{
    // Find length of word to attempt searching for
    int word_len = input_len_until(' ');
printf("## Call to TICK: found word from %d to %d, '%.*s'\n", input_ptr, word_len, word_len, input_buffer+input_ptr);

    // Search down the dictionary
    int found = dict_size-1;
    
    while(found >= 0)
    {
        if(strncmp(dictionary[found].name, input_buffer, word_len) == 0)
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


int word_number_intp(int exe_token)
{
    // Find length of number to grab
    int num_len = input_len_until(' ');
    int converted_num = 0;
    int neg_mult = 1;

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

    push_data(converted_num * neg_mult);

    return 0;
}

int word_execute_intp(int exe_token)
{
    int token = pop_data();
printf("### EXECUTEing word %d: %s\n", token, dictionary[token].name);
    dictionary[token].code_intp(token);
    return 0;
}

int interpreter_error = 0;

int word_interpret_intp(int exe_token)
{
    // Hop to next word
    input_snap_past_space();
 
printf("# Top of INTERPRET: %d\n", input_ptr);

    // Attempt to run this word
    int input_backup = input_ptr; // should this be put on stack?
    word_tick_intp(exe_token);

    if(peek_data() != -1)
    {
        word_execute_intp(exe_token);
    }
    else
    {
        // Restore input pointer and see if it's a number
        input_ptr = input_backup;
        word_number_intp(exe_token);

        if(peek_data() == INT_MIN)
        {
            pop_data();
            interpreter_error = 1;
        }
    }

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

int word_dot_intp(int exe_token)
{
    printf("%d", pop_data());
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
    
    strcpy(input_buffer, ".\" hello world\" 42 emit cr");

    input_ptr = 0;
    word_quit_intp(0);

    return 0;
}
