/***********************************************************************
 *
 * ssim.c - Sequential Y86-64 simulator
 * 
 * Copyright (c) 2002, 2015. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 ***********************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "isa.h"
#include "sim.h"

#define MAXBUF 1024

#define MAXARGS 128
#define MAXBUF 1024
#define TKARGS 3

/***************
 * Begin Globals
 ***************/

/* Simulator name defined and initialized by the compiled HCL file */
/* according to the -n argument supplied to hcl2c */
char simname[] = "Y86-64 Processor: SEQ";

/* Parameters modifed by the command line */
char *object_filename;   /* The input object file name. */
FILE *object_file;       /* Input file handle */
bool_t verbosity = 2;    /* Verbosity level [TTY only] (-v) */ 
word_t instr_limit = 10000; /* Instruction limit [TTY only] (-l) */
bool_t do_check = FALSE; /* Test with YIS? [TTY only] (-t) */

/* keep a copy of mem and reg for diff display */
mem_t mem0, reg0;

/************* 
 * End Globals 
 *************/


/***************************
 * Begin function prototypes 
 ***************************/

static void usage(char *name);           /* Print helpful usage message */
static void run_tty_sim();               /* Run simulator in TTY mode */

/*************************
 * End function prototypes
 *************************/


/*******************************************************************
 * Part 1: This part is the initial entry point that handles general
 * initialization. It parses the command line and does any necessary
 * setup to run in TTY mode, and then starts the
 * simulation.
 * Do not change any of these.
 *******************************************************************/

/* 
 * sim_main - main simulator routine. This function is called from the
 * main() routine.
 */
int sim_main(int argc, char **argv)
{
    int i;
    int c;
    
    /* Parse the command line arguments */
    while ((c = getopt(argc, argv, "htl:v:")) != -1) {
	switch(c) {
	case 'h':
	    usage(argv[0]);
	    break;
	case 'l':
	    instr_limit = atoll(optarg);
	    break;
	case 'v':
	    verbosity = atoi(optarg);
	    if (verbosity < 0 || verbosity > 3) {
		printf("Invalid verbosity %d\n", verbosity);
		usage(argv[0]);
	    }
	    break;
	case 't':
	    do_check = TRUE;
	    break;
	default:
	    printf("Invalid option '%c'\n", c);
	    usage(argv[0]);
	    break;
	}
    }


    /* Do we have too many arguments? */
    if (optind < argc - 1) {
	printf("Too many command line arguments:");
	for (i = optind; i < argc; i++)
	    printf(" %s", argv[i]);
	printf("\n");
	usage(argv[0]);
    }


    /* The single unflagged argument should be the object file name */
    object_filename = NULL;
    object_file = NULL;
    if (optind < argc) {
	object_filename = argv[optind];
	object_file = fopen(object_filename, "r");
	if (!object_file) {
	    fprintf(stderr, "Couldn't open object file %s\n", object_filename);
	    exit(1);
	}
    }

    /* Otherwise, run the simulator in TTY mode (no -g flag) */
    run_tty_sim();

    exit(0);
}


int main(int argc, char *argv[]) {return sim_main(argc,argv);}


/* 
 * run_tty_sim - Run the simulator in TTY mode
 */
static void run_tty_sim() 
{
    word_t icount = 0;
    status = STAT_AOK;
    cc_t result_cc = 0;
    word_t byte_cnt = 0;
    state_ptr isa_state = NULL;


    /* In TTY mode, the default object file comes from stdin */
    if (!object_file) {
	object_file = stdin;
    }

    /* Initializations */
    if (verbosity >= 2)
	sim_set_dumpfile(stdout);
    sim_init();

    /* Emit simulator name */
    printf("%s\n", simname);

    byte_cnt = load_mem(mem, object_file, 1);
    if (byte_cnt == 0) {
	fprintf(stderr, "No lines of code found\n");
	exit(1);
    } else if (verbosity >= 2) {
	printf("%lld bytes of code read\n", byte_cnt);
    }
    fclose(object_file);
    if (do_check) {
	isa_state = new_state(0);
	free_mem(isa_state->r);
	free_mem(isa_state->m);
	isa_state->m = copy_mem(mem);
	isa_state->r = copy_mem(reg);
	isa_state->cc = cc;
    }

    mem0 = copy_mem(mem);
    reg0 = copy_mem(reg);
    

    icount = sim_run(instr_limit, &status, &result_cc);
    if (verbosity > 0) {
	printf("%lld instructions executed\n", icount);
	printf("Status = %s\n", stat_name(status));
	printf("Condition Codes: %s\n", cc_name(result_cc));
	printf("Changed Register State:\n");
	diff_reg(reg0, reg, stdout);
	printf("Changed Memory State:\n");
	diff_mem(mem0, mem, stdout);
    }
    if (do_check) {
	byte_t e = STAT_AOK;
	int step;
	bool_t match = TRUE;

	for (step = 0; step < instr_limit && e == STAT_AOK; step++) {
	    e = step_state(isa_state, stdout);
	}

	if (diff_reg(isa_state->r, reg, NULL)) {
	    match = FALSE;
	    if (verbosity > 0) {
		printf("ISA Register != Pipeline Register File\n");
		diff_reg(isa_state->r, reg, stdout);
	    }
	}
	if (diff_mem(isa_state->m, mem, NULL)) {
	    match = FALSE;
	    if (verbosity > 0) {
		printf("ISA Memory != Pipeline Memory\n");
		diff_mem(isa_state->m, mem, stdout);
	    }
	}
	if (isa_state->cc != result_cc) {
	    match = FALSE;
	    if (verbosity > 0) {
		printf("ISA Cond. Codes (%s) != Pipeline Cond. Codes (%s)\n",
		       cc_name(isa_state->cc), cc_name(result_cc));
	    }
	}
	if (match) {
	    printf("ISA Check Succeeds\n");
	} else {
	    printf("ISA Check Fails\n");
	}
    }
}



/*
 * usage - print helpful diagnostic information
 */
static void usage(char *name)
{
    printf("Usage: %s [-htg] [-l m] [-v n] file.yo\n", name);
    printf("   -h     Print this message\n");
    printf("   -l m   Set instruction limit to m [TTY mode only] (default %lld)\n", instr_limit);
    printf("   -v n   Set verbosity level to 0 <= n <= 3 [TTY mode only] (default %d)\n", verbosity);
    printf("   -t     Test result against ISA simulator (yis) [TTY mode only]\n");
    exit(0);
}



/*********************************************************
 * Part 2: This part contains the core simulator routines.
 * You only need to modify function sim_step()
 *********************************************************/

/**********************
 * Begin Part 2 Globals
 **********************/

/*
 * Variables related to hardware units in the processor
 */
mem_t mem;  /* Instruction and data memory */
word_t minAddr = 0;
word_t memCnt = 0;

/* Other processor state */
mem_t reg;               /* Register file */
cc_t cc = DEFAULT_CC;    /* Condition code register */
cc_t cc_in = DEFAULT_CC; /* Input to condition code register */

/* Program Counter */
word_t pc = 0; /* Program counter value */
word_t pc_in = 0;/* Input to program counter */

/* Intermediate values */
byte_t imem_icode = I_NOP;
byte_t imem_ifun = F_NONE;
byte_t icode = I_NOP;
word_t ifun = 0;
byte_t instr = HPACK(I_NOP, F_NONE);
word_t ra = REG_NONE;
word_t rb = REG_NONE;
word_t valc = 0;
word_t valp = 0;
bool_t imem_error;
bool_t instr_valid;

word_t srcA = REG_NONE;
word_t srcB = REG_NONE;
word_t destE = REG_NONE;
word_t destM = REG_NONE;
word_t vala = 0;
word_t valb = 0;
word_t vale = 0;

bool_t bcond = FALSE;
bool_t cond = FALSE;
word_t valm = 0;
bool_t dmem_error;

bool_t mem_write = FALSE;
word_t mem_addr = 0;
word_t mem_data = 0;
byte_t status = STAT_AOK;

/* Log file */
FILE *dumpfile = NULL;


/********************
 * End Part 2 Globals
 ********************/


static int initialized = 0;
void sim_init()
{

    /* Create memory and register files */
    initialized = 1;
    mem = init_mem(MEM_SIZE);
    reg = init_reg();
    sim_reset();
    clear_mem(mem);
}

void sim_reset()
{
    if (!initialized)
	sim_init();
    clear_mem(reg);
    minAddr = 0;
    memCnt = 0;

	pc_in = 0;
    cc = DEFAULT_CC;
    cc_in = DEFAULT_CC;
    destE = REG_NONE;
    destM = REG_NONE;
    mem_write = FALSE;
    mem_addr = 0;
    mem_data = 0;

    /* Reset intermediate values to clear display */
    icode = I_NOP;
    ifun = 0;
    instr = HPACK(I_NOP, F_NONE);
    ra = REG_NONE;
    rb = REG_NONE;
    valc = 0;
    valp = 0;

    srcA = REG_NONE;
    srcB = REG_NONE;
    destE = REG_NONE;
    destM = REG_NONE;
    vala = 0;
    valb = 0;
    vale = 0;

    cond = FALSE;
    bcond = FALSE;
    valm = 0;
}

/* Update the processor state */
static void update_state()
{
	pc = pc_in;
    cc = cc_in;
    /* Writeback */
    if (destE != REG_NONE)
	set_reg_val(reg, destE, vale);
    if (destM != REG_NONE)
	set_reg_val(reg, destM, valm);

    if (mem_write) {
      /* Should have already tested this address */
        set_word_val(mem, mem_addr, mem_data);
	    sim_log("Wrote 0x%llx to address 0x%llx\n", mem_data, mem_addr);
    }
}

/*****************************************************************
 * This is the only function you need to modify for SEQ simulator.
 * It executes one instruction but split it into multiple stages.
 * For each stage, you should update the corresponding intermediate 
 * values. At the end of this function, you should make sure the 
 * global state values [mem, reg, cc, pc] are updated correctly, 
 * and then return the correct status.
 *****************************************************************/

static byte_t sim_step()
{
    status = STAT_AOK;
    imem_error = dmem_error = FALSE;

    update_state(); /* Update state from last cycle */

    /*********************** Fetch stage ************************
     * TODO: update [icode, ifun, instr, ra, rb, valc, valp, status]
     * you may find these functions useful: 
     * HPACK(), get_byte_val(), get_word_val(), HI4(), LO4()
     ************************************************************/

    /* dummy placeholders, replace them with your implementation */
		byte_t tempB;
		dmem_error |= !get_byte_val(mem, pc, &instr);
		icode = HI4(instr);
    ifun = LO4(instr);
		ra = REG_NONE;
		rb = REG_NONE;
		valc = 0;

		switch (instr) {
			case HPACK(I_NOP, F_NONE):
				valp = pc + 1;
				break;
			case HPACK(I_HALT, F_NONE):
				valp = pc + 1;
				break;

			case HPACK(I_RRMOVQ, F_NONE): 
			case HPACK(I_RRMOVQ, C_LE): 
			case HPACK(I_RRMOVQ, C_L): 
			case HPACK(I_RRMOVQ, C_E): 
			case HPACK(I_RRMOVQ, C_NE): 
			case HPACK(I_RRMOVQ, C_GE): 
			case HPACK(I_RRMOVQ, C_G): 
				dmem_error |= !get_byte_val(mem, pc + 1, &tempB);
				ra = HI4(tempB);
				rb = LO4(tempB);
				valp = pc + 2;
				break;

			case HPACK(I_IRMOVQ, F_NONE):
				dmem_error |= !get_byte_val(mem, pc + 1, &tempB);
				rb = LO4(tempB);
				dmem_error |= !get_word_val(mem, pc + 2, &valc);
				valp = pc + 10;
				break;

			case HPACK(I_RMMOVQ, F_NONE): 
				dmem_error |= !get_byte_val(mem, pc + 1, &tempB);
				ra = HI4(tempB);
				rb = LO4(tempB);
				dmem_error |= !get_word_val(mem, pc + 2, &valc);
				valp = pc + 10;
				break;

			case HPACK(I_MRMOVQ, F_NONE): 
				dmem_error |= !get_byte_val(mem, pc + 1, &tempB);
				ra = HI4(tempB);
				rb = LO4(tempB);
				dmem_error |= !get_word_val(mem, pc + 2, &valc);
				valp = pc + 10;
				break;

			case HPACK(I_ALU, A_ADD): 
			case HPACK(I_ALU, A_SUB): 
			case HPACK(I_ALU, A_AND): 
			case HPACK(I_ALU, A_XOR): 
				dmem_error |= !get_byte_val(mem, pc + 1, &tempB);
				ra = HI4(tempB);
				rb = LO4(tempB);
				valp = pc + 2;
				break;

			case HPACK(I_JMP, C_YES): 
			case HPACK(I_JMP, C_LE): 
			case HPACK(I_JMP, C_L): 
			case HPACK(I_JMP, C_E): 
			case HPACK(I_JMP, C_NE): 
			case HPACK(I_JMP, C_GE): 
			case HPACK(I_JMP, C_G): 
				dmem_error |= !get_word_val(mem, pc + 1, &valc);
				valp = pc + 9;
				break;

			case HPACK(I_CALL, F_NONE):
				dmem_error |= !get_word_val(mem, pc + 1, &valc);
				valp = pc + 9;
				break;

			case HPACK(I_RET, F_NONE):
				valp = pc + 1;
				break;

			case HPACK(I_PUSHQ, F_NONE): 
				dmem_error |= !get_byte_val(mem, pc + 1, &tempB);
				ra = HI4(tempB);
				rb = LO4(tempB);
				valp = pc + 2;
				break;

			case HPACK(I_POPQ, F_NONE):
				dmem_error |= !get_byte_val(mem, pc + 1, &tempB);
				ra = HI4(tempB);
				rb = LO4(tempB);
				valp = pc + 2;
				break;
				
			default:
				imem_error = TRUE;
				printf("Invalid instruction\n");
				break;
		}

    /* logging function, do not change this */
    sim_log("IF: Fetched %s at 0x%llx.  ra=%s, rb=%s, valC = 0x%llx\n",
	    iname(HPACK(icode,ifun)), pc, reg_name(ra), reg_name(rb), valc);
    
    /*********************** Decode stage ************************
     * TODO: update [srcA, srcB, destE, destM, vala, valb]
     * you may find these functions useful: 
     * get_reg_val(), cond_holds()
     *************************************************************/

    /* dummy placeholders, replace them with your implementation */
    srcA = REG_NONE;
    srcB = REG_NONE;
    destE = REG_NONE;
    destM = REG_NONE;
    vala = 0;
    valb = 0;
		switch (icode) {
			case I_HALT: break;

			case I_NOP: break;
		
			case I_RRMOVQ: // aka CMOVQ
				srcA = ra;
				destE = rb;
				break;

			case I_IRMOVQ:
				destE = rb;
				break;
				
			case I_RMMOVQ:
				srcA = ra;
				srcB = rb;
				break;
				
			case I_MRMOVQ:
				srcB = rb;
				destM = ra;
				break;

			case I_ALU:
				srcA = ra;
				srcB = rb;
				destE = rb;
				break;

			case I_JMP: break;

			case I_CALL:
				srcB = REG_RSP;
				destE = REG_RSP;
				break;
				
			case I_RET:
				srcA = REG_RSP;
				srcB = REG_RSP;
				destE = REG_RSP;
				break;

			case I_PUSHQ:
				srcA = ra;
				srcB = REG_RSP;
				destE = REG_RSP;
				break;
				
			case I_POPQ:
				srcA = REG_RSP;
				srcB = REG_RSP;
				destE = REG_RSP;
				destM = ra;
				break;

			default:
				printf("icode is not valid (%d)", icode);
				break;
		}

		vala = get_reg_val(reg, srcA);
		valb = get_reg_val(reg, srcB);

    /*********************** Execute stage **********************
     * TODO: update [vale, cc_in]
     * you may find these functions useful: 
     * compute_alu(), compute_cc()
     ************************************************************/

    /* dummy placeholders, replace them with your implementation */
    vale = 0;
    cc_in = cc;
		bool_t cnd = FALSE;

		switch (icode) {
			case I_HALT: break;

			case I_NOP: break;
		
			case I_RRMOVQ: // aka CMOVQ
				vale = vala;
				break;

			case I_IRMOVQ:
				vale = valc;
				break;
				
			case I_RMMOVQ:
				vale = valb + valc;
				break;
				
			case I_MRMOVQ:
				vale = valb + vala;
				break;

			case I_ALU:
				vale = compute_alu(ifun, vala, valb);
				cc_in = compute_cc(ifun, vala, valb);
				break;

			case I_JMP:
				cnd = cond_holds(cc, ifun);
				break;

			case I_CALL:
				vale = valb - 8;
				break;
				
			case I_RET:
				vale = valb + 8;
				break;

			case I_PUSHQ:
				vale = valb - 8;
				break;
				
			case I_POPQ:
				vale = valb + 8;
				break;

			default:
				printf("icode is not valid (%d)", icode);
				break;
		}

    /*********************** Memory stage ***********************
     * TODO: update [valm, mem_write, mem_addr, mem_data, status]
     * you may find these functions useful: 
     * get_word_val()
     ************************************************************/

    /* dummy placeholders, replace them with your implementation */
    valm = 0;
    mem_write = FALSE;
    mem_addr = 0;
    mem_data = 0;
    status = STAT_AOK;

		switch (icode) {
			case I_HALT:
				status = STAT_HLT;
				break;

			case I_NOP: break;
		
			case I_RRMOVQ: break; // aka CMOVQ

			case I_IRMOVQ: break;
				
			case I_RMMOVQ:
				mem_write = TRUE;
				mem_addr = vale;
				mem_data = vala;
				break;
				
			case I_MRMOVQ:
				dmem_error |= !get_word_val(mem, vale, &valm);
				break;

			case I_ALU: break;

			case I_JMP: break;

			case I_CALL:
				mem_write = TRUE;
				mem_addr = vale;
				mem_data = valp;
				break;
				
			case I_RET:
				dmem_error |= !get_word_val(mem, vala, &valm);
				break;

			case I_PUSHQ:
				mem_write = TRUE;
				mem_addr = vale;
				mem_data = vala;
				break;
				
			case I_POPQ:
				dmem_error |= !get_word_val(mem, vala, &valm);
				break;

			default:
				printf("icode is not valid (%d)", icode);
				break;
		}
		
		if (mem_write)
			dmem_error |= !set_word_val(mem, mem_addr, mem_data);

    /****************** Program Counter Update ******************
     * TODO: update [pc_in]
     ************************************************************/

	   /* dummy placeholders, replace them with your implementation */
    pc_in = 0; /* should not overwrite original pc */
	
		switch (icode) {
			case I_HALT:
				pc_in = valp;
				break;

			case I_NOP:
				pc_in = valp;
				break;
		
			case I_RRMOVQ: // aka CMOVQ
				pc_in = valp;
				break;

			case I_IRMOVQ:
				pc_in = valp;
				break;
				
			case I_RMMOVQ:
				pc_in = valp;
				break;
				
			case I_MRMOVQ:
				pc_in = valp;
				break;

			case I_ALU:
				pc_in = valp;
				break;

			case I_JMP:
				pc_in = cnd ? valc : valp;
				break;

			case I_CALL:
				pc_in = valc;
				break;
				
			case I_RET:
				pc_in = valm;
				break;

			case I_PUSHQ:
				pc_in = valp;
				break;
				
			case I_POPQ:
				pc_in = valp;
				break;

			default:
				printf("icode is not valid (%d)", icode);
				break;

		}

    return imem_error 
			? STAT_INS
			: dmem_error 
			? STAT_ADR 
			: status;
}

/*
  Run processor until one of following occurs:
  - An error status is encountered in WB.
  - max_instr instructions have completed through WB

  Return number of instructions executed.
  if statusp nonnull, then will be set to status of final instruction
  if ccp nonnull, then will be set to condition codes of final instruction
*/
word_t sim_run(word_t max_instr, byte_t *statusp, cc_t *ccp)
{
    word_t icount = 0;
    byte_t run_status = STAT_AOK;
    while (icount < max_instr) {
        if (verbosity == 3) {
            sim_log("-------- Step %d --------\n", icount + 1);
        }
        run_status = sim_step();
        icount++;

        /* print step-wise diff if verbosity = 3 */
        if (verbosity == 3) {
            sim_log("Status '%s', CC %s\n", stat_name(status), cc_name(cc_in));
            sim_log("Changes to registers:\n");
            diff_reg(reg0, reg, stdout);

            printf("\nChanges to memory:\n");
            diff_mem(mem0, mem, stdout);
            printf("\n");
        }

        if (run_status != STAT_AOK)
            break;
    }
    if (statusp)
	*statusp = run_status;
    if (ccp)
	*ccp = cc;
    return icount;
}

/* If dumpfile set nonNULL, lots of status info printed out */
void sim_set_dumpfile(FILE *df)
{
    dumpfile = df;
}

/*
 * sim_log dumps a formatted string to the dumpfile, if it exists
 * accepts variable argument list
 */
void sim_log( const char *format, ... ) {
    if (dumpfile) {
	va_list arg;
	va_start( arg, format );
	vfprintf( dumpfile, format, arg );
	va_end( arg );
    }
}